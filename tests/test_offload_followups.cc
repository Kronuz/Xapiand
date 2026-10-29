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


// The command drain of the offload bridge: it yields after engine::drain_batch commands so a job
// queued behind it runs (engine::drain_bounded).

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "blocking_concurrent_queue.h"       // for ConcurrentQueue
#include "engine/drain.h"
#include "reactor.h"

// Under a flood of commands a job queued behind the drain runs after at most 64 of them.
static void test_drain_yields_to_queued_job() {
	ConcurrentQueue<int> commands;
	constexpr std::size_t flood = 1000;
	for (std::size_t i = 0; i < flood; ++i) {
		commands.enqueue(static_cast<int>(i));
	}

	reactor::Reactor r(1, 1);
	std::atomic<std::size_t> handled{0};
	std::atomic<bool> stop_flooding{false};
	std::size_t handled_at_job = 0;
	std::promise<void> job_ran, drained;

	std::function<void()> drain = [&] {
		auto more = engine::drain_bounded<int>(commands, engine::drain_batch, [&](int) { ++handled; });
		if (more) {
			asio::post(*r.pool(), drain);
		} else {
			drained.set_value();
		}
	};

	std::thread flooder([&] {
		for (std::size_t i = 0; i < flood && !stop_flooding; ++i) {
			commands.enqueue(0);
			std::this_thread::yield();
		}
	});

	asio::post(r.io(), [&] {
		asio::post(*r.pool(), drain);
		asio::post(*r.pool(), [&] {
			handled_at_job = handled;
			job_ran.set_value();
		});
	});
	r.run();
	job_ran.get_future().get();
	stop_flooding = true;
	flooder.join();
	drained.get_future().get();

	CHECK_LE(engine::drain_batch, 64u);
	CHECK_LE(handled_at_job, 64u);
	CHECK_GT(handled_at_job, 0u);
}


// The drain still empties the queue, in order, and reports whether it stopped at the bound.
static void test_drain_bounded_contract() {
	ConcurrentQueue<int> q;
	for (int i = 0; i < 5; ++i) {
		q.enqueue(i);
	}
	std::vector<int> seen;
	CHECK_TRUE(engine::drain_bounded<int>(q, 3, [&](int v) { seen.push_back(v); }));
	CHECK_EQ(seen.size(), 3u);
	CHECK_FALSE(engine::drain_bounded<int>(q, 3, [&](int v) { seen.push_back(v); }));
	CHECK_EQ(seen.size(), 5u);
	CHECK_EQ(seen[0], 0);
	CHECK_EQ(seen[4], 4);
	CHECK_FALSE(engine::drain_bounded<int>(q, 3, [&](int) { }));
}

TEST_MAIN(test_drain_yields_to_queued_job, test_drain_bounded_contract)
