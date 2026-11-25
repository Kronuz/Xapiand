/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// The shard group's application (src/database/shard_group.h; cluster-release D3, D12, D16), driven entry by entry
// as the group's chore drives it: effects, the strata write, advance. Blobs land at the offsets the state names,
// volumes rotate and are numbered in log order, a chunked command applies at its final entry, and a lower term's
// open command is retired at the first entry of a higher one.

#include "test_harness.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <span>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "database/command.h"
#include "database/data.h"
#include "database/shard_group.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/strata/io_options.h"  // for engine::strata::log_barrier_warning
#include "engine/strata/shard.h"   // for engine::strata::index_options, new_uuid
#include "opts.h"                  // for opts

namespace {

namespace cs = kronuz::consensus;

std::string pattern(std::size_t n, char seed) {
	std::string out(n, '\0');
	for (std::size_t i = 0; i < n; ++i) {
		out[i] = static_cast<char>(seed + static_cast<char>(i * 131 % 251));
	}
	return out;
}

// A replica: an index, its file set and the application over them, in a fresh directory.
struct Replica {
	std::filesystem::path dir;
	std::optional<kronuz::journal::PosixIO> io;
	std::optional<prospect::index> index;
	std::optional<strata::group::file_set> files;
	std::optional<ShardApplication> app;
	ShardGroupState state;
	std::uint64_t at = 0;   // the last entry applied

	explicit Replica(const char* name) : dir(std::filesystem::current_path() / name) {
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		io.emplace(dir, kronuz::journal::PosixIOOptions{kronuz::journal::FlushMode::plain});
		index.emplace(prospect::index::create(*io, engine::strata::index_options({}), engine::strata::new_uuid()));
		files.emplace(*io);
		app.emplace(*index);
	}
	~Replica() {
		files.reset();
		index.reset();
		io.reset();
		std::filesystem::remove_all(dir);
	}

	// Applies one entry; returns the transaction it wrote, if any.
	std::optional<prospect::transaction> apply(std::string_view bytes, std::uint64_t term, cs::EntryKind kind = cs::EntryKind::Command) {
		const strata::group::entry e{++at, term, kind, bytes};
		app->effects(state, e, *files);
		auto t = app->transaction(state, e);
		if (t) {
			const auto v = index->apply(*t, {});
			CHECK(v.result == ::strata::verdict::accepted);
		}
		app->advance(state, e);
		return t;
	}
};

// A document whose data names a blob of `ct` by a placeholder locator, as prepare leaves it.
std::string document(const std::string& key, const std::string& ct, std::size_t blob) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), key);
	Data data;
	data.update(ct_type_t(ct), 0, 0, blob);
	data.flush();
	doc.set_data(data.serialize());
	return doc.serialize();
}

CommandWrite put(const std::string& key, const std::string& ct, std::string record) {
	CommandWrite w;
	w.op = CommandOp::replace_key;
	w.key = key;
	w.expected = 0;
	w.document.bytes = document(key, ct, record.size());
	w.blobs.push_back({ct, CommandBytes{std::move(record)}});
	return w;
}

// The locator the transaction's op `i` stores for `ct`.
const Locator* locator_of(const prospect::transaction& t, std::size_t i, const std::string& ct, Data& keep) {
	keep = Data(std::string(t.ops[i].doc.stored));
	return keep.get(ct_type_t(ct));
}

// A batch of two writes: each blob at the offset after the last, and the locators say so.
void test_blobs_at_the_offsets_the_state_names() {
	Replica r("shard_group_offsets");
	Command command;
	command.writes.push_back(put("Ka", "image/png", pattern(100, 'a')));
	command.writes.push_back(put("Kb", "text/plain", pattern(37, 'b')));
	const auto t = r.apply(encode_entry(command), 1);
	REQUIRE(t.has_value());
	REQUIRE_EQ(t->ops.size(), 2u);
	Data a, b;
	const auto* la = locator_of(*t, 0, "image/png", a);
	const auto* lb = locator_of(*t, 1, "text/plain", b);
	REQUIRE(la != nullptr);
	REQUIRE(lb != nullptr);
	CHECK_EQ(la->volume, 1);
	CHECK_EQ(la->offset, 0u);
	CHECK_EQ(la->size, 100u);
	CHECK_EQ(lb->volume, 1);
	CHECK_EQ(lb->offset, 100u);
	CHECK(r.files->read(volume_name(1), 100, 37) == pattern(37, 'b'));
	CHECK_EQ(r.state.length, 137u);

	// Applied again from the same starting state (a retry, a replay), the effects rewrite the same bytes.
	ShardGroupState again;
	const auto bytes = encode_entry(command);
	const strata::group::entry e{1, 1, cs::EntryKind::Command, bytes};
	r.app->effects(again, e, *r.files);
	CHECK_EQ(r.files->length(volume_name(1)), 137u);
	CHECK(r.files->read(volume_name(1), 0, 100) == pattern(100, 'a'));
}

// A record that would take the volume past VOLUME_BYTES opens the next volume number; the last one is sealed.
void test_rotation_in_log_order() {
	ShardGroupState s;
	s.length = VOLUME_BYTES - 10;
	Command command;
	command.writes.push_back(put("Ka", "a/a", pattern(8, 'a')));
	command.writes.push_back(put("Kb", "a/b", pattern(8, 'b')));
	const auto placement = place_blobs(s, command);
	CHECK(placement.at[0][0] == std::make_pair(std::uint64_t(1), VOLUME_BYTES - 10));
	CHECK(placement.at[1][0] == std::make_pair(std::uint64_t(2), std::uint64_t(0)));
	CHECK(placement.sealed == std::vector<std::uint64_t>{1});
	CHECK_EQ(placement.after.volume, 2u);
	CHECK_EQ(placement.after.next, 3u);
	CHECK_EQ(placement.after.length, 8u);
}

// A command over the entry limit applies at its final entry, with its staged bytes read back from the staging,
// which retires; its blob lands in the current volume.
void test_chunked_command_applies_at_its_final_entry() {
	Replica r("shard_group_chunked");
	Command command;
	command.writes.push_back(put("Kbig", "application/x", pattern(3 * 1024 * 1024 / 2, 'z')));
	const std::uint64_t id = (1ull << 32) | 7;
	const auto entries = split_command(command, id);
	REQUIRE(entries.size() > 2u);
	for (std::size_t i = 0; i + 1 < entries.size(); ++i) {
		CHECK_FALSE(r.apply(entries[i], 1).has_value());
		CHECK_EQ(r.state.open.count(id), 1u);
	}
	CHECK(r.files->live(staging_name(id)));
	const auto t = r.apply(entries.back(), 1);
	REQUIRE(t.has_value());
	CHECK_EQ(r.state.open.count(id), 0u);
	CHECK_FALSE(r.files->live(staging_name(id)));
	Data keep;
	const auto* l = locator_of(*t, 0, "application/x", keep);
	REQUIRE(l != nullptr);
	CHECK_EQ(l->volume, 1);
	CHECK(r.files->read(volume_name(1), 0, 3 * 1024 * 1024 / 2) == pattern(3 * 1024 * 1024 / 2, 'z'));
	CHECK(r.index->find_key("Kbig").has_value());
}

// An upload streams into a volume of its own, numbered at its first chunk; its final entry's blob names it.
void test_upload_into_its_own_volume() {
	Replica r("shard_group_upload");
	const std::uint64_t id = (1ull << 32) | 9;
	const auto bytes = pattern(5000, 'u');
	r.apply(encode_entry(CommandChunk{id, CommandStream::upload, 0, std::string_view(bytes).substr(0, 3000)}), 1);
	r.apply(encode_entry(CommandChunk{id, CommandStream::upload, 3000, std::string_view(bytes).substr(3000)}), 1);
	CHECK_EQ(r.state.open.at(id).upload, 2u);
	CHECK_EQ(r.state.next, 3u);
	auto w = put("Kup", "video/mp4", std::string());
	w.document.bytes = document("Kup", "video/mp4", bytes.size());
	w.blobs[0].record = CommandBytes{{}, true, CommandStream::upload, 0, bytes.size()};
	Command command;
	command.writes.push_back(std::move(w));
	const auto t = r.apply(encode_entry(CommandFinal{id, command}), 1);
	REQUIRE(t.has_value());
	Data keep;
	const auto* l = locator_of(*t, 0, "video/mp4", keep);
	REQUIRE(l != nullptr);
	CHECK_EQ(l->volume, 2);
	CHECK_EQ(l->offset, 0u);
	CHECK(r.files->read(volume_name(2), 0, bytes.size()) == bytes);
	CHECK_EQ(r.state.length, 0u);   // the current volume took nothing
	CHECK(r.state.open.empty());
}

// The first entry of a higher term retires every open chunked command of a lower one: its staging and its upload's
// volume, and its final entry then writes nothing.
void test_higher_term_retires_open_commands() {
	Replica r("shard_group_term");
	Command command;
	command.writes.push_back(put("Kbig", "application/x", pattern(2 * 1024 * 1024, 'q')));
	const std::uint64_t id = (1ull << 32) | 3;
	const auto entries = split_command(command, id);
	r.apply(entries[0], 1);
	r.apply(encode_entry(CommandChunk{id + 1, CommandStream::upload, 0, "abc"}), 1);
	CHECK_EQ(r.state.open.size(), 2u);
	CHECK(r.files->live(volume_name(2)));
	r.apply(std::string_view(), 2, cs::EntryKind::NoOp);   // the new leader's first entry
	CHECK(r.state.open.empty());
	CHECK_FALSE(r.files->live(staging_name(id)));
	CHECK_FALSE(r.files->live(volume_name(2)));
	CHECK_FALSE(r.apply(entries.back(), 2).has_value());   // a final entry the new term carried anyway
	CHECK_FALSE(r.index->find_key("Kbig").has_value());
}

// A chunk or final entry placed in a later term than its id names applies nothing (A9): at rf > 1 a deposed
// leader's proposer still places the chunks it had queued, after the new term's first entry retired the command.
void test_entries_outside_their_term_apply_nothing() {
	Replica r("shard_group_cut");
	Command command;
	command.writes.push_back(put("Kcut", "application/x", pattern(2 * 1024 * 1024, 'k')));
	const std::uint64_t id = (1ull << 32) | 11;
	const auto entries = split_command(command, id);
	REQUIRE(entries.size() >= 4u);
	r.apply(entries[0], 1);
	r.apply(entries[1], 1);
	r.apply(std::string_view(), 2, cs::EntryKind::NoOp);   // the new leader's first entry retires the command
	CHECK_FALSE(r.files->live(staging_name(id)));
	for (std::size_t i = 2; i < entries.size(); ++i) {
		bool threw = false;
		std::optional<prospect::transaction> t;
		try {
			t = r.apply(entries[i], 2);
		} catch (const std::exception& e) {
			threw = true;
			std::fprintf(stderr, "  entry %zu at term 2: %s\n", i, e.what());
		}
		CHECK_FALSE(threw);
		CHECK_FALSE(t.has_value());
		CHECK_FALSE(r.files->live(staging_name(id)));
		CHECK(r.state.open.empty());
	}
	CHECK_FALSE(r.index->find_key("Kcut").has_value());
	CHECK_EQ(r.state.length, 0u);
}

// An abort (a final entry with no writes) retires its upload's volume (A9): an abandoned upload frees its space.
// A final whose blob names the upload seals it instead.
void test_abort_retires_its_upload_volume() {
	Replica r("shard_group_abort");
	const std::uint64_t id = (1ull << 32) | 12;
	r.apply(encode_entry(CommandChunk{id, CommandStream::upload, 0, "abandoned"}), 1);
	const auto volume = r.state.open.at(id).upload;
	CHECK(r.files->live(volume_name(volume)));
	CHECK_FALSE(r.apply(encode_entry(CommandFinal{id, Command{}}), 1).has_value());
	CHECK_FALSE(r.files->live(volume_name(volume)));
	CHECK(r.state.open.empty());

	const std::uint64_t kept = (1ull << 32) | 13;
	const auto bytes = pattern(100, 'k');
	r.apply(encode_entry(CommandChunk{kept, CommandStream::upload, 0, bytes}), 1);
	const auto named = r.state.open.at(kept).upload;
	auto w = put("Kkept", "video/mp4", std::string());
	w.document.bytes = document("Kkept", "video/mp4", bytes.size());
	w.blobs[0].record = CommandBytes{{}, true, CommandStream::upload, 0, bytes.size()};
	Command final;
	final.writes.push_back(std::move(w));
	REQUIRE(r.apply(encode_entry(CommandFinal{kept, final}), 1).has_value());
	CHECK(r.files->live(volume_name(named)));
	CHECK(r.files->read(volume_name(named), 0, bytes.size()) == bytes);
}

// The slot's IO as the file set sees it, recording the largest read asked of any of its files.
class ReadBoundFile final : public kronuz::journal::File {
public:
	ReadBoundFile(std::unique_ptr<kronuz::journal::File> f, std::size_t& largest) : f_(std::move(f)), largest_(&largest) {}
	std::uint64_t size() override { return f_->size(); }
	std::size_t read_at(std::uint64_t o, std::span<char> b) override {
		*largest_ = std::max(*largest_, b.size());
		return f_->read_at(o, b);
	}
	std::size_t write_at(std::uint64_t o, std::string_view b) override { return f_->write_at(o, b); }
	void truncate(std::uint64_t n) override { f_->truncate(n); }
	void sync() override { f_->sync(); }
	void sync_data() override { f_->sync_data(); }

private:
	std::unique_ptr<kronuz::journal::File> f_;
	std::size_t* largest_;
};

class ReadBoundIO final : public kronuz::journal::IO {
public:
	ReadBoundIO(kronuz::journal::IO& in, std::size_t& largest) : in_(&in), largest_(&largest) {}
	std::unique_ptr<kronuz::journal::OwnerLock> acquire_owner(bool c) override { return in_->acquire_owner(c); }
	std::unique_ptr<kronuz::journal::File> open_existing(std::string_view n) override { return wrap(in_->open_existing(n)); }
	std::unique_ptr<kronuz::journal::File> create_exclusive(std::string_view n) override { return wrap(in_->create_exclusive(n)); }
	std::unique_ptr<kronuz::journal::File> open_existing_read_only(std::string_view n) override { return wrap(in_->open_existing_read_only(n)); }
	void open_existing_read_only_into(std::string_view n, kronuz::journal::File& f) override { in_->open_existing_read_only_into(n, f); }
	std::unique_ptr<kronuz::journal::File> open_reclaim_candidate_read_only(std::string_view n) override { return in_->open_reclaim_candidate_read_only(n); }
	bool open_reclaim_candidate_read_only_into(std::string_view n, kronuz::journal::File& f) override { return in_->open_reclaim_candidate_read_only_into(n, f); }
	void replace(std::string_view a, std::string_view b) override { in_->replace(a, b); }
	void remove(std::string_view n) override { in_->remove(n); }
	std::unique_ptr<kronuz::journal::DirectoryCursor> scan_directory() override { return in_->scan_directory(); }
	void sync_directory() override { in_->sync_directory(); }

private:
	std::unique_ptr<kronuz::journal::File> wrap(std::unique_ptr<kronuz::journal::File> f) { return std::make_unique<ReadBoundFile>(std::move(f), *largest_); }
	kronuz::journal::IO* in_;
	std::size_t* largest_;
};

// A completed upload's seal reads its volume in bounded pieces (review R2): an upload sixteen times strata's 64 KiB
// read, applied as chunks and a final, seals to its whole bytes' CRC32C with no read past the bound, so a
// volume of any size (a 32 GiB rotation, a multi-GiB upload) seals without holding it in memory.
void test_a_seal_reads_its_volume_in_bounded_pieces() {
	constexpr std::size_t bound = 64 * 1024;
	const auto dir = std::filesystem::current_path() / "shard_group_bounded_seal";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	std::size_t largest = 0;
	{
		kronuz::journal::PosixIO io(dir, kronuz::journal::PosixIOOptions{kronuz::journal::FlushMode::plain});
		auto index = prospect::index::create(io, engine::strata::index_options({}), engine::strata::new_uuid());
		ReadBoundIO bounded(io, largest);
		strata::group::file_set files(bounded);
		ShardApplication app(index);
		ShardGroupState state;
		std::uint64_t at = 0;
		const auto apply = [&](std::string_view bytes) {
			const strata::group::entry e{++at, 1, cs::EntryKind::Command, bytes};
			app.effects(state, e, files);
			auto t = app.transaction(state, e);
			if (t) {
				CHECK(index.apply(*t, {}).result == ::strata::verdict::accepted);
			}
			app.advance(state, e);
			return t.has_value();
		};
		const std::uint64_t id = (1ull << 32) | 21;
		const auto bytes = pattern(16 * bound + 7, 's');
		for (std::size_t off = 0; off < bytes.size(); off += 3 * bound) {
			apply(encode_entry(CommandChunk{id, CommandStream::upload, off, std::string_view(bytes).substr(off, 3 * bound)}));
		}
		const auto volume = state.open.at(id).upload;
		auto w = put("Kbig", "video/mp4", std::string());
		w.document.bytes = document("Kbig", "video/mp4", bytes.size());
		w.blobs[0].record = CommandBytes{{}, true, CommandStream::upload, 0, bytes.size()};
		Command final;
		final.writes.push_back(std::move(w));
		largest = 0;
		REQUIRE(apply(encode_entry(CommandFinal{id, final})));
		const auto named = files.capture();
		REQUIRE_EQ(named.count(volume_name(volume)), 1u);
		CHECK(named.at(volume_name(volume)).crc == kronuz::crc32c::crc32c(bytes));
		CHECK_LE(largest, bound);
		if (largest > bound) {
			std::fprintf(stderr, "  the seal read %zu bytes at once (bound %zu)\n", largest, bound);
		}
	}
	std::filesystem::remove_all(dir);
}

// Whether every locator of `data` views the data's own bytes.
bool views_its_bytes(const Data& data) {
	const auto& bytes = data.serialize();
	const auto* first = bytes.data();
	const auto* last = first + bytes.size();
	for (const auto& locator : data) {
		if (locator.raw.data() < first || locator.raw.data() + locator.raw.size() > last) {
			return false;
		}
	}
	return true;
}

// A flush leaves the locators on the bytes it built, never on the bytes it replaced or an update's holder, so a
// locator changed after a flush and flushed again (place_stored's codec pass) serializes live bytes.
void test_a_flush_leaves_its_locators_on_its_bytes() {
	const std::string big(300, 'x');   // compressed in place
	Data data;
	data.update(ct_type_t("text/plain"), std::string("tiny"));   // a short holder, moved when the updates grow
	data.update(ct_type_t("image/png"), std::string(big));
	data.update(ct_type_t("a/b"), 0, 0, 37);
	data.flush();
	CHECK(views_its_bytes(data));
	REQUIRE_EQ(data.size(), 4u);   // the empty main object a new Data holds, then the three
	CHECK(data.get(ct_type_t("text/plain"))->data() == "tiny");
	CHECK(data.get(ct_type_t("image/png"))->data() == big);

	for (auto& locator : data) {
		if (locator.ct_type == ct_type_t("a/b")) {
			locator.type = Locator::Type::zstd_compressed_stored;
		}
	}
	data.flush();
	CHECK(views_its_bytes(data));
	CHECK(data.get(ct_type_t("text/plain"))->data() == "tiny");
	CHECK(data.get(ct_type_t("image/png"))->data() == big);
	CHECK(data.get(ct_type_t("a/b"))->type == Locator::Type::zstd_compressed_stored);

	// Read from bytes, a replaced locator moves last: the ones kept are rebuilt ahead of it from live bytes.
	Data read{std::string(data.serialize())};
	read.update(ct_type_t("text/plain"), std::string(400, 'y'));
	read.flush();
	CHECK(views_its_bytes(read));
	CHECK(read.get(ct_type_t("image/png"))->data() == big);
	CHECK(read.get(ct_type_t("text/plain"))->data() == std::string(400, 'y'));
	CHECK_EQ(read.get(ct_type_t("a/b"))->size, 37u);
	const Data copy = read;
	CHECK(views_its_bytes(copy));
	CHECK(copy.serialize() == read.serialize());
}

// place_stored sets the codec on a placeholder and keeps the document's other locators' bytes.
void test_place_stored_keeps_the_other_locators() {
	const std::string big(300, 'z');
	Data data;
	data.update(ct_type_t("text/plain"), std::string(big));
	data.update(ct_type_t("image/png"), 0, 0, 37);
	data.flush();
	const auto placed = place_stored(std::string(data.serialize()),
		{{"image/png", static_cast<std::uint8_t>(Locator::Type::zstd_compressed_stored), 3, 40, 21}});
	const Data out{std::string(placed)};
	CHECK(views_its_bytes(out));
	REQUIRE(out.get(ct_type_t("text/plain")) != nullptr);
	CHECK(out.get(ct_type_t("text/plain"))->data() == big);
	const auto* png = out.get(ct_type_t("image/png"));
	REQUIRE(png != nullptr);
	CHECK(png->type == Locator::Type::zstd_compressed_stored);
	CHECK_EQ(png->volume, 3);
	CHECK_EQ(png->offset, 40u);
	CHECK_EQ(png->size, 21u);
}

// The state round-trips through the checkpoint bundle's bytes, and bytes that are not one are refused.
void test_state_bytes() {
	Replica r("shard_group_state");
	ShardGroupState s;
	s.volume = 4;
	s.length = 1ull << 35;
	s.next = 9;
	s.open[5] = {0};
	s.open[1ull << 40] = {8};
	const auto bytes = r.app->encode(s);
	CHECK(r.app->decode(bytes) == s);
	for (std::size_t n = 0; n < bytes.size(); ++n) {
		bool refused = false;
		try {
			(void)r.app->decode(std::string_view(bytes).substr(0, n));
		} catch (const engine::Error& e) {
			refused = e.kind == engine::errc::corrupt;
		}
		CHECK(refused);
	}
}

// --full-flush-ordering reaches the journal's IO (D10, D177): the host's PosixIO takes posix_io_options(), a file it
// opens syncs plain by default and full under the switch, and the barrier warning names it. Those options admit a
// group-writable directory, which PosixIO's defaults refuse.
void test_the_journal_io_takes_the_io_options() {
	const auto dir = std::filesystem::current_path() / "shard_group_io_options";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	std::filesystem::permissions(dir, std::filesystem::perms::group_write, std::filesystem::perm_options::add);
	const auto saved = opts.full_flush_ordering;
	for (const bool full : {false, true}) {
		opts.full_flush_ordering = full;
		bool opened = false;
		try {
			ShardGroups groups(dir.string(), 1);
			const auto mode = full ? kronuz::journal::FlushMode::full : kronuz::journal::FlushMode::plain;
			CHECK(groups.io_options().flush_mode == mode);
			CHECK(groups.io_options().barrier_warning == &engine::strata::log_barrier_warning);
			auto file = groups.journal_io().create_exclusive("flush-mode-probe");
			const auto* posix = dynamic_cast<const kronuz::journal::detail::PosixFile*>(file.get());
			CHECK(posix != nullptr && posix->flush_mode() == mode);
			file.reset();
			std::filesystem::remove(dir / "flush-mode-probe");
			opened = true;
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  the journal host refused its directory: %s\n", e.what());
		}
		CHECK(opened);
	}
	opts.full_flush_ordering = saved;
	std::filesystem::remove_all(dir);
}

}  // namespace

TEST_MAIN(test_blobs_at_the_offsets_the_state_names, test_rotation_in_log_order, test_chunked_command_applies_at_its_final_entry,
	test_upload_into_its_own_volume, test_higher_term_retires_open_commands, test_entries_outside_their_term_apply_nothing,
	test_abort_retires_its_upload_volume, test_a_seal_reads_its_volume_in_bounded_pieces, test_a_flush_leaves_its_locators_on_its_bytes, test_place_stored_keeps_the_other_locators,
	test_state_bytes, test_the_journal_io_takes_the_io_options)
