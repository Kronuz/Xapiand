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

// The log replay through the real DatabaseWAL::execute_line (wal.cc): a replayed erase of a missing document is
// a no-op, and replaying a log twice ends in the primary's state (CR4). The engine shard is opened directly and replayed into through
// a WalTarget; lines are built as DatabaseWALWriterTask builds them.
#include "test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include "compressor_lz4.h"
#include "database/wal.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/sync.h"
#include "length.h"
#include "node.h"
#include "utype.hh"

namespace {

// What the log replays into: an engine shard opened on its own, as the registry's open chore does (chores inline).
class Target final : public WalTarget {
public:
	Target(const std::string& path, engine::Opened opened) : path_(path), shard_(std::move(opened.shard)), start_(shard_->revision()) { }
	const std::string& path() const override { return path_; }
	engine::Shard& engine() override { return *shard_; }
	engine::rev start_revision() const override { return start_; }
	bool commit(bool) override { engine::sync_wait(shard_->commit(engine::Transaction::none)); return true; }
	void replace_document(engine::docid did, engine::Document&& doc, bool, bool, bool) override {
		engine::WriteArgs args;
		engine::sync_wait(shard_->replace(did, doc, args));
	}
	void delete_document(engine::docid did, bool, bool, bool) override { engine::sync_wait(shard_->erase(did, true, std::nullopt)); }
	void set_metadata(const std::string& key, const std::string& value, bool, bool) override { engine::sync_wait(shard_->set_metadata(key, value)); }
	void add_spelling(const std::string& word, engine::termcount freq, bool, bool) override { engine::sync_wait(shard_->add_spelling(word, freq)); }
	void remove_spelling(const std::string& word, engine::termcount freq, bool, bool) override { engine::sync_wait(shard_->remove_spelling(word, freq)); }

private:
	std::string path_;
	std::unique_ptr<engine::Shard> shard_;
	engine::rev start_;
};

std::string wal_line(engine::rev rev, DatabaseWAL::Type type, const std::string& data) {
	return serialise_length(rev) + serialise_length(toUType(type)) + compress_lz4(data);
}
std::string fresh(const char* name) {
	std::string p = name;
	std::filesystem::remove_all(p);
	return p;
}
std::string run(DatabaseWAL& wal, const std::string& line) {
	try { wal.execute_line(line, false); return "ok"; }
	catch (const engine::Error& e) { return std::string("engine::Error ") + engine::to_string(e.kind) + ": " + e.description(); }
	catch (const std::exception& e) { return std::string("exception: ") + e.what(); }
}
std::string state(engine::Shard& s) {
	std::string out;
	engine::sync_wait(s.scan(0, [&](const engine::DocView& v) { out += std::to_string(v.did()) + ":v" + std::to_string(v.version()) + ":" + v.data() + ";"; }));
	return out + "last=" + std::to_string(engine::sync_wait(s.stats()).lastdocid);
}

void erase_missing() {
	const auto path = fresh("test_wal_replay_erase.db");
	Target target(path, engine::sync_wait(engine::open_local(path, true, true)));
	DatabaseWAL wal(&target);
	auto rev = target.engine().revision();
	auto r = run(wal, wal_line(rev, DatabaseWAL::Type::DELETE_DOCUMENT, serialise_length(7)));
	std::printf("replayed erase of a missing document through execute_line: %s\n", r.c_str());
	CHECK_EQ(r, "ok");
}

void double_replay() {
	const auto path = fresh("test_wal_replay_twice.db");
	Target target(path, engine::sync_wait(engine::open_local(path, true, true)));
	DatabaseWAL wal(&target);
	auto& e = target.engine();
	const auto rev = e.revision();
	auto make = [&](const std::string& data) { engine::Document d; d.set_key("Qa"); d.add_term(d.field("f"), "body", 2); d.set_data(data); return d; };
	std::vector<std::string> lines;
	engine::WriteArgs a;
	auto d1 = make("a1"); auto r1 = engine::sync_wait(e.replace_by_key("Qa", d1, a));
	lines.push_back(wal_line(rev, DatabaseWAL::Type::REPLACE_DOCUMENT, serialise_length(r1.did) + d1.serialise()));
	engine::sync_wait(e.erase(r1.did, true, std::nullopt));
	lines.push_back(wal_line(rev, DatabaseWAL::Type::DELETE_DOCUMENT, serialise_length(r1.did)));
	auto d2 = make("a2"); auto r2 = engine::sync_wait(e.replace_by_key("Qa", d2, a));
	lines.push_back(wal_line(rev, DatabaseWAL::Type::REPLACE_DOCUMENT, serialise_length(r2.did) + d2.serialise()));
	const auto want = state(e);
	std::printf("primary: %s\n", want.c_str());
	for (int pass = 1; pass <= 2; ++pass) {
		std::string res;
		for (auto& l : lines) { auto r = run(wal, l); if (r != "ok") res += " [" + r + "]"; }
		std::printf("pass %d via execute_line: %s%s -> %s\n", pass, res.empty() ? "all ok" : "errors:", res.c_str(), state(e).c_str());
		CHECK_EQ(res, "");
		CHECK_EQ(state(e), want);
	}
}
void t_all() { erase_missing(); double_replay();
	std::filesystem::remove_all("test_wal_replay_erase.db");
	std::filesystem::remove_all("test_wal_replay_twice.db");
}
}
TEST_MAIN(t_all)
