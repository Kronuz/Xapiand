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

// A call that joins another call's load in SingleFlight waits under its own cancel token: when the token is
// requested (its client left, its deadline passed) it stops waiting and throws task::cancelled, while the load
// and the other waiters go on to the load's value.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <thread>

#include "asio.hpp"
#include "control_plane.h"
#include "engine/host_asio.h"
#include "reactor.h"
#include "single_flight.h"
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
		guard_.reset();
		reactor.io().stop();
		thread_.join();
	}

	reactor::Reactor reactor;
	engine::AsioHost host{reactor.scheduler()};

private:
	asio::executor_work_guard<asio::io_context::executor_type> guard_;
	std::thread thread_;
};

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit = 3s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	return done();
}

using Flights = SingleFlight<std::string, int>;

// One call of run("k"): its load waits for `release` and counts itself in `loads`; the call counts its value in
// `got` when it is 42, or its cancellation in `cancelled`.
task::task<void> call(Flights& flights, task::event& release, std::atomic<int>& loads, std::atomic<int>& got, std::atomic<int>& cancelled, task::cancel_token token) {
	try {
		const int v = co_await flights.run(
			"k",
			[&]() -> task::task<int> {
				++loads;
				co_await release;
				co_return 42;
			},
			[] { return std::optional<int>(); },
			std::move(token));
		if (v == 42) { ++got; }
	} catch (const task::cancelled&) {
		++cancelled;
	}
}

}  // namespace


// B joins A's slow load beside C; B's token fires first: B leaves within the bound, A's load goes on, and A and C
// get its value from the one load.
static void test_a_joiner_whose_token_fires_leaves_while_the_load_goes_on() {
	Loop loop;
	Flights flights;
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> loads{0}, got{0}, cancelled{0};
	task::cancel_source b_client;

	tasks.spawn(call(flights, release, loads, got, cancelled, {}));
	REQUIRE(eventually([&] { return loads.load() == 1; }));   // A leads
	tasks.spawn(call(flights, release, loads, got, cancelled, b_client.token()));
	tasks.spawn(call(flights, release, loads, got, cancelled, {}));
	REQUIRE(eventually([&] { return flights.waiting() == 2; }));   // B and C joined

	b_client.request();   // B's client leaves, from another thread as http's EOF does
	CHECK(eventually([&] { return cancelled.load() == 1; }, 1s));
	CHECK_EQ(flights.waiting(), 1u);   // C still waits
	CHECK_EQ(got.load(), 0);           // and the load still runs
	CHECK_EQ(tasks.running(), 2u);

	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(got.load(), 2);
	CHECK_EQ(cancelled.load(), 1);
	CHECK_EQ(loads.load(), 1);
	CHECK_EQ(flights.loads(), 1u);
	CHECK_EQ(flights.joined(), 2u);
	CHECK_EQ(flights.waiting(), 0u);
}

// A joiner whose token was requested before it joined leaves at once; the load is not disturbed.
static void test_a_joiner_already_cancelled_leaves_at_once() {
	Loop loop;
	Flights flights;
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> loads{0}, got{0}, cancelled{0};
	task::cancel_source gone;
	gone.request(task::cancel_reason::deadline);

	tasks.spawn(call(flights, release, loads, got, cancelled, {}));
	REQUIRE(eventually([&] { return loads.load() == 1; }));
	tasks.spawn(call(flights, release, loads, got, cancelled, gone.token()));
	CHECK(eventually([&] { return cancelled.load() == 1; }, 1s));
	CHECK_EQ(flights.waiting(), 0u);

	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(got.load(), 1);
	CHECK_EQ(flights.loads(), 1u);
}

// Many joiners leave one slow load; the load's value still reaches the one that stays, and the key is free after.
static void test_joiners_that_leave_do_not_disturb_the_one_that_stays() {
	Loop loop;
	Flights flights;
	task::event release;
	control::Tasks tasks(loop.host);
	std::atomic<int> loads{0}, got{0}, cancelled{0};
	task::cancel_source clients;

	tasks.spawn(call(flights, release, loads, got, cancelled, {}));
	REQUIRE(eventually([&] { return loads.load() == 1; }));
	for (int i = 0; i < 16; ++i) {
		tasks.spawn(call(flights, release, loads, got, cancelled, clients.token()));
	}
	tasks.spawn(call(flights, release, loads, got, cancelled, {}));
	REQUIRE(eventually([&] { return flights.waiting() == 17; }));

	clients.request();
	CHECK(eventually([&] { return cancelled.load() == 16; }, 1s));
	CHECK_EQ(flights.waiting(), 1u);

	release.set();
	CHECK(eventually([&] { return tasks.running() == 0; }));
	CHECK_EQ(got.load(), 2);
	CHECK_EQ(flights.loads(), 1u);
	CHECK_EQ(task::sync_wait(flights.run("k", []() -> task::task<int> { co_return 5; })), 5);   // the key is free
}


int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*fn)();
	};
	const Case cases[] = {
		{"test_a_joiner_whose_token_fires_leaves_while_the_load_goes_on", test_a_joiner_whose_token_fires_leaves_while_the_load_goes_on},
		{"test_a_joiner_already_cancelled_leaves_at_once", test_a_joiner_already_cancelled_leaves_at_once},
		{"test_joiners_that_leave_do_not_disturb_the_one_that_stays", test_joiners_that_leave_do_not_disturb_the_one_that_stays},
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
