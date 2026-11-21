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

// RESTORE's batches (src/database/restore_batch.h; cluster-release D17): writes packed in input order into commands
// that each fit one log entry, a write over it alone, and, applied through the shard group's application, batches
// prepared ahead of the ones before them: a later batch repeats an earlier one's keys and allocates new ids, and
// every write lands at the docid and version the verdict names, stamped on its stored document.

#include "test_harness.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "database/command.h"
#include "database/data.h"
#include "database/restore_batch.h"
#include "database/shard_group.h"
#include "database/utils.h"        // for DB_SLOT_VERSION
#include "engine/document.h"
#include "engine/error.h"
#include "engine/strata/document.h"
#include "engine/strata/shard.h"   // for engine::strata::index_options, new_uuid
#include "msgpack.h"
#include "reserved/fields.h"       // for VERSION_FIELD_NAME
#include "serialize.h"             // for Serialize::version

namespace {

namespace cs = kronuz::consensus;

// A replica: an index, its file set and the application over them, in a fresh directory.
struct Replica {
	std::filesystem::path dir;
	std::optional<kronuz::journal::PosixIO> io;
	std::optional<prospect::index> index;
	std::optional<strata::group::file_set> files;
	std::optional<ShardApplication> app;
	ShardGroupState state;
	std::uint64_t at = 0;

	explicit Replica(const char* name) : dir(std::filesystem::current_path() / name) {
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		io.emplace(dir);
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

	// Applies one entry; returns its verdict (accepted with nothing when it wrote nothing).
	::strata::verdict apply(std::string_view bytes) {
		const strata::group::entry e{++at, 1, cs::EntryKind::Command, bytes};
		app->effects(state, e, *files);
		::strata::verdict v;
		if (const auto t = app->transaction(state, e)) {
			v = index->apply(*t, {});
		}
		app->advance(state, e);
		return v;
	}
};

// A write as the leader's prepare leaves it against what the index held then: its expectation and the version it
// predicted, stamped on the document as prepare stamps it (the version slot, the data's _version).
CommandWrite prepared(const std::string& key, engine::rev seen, std::size_t body = 16) {
	const engine::rev version = seen + 1;
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), key);
	Data data;
	MsgPack obj = MsgPack::MAP();
	obj[VERSION_FIELD_NAME] = version;
	obj["body"] = std::string(body, 'x');
	data.set_obj(obj);
	data.flush();
	doc.set_data(data.serialize());
	dynamic_cast<engine::strata::StrataDocument&>(doc.native()).stamp(version);
	doc.add_value(DB_SLOT_VERSION, Serialize::version(version));
	CommandWrite w;
	w.op = CommandOp::replace_key;
	w.key = key;
	w.expected = seen;
	w.document.bytes = doc.serialize();
	return w;
}

// The _version the index stores for a key.
std::uint64_t stored_version(Replica& r, const std::string& key) {
	const auto stored = r.index->get(key);
	CHECK(stored.has_value());
	return stored ? Data(std::string(*stored)).get_obj()[VERSION_FIELD_NAME].u64() : 0;
}

std::vector<Command> batch_all(std::vector<CommandWrite> writes, std::size_t limit, std::uint64_t schema = 1) {
	RestoreBatcher batcher(limit);
	std::vector<Command> out;
	for (auto& w : writes) {
		if (auto closed = batcher.add(std::move(w), schema)) {
			out.push_back(std::move(*closed));
		}
	}
	if (auto closed = batcher.flush()) {
		out.push_back(std::move(*closed));
	}
	return out;
}

// encoded_size is what a write adds to its command's entry.
void test_encoded_size_matches_the_codec() {
	auto a = prepared("Ka", 0);
	auto b = prepared("Kb", 300, 70000);
	b.did = 1ull << 40;
	b.blobs.push_back({"image/png", CommandBytes{std::string(200, 'p')}});
	b.blobs.push_back({"video/mp4", CommandBytes{{}, true, CommandStream::upload, 1ull << 33, 5000}});
	auto c = prepared("Kc", 0);
	c.expected.reset();
	c.document = CommandBytes{{}, true, CommandStream::staging, 17, 1ull << 21};
	const Command none{9, {}};
	const Command all{9, {a, b, c}};
	CHECK_EQ(encode_entry(all, 1ull << 30).size(), encode_entry(none).size() + encoded_size(a) + encoded_size(b) + encoded_size(c));
}

// Writes pack in input order, each command within the limit and closed only when its next write would not fit;
// the expectations are gone.
void test_packing_fills_each_entry() {
	std::vector<CommandWrite> writes;
	std::vector<std::string> keys;
	for (int i = 0; i < 300; ++i) {
		keys.push_back("K" + std::to_string(i));
		writes.push_back(prepared(keys.back(), 0, 100 + static_cast<std::size_t>(i % 7) * 50));
	}
	for (const std::size_t limit : {std::size_t(4096), COMMAND_ENTRY_BYTES / 16}) {
		const auto commands = batch_all(writes, limit);
		REQUIRE(commands.size() > 1u);
		std::vector<std::string> order;
		for (std::size_t c = 0; c < commands.size(); ++c) {
			const auto& command = commands[c];
			CHECK(encode_entry(command, limit).size() <= limit);
			for (const auto& w : command.writes) {
				order.push_back(w.key);
				CHECK_FALSE(w.expected.has_value());
			}
			if (c + 1 < commands.size()) {
				auto grown = command;
				grown.writes.push_back(commands[c + 1].writes.front());
				CHECK(encode_entry(grown, 1ull << 30).size() > limit);
			}
		}
		CHECK(order == keys);
		std::printf("limit %zu: %zu writes in %zu commands\n", limit, writes.size(), commands.size());
	}
}

// A write over the entry limit is a command of its own, which split_command chunks; a schema change closes the open
// command; a write that is not a replace by key is refused.
void test_oversized_alone_schema_closes_refused() {
	std::vector<CommandWrite> writes{prepared("Ka", 0), prepared("Kbig", 0, 3 * COMMAND_ENTRY_BYTES / 2), prepared("Kb", 0)};
	const auto commands = batch_all(writes, COMMAND_ENTRY_BYTES);
	REQUIRE_EQ(commands.size(), 3u);
	CHECK_EQ(commands[1].writes.size(), 1u);
	CHECK_EQ(commands[1].writes[0].key, std::string("Kbig"));
	CHECK(split_command(commands[1], 1).size() > 2u);

	RestoreBatcher batcher;
	CHECK_FALSE(batcher.add(prepared("Ka", 0), 1).has_value());
	const auto closed = batcher.add(prepared("Kb", 0), 2);
	REQUIRE(closed.has_value());
	CHECK_EQ(closed->schema_version, 1u);
	CHECK_EQ(batcher.flush()->schema_version, 2u);

	auto erase = prepared("Kc", 0);
	erase.op = CommandOp::erase_key;
	bool refused = false;
	try {
		batcher.add(std::move(erase), 1);
	} catch (const engine::Error& e) {
		refused = e.kind == engine::errc::invalid;
	}
	CHECK(refused);
}

// Batch 2 is prepared while batch 1 is in flight (both against the index before batch 1 applies): it repeats batch
// 1's keys, at versions batch 1 then moves, and its new key takes the id prepare predicted for batch 1's. Both
// apply; each write's docid and version are the verdict's, its stored _version says the same, and a write repeated
// within one batch lands one version later.
void test_pipelined_batches_repeat_terms_and_allocate_ids() {
	Replica r("restore_batch_pipelined");
	REQUIRE(r.apply(encode_entry(Command{1, {prepared("Ka", 0)}})).result == ::strata::verdict::accepted);
	REQUIRE(r.index->find_key("Ka")->second == 1u);
	// Prepared now: Ka at version 1, Kb and Kc absent.
	std::vector<CommandWrite> first{prepared("Ka", 1), prepared("Kb", 0)};
	std::vector<CommandWrite> second{prepared("Ka", 1), prepared("Kb", 0), prepared("Kc", 0), prepared("Kc", 0)};
	const auto b1 = batch_all(first, COMMAND_ENTRY_BYTES);
	const auto b2 = batch_all(second, COMMAND_ENTRY_BYTES);
	REQUIRE_EQ(b1.size(), 1u);
	REQUIRE_EQ(b2.size(), 1u);

	const auto v1 = r.apply(encode_entry(b1[0]));
	REQUIRE(v1.result == ::strata::verdict::accepted);
	const auto v2 = r.apply(encode_entry(b2[0]));
	std::printf("batch 2 verdict %d at op %zu\n", static_cast<int>(v2.result), v2.op);
	REQUIRE(v2.result == ::strata::verdict::accepted);

	const auto r1 = restore_results(b1[0], v1);
	const auto r2 = restore_results(b2[0], v2);
	CHECK_EQ(r1[0].did, 1u);
	CHECK_EQ(r1[0].version, 2u);
	CHECK_EQ(r1[1].did, 2u);
	CHECK_EQ(r1[1].version, 1u);
	CHECK_EQ(r2[0].did, 1u);
	CHECK_EQ(r2[0].version, 3u);
	CHECK_EQ(r2[1].did, 2u);
	CHECK_EQ(r2[1].version, 2u);
	CHECK_EQ(r2[2].did, 3u);
	CHECK_EQ(r2[2].version, 1u);
	CHECK_EQ(r2[3].did, 3u);
	CHECK_EQ(r2[3].version, 2u);
	CHECK_EQ(r2[2].term, std::string("Kc"));
	for (const auto& [key, version] : std::vector<std::pair<std::string, std::uint64_t>>{{"Ka", 3}, {"Kb", 2}, {"Kc", 2}}) {
		CHECK_EQ(r.index->find_key(key).value_or(std::pair<std::uint64_t, std::uint64_t>{0, 0}).second, version);
		CHECK_EQ(stored_version(r, key), version);
	}
	CHECK_EQ(r.index->last_docid(), 3u);
}

// A verdict that does not answer every write of its command is refused.
void test_results_need_every_write() {
	const Command command{1, {prepared("Ka", 0), prepared("Kb", 0)}};
	::strata::verdict v;
	v.docids = {1};
	v.versions = {1};
	bool refused = false;
	try {
		(void)restore_results(command, v);
	} catch (const engine::Error& e) {
		refused = e.kind == engine::errc::internal;
	}
	CHECK(refused);
}

}  // namespace

TEST_MAIN(test_encoded_size_matches_the_codec, test_packing_fills_each_entry, test_oversized_alone_schema_closes_refused,
	test_pipelined_batches_repeat_terms_and_allocate_ids, test_results_need_every_write)
