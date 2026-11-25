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

// Chunked commands from the proposer's side (src/database/handler.h; cluster-release D12), proposed into the
// shard group's application as its chore applies them: a command past the entry limit goes as chunks and a final
// entry, each within the limit, and applies at the final; an upload streams into its own volume; a leader change
// between a command's entries cuts it, which answers errc::not_available and applies nothing.

#include "test_harness.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "database/command.h"
#include "database/data.h"
#include "database/handler.h"
#include "database/shard_group.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/strata/shard.h"   // for engine::strata::index_options, new_uuid
#include "strata/group/answer.hh"
#include "xapiand_exception.h"     // for ClientError

namespace {

namespace cs = kronuz::consensus;
namespace sg = strata::group;

std::string pattern(std::size_t n, char seed) {
	std::string out(n, '\0');
	for (std::size_t i = 0; i < n; ++i) {
		out[i] = static_cast<char>(seed + static_cast<char>(i * 131 % 251));
	}
	return out;
}

// One replica of a shard group, applying each proposed entry at once in the current term, as a lone voter's
// chore does; it refuses an entry past the limit, as consensus's `command_bytes` does.
struct Group {
	std::filesystem::path dir;
	std::optional<kronuz::journal::PosixIO> io;
	std::optional<prospect::index> index;
	std::optional<sg::file_set> files;
	std::optional<ShardApplication> app;
	ShardGroupState state;
	std::uint64_t at = 0;
	std::uint64_t term = 1;
	std::vector<std::string> log;   // every entry placed, in order
	ProposeEntry propose;

	explicit Group(const char* name) : dir(std::filesystem::current_path() / name) {
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		io.emplace(dir, kronuz::journal::PosixIOOptions{kronuz::journal::FlushMode::plain});
		index.emplace(prospect::index::create(*io, engine::strata::index_options({}), engine::strata::new_uuid()));
		files.emplace(*io);
		app.emplace(*index);
		propose = [this](std::string entry, sg::ack level) -> task::task<sg::answer> { co_return place(std::move(entry), level); };
	}
	~Group() {
		files.reset();
		index.reset();
		io.reset();
		std::filesystem::remove_all(dir);
	}

	sg::answer place(std::string entry, sg::ack level, cs::EntryKind kind = cs::EntryKind::Command) {
		if (entry.size() > COMMAND_ENTRY_BYTES) {
			throw cs::rejected(cs::RejectReason::TooLarge);
		}
		const sg::entry e{++at, term, kind, entry};
		app->effects(state, e, *files);
		::strata::verdict v;
		if (auto t = app->transaction(state, e)) {
			v = index->apply(*t, {});
		}
		app->advance(state, e);
		log.push_back(std::move(entry));
		sg::answer answer{cs::Placement{term, e.index}, std::nullopt};
		if (level == sg::ack::quorum) {
			answer.applied = v;
		}
		return answer;
	}

	// A new leader's first entry.
	void elect(std::uint64_t next) {
		term = next;
		place(std::string(), sg::ack::none, cs::EntryKind::NoOp);
	}

	// The blob `ct` of `key` as its locator names it.
	std::optional<std::string> blob(const std::string& key, const std::string& ct) {
		const auto stored = index->get(key);
		if (!stored) {
			return std::nullopt;
		}
		Data data{std::string(*stored)};
		const auto* locator = data.get(ct_type_t(ct));
		if (locator == nullptr || locator->volume < 1) {
			return std::nullopt;
		}
		return files->read(volume_name(static_cast<std::uint64_t>(locator->volume)), locator->offset, locator->size);
	}
};

template <typename T>
T run(task::task<T> work) {
	task::inline_scheduler s;
	return task::run(s, std::move(work));
}

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

Command put(const std::string& key, const std::string& ct, CommandBytes record, std::size_t size) {
	CommandWrite w;
	w.op = CommandOp::replace_key;
	w.key = key;
	w.expected = 0;
	w.document.bytes = document(key, ct, size);
	w.blobs.push_back({ct, std::move(record)});
	Command command;
	command.writes.push_back(std::move(w));
	return command;
}

// Body chunks of a fixed size, as a request's body arrives.
class Body : public DatabaseHandler::BodyChunks {
	std::string bytes_;
	std::size_t piece_;
	std::size_t at_ = 0;

public:
	Body(std::string bytes, std::size_t piece) : bytes_(std::move(bytes)), piece_(piece) { }
	task::task<bool> next(std::string& chunk) override {
		if (at_ == bytes_.size()) {
			co_return false;
		}
		chunk.assign(bytes_, at_, piece_);
		at_ += chunk.size();
		co_return true;
	}
};

engine::errc error_of(const std::function<void()>& fn) {
	try {
		fn();
	} catch (const engine::Error& e) {
		return e.kind;
	}
	return engine::errc::internal;
}

// A command whose entry fits goes whole; past the limit it goes as chunks and a final entry, each within the limit,
// and applies at the final: the blob's bytes in the current volume, the staging retired.
void test_command_over_the_limit_goes_as_chunks() {
	Group g("group_chunk_command");
	const auto small = pattern(1000, 's');
	auto a = run(propose_command(g.propose, put("Ks", "a/s", CommandBytes{small}, small.size()), chunked_id(1, 1), sg::ack::quorum));
	CHECK_EQ(g.log.size(), 1u);
	CHECK(std::holds_alternative<Command>(decode_entry(g.log[0])));
	CHECK(a.applied && a.applied->result == ::strata::verdict::accepted);

	const auto big = pattern(3 * 1024 * 1024 + 17, 'b');
	const auto id = chunked_id(1, 2);
	a = run(propose_command(g.propose, put("Kb", "a/b", CommandBytes{big}, big.size()), id, sg::ack::quorum));
	REQUIRE_EQ(g.log.size(), 6u);   // 3 MiB + 17 bytes: four chunks and the final
	std::uint64_t offset = 0;
	for (std::size_t i = 1; i + 1 < g.log.size(); ++i) {
		CHECK_LE(g.log[i].size(), COMMAND_ENTRY_BYTES);
		const auto entry = decode_entry(g.log[i]);
		const auto* chunk = std::get_if<CommandChunk>(&entry);
		REQUIRE(chunk != nullptr);
		CHECK_EQ(chunk->id, id);
		CHECK(chunk->stream == CommandStream::staging);
		CHECK_EQ(chunk->offset, offset);
		offset += chunk->bytes.size();
	}
	CHECK_EQ(offset, big.size());
	CHECK_EQ(g.log[1].size(), COMMAND_ENTRY_BYTES);   // full chunks fill the entry, header included
	CHECK_LE(g.log.back().size(), COMMAND_ENTRY_BYTES);
	CHECK(std::holds_alternative<CommandFinal>(decode_entry(g.log.back())));
	CHECK(a.applied && a.applied->result == ::strata::verdict::accepted);
	CHECK_EQ(a.placement.term, 1u);
	CHECK(g.blob("Ks", "a/s") == small);
	CHECK(g.blob("Kb", "a/b") == big);
	CHECK_FALSE(g.files->live(staging_name(id)));
	CHECK(g.state.open.empty());
	CHECK_EQ(g.state.length, small.size() + big.size());
}

// An upload streams in body pieces of any size as chunks of the upload stream, each within the limit; its final
// entry names the upload's own volume.
void test_upload_streams_into_its_own_volume() {
	Group g("group_chunk_upload");
	const auto bytes = pattern(2 * 1024 * 1024 + 300 * 1024 + 7, 'u');
	const auto id = chunked_id(1, 5);
	ChunkedUpload upload(id, bytes.size());
	Body body(bytes, 300 * 1024 + 1);
	run(propose_upload(g.propose, upload, body));
	REQUIRE_EQ(g.log.size(), 3u);
	std::uint64_t offset = 0;
	for (const auto& e : g.log) {
		CHECK_LE(e.size(), COMMAND_ENTRY_BYTES);
		const auto entry = decode_entry(e);
		const auto* chunk = std::get_if<CommandChunk>(&entry);
		REQUIRE(chunk != nullptr);
		CHECK(chunk->stream == CommandStream::upload);
		CHECK_EQ(chunk->offset, offset);
		offset += chunk->bytes.size();
	}
	CHECK_EQ(offset, bytes.size());
	CHECK_EQ(g.state.open.at(id).upload, 2u);
	const auto a = run(propose_upload_final(g.propose, upload, put("Ku", "video/mp4", upload.record(), bytes.size()), sg::ack::quorum));
	CHECK(a.applied && a.applied->result == ::strata::verdict::accepted);
	const auto stored = g.index->get("Ku");
	REQUIRE(stored.has_value());
	Data data{std::string(*stored)};
	const auto* locator = data.get(ct_type_t("video/mp4"));
	REQUIRE(locator != nullptr);
	CHECK_EQ(locator->volume, 2);
	CHECK_EQ(locator->offset, 0u);
	CHECK_EQ(locator->size, bytes.size());
	CHECK(g.blob("Ku", "video/mp4") == bytes);
	CHECK(g.state.open.empty());
	CHECK_EQ(g.state.length, 0u);   // the current volume took nothing
}

// A body longer or shorter than the upload's size is the client's error; the upload's abort ends its command.
void test_upload_body_of_another_size() {
	Group g("group_chunk_size");
	// A body past its size, then one short of it: each after a chunk was placed.
	for (const auto& [size, n] : {std::pair{std::size_t(1024 * 1024), std::size_t(1536 * 1024)}, std::pair{std::size_t(2048 * 1024), std::size_t(1536 * 1024)}}) {
		const auto id = chunked_id(1, size);
		const auto volume = g.state.next;   // the upload's, at its first chunk
		ChunkedUpload upload(id, size);
		Body body(pattern(n, 'x'), 64 * 1024);
		bool client = false;
		try {
			run(propose_upload(g.propose, upload, body));
		} catch (const ClientError&) {
			client = true;
		}
		CHECK(client);
		const auto last = decode_entry(g.log.back());
		const auto* final = std::get_if<CommandFinal>(&last);
		REQUIRE(final != nullptr);   // the abort
		CHECK_EQ(final->id, id);
		CHECK(final->command.writes.empty());
		CHECK_EQ(g.state.open.count(id), 0u);
		CHECK_FALSE(g.files->live(volume_name(volume)));   // the abort freed it (A9)
	}
}

// A leader change between a command's chunks and its final entry retires the chunks at the new term's first entry:
// the final applies nothing, and its proposer hears errc::not_available, not an acknowledgement.
void test_leader_change_cuts_a_chunked_command() {
	Group g("group_chunk_term");
	const auto big = pattern(2 * 1024 * 1024, 'c');
	const auto id = chunked_id(1, 9);
	const auto entries = split_command(put("Kc", "a/c", CommandBytes{big}, big.size()), id);
	REQUIRE_EQ(entries.size(), 4u);
	// The chunks at term 1, then a new leader of term 2 before the final entry arrives.
	std::size_t placed = 0;
	g.propose = [&](std::string entry, sg::ack level) -> task::task<sg::answer> {
		if (placed++ == entries.size() - 1) {
			g.elect(2);
		}
		co_return g.place(std::move(entry), level);
	};
	const auto kind = error_of([&] { run(propose_command(g.propose, put("Kc", "a/c", CommandBytes{big}, big.size()), id, sg::ack::quorum)); });
	CHECK(kind == engine::errc::not_available);
	CHECK_FALSE(g.index->find_key("Kc").has_value());
	CHECK_FALSE(g.files->live(staging_name(id)));
	CHECK(g.state.open.empty());
	CHECK_EQ(g.state.length, 0u);

	// An upload whose final entry lands in the new term fails the same way; its volume retired at that term's first
	// entry.
	const auto bytes = pattern(1536 * 1024, 'v');
	const auto up = chunked_id(2, 1);
	ChunkedUpload upload(up, bytes.size());
	Body body(bytes, 1024 * 1024);
	g.propose = [&](std::string entry, sg::ack level) -> task::task<sg::answer> { co_return g.place(std::move(entry), level); };
	run(propose_upload(g.propose, upload, body));
	const auto volume = g.state.open.at(up).upload;
	CHECK(g.files->live(volume_name(volume)));
	g.elect(3);
	CHECK_FALSE(g.files->live(volume_name(volume)));
	const auto cut = error_of([&] { run(propose_upload_final(g.propose, upload, put("Kv", "a/v", upload.record(), bytes.size()), sg::ack::quorum)); });
	CHECK(cut == engine::errc::not_available);
	CHECK_FALSE(g.index->find_key("Kv").has_value());
	CHECK(g.state.open.empty());
}

// A command's first chunk placed in a later term than its id names (an election between the id and the proposal)
// is cut at once, and its abort ends what that chunk opened.
void test_first_chunk_in_another_term() {
	Group g("group_chunk_first");
	g.term = 4;
	const auto big = pattern(1536 * 1024, 'f');
	const auto id = chunked_id(3, 1);
	const auto kind = error_of([&] { run(propose_command(g.propose, put("Kf", "a/f", CommandBytes{big}, big.size()), id, sg::ack::quorum)); });
	CHECK(kind == engine::errc::not_available);
	REQUIRE_EQ(g.log.size(), 2u);   // the first chunk and the abort
	CHECK(std::holds_alternative<CommandFinal>(decode_entry(g.log.back())));
	CHECK_FALSE(g.files->live(staging_name(id)));
	CHECK(g.state.open.empty());
	CHECK_FALSE(g.index->find_key("Kf").has_value());
}

// An id carries its term in the high bits, and one leader's ids start past its floor.
void test_chunked_ids() {
	CHECK_EQ(chunked_id(7, 5), (std::uint64_t(7) << 32) | 5);
	CHECK_EQ(chunked_term(chunked_id(7, 5)), 7u);
	CHECK_EQ(chunked_term(chunked_id(7, (std::uint64_t(1) << 32) + 3)), 7u);   // the sequence wraps below the term
	ChunkedIds ids(9, 41);
	CHECK_EQ(ids.next(), chunked_id(9, 42));
	CHECK_EQ(ids.next(), chunked_id(9, 43));
	CHECK(error_of([] { ChunkedUpload(chunked_id(1, 1), 0); }) == engine::errc::invalid);
}

}  // namespace

TEST_MAIN(test_command_over_the_limit_goes_as_chunks, test_upload_streams_into_its_own_volume, test_upload_body_of_another_size,
	test_leader_change_cuts_a_chunked_command, test_first_chunk_in_another_term, test_chunked_ids)
