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

// A shard's log replayed through its group's application (src/database/shard_group.h; cluster-release D16, D18),
// entry by entry as the group's chore drives it. An erase of a missing document applies as a no-op; a replica
// applying the log from empty ends in the primary's state; and a recovery from the checkpoint at any entry (the
// file set truncated to the capture by strata::group::file_set::recover, the state from its bundle bytes, the
// later entries' file effects rerun while strata skips them) rebuilds the primary's files and state, and every
// replayed transaction stores the locators the primary's did. Recovering twice converges.
#include "test_harness.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "database/command.h"
#include "database/data.h"
#include "database/shard_group.h"
#include "engine/document.h"
#include "engine/strata/shard.h"   // for engine::strata::index_options, new_uuid

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

	// Applies entry `at` of the log: its effects, its strata write when `write` (strata at or past it skips it),
	// its advance. Returns the transaction it built, if any, and the write's verdict in `verdict`.
	std::optional<prospect::transaction> apply(std::uint64_t at, std::string_view bytes, bool write = true, ::strata::verdict* verdict = nullptr) {
		const strata::group::entry e{at, 1, cs::EntryKind::Command, bytes};
		app->effects(state, e, *files);
		auto t = app->transaction(state, e);
		if (t && write) {
			const auto v = index->apply(*t, {});
			CHECK(v.result == ::strata::verdict::accepted);
			if (verdict != nullptr) {
				*verdict = v;
			}
		}
		app->advance(state, e);
		return t;
	}

	// The live files' bytes and the application's state: what recovery must rebuild.
	std::string files_digest() {
		std::string out = app->encode(state);
		for (const auto& [name, file] : files->capture()) {
			out += "|" + name + (file.crc ? ":" + std::to_string(*file.crc) : "") + "=" + files->read(name, 0, static_cast<std::size_t>(file.length));
		}
		return out;
	}

	// The documents of `keys`: docid, version and stored record; and the last docid.
	std::string index_digest(const std::vector<std::string>& keys) {
		std::string out = "last=" + std::to_string(index->last_docid());
		for (const auto& key : keys) {
			out += "|" + key + "=";
			if (const auto found = index->find_key(key)) {
				out += std::to_string(found->first) + ":v" + std::to_string(found->second) + ":" + index->get(key).value_or("?");
			}
		}
		return out;
	}
};

// A document whose data names a blob of `ct` by a placeholder locator, as prepare leaves it.
std::string document(const std::string& key, const std::string& ct, std::size_t blob) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), key);
	Data data;
	if (blob != 0) {
		data.update(ct_type_t(ct), 0, 0, blob);
	}
	data.flush();
	doc.set_data(data.serialize());
	return doc.serialize();
}

CommandWrite put(const std::string& key, std::string record = {}) {
	CommandWrite w;
	w.op = CommandOp::replace_key;
	w.key = key;
	w.document.bytes = document(key, "application/x", record.size());
	if (!record.empty()) {
		w.blobs.push_back({"application/x", CommandBytes{std::move(record)}});
	}
	return w;
}

CommandWrite erase(engine::docid did) {
	CommandWrite w;
	w.op = CommandOp::erase;
	w.did = did;
	return w;
}

std::string entry(std::vector<CommandWrite> writes) {
	Command command;
	command.writes = std::move(writes);
	return encode_entry(command);
}

// What the transaction stores per operation: the replayed one must equal the primary's (its field references may
// differ: replay builds it against a table that already holds the fields).
std::string stored_digest(const std::optional<prospect::transaction>& t) {
	if (!t) {
		return "none";
	}
	std::string out;
	for (const auto& op : t->ops) {
		out += std::to_string(op.type) + "/" + op.key + "/" + std::to_string(op.docid) + "/" +
			(op.expected_version ? std::to_string(*op.expected_version) : "-") + "/" + op.doc.key + "/" + op.doc.stored + ";";
	}
	return out;
}

const std::vector<std::string> KEYS = {"Ka", "Kb", "Kc", "Kbig", "Kup", "Kd"};

// A shard's log: single writes and a RESTORE batch with blobs, an erase, a key written again, a chunked command,
// an upload, a write after them.
std::vector<std::string> shard_log() {
	std::vector<std::string> log;
	log.push_back(entry({put("Ka", pattern(100, 'a'))}));
	log.push_back(entry({put("Kb", pattern(37, 'b')), put("Kc")}));   // a batch (D17)
	log.push_back(entry({erase(1)}));
	log.push_back(entry({put("Ka", pattern(64, 'A'))}));
	Command big;
	big.writes.push_back(put("Kbig", pattern(3 * 1024 * 1024 / 2, 'z')));
	for (auto& e : split_command(big, (1ull << 32) | 7)) {
		log.push_back(std::move(e));
	}
	const std::uint64_t upload = (1ull << 32) | 9;
	const auto bytes = pattern(5000, 'u');
	log.push_back(encode_entry(CommandChunk{upload, CommandStream::upload, 0, std::string_view(bytes).substr(0, 3000)}));
	log.push_back(encode_entry(CommandChunk{upload, CommandStream::upload, 3000, std::string_view(bytes).substr(3000)}));
	auto w = put("Kup", std::string());
	w.document.bytes = document("Kup", "video/mp4", bytes.size());
	w.blobs.push_back({"video/mp4", CommandBytes{{}, true, CommandStream::upload, 0, bytes.size()}});
	Command up;
	up.writes.push_back(std::move(w));
	log.push_back(encode_entry(CommandFinal{upload, up}));
	log.push_back(entry({put("Kd", pattern(9, 'd'))}));
	return log;
}

// Erases of a missing document, by id and by key, apply: accepted, nothing removed.
void test_erase_of_a_missing_document_is_a_no_op() {
	Replica r("journal_replay_erase");
	CommandWrite by_key;
	by_key.op = CommandOp::erase_key;
	by_key.key = "Qx";
	::strata::verdict v;
	const auto t = r.apply(1, entry({erase(7), by_key}), true, &v);
	REQUIRE(t.has_value());
	CHECK_EQ(t->ops.size(), 2u);
	CHECK(v.docids == std::vector<std::uint64_t>({0, 0}));
	CHECK_EQ(r.index->last_docid(), 0u);
	CHECK_FALSE(r.index->find_key("Qx").has_value());
}

// A replica applying the log from empty ends in the primary's documents, files and state.
void test_log_from_empty_ends_in_the_primary_state() {
	const auto log = shard_log();
	Replica primary("journal_replay_primary");
	Replica replica("journal_replay_replica");
	for (std::size_t i = 0; i < log.size(); ++i) {
		const auto a = primary.apply(i + 1, log[i]);
		const auto b = replica.apply(i + 1, log[i]);
		CHECK_EQ(stored_digest(a), stored_digest(b));
	}
	const auto want = primary.index_digest(KEYS);
	std::printf("primary: %s\n", want.substr(0, 160).c_str());
	CHECK_EQ(replica.index_digest(KEYS), want);
	CHECK_EQ(replica.files_digest(), primary.files_digest());
	CHECK(primary.index->find_key("Kbig").has_value());
	CHECK_FALSE(primary.index->find_docid(1).has_value());   // Ka's first copy, erased
	CHECK_EQ(primary.state.next, 3u);                         // the upload took volume 2
}

// Recovery from the checkpoint captured after each entry k, with strata already at the log's end (P_b = k <= P_H):
// the file set truncated to the capture, the state decoded from its bundle bytes, entries (k, n] replayed with
// their file effects and no strata write. The files and state equal the primary's, each replayed transaction
// stores what the primary's did, and a second recovery from the same capture (a crash inside recovery) converges.
void test_recovery_from_every_checkpoint_converges() {
	const auto log = shard_log();
	Replica r("journal_replay_recovery");
	std::vector<strata::group::file_map> captures{r.files->capture()};
	std::vector<std::string> bundles{r.app->encode(r.state)};
	std::vector<std::string> stored;
	for (std::size_t i = 0; i < log.size(); ++i) {
		stored.push_back(stored_digest(r.apply(i + 1, log[i])));
		captures.push_back(r.files->capture());
		bundles.push_back(r.app->encode(r.state));
	}
	const auto want_files = r.files_digest();
	const auto want_index = r.index_digest(KEYS);
	std::size_t replayed = 0;
	for (std::size_t k = 0; k <= log.size(); ++k) {
		for (int pass = 1; pass <= 2; ++pass) {
			r.files->recover(captures[k]);
			r.state = r.app->decode(bundles[k]);
			std::string mismatched;
			for (std::size_t i = k; i < log.size(); ++i) {
				if (stored_digest(r.apply(i + 1, log[i], false)) != stored[i]) {
					mismatched += " " + std::to_string(i + 1);
				}
				++replayed;
			}
			CHECK_EQ(mismatched, "");
			CHECK_EQ(r.files_digest(), want_files);
		}
	}
	std::printf("recovered from %zu checkpoints, twice each: %zu entries replayed\n", log.size() + 1, replayed);
	CHECK_EQ(r.index_digest(KEYS), want_index);
}

}  // namespace

TEST_MAIN(test_erase_of_a_missing_document_is_a_no_op, test_log_from_empty_ends_in_the_primary_state, test_recovery_from_every_checkpoint_converges)
