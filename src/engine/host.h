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

// The schedulers the database layer's tasks run on. A Host is a task scheduler that can also arm a
// timer, which a turn's deadline needs (task::deadline, passed to task::async_mutex::lock). Two exist:
//
//   BlockingScheduler  runs a task on the thread that waits for it: every resume (a chore's completion,
//                      a turn handed over, an event) is posted to that thread's queue and run there, so
//                      nothing resumes inside another task's call stack. engine::sync_wait uses it.
//   AsioHost           runs tasks on an Asio executor (the control loop); engine/host_asio.h.

#include <algorithm>               // for std::min_element
#include <cassert>                 // for assert
#include <chrono>                  // for std::chrono
#include <condition_variable>      // for std::condition_variable
#include <cstdio>                  // for std::fprintf
#include <cstdlib>                 // for std::abort
#include <coroutine>               // for std::coroutine_handle
#include <cstdint>                 // for std::uint64_t
#include <deque>                   // for std::deque
#include <exception>               // for std::exception_ptr
#include <memory>                  // for std::unique_ptr
#include <mutex>                   // for std::mutex
#include <optional>                // for std::optional
#include <thread>                  // for std::this_thread
#include <type_traits>             // for std::decay_t
#include <utility>                 // for std::move
#include <variant>                 // for std::monostate
#include <vector>                  // for std::vector

#include "task.hh"                 // for task::task, task::spawn


namespace engine {

// What the task running on a host holds, counted in debug builds: the rules are that a task holds one index's
// turn at most, and that it takes no turn, open or drain while it holds a view (docs/mechanisms/reader-views.md).
struct TaskCounters {
	int turns = 0;
	int views = 0;
};

class Host {
public:
	// What a timer runs at its deadline. It only posts or settles: it never blocks.
	struct TimerNode {
		virtual ~TimerNode() = default;
		virtual void fire() noexcept = 0;
	};

	// A timer that has been armed; destroying it cancels it (the node is then destroyed unrun).
	class Timer {
	public:
		virtual ~Timer() = default;
	};

	virtual ~Host() = default;

	// Thread-safe. The coroutine resumes on this host's thread.
	virtual void post(std::coroutine_handle<> h) = 0;
	virtual void post_demoted(std::coroutine_handle<> h) { post(h); }
	// Queues h in the heavy lane, to resume in a heavy scope (docs/design/loop-scheduling.md D2); a host without lanes
	// demotes it.
	virtual void post_heavy(std::coroutine_handle<> h) { post_demoted(h); }

	// task's optional scheduler members, which scheduler_ref::of(*this) finds on Host and so a host forwards
	// (task's cancellation D7): fn(arg) runs on the host's thread (a cross-thread cancel's emit), dropped once
	// the host closes; whether the caller is on that thread (an Asio completion then resumes inline); and the
	// list its spawned tasks are roots of, destroyed at teardown (null: they are not roots).
	virtual void post_call(void (*fn)(void*), void* arg) = 0;
	virtual bool runs_here() const noexcept { return false; }
	virtual task::root_list* roots() noexcept { return nullptr; }

	// What the task running here holds, when the host can tell: a blocking wait runs one task at a time,
	// and a debug build tracks each request on a reactor; null otherwise.
	virtual TaskCounters* counters() noexcept { return nullptr; }

	// Runs t on this host to its end, with nobody awaiting it. A task that can fail catches its own errors.
	void spawn(task::task<void> t) {
		task::spawn(task::scheduler_ref::of(*this), std::move(t), [](std::exception_ptr e) {
			if (e) { std::terminate(); }
		});
	}

	std::uint64_t now() const noexcept {
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	// Runs f() once at `at` on this host, unless the returned timer is destroyed first.
	template <typename F>
	std::unique_ptr<Timer> start_timer(std::chrono::steady_clock::time_point at, F&& f) {
		struct Node final : TimerNode {
			std::decay_t<F> fn;
			explicit Node(F&& f_) : fn(std::forward<F>(f_)) { }
			void fire() noexcept override { fn(); }
		};
		return arm(at, std::make_unique<Node>(std::forward<F>(f)));
	}

	// The host whose thread is running a task right now (null outside any).
	static Host* current() noexcept { return current_; }

	// The running task's counters, or null when it runs on no host or the host does not count.
	static TaskCounters* running_counters() noexcept { return current_ ? current_->counters() : nullptr; }

	// Marks the calling thread as this host's for the rest of its life: the thread of a loop that only
	// the host's tasks run on (an Asio host).
	static void bind_thread(Host* host) noexcept { current_ = host; }

protected:
	virtual std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) = 0;

private:
	friend class HostScope;
	static inline thread_local Host* current_ = nullptr;
};

// Makes the calling thread `host`'s for a scope, and puts back what it was: a thread that drives a host's loop for
// a while (the control loop, or the drain of its last tasks at exit) binds it so that the tasks it resumes find it.
class HostBinding {
public:
	explicit HostBinding(Host* host) noexcept : previous_(Host::current()) { Host::bind_thread(host); }
	~HostBinding() { Host::bind_thread(previous_); }
	HostBinding(const HostBinding&) = delete;
	HostBinding& operator=(const HostBinding&) = delete;

private:
	Host* previous_;
};


// Marks the code that runs a resume as running on `host`.
class HostScope {
public:
	explicit HostScope(Host* host) noexcept : previous_(Host::current_) { Host::current_ = host; }
	~HostScope() { Host::current_ = previous_; }
	HostScope(const HostScope&) = delete;
	HostScope& operator=(const HostScope&) = delete;

private:
	Host* previous_;
};


// Suspends the running task for `d` and resumes it on the scheduler it suspended on. A task that runs on no
// host (inline, in a test) sleeps its thread instead.
class [[nodiscard]] SleepFor {
public:
	explicit SleepFor(std::chrono::steady_clock::duration d) noexcept : d_(d) { }
	bool await_ready() const noexcept { return d_ <= std::chrono::steady_clock::duration::zero(); }
	bool await_suspend(std::coroutine_handle<> h) {
		Host* host = Host::current();
		if (host == nullptr) {
			std::this_thread::sleep_for(d_);
			return false;
		}
		const auto scheduler = task::current();
		timer_ = host->start_timer(std::chrono::steady_clock::now() + d_, [scheduler, h]() mutable { scheduler.post(h); });
		return true;
	}
	void await_resume() noexcept { }

private:
	std::chrono::steady_clock::duration d_;
	std::unique_ptr<Host::Timer> timer_;
};

inline SleepFor sleep_for(std::chrono::steady_clock::duration d) noexcept {
	return SleepFor(d);
}


// What a debug build does when a task that holds a view is about to wait (docs/mechanisms/reader-views.md, "The rule
// for a request that holds a view"): abort. A test replaces it with a function that throws.
using ViewRuleHandler = void (*)(const char* waits_for);
inline ViewRuleHandler& view_rule_handler() noexcept {
	static ViewRuleHandler handler = nullptr;
	return handler;
}

// Fails (debug builds) when the running task holds a view and is about to wait for a turn, an open or a drain:
// the view could be what that wait is waiting for. Release builds compile it out.
inline void check_no_view_held([[maybe_unused]] const char* waits_for) {
#ifndef NDEBUG
	if (const auto* counters = Host::running_counters(); counters != nullptr && counters->views != 0) {
		if (auto handler = view_rule_handler()) {
			handler(waits_for);
		}
		std::fprintf(stderr, "a task that holds %d view(s) waits for %s\n", counters->views, waits_for);
		std::abort();
	}
#endif
}


// A scheduler owned by one waiting thread: run() drives a task there until it ends.
class BlockingScheduler final : public Host {
public:
	BlockingScheduler() = default;
	BlockingScheduler(const BlockingScheduler&) = delete;
	BlockingScheduler& operator=(const BlockingScheduler&) = delete;
	~BlockingScheduler() override {
		assert(ready_.empty() && timers_.empty());
	}

	void post(std::coroutine_handle<> h) override {
		// Signals under the lock: once the waiter sees its work (or `done`) it may leave and destroy this
		// scheduler, so no signal may follow the unlock.
		std::lock_guard<std::mutex> lk(m_);
		ready_.push_back({h, nullptr, nullptr});
		cv_.notify_one();
	}

	void post_call(void (*fn)(void*), void* arg) override {
		std::lock_guard<std::mutex> lk(m_);
		ready_.push_back({{}, fn, arg});
		cv_.notify_one();
	}

	TaskCounters* counters() noexcept override { return &counters_; }

	// Runs `t` to its end on the calling thread and returns its value or rethrows its exception. Every
	// resume of the task, and of anything it awaits that resumes through this host, runs here.
	template <typename T>
	T run(task::task<T> t) {
		std::exception_ptr error;
		std::optional<std::conditional_t<std::is_void_v<T>, std::monostate, T>> value;
		bool done = false;
		auto finished = [this, &done] {
			std::lock_guard<std::mutex> lk(m_);
			done = true;
			cv_.notify_one();  // under the lock: the waiter may destroy this scheduler as soon as it sees `done`
		};
		// This scheduler owns no roots (roots() is null) and never closes, so the spawn always runs the task and
		// `finished` always comes: the wait cannot outlive a task destroyed unrun.
		if constexpr (std::is_void_v<T>) {
			task::spawn(task::scheduler_ref::of(*this), std::move(t), [&](std::exception_ptr e) {
				error = e;
				finished();
			});
		} else {
			task::spawn(task::scheduler_ref::of(*this), std::move(t), [&](std::exception_ptr e, std::optional<T> v) {
				error = e;
				value = std::move(v);
				finished();
			});
		}
		drive(done);
		if (error) {
			std::rethrow_exception(error);
		}
		if constexpr (!std::is_void_v<T>) {
			return std::move(*value);
		}
	}

protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) override {
		std::uint64_t id;
		{
			std::lock_guard<std::mutex> lk(m_);
			id = ++last_id_;
			timers_.push_back({at, id, std::move(node)});
			cv_.notify_one();
		}
		return std::make_unique<Armed>(*this, id);
	}

private:
	// A resume, or a function post_call queued.
	struct Ready {
		std::coroutine_handle<> h;
		void (*fn)(void*);
		void* arg;
	};

	struct Entry {
		std::chrono::steady_clock::time_point at;
		std::uint64_t id;
		std::unique_ptr<TimerNode> node;
	};

	// Cancels on destruction. It lives in the frame of the task that armed it, which ends before run() does.
	class Armed final : public Timer {
	public:
		Armed(BlockingScheduler& owner, std::uint64_t id) : owner_(owner), id_(id) { }
		~Armed() override {
			std::unique_ptr<TimerNode> dropped;  // destroyed outside the lock
			std::lock_guard<std::mutex> lk(owner_.m_);
			const auto it = std::find_if(owner_.timers_.begin(), owner_.timers_.end(), [this](const Entry& e) { return e.id == id_; });
			if (it != owner_.timers_.end()) {
				dropped = std::move(it->node);
				owner_.timers_.erase(it);
			}
		}

	private:
		BlockingScheduler& owner_;
		std::uint64_t id_;
	};

	void drive(const bool& done) {
		std::unique_lock<std::mutex> lk(m_);
		while (!done) {
			if (!ready_.empty()) {
				const auto next = ready_.front();
				ready_.pop_front();
				lk.unlock();
				{
					task::running_on scope(task::scheduler_ref::of(*this));
					HostScope host(this);
					if (next.fn != nullptr) {
						next.fn(next.arg);
					} else {
						next.h.resume();
					}
				}
				lk.lock();
				continue;
			}
			if (!timers_.empty()) {
				const auto next = std::min_element(timers_.begin(), timers_.end(), [](const Entry& a, const Entry& b) { return a.at < b.at; });
				if (next->at <= std::chrono::steady_clock::now()) {
					auto node = std::move(next->node);
					timers_.erase(next);
					lk.unlock();
					node->fire();  // posts or settles; the node dies here, outside the lock
					node.reset();
					lk.lock();
					continue;
				}
				cv_.wait_until(lk, next->at);
				continue;
			}
			cv_.wait(lk);
		}
	}

	std::mutex m_;
	std::condition_variable cv_;
	std::deque<Ready> ready_;
	std::vector<Entry> timers_;
	std::uint64_t last_id_ = 0;
	TaskCounters counters_;
};

}  // namespace engine
