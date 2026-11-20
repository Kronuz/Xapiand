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

// Two hosts over loopback TLS for the shard RPC's tests: node 1 calls, node 2 serves.

#pragma once

#include "cluster/rpc.h"
#include "hosts.h"

#include <atomic>     // for std::atomic
#include <mutex>      // for std::mutex
#include <unistd.h>   // for getpid

using cluster_test::addresses;
using cluster_test::Certificates;
using cluster_test::Host;
using cluster_test::NodeId;
using cluster_test::steady;
using cluster_test::until;
using namespace std::chrono_literals;
namespace rpc = cluster::rpc;

inline char byte_at(std::uint64_t offset) { return char((offset * 2654435761u) >> 13); }
inline std::string bytes_at(std::uint64_t offset, std::size_t n) {
	std::string s(n, '\0');
	for (std::size_t i = 0; i != n; ++i)
		s[i] = byte_at(offset + i);
	return s;
}

// One call's end, as the test thread waits for it.
struct Outcome {
	std::mutex mutex;
	std::atomic<bool> done{false};
	std::optional<rpc::Failure> failure;
	rpc::Reply reply;
	std::atomic<std::uint64_t> received{0}, mismatched{0};
	steady::time_point started = steady::now(), finished;
	void end(const std::optional<rpc::Failure>& f) {
		{
			std::lock_guard hold(mutex);
			failure = f;
			finished = steady::now();
		}
		done = true;
	}
	bool wait(std::chrono::milliseconds limit = 10s) { return until([this] { return done.load(); }, limit); }
	long ms() const { return long(std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count()); }
};

// Two nodes: 1 calls, 2 serves.
struct Pair {
	Certificates certs{std::filesystem::current_path() / ("test_rpc.tls." + std::to_string(::getpid())), 3};
	Host one{1}, two{2};
	std::unique_ptr<rpc::Client> client;
	std::unique_ptr<rpc::Service> service;
	explicit Pair(rpc::Versions range = rpc::versions) {
		const auto a = addresses({&one, &two});
		one.open(certs, 3, a);
		two.open(certs, 3, a);
		client = std::make_unique<rpc::Client>(*one.transport, one.executors());
		service = std::make_unique<rpc::Service>(*two.transport, two.executors(), range);
	}
	void start() {
		one.start();
		two.start();
	}
	// A buffered call from 1 to 2.
	std::shared_ptr<Outcome> unary(rpc::Method m, std::string envelope, std::string payload = {},
								   std::chrono::milliseconds deadline = 10s, std::uint16_t version = rpc::versions.max,
								   NodeId to = 2) {
		auto o = std::make_shared<Outcome>();
		client->unary(to, m, std::move(envelope), std::move(payload), rpc::Clock::now() + deadline,
					  [o](std::optional<rpc::Failure> f, rpc::Reply r) {
						  o->reply = std::move(r);
						  o->end(f);
					  },
					  version);
		return o;
	}
};
