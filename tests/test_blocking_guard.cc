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


// The blocking guard (engine::check_may_block): engine::sync_wait and DatabasePool's checkout,
// checkin and cleanup refuse to run inside a reactor loop's run() or on a task scheduler's thread, and run on
// a reactor's offload pool, on the loop's thread after run() returns, and on a plain thread.

#include "test_harness.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <functional>
#include <future>
#include <memory>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "engine/release.h"
#include "engine/sync.h"
#include "reactor.h"
#include "task.hh"

namespace {

task::task<int> answer() {
	co_return 42;
}

// A scheduler that is never posted to: only its identity marks a thread as its own.
struct Idle {
	void post(std::coroutine_handle<>) { }
	std::uint64_t now() const noexcept { return 0; }
};

// Runs body in a child process; true when the guard stopped it (an abort with asserts on, a
// counted violation without).
bool guard_fires(const std::function<void()>& body) {
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		body();
		::_exit(engine::blocking_violations() != 0 ? 42 : 0);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	return (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) || (WIFEXITED(status) && WEXITSTATUS(status) == 42);
}

// Runs fn on a reactor loop, inside Reactor::run().
void on_loop(const std::function<void()>& fn) {
	reactor::Reactor r(1, 8);
	asio::post(r.io(), fn);
	r.run();
}

void checkout_and_checkin() {
	DatabasePool pool(8, 1);
	auto shard = pool.checkout(Endpoint("blocking_guard_test"), DB_WRITABLE | DB_CREATE_OR_OPEN, 0.5);
	pool.checkin(shard);
}

// Checks in on the calling thread a shard that was checked out off the loop.
void checkin_only(bool vector_overload) {
	DatabasePool pool(8, 1);
	auto shard = pool.checkout(Endpoint("blocking_guard_test"), DB_WRITABLE | DB_CREATE_OR_OPEN, 0.5);
	on_loop([&] {
		if (vector_overload) {
			std::vector<std::shared_ptr<Shard>> shards{shard};
			shard.reset();
			pool.checkin(shards);
		} else {
			pool.checkin(shard);
		}
	});
}

// An object whose destructor does database work.
struct Teardown {
	~Teardown() { engine::check_may_block("Teardown"); }
};

void destroy_on_loop() {
	on_loop([] { Teardown t; });
}

void release_on_loop_via_pool() {
	reactor::Reactor r(1, 8);
	asio::co_spawn(r.io(), engine::release_on_pool(&r, std::make_unique<Teardown>()), asio::detached);
	r.run();
}

// Releases from a session whose cancellation slot fired, as TcpServer::stop() leaves one.
void release_after_cancel(bool keep_hop) {
	reactor::Reactor r(1, 8);
	asio::cancellation_signal stop;
	auto session = [&r, keep_hop]() -> asio::awaitable<void> {
		auto t = std::make_unique<Teardown>();
		asio::steady_timer wait(co_await asio::this_coro::executor, std::chrono::hours(1));
		try { co_await wait.async_wait(asio::use_awaitable); } catch (const std::exception&) { }
		if (keep_hop) {
			co_await asio::this_coro::throw_if_cancelled(false);
		}
		try { co_await engine::release_on_pool(&r, std::move(t)); } catch (const std::exception&) { }
	};
	asio::co_spawn(r.io(), session(), asio::bind_cancellation_slot(stop.slot(), asio::detached));
	asio::post(r.io(), [&] { stop.emit(asio::cancellation_type::all); });
	r.run();
}

void cleanup() {
	DatabasePool pool(8, 1);
	pool.cleanup(true, false);
}

void wait_answer() {
	CHECK_EQ(engine::sync_wait(answer()), 42);
}

}  // namespace

// Forks first, before any test starts a thread.
static void test_fires_on_loop() {
	CHECK_TRUE(guard_fires([] { on_loop(wait_answer); }));
	CHECK_TRUE(guard_fires([] { on_loop(checkout_and_checkin); }));
	CHECK_TRUE(guard_fires([] { on_loop(cleanup); }));
	CHECK_TRUE(guard_fires([] { checkin_only(false); }));
	CHECK_TRUE(guard_fires([] { checkin_only(true); }));
	CHECK_TRUE(guard_fires(destroy_on_loop));
	CHECK_TRUE(guard_fires([] { release_after_cancel(false); }));
}

static void test_fires_on_scheduler_thread() {
	CHECK_TRUE(guard_fires([] {
		Idle idle;
		task::running_on scope(task::scheduler_ref::of(idle));
		wait_answer();
	}));
}

static void test_quiet_off_loop() {
	CHECK_FALSE(guard_fires(wait_answer));
	CHECK_FALSE(guard_fires(release_on_loop_via_pool));
	CHECK_FALSE(guard_fires([] { release_after_cancel(true); }));
	CHECK_FALSE(guard_fires([] {
		on_loop([] { });
		checkout_and_checkin();  // the loop's thread, after run() returns
		cleanup();
		wait_answer();
	}));

	reactor::Reactor r(1, 8);
	std::promise<void> done;
	asio::post(r.io(), [&] {
		asio::post(*r.pool(), [&] {
			checkout_and_checkin();
			cleanup();
			wait_answer();
			done.set_value();
		});
	});
	r.run();
	done.get_future().get();
	CHECK_EQ(engine::blocking_violations(), 0u);
}

TEST_MAIN(test_fires_on_loop, test_fires_on_scheduler_thread, test_quiet_off_loop)
