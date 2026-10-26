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


// The two control-worker follow-ups to the offload bridge: a bus message refreshes the sender's
// liveness on the loop even while the worker is busy (Node::touch_sender), and a command drain
// yields after engine::drain_batch commands so a job queued behind it runs (engine::drain_bounded).

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
#include "node.h"
#include "reactor.h"

namespace {

using namespace std::chrono_literals;

Node make_node(const std::string& name) {
	Node node;
	node.name(name);
	node.http_port = 8880;
	node.remote_port = 8890;
	node.replication_port = 8891;
	return node;
}

// What a DB_UPDATED message carries: the sender, then a revision and a path.
std::string db_updated_message(const Node& sender) {
	return sender.serialise() + std::string("\x01", 1) + "index";
}

}  // namespace


// The bus loop refreshes last_seen at message time while the control worker is stuck on a long job.
static void test_last_seen_refreshes_while_worker_busy() {
	Node::reset();
	auto peer = make_node("followup-peer");
	Node::touch_node(peer, true);
	auto before = Node::get_node("followup-peer")->last_seen();
	std::this_thread::sleep_for(20ms);

	reactor::Reactor r(1, 1);
	std::promise<void> worker_started, release;
	auto release_future = release.get_future().share();
	asio::post(*r.pool(), [&] {
		worker_started.set_value();
		release_future.wait();
	});
	worker_started.get_future().get();

	std::chrono::steady_clock::time_point during;
	bool touched = false;
	asio::post(r.io(), [&] {
		touched = static_cast<bool>(Node::touch_sender(db_updated_message(peer)));
		during = Node::get_node("followup-peer")->last_seen();
		release.set_value();
	});
	r.run();

	CHECK(touched);
	CHECK_GT(during, before);
	CHECK_GE(during - before, 20ms);
}


// A message from the local node touches nothing.
static void test_touch_sender_ignores_local() {
	Node::reset();
	auto self = make_node("followup-self");
	Node::set_local_node(self);
	CHECK(Node::touch_sender(db_updated_message(self)) == nullptr);
	Node::reset();
}


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

TEST_MAIN(test_last_seen_refreshes_while_worker_busy, test_touch_sender_ignores_local, test_drain_yields_to_queued_job, test_drain_bounded_contract)
