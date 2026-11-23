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

#pragma once

// One load per key at a time (docs/design/phase7c.md D11): the first caller of run(key, load) runs the
// load, the others await its result, each under its own cancel token. The map's lock guards a lookup, an
// erase or a flight's list of joiners, and is never held across the load or a wait.
//
// A caller that missed its cache and then found no flight may have missed a flight that ended in between.
// So a leader first asks the caller's probe whether the cache holds the value now, and a load publishes
// its value into that cache before it returns: the flight a probe follows has ended only after the publication.

#include <atomic>                  // for std::atomic
#include <cstdint>                 // for std::uint64_t
#include <exception>               // for std::exception_ptr
#include <memory>                  // for std::shared_ptr
#include <mutex>                   // for std::mutex
#include <optional>                // for std::optional
#include <unordered_map>           // for std::unordered_map
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "task.hh"                 // for task::task, task::event, task::cancel_callback


template <typename Key, typename Value>
class SingleFlight {
	struct Abandoned : std::exception {
		const char* what() const noexcept override { return "the load this call joined was abandoned"; }
	};

	// A joiner awaits its own wake, which the flight's end or the joiner's token sets, whichever comes first.
	struct Flight {
		std::vector<std::shared_ptr<task::event>> wakes;   // the joiners waiting, under the map's lock
		bool ended = false;                                // set under the map's lock, after value or error
		std::optional<Value> value;
		std::exception_ptr error;
	};

	// Ends the leader's flight on every exit, the destruction of a suspended frame included, so a joiner
	// never waits for a load nobody runs.
	struct Lead {
		SingleFlight& owner;
		const Key& key;
		std::shared_ptr<Flight> flight;

		void end() noexcept {
			if (flight->ended) { return; }
			std::vector<std::shared_ptr<task::event>> wakes;
			{
				std::lock_guard<std::mutex> lk(owner.mtx_);
				owner.flights_.erase(key);
				flight->ended = true;
				wakes.swap(flight->wakes);
			}
			for (auto& wake : wakes) {
				wake->set();
			}
		}
		~Lead() {
			if (!flight->ended && !flight->value && !flight->error) {
				flight->error = std::make_exception_ptr(Abandoned{});
			}
			end();
		}
	};

public:
	// The value of load() for `key`: this call's own load when none is in flight, else the one in flight.
	// An error of the load is rethrown to every call that awaited it. A joiner whose leader was cancelled or
	// abandoned runs the load itself.
	template <typename Load>
	task::task<Value> run(Key key, Load load)
	{
		return run(std::move(key), std::move(load), [] { return std::optional<Value>(); });
	}

	// The same, where a leader takes probe()'s value, when it has one, instead of loading (not a load), and a
	// joiner whose `token` is requested stops waiting and throws task::cancelled while the load and the other
	// waiters go on. A leader's load ends by its own token, which load() carries.
	template <typename Load, typename Probe>
	task::task<Value> run(Key key, Load load, Probe probe, task::cancel_token token = {})
	{
		for (;;) {
			std::shared_ptr<Flight> flight;
			std::shared_ptr<task::event> wake;
			{
				std::lock_guard<std::mutex> lk(mtx_);
				auto [it, inserted] = flights_.try_emplace(key);
				if (inserted) {
					it->second = std::make_shared<Flight>();
				} else {
					wake = std::make_shared<task::event>();
					it->second->wakes.push_back(wake);
				}
				flight = it->second;
			}

			if (!wake) {
				Lead lead{*this, key, flight};
				try {
					flight->value = probe();
					if (!flight->value) {
						loads_.fetch_add(1, std::memory_order_relaxed);
						flight->value = co_await load();
					}
				} catch (...) {
					flight->error = std::current_exception();
				}
				lead.end();
				if (flight->error) {
					std::rethrow_exception(flight->error);
				}
				co_return *flight->value;
			}

			joined_.fetch_add(1, std::memory_order_relaxed);
			waiting_.fetch_add(1, std::memory_order_relaxed);
			{
				task::cancel_callback leave(token, [w = wake.get()] { w->set(); });
				co_await *wake;
			}
			waiting_.fetch_sub(1, std::memory_order_relaxed);
			bool left = false;
			{
				std::lock_guard<std::mutex> lk(mtx_);
				if (!flight->ended) {   // the token woke this call first: it leaves the flight
					std::erase(flight->wakes, wake);
					left = true;
				}
			}
			if (left) {
				throw task::cancelled{};
			}
			if (!flight->error) {
				co_return *flight->value;
			}
			try {
				std::rethrow_exception(flight->error);
			} catch (const Abandoned&) {
			} catch (const task::cancelled&) {
				// the leader's own cancellation says nothing about this call: load again
			}
			if (token.requested()) {
				throw task::cancelled{};
			}
		}
	}

	// Loads started, calls that joined a load, and the joined calls still waiting for one.
	std::uint64_t loads() const noexcept { return loads_.load(std::memory_order_relaxed); }
	std::uint64_t joined() const noexcept { return joined_.load(std::memory_order_relaxed); }
	std::uint64_t waiting() const noexcept { return waiting_.load(std::memory_order_relaxed); }

private:
	std::mutex mtx_;
	std::unordered_map<Key, std::shared_ptr<Flight>> flights_;
	std::atomic<std::uint64_t> loads_{0};
	std::atomic<std::uint64_t> joined_{0};
	std::atomic<std::uint64_t> waiting_{0};
};
