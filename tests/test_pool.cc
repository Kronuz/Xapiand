/*
 * Copyright (c) 2026 Dubalu LLC
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

// Concurrency tests for DatabasePool and ShardEndpoint: wake-ups on checkin and unlock, and
// concurrent cleanup. Shards here are never opened, so no database is touched.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "engine/xapian/errors.h"

using namespace std::chrono_literals;

struct PoolTestAccess {
	static std::mutex& mtx(ShardEndpoint& endpoint) { return endpoint.mtx; }
	static std::mutex& mtx(DatabasePool& pool) { return pool.mtx; }
	static std::atomic_int& locks(DatabasePool& pool) { return pool.locks; }
	static std::condition_variable& writable_cond(ShardEndpoint& endpoint) { return endpoint.writable_cond; }
	static std::condition_variable& readables_cond(ShardEndpoint& endpoint) { return endpoint.readables_cond; }
	static void set_local(Shard& shard) { shard._local = true; }  // as if opened locally
	static void close(Shard& shard) { shard._closed.store(true); }  // as if closed by a failed commit
};

namespace {

using A = PoolTestAccess;

// Plays a waiter that has just found the shard unavailable under the endpoint mutex and is about to
// block, while `release` (on another thread) makes it available. `release` first stops on the pool
// mutex, past its own endpoint-mutex sections, so the waiter can take the endpoint mutex at the
// racy point. The waiter must still be woken.
bool woken_after(DatabasePool& pool, ShardEndpoint& endpoint, std::condition_variable& cond, const std::function<void()>& release) {
	std::atomic_bool returned{false};
	std::unique_lock<std::mutex> pool_lk(A::mtx(pool));
	std::thread releaser([&] {
		release();
		returned = true;
	});
	std::this_thread::sleep_for(200ms);  // releaser is now blocked on the pool mutex
	std::unique_lock<std::mutex> lk(A::mtx(endpoint));
	pool_lk.unlock();
	for (int i = 0; i < 50 && !returned; ++i) {
		std::this_thread::sleep_for(10ms);
	}
	bool woken = cond.wait_for(lk, 2s) == std::cv_status::no_timeout;
	lk.unlock();
	releaser.join();
	return woken;
}

void test_checkin_wakes_writable_waiter() {
	DatabasePool pool(16, 4);
	auto shard = pool.checkout(Endpoint("pool_test_writable"), DB_WRITABLE | DB_CREATE_OR_OPEN, 1.0);
	REQUIRE(shard);
	auto& endpoint = shard->endpoint;
	A::locks(pool) += 1;  // makes checkin's notify_lockable take the pool mutex
	CHECK_TRUE(woken_after(pool, endpoint, A::writable_cond(endpoint), [&] { pool.checkin(shard); }));
	A::locks(pool) -= 1;
}

void test_checkin_wakes_readable_waiter() {
	DatabasePool pool(16, 1);
	auto shard = pool.checkout(Endpoint("pool_test_readable"), DB_OPEN, 1.0);
	REQUIRE(shard);
	auto& endpoint = shard->endpoint;
	A::locks(pool) += 1;
	CHECK_TRUE(woken_after(pool, endpoint, A::readables_cond(endpoint), [&] { pool.checkin(shard); }));
	A::locks(pool) -= 1;
}

void test_unlock_wakes_readable_waiter() {
	DatabasePool pool(16, 1);
	auto shard = pool.checkout(Endpoint("pool_test_unlock"), DB_WRITABLE | DB_CREATE_OR_OPEN, 1.0);
	REQUIRE(shard);
	auto& endpoint = shard->endpoint;
	A::set_local(*shard);
	pool.lock(shard, 1.0);
	CHECK_TRUE(endpoint.is_locked());
	CHECK_TRUE(woken_after(pool, endpoint, A::readables_cond(endpoint), [&] { pool.unlock(shard); }));
	CHECK_FALSE(endpoint.is_locked());
	pool.checkin(shard);
}

// Cleanups running at once (concurrent FLUSH) while readers and writers check shards in and out
// of the same endpoints: no use-after-free in the LRU walk, no checkout of a slot that cleanup is
// clearing, and no waiter left asleep (a lost wake-up shows as a timeout).
void test_concurrent_cleanup() {
	DatabasePool pool(8, 2);
	std::atomic_bool stop{false};
	std::atomic_int checkouts{0};
	std::atomic_int timeouts{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&, t] {
			int flags = t < 2 ? DB_OPEN : DB_WRITABLE | DB_CREATE_OR_OPEN;
			for (int i = 0; !stop; ++i) {
				try {
					auto shard = pool.checkout(Endpoint("pool_test_cleanup_" + std::to_string((t * 7 + i) % 16)), flags, 5.0);
					pool.checkin(shard);
					++checkouts;
				} catch (const engine::xapian::NotAvailable&) {
					++timeouts;
				}
			}
		});
	}
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&] {
			while (!stop) {
				pool.cleanup(true, false);
			}
		});
	}
	std::this_thread::sleep_for(3s);
	stop = true;
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_GT(checkouts.load(), 0);
	CHECK_EQ(timeouts.load(), 0);
}

// A waiter blocked on a busy shard while clear() takes that shard right after its checkin: clear
// must wake the waiter when it releases or drops the shard, and must not leave the shard it holds
// counted as available (a reader at the limit then spins holding the mutex and clear hangs).
void clear_wakes_waiters(int flags) {
	std::atomic_bool done{false};
	std::thread watchdog([&] {
		for (int i = 0; i < 600 && !done; ++i) {
			std::this_thread::sleep_for(100ms);
		}
		if (!done) {
			std::fprintf(stderr, "  FAIL clear_wakes_waiters(%d): hung\n", flags);
			std::_Exit(1);
		}
	});
	DatabasePool pool(8, 1);
	const Endpoint endpoint("pool_test_clear");
	std::atomic_bool stop{false};
	std::atomic_int timeouts{0};
	auto first = pool.checkout(endpoint, flags, 0.5);
	ShardEndpoint& shard_endpoint = first->endpoint;  // stays in the pool: nothing evicts here
	pool.checkin(first);
	std::thread cleaner([&] {
		while (!stop) {
			shard_endpoint.clear();
		}
	});
	for (int i = 0; i < 300; ++i) {
		std::shared_ptr<Shard> shard;
		try {
			shard = pool.checkout(endpoint, flags, 0.5);
		} catch (const engine::xapian::NotAvailable&) {
			++timeouts;
			continue;
		}
		std::atomic_bool started{false};
		std::thread waiter([&] {
			started = true;
			try {
				auto other = pool.checkout(endpoint, flags, 0.5);
				pool.checkin(other);
			} catch (const engine::xapian::NotAvailable&) {
				++timeouts;
			}
		});
		while (!started) {
			std::this_thread::yield();
		}
		std::this_thread::sleep_for(1ms);  // the waiter is (most likely) blocked on the busy shard
		pool.checkin(shard);
		waiter.join();
	}
	stop = true;
	cleaner.join();
	done = true;
	watchdog.join();
	CHECK_EQ(timeouts.load(), 0);
}

void test_clear_wakes_writable_waiters() { clear_wakes_waiters(DB_WRITABLE | DB_CREATE_OR_OPEN); }
void test_clear_wakes_readable_waiters() { clear_wakes_waiters(DB_OPEN); }

// Closed writables checked in while cleanups evict their idle endpoint: checkin must not touch
// the endpoint after dropping the writable (a destroyed mutex aborts the process).
void test_checkin_drop_races_cleanup() {
	DatabasePool pool(8, 2);
	std::atomic_bool stop{false};
	std::atomic_long checkins{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 2; ++t) {
		threads.emplace_back([&, t] {
			while (!stop) {
				auto shard = pool.checkout(Endpoint("pool_test_drop_" + std::to_string(t)), DB_WRITABLE | DB_CREATE_OR_OPEN, 5.0);
				A::close(*shard);
				pool.checkin(shard);
				++checkins;
			}
		});
	}
	for (int t = 0; t < 3; ++t) {
		threads.emplace_back([&] {
			while (!stop) {
				pool.cleanup(true, false);
			}
		});
	}
	std::this_thread::sleep_for(5s);
	stop = true;
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_GT(checkins.load(), 0);
}

// clear() keeps a shard that something else still references (a committer's weak_ptr lock), and
// frees it once that reference is gone.
void clear_keeps_shared_shard(int flags) {
	DatabasePool pool(8, 2);
	const Endpoint endpoint("pool_test_clear_shared");
	auto shard = pool.checkout(endpoint, flags, 1.0);
	ShardEndpoint& shard_endpoint = shard->endpoint;
	std::shared_ptr<Shard> extra = shard;
	pool.checkin(shard);
	auto cleared = shard_endpoint.clear();
	CHECK_EQ(cleared.first + cleared.second, 1u);
	CHECK_FALSE(extra->is_busy());
	auto again = pool.checkout(endpoint, flags, 1.0);
	CHECK_TRUE(again.get() == extra.get());
	pool.checkin(again);
	again.reset();
	extra.reset();
	cleared = shard_endpoint.clear();
	CHECK_EQ(cleared.first + cleared.second, 0u);
}

void test_clear_keeps_shared_writable() { clear_keeps_shared_shard(DB_WRITABLE | DB_CREATE_OR_OPEN); }
void test_clear_keeps_shared_readable() { clear_keeps_shared_shard(DB_OPEN); }

}  // namespace

TEST_MAIN(test_checkin_wakes_writable_waiter, test_checkin_wakes_readable_waiter, test_unlock_wakes_readable_waiter, test_concurrent_cleanup, test_clear_wakes_writable_waiters, test_clear_wakes_readable_waiters, test_checkin_drop_races_cleanup, test_clear_keeps_shared_writable, test_clear_keeps_shared_readable)
