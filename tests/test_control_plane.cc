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

// The control plane's pieces (docs/design/phase7c.md D13): control::Tasks, Periodic and Debounce on a loop of their
// own, and the SingleFlight that keeps concurrent first uses of a schema or a settings document to one load (D11).
// Every wait here is a timer or an event on the loop; a stop ends a waiting task at once.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "asio.hpp"
#include "control_plane.h"
#include "database/data.h"
#include "engine/host_asio.h"
#include "reactor.h"
#include "reactor_lag.h"
#include "shutdown_wait.h"
#include "server/mime_types.h"
#include "single_flight.h"
#include "stopper.h"
#include "task.hh"

using namespace std::chrono_literals;

namespace {

// A loop on a thread of its own with its host, as the manager's control loop.
class Loop {
public:
	Loop() : guard_(asio::make_work_guard(reactor.io())) {
		asio::post(reactor.io(), [this] { host.bind_loop_thread(); });
		thread_ = std::thread([this] { reactor.run(); });
	}
	~Loop() {
		stop();
	}
	// Starts the loop's lag probe and keeps it at its 1 ms interval, as a loop that serves requests does.
	void watch_lag() {
		reactor.lag().start();
		pulse_ = std::thread([this] {
			while (!stopping_) {
				reactor.lag().note_activity();
				std::this_thread::sleep_for(20ms);
			}
		});
	}
	void stop() {
		stopping_ = true;
		if (pulse_.joinable()) {
			pulse_.join();
		}
		if (thread_.joinable()) {
			guard_.reset();
			reactor.io().stop();
			thread_.join();
		}
	}

	reactor::Reactor reactor;
	engine::AsioHost host{reactor.scheduler()};

private:
	asio::executor_work_guard<asio::io_context::executor_type> guard_;
	std::thread thread_;
	std::thread pulse_;
	std::atomic<bool> stopping_{false};
};

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit = 3s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	return done();
}

}  // namespace


static void test_tasks_run_on_the_host_and_are_counted() {
	Loop loop;
	control::Tasks tasks(loop.host);
	std::atomic<int> ran{0};
	std::atomic<bool> go{false};
	task::event release;
	for (int i = 0; i < 3; ++i) {
		tasks.spawn([](std::atomic<int>& ran, task::event& release) -> task::task<void> {
			co_await release;
			++ran;
		}(ran, release));
	}
	CHECK_EQ(tasks.running(), 3u);
	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(ran.load(), 3);
	(void)go;
}

static void test_a_failing_task_is_reported_and_counted_out() {
	Loop loop;
	control::Tasks tasks(loop.host);
	std::atomic<int> failures{0};
	tasks.failed = [&](const std::exception&) { ++failures; };
	tasks.spawn([]() -> task::task<void> {
		throw std::runtime_error("boom");
		co_return;
	}());
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(failures.load(), 1);
}

static void test_periodic_ticks_on_the_loop_and_stops_at_once() {
	Loop loop;
	control::Tasks tasks(loop.host);
	std::atomic<int> ticks{0};
	std::atomic<bool> on_loop{true};
	control::Periodic periodic(tasks, 20ms, [&] {
		on_loop = on_loop && loop.reactor.on_loop();
		++ticks;
	});
	periodic.start();
	CHECK(eventually([&] { return ticks.load() >= 3; }));
	CHECK(on_loop.load());
	periodic.stop();
	CHECK(eventually([&] { return tasks.running() == 0; }, 500ms));
	const int stopped_at = ticks.load();
	std::this_thread::sleep_for(100ms);
	CHECK_EQ(ticks.load(), stopped_at);
}

static void test_a_long_wait_ends_when_stopped() {
	Loop loop;
	control::Tasks tasks(loop.host);
	std::atomic<int> ticks{0};
	control::Periodic periodic(tasks, 1h, [&] { ++ticks; });
	periodic.start();
	std::this_thread::sleep_for(50ms);
	CHECK_EQ(tasks.running(), 1u);
	const auto began = std::chrono::steady_clock::now();
	periodic.stop();
	CHECK(eventually([&] { return tasks.running() == 0; }, 1s));
	CHECK_LT(std::chrono::steady_clock::now() - began, 500ms);   // not the hour
	CHECK_EQ(ticks.load(), 0);
}

static void test_a_failing_tick_does_not_end_the_periodic_task() {
	Loop loop;
	control::Tasks tasks(loop.host);
	std::atomic<int> failures{0};
	tasks.failed = [&](const std::exception&) { ++failures; };
	std::atomic<int> calls{0};
	control::Periodic periodic(tasks, 10ms, [&] {
		if (++calls == 1) {
			throw std::runtime_error("one bad tick");
		}
	});
	periodic.start();
	CHECK(eventually([&] { return calls.load() >= 3; }));
	periodic.stop();
	CHECK_EQ(failures.load(), 1);
	CHECK(eventually([&] { return tasks.running() == 0; }, 500ms));
}

using Autocommit = control::Debounce<int>;

static void test_a_burst_of_writes_is_one_commit_after_the_quiet_time() {
	Loop loop;
	std::atomic<int> fired{0};
	std::atomic<long long> fired_at{0};
	Autocommit autocommit(loop.host, {60ms, 120ms, 2s, 3s}, [&](const int&) {
		fired_at = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		++fired;
	});
	long long last_touch = 0;
	for (int i = 0; i < 10; ++i) {
		CHECK(autocommit.touch(7));
		last_touch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		std::this_thread::sleep_for(15ms);
	}
	CHECK_EQ(fired.load(), 0);   // still being written to
	CHECK(eventually([&] { return fired.load() > 0; }));
	std::this_thread::sleep_for(300ms);
	CHECK_EQ(fired.load(), 1);   // one call for the whole burst
	CHECK_GE(fired_at.load() - last_touch, 60);   // after the quiet time
	CHECK_EQ(autocommit.pending(), 0u);
	CHECK(eventually([&] { return autocommit.tasks().running() == 0; }, 500ms));
}

static void test_keys_are_debounced_apart() {
	Loop loop;
	std::mutex m;
	std::vector<int> fired;
	Autocommit autocommit(loop.host, {30ms, 60ms, 2s, 3s}, [&](const int& key) {
		std::lock_guard<std::mutex> lk(m);
		fired.push_back(key);
	});
	for (int key = 1; key <= 5; ++key) {
		autocommit.touch(key);
		autocommit.touch(key);
	}
	CHECK_EQ(autocommit.pending(), 5u);
	CHECK(eventually([&] { std::lock_guard<std::mutex> lk(m); return fired.size() >= 5; }));
	std::this_thread::sleep_for(100ms);
	std::lock_guard<std::mutex> lk(m);
	CHECK_EQ(fired.size(), 5u);
}

static void test_writes_that_never_stop_are_committed_by_the_forced_time() {
	Loop loop;
	std::atomic<int> fired{0};
	Autocommit autocommit(loop.host, {50ms, 100ms, 300ms, 300ms}, [&](const int&) { ++fired; });
	const auto began = std::chrono::steady_clock::now();
	while (fired.load() == 0 && std::chrono::steady_clock::now() - began < 2s) {
		autocommit.touch(1);   // a write every 10 ms: the quiet time never comes
		std::this_thread::sleep_for(10ms);
	}
	const auto waited = std::chrono::steady_clock::now() - began;
	CHECK_EQ(fired.load(), 1);
	CHECK_GE(waited, 250ms);
	CHECK_LT(waited, 900ms);
}

static void test_a_stop_ends_the_waiting_without_running_the_calls() {
	Loop loop;
	std::atomic<int> fired{0};
	Autocommit autocommit(loop.host, {10s, 10s, 10s, 10s}, [&](const int&) { ++fired; });
	for (int key = 0; key < 20; ++key) {
		CHECK(autocommit.touch(key));
	}
	std::this_thread::sleep_for(50ms);
	CHECK_EQ(autocommit.tasks().running(), 20u);
	autocommit.stop();
	CHECK(eventually([&] { return autocommit.tasks().running() == 0; }, 1s));
	CHECK_EQ(fired.load(), 0);
	CHECK_FALSE(autocommit.touch(1));   // the caller commits now
}

// The loop answers its lag probe while thousands of keys wait and fire: nothing on it waits or scans.
static void test_pending_keys_cost_the_loop_nothing() {
	Loop loop;
	loop.watch_lag();
	std::atomic<int> fired{0};
	Autocommit autocommit(loop.host, {500ms, 600ms, 1s, 1s}, [&](const int&) { ++fired; });
	for (int key = 0; key < 5000; ++key) {
		autocommit.touch(key);
	}
	CHECK(eventually([&] { return fired.load() == 5000; }, 5s));
	const auto snap = loop.reactor.lag().snapshot();
	CHECK_GT(snap.wakes, 50u);
	CHECK_LE(snap.quantile_upper_us(0.99), 16384u);   // p99 under 16 ms with 5,000 watchers started and fired
	if (snap.quantile_upper_us(0.99) > 16384u) {
		std::uint64_t late = 0;
		for (std::size_t i = 0; i < reactor::LagSnapshot::buckets; ++i) {
			if (reactor::LagSnapshot::upper_us(i) > 16384u) {
				late += snap.count[i];
			}
		}
		std::fprintf(stderr, "  lag: %llu wakes, %llu over 16 ms, p50 %llu us, p99 %llu us, max %llu us\n", static_cast<unsigned long long>(snap.wakes), static_cast<unsigned long long>(late), static_cast<unsigned long long>(snap.quantile_upper_us(0.5)), static_cast<unsigned long long>(snap.quantile_upper_us(0.99)), static_cast<unsigned long long>(snap.max_us));
	}
}


// ---- Teardown (task's cancellation D8) ----

// The loop's scheduler destroys the periodic task while it waits: its published wake goes with it, so a later stop()
// sets nothing, and the task is counted out.
static void test_teardown_unpublishes_the_periodic_wake() {
	Loop loop;
	control::Tasks tasks(loop.host);
	control::Periodic periodic(tasks, 1h, [] { });
	periodic.start();
	CHECK(eventually([&] { return periodic.waiting(); }));
	loop.stop();
	loop.reactor.scheduler().close();
	loop.reactor.scheduler().destroy_roots();
	CHECK_FALSE(periodic.waiting());
	CHECK_EQ(tasks.running(), 0u);
	periodic.stop();
}

// The same for the debounced autocommit's waiting keys.
static void test_teardown_unpublishes_the_autocommit_wakes() {
	Loop loop;
	std::atomic<int> fired{0};
	Autocommit autocommit(loop.host, {10s, 10s, 10s, 10s}, [&](const int&) { ++fired; });
	for (int key = 0; key < 3; ++key) {
		CHECK(autocommit.touch(key));
	}
	CHECK(eventually([&] { return autocommit.waiting() == 3; }));
	loop.stop();
	loop.reactor.scheduler().close();
	loop.reactor.scheduler().destroy_roots();
	CHECK_EQ(autocommit.waiting(), 0u);
	CHECK_EQ(autocommit.tasks().running(), 0u);
	autocommit.stop();
	CHECK_EQ(fired.load(), 0);
}

// A task spawned on a closed host is destroyed unrun and counted out, so a drain that waits for the count ends.
static void test_a_task_spawned_on_a_closed_host_is_counted_out() {
	Loop loop;
	loop.stop();
	loop.reactor.scheduler().close();
	control::Tasks tasks(loop.host);
	bool ran = false;
	tasks.spawn([](bool& ran) -> task::task<void> {
		ran = true;
		co_return;
	}(ran));
	CHECK_EQ(tasks.running(), 0u);
	CHECK_FALSE(ran);
}

// A blocking wait's scheduler owns no roots, so a spawn onto it always runs its task (Host::run never waits on a
// task destroyed unrun).
static void test_a_blocking_scheduler_owns_no_roots() {
	engine::BlockingScheduler blocking;
	CHECK(task::scheduler_ref::of(blocking).roots() == nullptr);
	CHECK_EQ(blocking.run([]() -> task::task<int> { co_return 7; }()), 7);
}


// ---- SingleFlight ----

static void test_concurrent_first_uses_load_once() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	std::atomic<int> loads{0};
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> got{0};
	for (int i = 0; i < 8; ++i) {
		tasks.spawn([](SingleFlight<std::string, int>& flights, std::atomic<int>& loads, task::event& release, std::atomic<int>& got) -> task::task<void> {
			auto value = co_await flights.run("k", [&]() -> task::task<int> {
				++loads;
				co_await release;
				co_return 42;
			});
			if (value == 42) { ++got; }
		}(flights, loads, release, got));
	}
	CHECK(eventually([&] { return flights.waiting() == 7; }));   // seven awaited the one load
	CHECK_EQ(loads.load(), 1);
	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(loads.load(), 1);
	CHECK_EQ(got.load(), 8);
	CHECK_EQ(flights.loads(), 1u);
	CHECK_EQ(flights.joined(), 7u);
}

static void test_a_later_use_loads_again() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	int loads = 0;
	for (int i = 0; i < 3; ++i) {
		const int v = task::sync_wait(flights.run("k", [&]() -> task::task<int> {
			++loads;
			co_return loads;
		}));
		CHECK_EQ(v, i + 1);
	}
	CHECK_EQ(flights.loads(), 3u);
}

static void test_other_keys_load_beside_each_other() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	std::atomic<int> loads{0};
	task::event release;
	control::Tasks tasks(loop.host);
	for (int i = 0; i < 4; ++i) {
		tasks.spawn([](SingleFlight<std::string, int>& flights, std::atomic<int>& loads, task::event& release, int i) -> task::task<void> {
			(void)co_await flights.run("key" + std::to_string(i), [&]() -> task::task<int> {
				++loads;
				co_await release;
				co_return i;
			});
		}(flights, loads, release, i));
	}
	CHECK(eventually([&] { return loads.load() == 4; }));
	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(flights.joined(), 0u);
}

static void test_a_failed_load_reaches_every_waiter() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> errors{0};
	for (int i = 0; i < 4; ++i) {
		tasks.spawn([](SingleFlight<std::string, int>& flights, task::event& release, std::atomic<int>& errors) -> task::task<void> {
			try {
				(void)co_await flights.run("k", [&]() -> task::task<int> {
					co_await release;
					throw std::runtime_error("the load failed");
					co_return 0;
				});
			} catch (const std::runtime_error&) {
				++errors;
			}
		}(flights, release, errors));
	}
	CHECK(eventually([&] { return flights.waiting() == 3; }));
	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(errors.load(), 4);
	CHECK_EQ(flights.loads(), 1u);
	// The key is free again: the next use loads.
	CHECK_EQ(task::sync_wait(flights.run("k", []() -> task::task<int> { co_return 5; })), 5);
}

static void test_a_cancelled_leader_leaves_the_waiters_to_load() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> got{0};
	std::atomic<int> leader_cancelled{0};
	std::atomic<int> loads{0};
	// The first call becomes the leader and is cancelled once released; the others load for themselves.
	auto call = [&](bool cancel) {
		tasks.spawn([](SingleFlight<std::string, int>& flights, task::event& release, std::atomic<int>& got, std::atomic<int>& cancelled, std::atomic<int>& loads, bool cancel) -> task::task<void> {
			try {
				const int v = co_await flights.run("k", [&]() -> task::task<int> {
					++loads;
					if (cancel) {
						co_await release;
						throw task::cancelled{};
					}
					co_return 9;
				});
				if (v == 9) { ++got; }
			} catch (const task::cancelled&) {
				++cancelled;
			}
		}(flights, release, got, leader_cancelled, loads, cancel));
	};
	call(true);
	CHECK(eventually([&] { return loads.load() == 1; }));
	call(false);
	call(false);
	CHECK(eventually([&] { return flights.waiting() == 2; }));
	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(leader_cancelled.load(), 1);
	CHECK_EQ(got.load(), 2);
}

static void test_a_destroyed_leader_does_not_strand_the_waiters() {
	Loop loop;
	SingleFlight<std::string, int> flights;
	control::Tasks tasks(loop.host);
	std::atomic<int> got{0};
	// The leader's frame is destroyed while it waits (a shutdown drops it): its waiter loads for itself.
	auto leader = flights.run("k", []() -> task::task<int> {
		co_await std::suspend_always{};
		co_return 1;
	});
	std::atomic<bool> started{false};
	auto handle = leader.release();
	asio::post(loop.reactor.io(), [&] {
		task::running_on scope(task::scheduler_ref::of(loop.host));
		handle.resume();
		started = true;
	});
	CHECK(eventually([&] { return started.load(); }));
	CHECK_EQ(flights.loads(), 1u);
	tasks.spawn([](SingleFlight<std::string, int>& flights, std::atomic<int>& got) -> task::task<void> {
		got = co_await flights.run("k", []() -> task::task<int> { co_return 77; });
	}(flights, got));
	CHECK(eventually([&] { return flights.waiting() == 1; }));
	asio::post(loop.reactor.io(), [handle] { handle.destroy(); });
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(got.load(), 77);
}


// ---- inventory J: files read at startup ----

// The stop words and the mime types are read by the preload, before any listener opens, and a lookup after it reads
// the table it built: the files can go and the answers stay. Every lookup of a language without a list is null.
static void test_stop_words_and_mime_types_are_read_once_at_startup() {
	namespace fs = std::filesystem;
	const auto root = fs::current_path() / "startup_files";
	fs::remove_all(root);
	fs::create_directories(root / "stopwords");
	{
		std::ofstream(root / "stopwords" / "xx.txt") << "alpha beta\ngamma\n";
		std::ofstream(root / "mime.types") << "types {\n  text/x-startup startup;\n}\n";
	}
	::setenv("XAPIAND_DATA_PATH", root.c_str(), 1);

	preloadStopWords();
	preload_mime_types();
	fs::remove_all(root);   // nothing may read the files again

	const auto* stop = getStopWords("xx");
	REQUIRE(stop != nullptr);
	CHECK((*stop)("alpha"));
	CHECK((*stop)("gamma"));
	CHECK_FALSE((*stop)("delta"));
	CHECK(getStopWords("yy") == nullptr);   // no list for the language, and nothing to read for it
	CHECK_EQ(mime_type("file.startup").to_string(), "text/x-startup");
	CHECK(mime_type("file.unknown").empty());
}


// ---- the shutdown's bound ----

static void test_a_stop_that_returns_in_time_is_not_abandoned() {
	std::atomic<int> described{0};
	const auto report = shutdown_wait::wait(5s, [] { std::this_thread::sleep_for(20ms); }, [&] {
		++described;
		return std::vector<std::string>{"never listed"};
	});
	CHECK(report.finished);
	CHECK(report.abandoned.empty());
	CHECK_EQ(described.load(), 0);
	CHECK_LT(report.waited, 2s);
}

static void test_a_stop_that_does_not_return_is_reported_at_the_bound_and_abandoned() {
	auto release = std::make_shared<std::promise<void>>();
	std::atomic<bool> stop_returned{false};
	const auto began = std::chrono::steady_clock::now();
	const auto report = shutdown_wait::wait(200ms, [release, &stop_returned] {
		release->get_future().wait();
		stop_returned = true;
	}, [] {
		return std::vector<std::string>{"2 requests in flight on http0", "'idx' (turn held, 1 reference)"};
	});
	const auto waited = std::chrono::steady_clock::now() - began;
	CHECK_FALSE(report.finished);
	CHECK_GE(waited, 200ms);                    // it waited the bound
	CHECK_LT(waited, 3s);                       // and not for the stop
	CHECK_GE(report.waited, 200ms);
	REQUIRE_EQ(report.abandoned.size(), 2u);    // what was unfinished is named
	CHECK_EQ(report.abandoned[0], "2 requests in flight on http0");
	CHECK_FALSE(stop_returned.load());
	release->set_value();                       // the abandoned stop ends when it can
	CHECK(eventually([&] { return stop_returned.load(); }));
}


int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*fn)();
	};
	const Case cases[] = {
		{"test_tasks_run_on_the_host_and_are_counted", test_tasks_run_on_the_host_and_are_counted},
		{"test_a_failing_task_is_reported_and_counted_out", test_a_failing_task_is_reported_and_counted_out},
		{"test_periodic_ticks_on_the_loop_and_stops_at_once", test_periodic_ticks_on_the_loop_and_stops_at_once},
		{"test_a_long_wait_ends_when_stopped", test_a_long_wait_ends_when_stopped},
		{"test_a_failing_tick_does_not_end_the_periodic_task", test_a_failing_tick_does_not_end_the_periodic_task},
		{"test_a_burst_of_writes_is_one_commit_after_the_quiet_time", test_a_burst_of_writes_is_one_commit_after_the_quiet_time},
		{"test_keys_are_debounced_apart", test_keys_are_debounced_apart},
		{"test_writes_that_never_stop_are_committed_by_the_forced_time", test_writes_that_never_stop_are_committed_by_the_forced_time},
		{"test_a_stop_ends_the_waiting_without_running_the_calls", test_a_stop_ends_the_waiting_without_running_the_calls},
		{"test_pending_keys_cost_the_loop_nothing", test_pending_keys_cost_the_loop_nothing},
		{"test_teardown_unpublishes_the_periodic_wake", test_teardown_unpublishes_the_periodic_wake},
		{"test_teardown_unpublishes_the_autocommit_wakes", test_teardown_unpublishes_the_autocommit_wakes},
		{"test_a_task_spawned_on_a_closed_host_is_counted_out", test_a_task_spawned_on_a_closed_host_is_counted_out},
		{"test_a_blocking_scheduler_owns_no_roots", test_a_blocking_scheduler_owns_no_roots},
		{"test_concurrent_first_uses_load_once", test_concurrent_first_uses_load_once},
		{"test_a_later_use_loads_again", test_a_later_use_loads_again},
		{"test_other_keys_load_beside_each_other", test_other_keys_load_beside_each_other},
		{"test_a_failed_load_reaches_every_waiter", test_a_failed_load_reaches_every_waiter},
		{"test_a_cancelled_leader_leaves_the_waiters_to_load", test_a_cancelled_leader_leaves_the_waiters_to_load},
		{"test_a_destroyed_leader_does_not_strand_the_waiters", test_a_destroyed_leader_does_not_strand_the_waiters},
		{"test_stop_words_and_mime_types_are_read_once_at_startup", test_stop_words_and_mime_types_are_read_once_at_startup},
		{"test_a_stop_that_returns_in_time_is_not_abandoned", test_a_stop_that_returns_in_time_is_not_abandoned},
		{"test_a_stop_that_does_not_return_is_reported_at_the_bound_and_abandoned", test_a_stop_that_does_not_return_is_reported_at_the_bound_and_abandoned},
	};
	for (const auto& c : cases) {
		if (argc > 1 && std::string(c.name).find(argv[1]) == std::string::npos) {
			continue;
		}
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
