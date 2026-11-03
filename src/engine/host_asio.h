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

// A Host over an Asio loop: a reactor's tasks (a request, a timer's commit, an idle close) run here. The
// loop's tasks go through one task::asio_scheduler, which runs fresh work ahead of demoted work and counts
// the demoted handles for admission (docs/design/phase7c.md D2).

#include <coroutine>               // for std::coroutine_handle
#include <exception>               // for std::exception_ptr
#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <mutex>                   // for std::mutex, std::lock_guard
#include <optional>                // for std::optional
#include <utility>                 // for std::move

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/dispatch.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>

#include "engine/host.h"           // for Host
#include "task_asio.hh"            // for task::asio_scheduler


namespace engine {

class AsioHost final : public Host {
public:
	explicit AsioHost(asio::any_io_executor executor) : scheduler_(executor), executor_(std::move(executor)) { }

	void post(std::coroutine_handle<> h) override { scheduler_.post(h); }
	void post_demoted(std::coroutine_handle<> h) override { scheduler_.post_demoted(h); }
	void post_call(void (*fn)(void*), void* arg) override { scheduler_.post_call(fn, arg); }
	bool runs_here() const noexcept override { return scheduler_.runs_here(); }
	task::root_list* roots() noexcept override { return scheduler_.roots(); }

	// Teardown, after the loop has stopped and every pool a task here may wait on has drained (task's
	// cancellation D8): close() drops what is posted from then on and destroys what is spawned; destroy_roots()
	// destroys the tasks spawned here that have not ended, with the loop's io_context alive.
	void close() noexcept { scheduler_.close(); }
	void destroy_roots() noexcept { scheduler_.destroy_roots(); }
	bool closed() const noexcept { return scheduler_.closed(); }

	// The scheduler every task of this loop resumes through; its demoted() is the loop's admission count.
	task::asio_scheduler& scheduler() noexcept { return scheduler_; }

	// Makes the loop's own thread the host's thread. Posted to the loop once, before it serves anything.
	void bind_loop_thread() noexcept { Host::bind_thread(this); }

	// The counters of the request whose task is running on this loop (debug builds, through a Run).
	TaskCounters* counters() noexcept override { return running_; }


protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) override {
		auto state = std::make_shared<State>(executor_, at, std::move(node));
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

	friend class Run;
	static inline thread_local TaskCounters* running_ = nullptr;

	task::asio_scheduler scheduler_;
	asio::any_io_executor executor_;
};


// One request's task on a loop. Release builds run it straight through the loop's scheduler. A debug build
// gives each request its own scheduler, so that every resume of the request's task (after a turn, a chore, a
// slice) first makes the request's counters the running ones, and the rules on what a task holds are checked
// (check_no_view_held, Turn).
class Run {
public:
	explicit Run(AsioHost& host) : host_(host) { }
	Run(const Run&) = delete;
	Run& operator=(const Run&) = delete;

	void post(std::coroutine_handle<> h) {
#ifndef NDEBUG
		if (host_.closed()) { return; }   // the scheduler would drop the resume and leak its wrapper
		host_.scheduler().post(resume_in(this, h));
#else
		host_.scheduler().post(h);
#endif
	}
	void post_demoted(std::coroutine_handle<> h) {
#ifndef NDEBUG
		if (host_.closed()) { return; }
		host_.scheduler().post_demoted(resume_in(this, h));
#else
		host_.scheduler().post_demoted(h);
#endif
	}
	// Forwarded, so that a task spawned through a Run is a root of the loop's scheduler (task's cancellation D7).
	void post_call(void (*fn)(void*), void* arg) { host_.scheduler().post_call(fn, arg); }
	bool runs_here() const noexcept { return host_.scheduler().runs_here(); }
	task::root_list* roots() noexcept { return host_.scheduler().roots(); }
	std::uint64_t now() const noexcept { return host_.scheduler().now(); }

	// Runs t on the loop and completes on the awaiting coroutine's executor; an exception of t propagates.
	template <typename T>
	asio::awaitable<T> await(task::task<T> t) {
		auto initiate = [this](auto handler, task::task<T> work) {
			task::spawn(task::scheduler_ref::of(*this), std::move(work), Complete<decltype(handler)>{std::move(handler)});
		};
		if constexpr (std::is_void_v<T>) {
			co_await asio::async_initiate<const asio::use_awaitable_t<>&, void(std::exception_ptr)>(initiate, asio::use_awaitable, std::move(t));
		} else {
			auto v = co_await asio::async_initiate<const asio::use_awaitable_t<>&, void(std::exception_ptr, std::optional<T>)>(
				initiate, asio::use_awaitable, std::move(t));
			co_return std::move(*v);
		}
	}

private:
	// Completes the awaiting coroutine with the task's result, on the coroutine's executor. When teardown destroys
	// the task unrun, the handler is not destroyed with it: that would destroy the awaiting frame, which holds this
	// Run, before the task's own chain, whose frames still point at it. It is queued on the stopped loop instead,
	// and goes with the loop's io_context.
	template <typename H>
	struct Complete {
		std::optional<H> h;
		explicit Complete(H handler) : h(std::move(handler)) { }
		Complete(Complete&& other) noexcept : h(std::move(other.h)) { other.h.reset(); }
		Complete(const Complete&) = delete;
		~Complete() {
			if (h) {
				auto ex = asio::get_associated_executor(*h);
				asio::post(ex, [handler = std::move(*h)]() mutable { (void)handler; });
			}
		}
		template <typename... V>
		void operator()(std::exception_ptr e, V&&... value) {
			auto handler = std::move(*h);
			h.reset();
			auto ex = asio::get_associated_executor(handler);
			asio::dispatch(ex, [handler = std::move(handler), e, ... value = std::forward<V>(value)]() mutable { std::move(handler)(e, std::move(value)...); });
		}
	};

public:
#ifndef NDEBUG
	TaskCounters counters;

	// A wrapper the loop never ran (its loop stopped first) is destroyed with the Run: the loop's queue drops it unrun.
	~Run() {
		for (;;) {
			std::coroutine_handle<> pending;
			{
				std::lock_guard<std::mutex> lk(pending_mtx_);
				if (pending_ == nullptr) { break; }
				pending = std::coroutine_handle<Resume::promise_type>::from_promise(*pending_);
			}
			pending.destroy();   // unlinks itself
		}
	}

private:
	struct Resume {
		struct promise_type {
			Run* run;
			promise_type* prev = nullptr;
			promise_type* next = nullptr;
			promise_type(Run* r, std::coroutine_handle<>) noexcept : run(r) {
				std::lock_guard<std::mutex> lk(run->pending_mtx_);
				next = run->pending_;
				if (next != nullptr) { next->prev = this; }
				run->pending_ = this;
			}
			~promise_type() {
				std::lock_guard<std::mutex> lk(run->pending_mtx_);
				(prev != nullptr ? prev->next : run->pending_) = next;
				if (next != nullptr) { next->prev = prev; }
			}
			Resume get_return_object() noexcept { return Resume{std::coroutine_handle<promise_type>::from_promise(*this)}; }
			std::suspend_always initial_suspend() noexcept { return {}; }
			std::suspend_never final_suspend() noexcept { return {}; }
			void return_void() noexcept { }
			void unhandled_exception() noexcept { std::terminate(); }
		};
		std::coroutine_handle<promise_type> handle;
	};

	std::mutex pending_mtx_;
	Resume::promise_type* pending_ = nullptr;   // the wrappers posted and not yet run

	static Resume run_resume(Run* run, std::coroutine_handle<> h) {
		TaskCounters* const previous = AsioHost::running_;
		AsioHost::running_ = &run->counters;
		{
			task::running_on scope(task::scheduler_ref::of(*run));
			h.resume();
		}
		AsioHost::running_ = previous;
		co_return;
	}
	static std::coroutine_handle<> resume_in(Run* run, std::coroutine_handle<> h) { return run_resume(run, h).handle; }
#else

private:
#endif
	AsioHost& host_;
};

}  // namespace engine
