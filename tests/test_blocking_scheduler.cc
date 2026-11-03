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

// engine::BlockingScheduler: a thread's scheduler is destroyed with the thread, right after sync_wait
// returns, while the thread that set the event may still be inside its post() signalling that
// scheduler. The signal must be done by then (TSan reports a condition variable destroyed under a signal).

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "engine/sync.h"
#include "task.hh"

namespace {

task::task<void> wait_for(task::event& ev, std::atomic<bool>& waiting) {
	waiting = true;
	co_await ev;
}

void test_scheduler_outlives_the_signal() {
	for (int i = 0; i < 500; ++i) {
		task::event ev;
		std::atomic<bool> waiting{false};
		std::thread waiter([&] { engine::sync_wait(wait_for(ev, waiting)); });
		while (!waiting) {
			std::this_thread::yield();
		}
		std::this_thread::sleep_for(std::chrono::microseconds(20));
		ev.set();
		waiter.join();
	}
	CHECK_EQ(engine::blocking_violations(), 0u);
}

task::task<int> answer() {
	co_return 42;
}

// Awaits a child after the nested wait: the outer loop's queue must still be there to resume it.
task::task<int> waits_inside() {
	const int nested = engine::sync_wait(answer());
	co_return nested + 1 + (co_await answer() - 42);
}

task::task<int> waits_two_deep() {
	co_return engine::sync_wait(waits_inside()) + 1;
}

// Aborts the process when the test has not ended in time, so a deadlock fails instead of hanging.
struct Watchdog {
	std::atomic<bool> stop{false};
	std::thread thread{[this] {
		for (int i = 0; i < 300 && !stop; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (!stop) {
			std::fprintf(stderr, "a nested blocking wait did not return\n");
			std::abort();
		}
	}};
	~Watchdog() {
		stop = true;
		thread.join();
	}
};

void test_a_blocking_wait_nests() {
	Watchdog watchdog;
	CHECK_EQ(engine::sync_wait(waits_inside()), 43);
	CHECK_EQ(engine::sync_wait(waits_two_deep()), 44);
	CHECK_EQ(engine::blocking_violations(), 0u);
}

}  // namespace

TEST_MAIN(test_scheduler_outlives_the_signal, test_a_blocking_wait_nests)
