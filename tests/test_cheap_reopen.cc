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

// The reopen of an evicted index (docs/design/cheap-reopen.md): the pool's resolve opens the index's directory once
// (D2), so a missing index without create is not_found and leaves nothing behind, and an index the catalog closed,
// or one a crashed process left, reopens serving exactly its last commit.

#include "test_harness.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/shard.h"
#include "engine/sync.h"
#include "pool_support.h"


// The pin's slot in the catalog (IndexPin befriends this name for the tests).
struct PoolTestAccess {
	static std::uint32_t slot(const IndexPin& pin) { return pin.pin_.where().slot; }
};

namespace {

// Writes without a journal or an autocommit (those need a manager): what reaches the index is what a commit made durable.
constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT;

std::string fresh(const char* name) {
	std::string path = std::string("cheap_reopen_test_") + name + ".db";
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.set_data(data);
	return doc;
}

engine::errc pin_error(DatabasePool& pool, const std::string& path, int flags) {
	try {
		(void)pin_index(pool, path, flags);
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

engine::doccount doc_count(Shard& shard) {
	return engine::sync_wait(shard.engine_for_read())->doc_count();
}

// Closes the index `pin` holds in the catalog (demoted, then closed), as the sweep evicts it; the pin is released.
void evict(DatabasePool& pool, IndexPin pin, const std::string& path) {
	const auto slot = PoolTestAccess::slot(pin);
	pin.release();
	auto& catalog = pool.catalog(path);
	CHECK_TRUE(catalog.begin_demote(slot));
	catalog.wait_chores();
	CHECK_TRUE(catalog.begin_close(slot));
	catalog.wait_chores();
	for (const auto& view : catalog.snapshot()) {
		CHECK_TRUE(view.key != path);
	}
}

}  // namespace


// A process that dies after its commits, with a write it never committed, leaves an index that reopens at exactly
// its last commit. The child forks before this process starts any thread, so it runs a pool of its own.
void test_a_crashed_index_reopens_at_its_last_commit() {
	const auto path = fresh("crash");
	const pid_t child = ::fork();
	if (child == 0) {
		DatabasePool pool(control_loop(), 8);
		Shard shard(pin_index(pool, path, WRITE), WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.add_document(make("Kb", "b")));
		const bool committed = engine::sync_wait(shard.commit());
		engine::sync_wait(shard.add_document(make("Kc", "c")));   // never committed
		std::fflush(nullptr);
		::_exit(committed ? 0 : 3);   // no destructor runs: nothing closes or commits the index
	}
	int status = 0;
	CHECK_EQ(::waitpid(child, &status, 0), child);
	CHECK_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	DatabasePool pool(control_loop(), 8);
	Shard shard(pin_index(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(doc_count(shard), 2u);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
}

// A missing index opened without create answers not_found and makes no directory; with create it is made.
void test_a_missing_index_is_not_found_and_not_made() {
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("missing");
	CHECK_TRUE(pin_error(pool, path, DB_OPEN) == engine::errc::not_found);
	CHECK_FALSE(std::filesystem::exists(path));
	CHECK_TRUE(pin_error(pool, path, DB_OPEN) == engine::errc::not_found);   // after the negative entry too
	{
		Shard shard(pin_index(pool, path, WRITE), WRITE);
		CHECK_EQ(doc_count(shard), 0u);
	}
	CHECK_TRUE(std::filesystem::is_directory(std::filesystem::path(path) / "strata"));
}

// An index the catalog closed reopens serving exactly its last commit, each time it is evicted.
void test_an_evicted_index_reopens_at_its_last_commit() {
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("evicted");
	{
		auto held = pin_index(pool, path, WRITE);
		{
			Shard shard = use_index(held, WRITE);
			engine::sync_wait(shard.add_document(make("Ka", "a")));
			CHECK_TRUE(engine::sync_wait(shard.commit()));
		}
		evict(pool, std::move(held), path);
	}
	{
		auto held = pin_index(pool, path, WRITE);
		{
			Shard shard = use_index(held, WRITE);
			CHECK_EQ(doc_count(shard), 1u);
			CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
			engine::sync_wait(shard.add_document(make("Kb", "b")));
			CHECK_TRUE(engine::sync_wait(shard.commit()));
		}
		evict(pool, std::move(held), path);
	}
	Shard shard(pin_index(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(doc_count(shard), 2u);
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	CHECK_EQ(pool.catalog(path).stats().opens, 3u);
}

// A pool of 200 on 8 threads is 8 partitions of 25 (D1): each key lives in its own partition's catalog only, the
// keys spread over all of them, and an index evicted from its partition reopens there.
void test_a_wide_pool_spreads_keys_over_its_partitions() {
	DatabasePool pool(control_loop(), 200, 8);
	CHECK_EQ(pool.partitions(), 8u);
	constexpr int keys = 64;
	std::vector<std::string> paths;
	for (int i = 0; i < keys; ++i) {
		paths.push_back(fresh(("wide" + std::to_string(i)).c_str()));
		Shard shard(pin_index(pool, paths.back(), WRITE), WRITE);
		engine::sync_wait(shard.add_document(make("K" + std::to_string(i), std::to_string(i))));
		CHECK_TRUE(engine::sync_wait(shard.commit()));
	}
	std::size_t used = 0;
	std::uint64_t opens = 0;
	for (std::size_t p = 0; p < pool.partitions(); ++p) {
		const auto n = pool.partition(p).stats().opens;
		opens += n;
		used += n != 0;
		for (const auto& view : pool.partition(p).snapshot()) {
			CHECK_TRUE(&pool.catalog(view.key) == &pool.partition(p));
		}
	}
	CHECK_EQ(opens, static_cast<std::uint64_t>(keys));
	CHECK_EQ(used, pool.partitions());
	CHECK_EQ(pool.count().first, static_cast<std::size_t>(keys));
	auto& home = pool.catalog(paths[5]);
	const auto before = home.stats().opens;
	evict(pool, pin_index(pool, paths[5], DB_OPEN), paths[5]);
	Shard shard(pin_index(pool, paths[5], DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "5");
	CHECK_EQ(home.stats().opens, before + 1);
}


int main() {
	struct Case { const char* name; void (*fn)(); };
	const Case cases[] = {
		{"test_a_crashed_index_reopens_at_its_last_commit", test_a_crashed_index_reopens_at_its_last_commit},   // first: it forks
		{"test_a_missing_index_is_not_found_and_not_made", test_a_missing_index_is_not_found_and_not_made},
		{"test_an_evicted_index_reopens_at_its_last_commit", test_an_evicted_index_reopens_at_its_last_commit},
		{"test_a_wide_pool_spreads_keys_over_its_partitions", test_a_wide_pool_spreads_keys_over_its_partitions},
	};
	for (const auto& c : cases) {
		const int before = testh::failures;
		try {
			c.fn();
		} catch (const std::exception& exc) {
			std::fprintf(stderr, "  FAIL %s: threw %s\n", c.name, exc.what());
			++testh::failures;
		}
		std::fprintf(stderr, "%s %s\n", testh::failures == before ? "ok  " : "FAIL", c.name);
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
