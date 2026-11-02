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

// The control plane's pieces (docs/design/phase7c.md D13): tasks on the control loop, a periodic task, and the
// debounced autocommit. Each runs on an engine::Host (the control loop's), waits on its timers and never blocks;
// stop() ends a task that waits at once, from any thread. Their locks guard O(1) state and are never held across a
// wait.

#include <algorithm>               // for std::min, std::max
#include <atomic>                  // for std::atomic
#include <chrono>                  // for std::chrono
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <exception>               // for std::exception
#include <functional>              // for std::function
#include <memory>                  // for std::unique_ptr
#include <mutex>                   // for std::mutex
#include <random>                  // for std::mt19937_64
#include <stdexcept>                // for std::runtime_error
#include <unordered_map>           // for std::unordered_map
#include <utility>                 // for std::move

#include "engine/host.h"           // for engine::Host
#include "task.hh"                 // for task::task, task::event


namespace control {

// Runs tasks on a host and counts those that have not ended, so that a shutdown can run the loop until they do.
class Tasks {
public:
	explicit Tasks(engine::Host& host) noexcept : host_(host) { }
	Tasks(const Tasks&) = delete;
	Tasks& operator=(const Tasks&) = delete;

	engine::Host& host() noexcept { return host_; }

	// Thread-safe. The task runs on the host; its failure is passed to `failed` (when set) and ends only it.
	void spawn(task::task<void> t) {
		running_.fetch_add(1);
		host_.spawn(counted(std::move(t)));
	}

	std::size_t running() const noexcept { return running_.load(); }

	// Called with the exception of a task that failed, on the host.
	std::function<void(const std::exception&)> failed;

private:
	task::task<void> counted(task::task<void> t) {
		try {
			co_await std::move(t);
		} catch (const std::exception& error) {
			if (failed) { failed(error); }
		} catch (...) {
			if (failed) { failed(std::runtime_error("unknown error")); }
		}
		running_.fetch_sub(1);
	}

	engine::Host& host_;
	std::atomic<std::size_t> running_{0};
};


// Runs `tick` on the host every `period` until stop(), which ends the wait at once. A tick that throws is
// passed to Tasks::failed and the next one still runs.
class Periodic {
public:
	Periodic(Tasks& tasks, std::chrono::steady_clock::duration period, std::function<void()> tick)
		: tasks_(tasks), period_(period), tick_(std::move(tick)) { }

	void start() {
		tasks_.spawn(loop());
	}

	void stop() {
		std::lock_guard<std::mutex> lk(mtx_);
		stopped_ = true;
		if (wake_) {
			wake_->set();
		}
	}

	std::uint64_t ticks() const noexcept { return ticks_.load(); }

private:
	task::task<void> loop() {
		for (;;) {
			task::event wake;
			{
				std::lock_guard<std::mutex> lk(mtx_);
				if (stopped_) {
					co_return;
				}
				wake_ = &wake;
			}
			{
				auto timer = tasks_.host().start_timer(std::chrono::steady_clock::now() + period_, [&wake] { wake.set(); });
				co_await wake;
			}
			{
				std::lock_guard<std::mutex> lk(mtx_);
				wake_ = nullptr;
				if (stopped_) {
					co_return;
				}
			}
			try {
				tick_();
			} catch (const std::exception& error) {
				if (tasks_.failed) { tasks_.failed(error); }
			}
			ticks_.fetch_add(1);
		}
	}

	Tasks& tasks_;
	std::chrono::steady_clock::duration period_;
	std::function<void()> tick_;
	std::mutex mtx_;
	bool stopped_ = false;
	task::event* wake_ = nullptr;
	std::atomic<std::uint64_t> ticks_{0};
};


// The debounced commit of a write that asked for none: touch(key) after each write, and `fire(key)` runs once
// the key has been quiet for `debounce`, later while writes keep coming (`busy` after each), and never later than
// a random time in [min_force, max_force] after the first write. One task per pending key waits on the host's
// timer; a touch moves its target, never earlier.
template <typename Key, typename Hash = std::hash<Key>>
class Debounce {
public:
	struct Timing {
		std::chrono::milliseconds debounce;
		std::chrono::milliseconds busy;
		std::chrono::milliseconds min_force;
		std::chrono::milliseconds max_force;
	};

	Debounce(engine::Host& host, Timing timing, std::function<void(const Key&)> fire)
		: tasks_(host), timing_(timing), fire_(std::move(fire)), rng_(std::random_device{}()) { }

	Tasks& tasks() noexcept { return tasks_; }

	// True when a call for the key is pending, false after stop(): the caller then runs it now.
	bool touch(const Key& key) {
		const auto now = std::chrono::steady_clock::now();
		bool fresh = false;
		{
			std::lock_guard<std::mutex> lk(mtx_);
			if (stopped_) {
				return false;
			}
			auto [it, inserted] = states_.try_emplace(key);
			auto& state = it->second;
			if (inserted) {
				state.max_wakeup = now + force_after();
				state.target = std::min(now + timing_.debounce, state.max_wakeup);
				fresh = true;
			} else {
				state.target = std::max(state.target, std::min(now + timing_.busy, state.max_wakeup));
			}
		}
		if (fresh) {
			tasks_.spawn(watch(key));
		}
		return true;
	}

	// Ends every waiting task without running its call (a close commits what is pending); later touches
	// return false.
	void stop() {
		std::lock_guard<std::mutex> lk(mtx_);
		stopped_ = true;
		for (auto& [key, state] : states_) {
			if (state.wake) {
				state.wake->set();
			}
		}
	}

	std::size_t pending() const {
		std::lock_guard<std::mutex> lk(mtx_);
		return states_.size();
	}

	std::uint64_t fired() const noexcept { return fired_.load(); }

private:
	struct State {
		std::chrono::steady_clock::time_point max_wakeup;
		std::chrono::steady_clock::time_point target;
		task::event* wake = nullptr;
	};

	std::chrono::milliseconds force_after() {
		const auto low = timing_.min_force.count();
		const auto high = std::max(low, timing_.max_force.count());
		return std::chrono::milliseconds(std::uniform_int_distribution<long long>(low, high)(rng_));
	}

	task::task<void> watch(Key key) {
		for (;;) {
			task::event wake;
			std::chrono::steady_clock::time_point target;
			bool due = false;
			{
				std::lock_guard<std::mutex> lk(mtx_);
				auto it = states_.find(key);
				if (it == states_.end() || stopped_) {
					states_.erase(key);
					co_return;
				}
				target = it->second.target;
				if (target <= std::chrono::steady_clock::now()) {
					states_.erase(it);   // a touch from here on starts a new call
					due = true;
				} else {
					it->second.wake = &wake;
				}
			}
			if (due) {
				fired_.fetch_add(1);
				fire_(key);
				co_return;
			}
			{
				auto timer = tasks_.host().start_timer(target, [&wake] { wake.set(); });
				co_await wake;
			}
			std::lock_guard<std::mutex> lk(mtx_);
			if (auto it = states_.find(key); it != states_.end()) {
				it->second.wake = nullptr;
			}
		}
	}

	Tasks tasks_;
	Timing timing_;
	std::function<void(const Key&)> fire_;
	mutable std::mutex mtx_;
	std::unordered_map<Key, State, Hash> states_;
	std::mt19937_64 rng_;
	bool stopped_ = false;
	std::atomic<std::uint64_t> fired_{0};
};

}  // namespace control
