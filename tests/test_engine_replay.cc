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

// The engine behavior that WAL replay depends on (docs/mechanisms/wal-replay.md): an explicit commit
// publishes the replay at once and moves the revision on exactly once.

#include "test_harness.h"

#include <filesystem>
#include <string>

#include "engine/open.h"
#include "engine/sync.h"

namespace {

engine::Document make(const std::string& key) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_boolean_term(doc.field("f"), key);
	doc.set_data(key);
	return doc;
}

void put(engine::Shard& shard, const std::string& key) {
	auto doc = make(key);
	engine::sync_wait(shard.add(doc, engine::WriteArgs{}));
}

engine::doccount visible(const std::string& path) {
	auto reader = engine::sync_wait(engine::open_local(path, false, false));
	return reader.shard->doc_count();
}

std::string fresh(const char* name) {
	const auto path = (std::filesystem::current_path() / name).string();
	std::filesystem::remove_all(path);
	return path;
}

// Replay with an explicit commit: visible at once, and the revision moves on exactly once.
void test_explicit_commit() {
	const auto path = fresh("engine_replay_commit.db");
	auto opened = engine::sync_wait(engine::open_local(path, true, true));
	auto& shard = *opened.shard;
	put(shard, "Qa");
	put(shard, "Qb");
	const auto before = shard.revision();
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);
	CHECK_EQ(visible(path), 2u);
	// Nothing pending: a further commit leaves the revision alone.
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

}  // namespace

TEST_MAIN(test_explicit_commit)
