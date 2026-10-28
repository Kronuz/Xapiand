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

// The engine behavior that WAL replay depends on (docs/mechanisms/wal-replay.md): what dropping a
// writer handle does to pending work, and how a replay inside a transaction commits. CR1 hid in the
// first: a replay left pending in the writer is invisible to every other handle.

#include "test_harness.h"

#include <filesystem>
#include <string>

#include "engine/sync.h"
#include "engine/xapian/errors.h"
#include "engine/xapian/shard.h"

namespace {

engine::Document make(const std::string& key) {
	engine::Document doc;
	doc.add_boolean_term(key);
	doc.set_data(key);
	return doc;
}

void put(engine::Shard& shard, const std::string& key) {
	auto doc = make(key);
	engine::sync_wait(shard.add(doc, engine::WriteArgs{}));
}

engine::doccount visible(const std::string& path) {
	auto reader = engine::sync_wait(engine::xapian::open_local(path, false, false));
	return reader.shard->doc_count();
}

std::string fresh(const char* name) {
	const auto path = (std::filesystem::current_path() / name).string();
	std::filesystem::remove_all(path);
	return path;
}

// Pending writes are invisible to other handles; dropping the writer flushes them. This flush, once
// done by a retry loop that opened the shard many times, is what made a replayed WAL visible.
void test_plain_handle() {
	const auto path = fresh("engine_replay_plain.db");
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	put(*opened.shard, "Qa");
	CHECK_EQ(visible(path), 0u);
	opened.shard.reset();
	CHECK_EQ(visible(path), 1u);
	std::filesystem::remove_all(path);
}

// Replay with an explicit commit: visible at once, and the revision moves on exactly once.
void test_explicit_commit() {
	const auto path = fresh("engine_replay_commit.db");
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
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

// A writer in a transaction: the plain commit is refused, the transaction's own commit publishes the
// replay and keeps the transaction open, and dropping the writer discards what the transaction held.
void transaction_case(bool flushed) {
	const auto path = fresh(flushed ? "engine_replay_flushed.db" : "engine_replay_unflushed.db");
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	engine::sync_wait(shard.begin_transaction(flushed));
	put(shard, "Qa");
	CHECK_EQ(visible(path), 0u);
	bool refused = false;
	try {
		engine::sync_wait(shard.commit(engine::Transaction::none));
	} catch (const engine::Error&) {
		refused = true;
	}
	CHECK_TRUE(refused);
	CHECK_EQ(visible(path), 0u);

	// The transaction commit of the shard's own mode publishes the replay and starts the next transaction.
	engine::sync_wait(shard.commit(flushed ? engine::Transaction::flushed : engine::Transaction::unflushed));
	CHECK_EQ(visible(path), 1u);
	put(shard, "Qb");
	CHECK_EQ(visible(path), 1u);

	// Dropping the writer with the transaction open does not publish what it holds.
	opened.shard.reset();
	CHECK_EQ(visible(path), 1u);
	std::filesystem::remove_all(path);
}

void test_flushed_transaction() { transaction_case(true); }
void test_unflushed_transaction() { transaction_case(false); }

}  // namespace

TEST_MAIN(test_plain_handle, test_explicit_commit, test_flushed_transaction, test_unflushed_transaction)
