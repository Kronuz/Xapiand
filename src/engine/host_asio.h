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

// A Host over a reactor's loop: a reactor's tasks (a request, a timer's commit, an idle close) run here. The
// host wraps the loop's one task::asio_scheduler, which the reactor owns: it runs light work ahead of the heavy lane
// and counts the heavy lane's handles for admission (docs/design/loop-scheduling.md D2). The reactor's owner tears the
// scheduler down in task's phases (HttpAsioService, or the manager for the control loop).

#include <coroutine>               // for std::coroutine_handle
#include <exception>               // for std::exception_ptr
#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <mutex>                   // for std::mutex
#include <optional>                // for std::optional
#include <type_traits>             // for std::is_void_v
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include <asio/steady_timer.hpp>

#include "engine/host.h"           // for Host
#include "task_asio.hh"            // for task::asio_scheduler


namespace engine {

class AsioHost final : public Host {
public:
	explicit AsioHost(task::asio_scheduler& scheduler) noexcept : scheduler_(scheduler) { }

	void post(std::coroutine_handle<> h) override { scheduler_.post(h); }
	void post_demoted(std::coroutine_handle<> h) override { scheduler_.post_demoted(h); }
	void post_heavy(std::coroutine_handle<> h) override { scheduler_.post_heavy(h); }
	void post_call(void (*fn)(void*), void* arg) override { scheduler_.post_call(fn, arg); }
	bool runs_here() const noexcept override { return scheduler_.runs_here(); }
	task::root_list* roots() noexcept override { return scheduler_.roots(); }

	// The scheduler every task of this loop resumes through; its demoted() is the loop's admission count.
	task::asio_scheduler& scheduler() noexcept { return scheduler_; }

	// Makes the loop's own thread the host's thread. Posted to the loop once, before it serves anything.
	void bind_loop_thread() noexcept { Host::bind_thread(this); }

	// The counters of the request whose task is running on this loop (debug builds, through counted()).
	TaskCounters* counters() noexcept override { return running_; }


protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) override {
		auto state = std::make_shared<State>(scheduler_.executor(), at, std::move(node));
		state->timer.async_wait([state](const asio::error_code& ec) {
			if (!ec && !state->cancelled) {
				state->node->fire();
			}
		});
		return std::make_unique<Armed>(std::move(state));
	}

private:
	struct State {
		State(const asio::any_io_executor& ex, std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> n) : timer(ex, at), node(std::move(n)) { }
		asio::steady_timer timer;
		std::unique_ptr<TimerNode> node;
		bool cancelled = false;  // the loop's thread only: a timer is armed and cancelled there
	};

	class Armed final : public Timer {
	public:
		explicit Armed(std::shared_ptr<State> state) : state_(std::move(state)) { }
		~Armed() override {
			state_->cancelled = true;
			state_->timer.cancel();
		}

	private:
		std::shared_ptr<State> state_;
	};

	friend class Counted;
	static inline thread_local TaskCounters* running_ = nullptr;

	task::asio_scheduler& scheduler_;
};


#ifndef NDEBUG
// A debug build's scheduler for one request's work: it forwards to the loop's, and makes the request's counters the
// running ones around every resume it runs, so the rules on what a task holds are checked (check_no_view_held, Turn).
// Its resumes queue here and one pump coroutine, owned by this scheduler, runs them from the loop's queues (the
// demoted one when none is light, so the loop's admission count still sees them), each in the lane it was posted in.
// Nothing is allocated per resume, and a pump the closed loop drops is destroyed with this scheduler.
class Counted {
public:
	explicit Counted(AsioHost& host) : host_(host), pump_(run(this).handle) { }
	Counted(const Counted&) = delete;
	Counted& operator=(const Counted&) = delete;
	~Counted() { pump_.destroy(); }

	void post(std::coroutine_handle<> h) { queue(h, Lane::light); }
	void post_demoted(std::coroutine_handle<> h) { queue(h, Lane::demoted); }
	void post_heavy(std::coroutine_handle<> h) { queue(h, Lane::heavy); }
	void post_call(void (*fn)(void*), void* arg) { host_.scheduler().post_call(fn, arg); }
	// False: an Asio completion is posted here instead of resumed inline, so every resume sets the counters.
	bool runs_here() const noexcept { return false; }
	task::root_list* roots() noexcept { return host_.scheduler().roots(); }
	std::uint64_t now() const noexcept { return host_.scheduler().now(); }

	TaskCounters counters;

private:
	struct Pump {
		struct promise_type {
			Pump get_return_object() noexcept { return Pump{std::coroutine_handle<promise_type>::from_promise(*this)}; }
			std::suspend_always initial_suspend() noexcept { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }
			void return_void() noexcept { }
			void unhandled_exception() noexcept { std::terminate(); }
		};
		std::coroutine_handle<promise_type> handle;
	};

	// Where a resume was posted: the loop's FIFO, demoted (resumed light), or the heavy lane (resumed heavy).
	enum class Lane { light, demoted, heavy };

	struct Ready {
		std::coroutine_handle<> h;
		Lane lane;
	};

	void queue(std::coroutine_handle<> h, Lane lane) {
		bool idle;
		{
			std::lock_guard<std::mutex> lk(mtx_);
			ready_.push_back({h, lane});
			idle = !pumping_;
			pumping_ = true;
		}
		if (idle) {
			post_pump(lane == Lane::light);
		}
	}

	void post_pump(bool light) {
		light ? host_.scheduler().post(pump_) : host_.scheduler().post_demoted(pump_);
	}

	// Each pass runs the resumes queued before it began; what they queue goes back to the loop with the pump.
	static Pump run(Counted* self) {
		for (;;) {
			std::vector<Ready> batch;
			{
				std::lock_guard<std::mutex> lk(self->mtx_);
				batch.swap(self->ready_);
			}
			for (const auto& ready : batch) {
				TaskCounters* const previous = AsioHost::running_;
				AsioHost::running_ = &self->counters;
				{
					task::running_on scope(task::scheduler_ref::of(*self).with_heavy(ready.lane == Lane::heavy));
					ready.h.resume();
				}
				AsioHost::running_ = previous;
			}
			bool again;
			bool light = false;
			{
				std::lock_guard<std::mutex> lk(self->mtx_);
				again = !self->ready_.empty();
				for (const auto& ready : self->ready_) {
					light = light || ready.lane == Lane::light;
				}
				self->pumping_ = again;
			}
			if (again) {
				// Posted before this pass suspends: the loop runs it only after this handler returns.
				self->post_pump(light);
			}
			co_await std::suspend_always{};
		}
	}

	AsioHost& host_;
	std::coroutine_handle<> pump_;
	std::mutex mtx_;
	std::vector<Ready> ready_;
	bool pumping_ = false;
};
#endif


// Runs a request's work on the loop. Release builds return it to be awaited in place. A debug build moves it to a
// Counted scheduler of its own and back to the loop before it returns, so the request's counters follow its resumes;
// both moves keep the current lane, as awaiting in place does.
template <typename T>
task::task<T> counted([[maybe_unused]] AsioHost& host, task::task<T> work) {
#ifdef NDEBUG
	return work;
#else
	return [](AsioHost& host, task::task<T> work) -> task::task<T> {
		Counted run(host);
		co_await task::schedule_on{task::scheduler_ref::of(run).with_heavy(task::current().heavy())};
		std::exception_ptr error;
		std::optional<std::conditional_t<std::is_void_v<T>, bool, T>> value;
		try {
			if constexpr (std::is_void_v<T>) {
				co_await std::move(work);
			} else {
				value.emplace(co_await std::move(work));
			}
		} catch (...) {
			error = std::current_exception();
		}
		co_await task::schedule_on{task::scheduler_ref::of(host).with_heavy(task::current().heavy())};  // back on the loop before `run` goes
		if (error) {
			std::rethrow_exception(error);
		}
		if constexpr (!std::is_void_v<T>) {
			co_return std::move(*value);
		}
	}(host, std::move(work));
#endif
}

}  // namespace engine
