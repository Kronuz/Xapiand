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


// The blocking guard (engine::check_may_block): engine::sync_wait and the registry's waiting entry points
// (a flush, the pool's join) refuse to run inside a reactor loop's run() or on a task scheduler's
// thread, and run off the loop on a plain thread, and on the loop's thread after run() returns.

#include "test_harness.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/pool.h"
#include "pool_support.h"
#include "database/shard.h"
#include "engine/host_asio.h"
#include "engine/sync.h"
#include "reactor.h"
#include "shutdown_wait.h"
#include "task.hh"

#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define BLOCKING_GUARD_TSAN 1
#  endif
#endif
#if defined(__SANITIZE_THREAD__) || defined(BLOCKING_GUARD_TSAN)
// guard_fires forks after the control loop's threads exist; the child starts threads of its own.
extern "C" const char* __tsan_default_options() { return "die_after_fork=0"; }
#endif

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
	reactor::Reactor r;
	asio::post(r.io(), fn);
	r.run();
}

// Opens an index through the registry: a wait for the open. The index starts empty each time, so an earlier
// build's files left in the build directory never reach the open.
void open_through_the_registry() {
	std::filesystem::remove_all("blocking_guard_test");
	DatabasePool pool(control_loop(), 8);
	{
		Shard shard(engine::sync_wait(pool.pin(Endpoint("blocking_guard_test"), DB_WRITABLE | DB_CREATE_OR_OPEN)), DB_WRITABLE | DB_CREATE_OR_OPEN);
		engine::sync_wait(shard.ready());
	}
	pool.finish();
	pool.join();
	std::filesystem::remove_all("blocking_guard_test");
}

// An object whose destructor does database work.
struct Teardown {
	~Teardown() { engine::check_may_block("Teardown"); }
};

void destroy_on_loop() {
	on_loop([] { Teardown t; });
}

void cleanup() {
	DatabasePool pool(control_loop(), 8);
	engine::sync_wait(pool.flush());
}

void join() {
	DatabasePool pool(control_loop(), 8);
	pool.finish();
	pool.join();
}

void wait_answer() {
	CHECK_EQ(engine::sync_wait(answer()), 42);
}

}  // namespace

void bounded_wait() {
	shutdown_wait::wait(std::chrono::milliseconds(50), [] { }, {});
}

// Forks first, before any test starts a thread.
static void test_fires_on_loop() {
	CHECK_TRUE(guard_fires([] { on_loop(bounded_wait); }));   // the shutdown's bounded wait is a main-thread entry (D14)
	CHECK_TRUE(guard_fires([] { on_loop(wait_answer); }));
	CHECK_TRUE(guard_fires([] { on_loop(open_through_the_registry); }));
	CHECK_TRUE(guard_fires([] { on_loop(cleanup); }));
	CHECK_TRUE(guard_fires([] { on_loop(join); }));
	CHECK_TRUE(guard_fires(destroy_on_loop));
}

// The chores of an index end with a WorkGroup, and joining one is a wait: on a loop it is left to a worker.
static void test_quiet_when_a_loop_drops_a_group_of_chores() {
	CHECK_FALSE(guard_fires([] {
		DatabasePool pool(control_loop(), 8);
		on_loop([&] {
			IndexChores chores(pool.work_pool());
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(200));   // the worker that joins it
	}));
}

static void test_fires_on_scheduler_thread() {
	CHECK_TRUE(guard_fires([] {
		Idle idle;
		task::running_on scope(task::scheduler_ref::of(idle));
		wait_answer();
	}));
}

// A task that a blocking wait drives runs on the waiting thread, which may block: a wait inside it is not a violation. The
// control loop's tasks are not so.
static task::task<int> nested_wait() {
	co_return engine::sync_wait(answer()) + 1;
}

static void test_quiet_inside_a_blocking_wait() {
	CHECK_FALSE(guard_fires([] { CHECK_EQ(engine::sync_wait(nested_wait()), 43); }));
	CHECK_TRUE(guard_fires([] {
		asio::io_context io;
		task::asio_scheduler scheduler(io.get_executor());
		engine::AsioHost host(scheduler);
		host.spawn([]() -> task::task<void> { engine::sync_wait(answer()); co_return; }());
		io.run();
	}));
}

static void test_quiet_off_loop() {
	CHECK_FALSE(guard_fires(wait_answer));
	CHECK_FALSE(guard_fires([] {
		on_loop([] { });
		open_through_the_registry();  // the loop's thread, after run() returns
		cleanup();
		join();
		wait_answer();
	}));

	// A thread a loop's handler started is not the loop.
	reactor::Reactor r;
	std::promise<void> done;
	std::thread worker;
	asio::post(r.io(), [&] {
		worker = std::thread([&] {
			open_through_the_registry();
			cleanup();
			join();
			wait_answer();
			done.set_value();
		});
	});
	r.run();
	done.get_future().get();
	worker.join();
	CHECK_EQ(engine::blocking_violations(), 0u);
}

TEST_MAIN(test_fires_on_loop, test_fires_on_scheduler_thread, test_quiet_inside_a_blocking_wait, test_quiet_off_loop, test_quiet_when_a_loop_drops_a_group_of_chores)
