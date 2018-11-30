/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <atomic>                 // for std::atomic_ullong
#include <chrono>                 // for std::chrono
#include <memory>
#include <mutex>                  // for std::condition_variable, std::mutex
#include <utility>                // for std::move

#include "cassert.h"              // for ASSERT
#include "log.h"                  // for L_*
#include "stash.h"                // for StashValues, StashSlots, StashContext
#include "thread.hh"              // for Thread
#include "threadpool.hh"          // for ThreadPool
#include "time_point.hh"          // for time_point_to_ullong


using namespace std::chrono_literals;


// #define L_SCHEDULER L_PRINT


#ifndef L_SCHEDULER
#define L_SCHEDULER_DEFINED
#define L_SCHEDULER L_NOTHING
#endif


template <typename SchedulerImpl, typename ScheduledTaskImpl, ThreadPolicyType thread_policy>
class BaseScheduler;

template <typename ScheduledTaskImpl, ThreadPolicyType thread_policy>
class Scheduler;

template <typename ScheduledTaskImpl, ThreadPolicyType thread_policy>
class ThreadedScheduler;


template <typename SchedulerImpl, typename ScheduledTaskImpl, ThreadPolicyType thread_policy = ThreadPolicyType::regular>
class ScheduledTask : public std::enable_shared_from_this<ScheduledTask<SchedulerImpl, ScheduledTaskImpl, thread_policy>> {
	friend BaseScheduler<SchedulerImpl, ScheduledTaskImpl, thread_policy>;
	friend Scheduler<ScheduledTaskImpl, thread_policy>;
	friend ThreadedScheduler<ScheduledTaskImpl, thread_policy>;

protected:
	unsigned long long wakeup_time;
	std::atomic_ullong created_at;
	std::atomic_ullong cleared_at;

public:
	ScheduledTask(const std::chrono::time_point<std::chrono::system_clock>& created_at = std::chrono::system_clock::now()) :
		wakeup_time(0),
		created_at(time_point_to_ullong(created_at)),
		cleared_at(0) {}

	operator bool() const noexcept {
		return !cleared_at;
	}

	void operator()() {
		static_cast<ScheduledTaskImpl*>(this)->operator()();
	}

	bool clear(bool /*internal*/ = false) {
		unsigned long long c = 0;
		return cleared_at.compare_exchange_strong(c, time_point_to_ullong(std::chrono::system_clock::now()));
	}
};


#define MS 1000000ULL


template <typename SchedulerImpl, typename ScheduledTaskImpl, ThreadPolicyType thread_policy>
class SchedulerQueue {
	using TaskType = std::shared_ptr<ScheduledTask<SchedulerImpl, ScheduledTaskImpl, thread_policy>>;

public:
	static unsigned long long now() {
		return time_point_to_ullong(std::chrono::system_clock::now());
	}

private:
	/*                               <  _Tp         _Size  _CurrentKey     _Div        _Mod    _Ring >*/
	using _tasks =        StashValues<TaskType,     10ULL,  &now>;
	using _50_1ms =       StashSlots<_tasks,        10ULL,  &now,        1ULL * MS,    50ULL,  false>;
	using _10_50ms =      StashSlots<_50_1ms,       10ULL,  &now,       50ULL * MS,    10ULL,  false>;
	using _36_500ms =     StashSlots<_10_50ms,      12ULL,  &now,      500ULL * MS,    36ULL,  false>;
	using _4800_18s =     StashSlots<_36_500ms,   4800ULL,  &now,    18000ULL * MS,  4800ULL,  true>;

	StashContext ctx;
	StashContext cctx;
	_4800_18s queue;

public:
	SchedulerQueue() :
		ctx(now()),
		cctx(now()) {}

	TaskType peep(unsigned long long current_key) {
		ctx.op = StashContext::Operation::peep;
		ctx.cur_key = ctx.atom_first_key.load();
		ctx.current_key = current_key;
		TaskType task;
		queue.next(ctx, &task);
		return task;
	}

	TaskType walk() {
		ctx.op = StashContext::Operation::walk;
		ctx.cur_key = ctx.atom_first_key.load();
		ctx.current_key = time_point_to_ullong(std::chrono::system_clock::now());
		TaskType task;
		queue.next(ctx, &task);
		return task;
	}

	void clean_checkpoint() {
		auto cur_key = ctx.atom_first_key.load();
		if (cur_key < cctx.atom_first_key.load()) {
			cctx.atom_first_key = cur_key;
		}
		cctx.atom_last_key = ctx.atom_last_key.load();
	}

	void clean() {
		cctx.op = StashContext::Operation::clean;
		cctx.cur_key = cctx.atom_first_key.load();
		cctx.current_key = time_point_to_ullong(std::chrono::system_clock::now() - 1s);
		TaskType task;
		queue.next(cctx, &task);
	}

	void add(const TaskType& task, unsigned long long key = 0) {
		try {
			queue.add(ctx, key, task);
		} catch (const std::out_of_range&) {
			L_SCHEDULER("BaseScheduler::" + BROWN + "Stash overflow!" + CLEAR_COLOR + "\n");
		}
	}
};


template <typename SchedulerImpl, typename ScheduledTaskImpl, ThreadPolicyType thread_policy = ThreadPolicyType::regular>
class BaseScheduler : public Thread<BaseScheduler<SchedulerImpl, ScheduledTaskImpl, thread_policy>, thread_policy> {
	using TaskType = std::shared_ptr<ScheduledTask<SchedulerImpl, ScheduledTaskImpl, thread_policy>>;

	std::mutex mtx;

	std::condition_variable wakeup_signal;
	std::atomic_ullong atom_next_wakeup_time;

	SchedulerQueue<SchedulerImpl, ScheduledTaskImpl, thread_policy> scheduler_queue;

	std::string _name;
	std::atomic_int ending;

protected:
	void end(int wait = 10) {
		ending = wait;

		std::lock_guard<std::mutex> lk(mtx);
		wakeup_signal.notify_all();
	}

public:
	BaseScheduler(std::string name) :
		atom_next_wakeup_time(0),
		_name(std::move(name)),
		ending(-1) {
		Thread<BaseScheduler<SchedulerImpl, ScheduledTaskImpl, thread_policy>, thread_policy>::run();
	}

	const std::string& name() {
		return _name;
	}

	void operator()() {
		L_SCHEDULER("BaseScheduler::" + LIGHT_SKY_BLUE + "STARTED" + CLEAR_COLOR);

		std::unique_lock<std::mutex> lk(mtx);
		auto next_wakeup_time = atom_next_wakeup_time.load();
		lk.unlock();

		while (ending != 0) {
			if (--ending < 0) {
				ending = -1;
			}

			bool pending = false;

			// Propose a wakeup time some time in the future:
			auto now = std::chrono::system_clock::now();
			auto wakeup_time = time_point_to_ullong(now + (ending < 0 ? 30s : 100ms));

			// Then figure out if there's something that needs to be acted upon sooner
			// than that wakeup time in the scheduler queue (an earlier wakeup time needed):
			TaskType task;
			L_SCHEDULER("BaseScheduler::" + DIM_GREY + "PEEPING" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(now), wakeup_time);
			if ((task = scheduler_queue.peep(wakeup_time))) {
				if (task) {
					pending = true;  // flag there are still scheduled things pending.
					if (wakeup_time > task->wakeup_time) {
						wakeup_time = task->wakeup_time;
						L_SCHEDULER("BaseScheduler::" + PURPLE + "PEEP_UPDATED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu  (%s)", time_point_to_ullong(now), wakeup_time, *task ? "valid" : "cleared");
					} else {
						L_SCHEDULER("BaseScheduler::" + DIM_GREY + "PEEPED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu  (%s)", time_point_to_ullong(now), wakeup_time, *task ? "valid" : "cleared");
					}
				}
			}

			// Try setting the worked out wakeup time as the real next wakeup time:
			if (atom_next_wakeup_time.compare_exchange_strong(next_wakeup_time, wakeup_time)) {
				if (ending >= 0 && !pending) {
					break;
				}
			}
			while (next_wakeup_time > wakeup_time && !atom_next_wakeup_time.compare_exchange_weak(next_wakeup_time, wakeup_time)) { }

			// Sleep until wakeup time arrives or someone adding a task wakes us up;
			// make sure we first lock mutex so there cannot be race condition between
			// the time we load the next_wakeup_time and we actually start waiting:
			L_DEBUG_HOOK("BaseScheduler::LOOP", "BaseScheduler::" + STEEL_BLUE + "LOOP" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu", time_point_to_ullong(now), atom_next_wakeup_time.load());
			lk.lock();
			next_wakeup_time = atom_next_wakeup_time.load();
			auto next_wakeup_time_point = time_point_from_ullong(next_wakeup_time);
			if (next_wakeup_time_point > now) {
				wakeup_signal.wait_until(lk, next_wakeup_time_point);
			}
			lk.unlock();
			L_SCHEDULER("BaseScheduler::" + DODGER_BLUE + "WAKEUP" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), wakeup_time);

			// Start walking the queue and running still pending tasks.
			scheduler_queue.clean_checkpoint();
			while ((task = scheduler_queue.walk())) {
				if (task) {
					static_cast<SchedulerImpl*>(this)->operator()(task);
				}
			}
			scheduler_queue.clean();
		}
	}

	void add(const TaskType& task) {
		if (ending != 0) {
			unsigned long long wakeup_time = task->wakeup_time;
			ASSERT(wakeup_time != 0);

			scheduler_queue.add(task, wakeup_time);

			auto next_wakeup_time = atom_next_wakeup_time.load();
			while (next_wakeup_time > wakeup_time && !atom_next_wakeup_time.compare_exchange_weak(next_wakeup_time, wakeup_time)) { }

			auto now = time_point_to_ullong(std::chrono::system_clock::now());
			if (next_wakeup_time >= wakeup_time || next_wakeup_time <= now) {
				std::lock_guard<std::mutex> lk(mtx);
				wakeup_signal.notify_one();
				L_SCHEDULER("BaseScheduler::" + LIGHT_GREEN + "ADDED_NOTIFY" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu, wakeup_time:%llu", now, atom_next_wakeup_time.load(), wakeup_time);
			} else {
				L_SCHEDULER("BaseScheduler::" + FOREST_GREEN + "ADDED" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu, wakeup_time:%llu", now, atom_next_wakeup_time.load(), wakeup_time);
			}
		}
	}

	void add(const TaskType& task, unsigned long long wakeup_time) {
		auto now = time_point_to_ullong(std::chrono::system_clock::now());
		if (wakeup_time < now) {
			wakeup_time = now;
		}
		task->wakeup_time = wakeup_time;
		add(task);
	}

	void add(const TaskType& task, const std::chrono::time_point<std::chrono::system_clock>& wakeup) {
		add(task, time_point_to_ullong(wakeup));
	}
};


template <typename ScheduledTaskImpl, ThreadPolicyType thread_policy = ThreadPolicyType::regular>
class Scheduler : public BaseScheduler<Scheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy> {
	using TaskType = std::shared_ptr<ScheduledTask<Scheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>>;

public:
	Scheduler(std::string name_) :
		BaseScheduler<Scheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>(name_) {}

	~Scheduler() {
		finish(1);
	}

	bool finish(int wait = 10) {
		BaseScheduler<Scheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>::end(wait);

		if (wait != 0) {
			return join(2 * wait * 100ms);
		}

		return true;
	}

	bool join(std::chrono::milliseconds timeout = 60s) {
		if (!BaseScheduler<Scheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>::join(timeout)) {
			return false;
		}
		return true;
	}

	void operator()(TaskType& task) {
		if (*task) {
			if (task->clear(true)) {
				L_SCHEDULER("Scheduler::" + STEEL_BLUE + "RUNNING" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
				task->operator()();
			}
		}
		L_SCHEDULER("Scheduler::" + BROWN + "ABORTED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
	}
};


template <typename ScheduledTaskImpl, ThreadPolicyType thread_policy = ThreadPolicyType::regular>
class ThreadedScheduler : public BaseScheduler<ThreadedScheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy> {
	using TaskType = std::shared_ptr<ScheduledTask<ThreadedScheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>>;

	ThreadPool<TaskType, thread_policy> thread_pool;

public:
	ThreadedScheduler(std::string name_, const char* format, size_t num_threads)  :
		BaseScheduler<ThreadedScheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>(name_),
		thread_pool(format, num_threads) {}

	~ThreadedScheduler() {
		finish(1);
	}

	size_t threadpool_capacity() {
		return thread_pool.threadpool_capacity();
	}

	size_t threadpool_size() {
		return thread_pool.threadpool_size();
	}

	size_t running_size() {
		return thread_pool.running_size();
	}

	size_t size() {
		return thread_pool.size();
	}

	bool finish(int wait = 10) {
		BaseScheduler<ThreadedScheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>::end(wait);

		thread_pool.finish();

		if (wait != 0) {
			return join(2 * wait * 100ms);
		}

		return true;
	}

	bool join(std::chrono::milliseconds timeout = 60s) {
		auto threadpool_workers = thread_pool.threadpool_workers() + 1;
		auto single_timeout = timeout / threadpool_workers;

		if (!BaseScheduler<ThreadedScheduler<ScheduledTaskImpl, thread_policy>, ScheduledTaskImpl, thread_policy>::join(single_timeout)) {
			return false;
		}
		if (!thread_pool.join(single_timeout * threadpool_workers)) {
			return false;
		}
		return true;
	}

	void operator()(TaskType& task) {
		if (*task) {
			if (task->clear(true)) {
				L_SCHEDULER("ThreadedScheduler::" + STEEL_BLUE + "RUNNING" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
				try {
					thread_pool.enqueue(task);
				} catch (const std::logic_error&) { }
			}
		}
		L_SCHEDULER("ThreadedScheduler::" + BROWN + "ABORTED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
	}
};


#ifdef L_SCHEDULER_DEFINED
#undef L_SCHEDULER_DEFINED
#undef L_SCHEDULER
#endif
