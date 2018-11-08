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

#include <memory>
#include <utility>

#include "scheduler.h"

#include "log.h"            // for L_*
#include "string.hh"        // for string::format


// #define L_SCHEDULER L_COLLECT
#ifndef L_SCHEDULER
#define L_SCHEDULER_DEFINED
#define L_SCHEDULER L_NOTHING
#endif


ScheduledTask::ScheduledTask(const std::chrono::time_point<std::chrono::system_clock>& created_at_)
	: wakeup_time(0),
	  created_at(time_point_to_ullong(created_at_)),
	  cleared_at(0) { }


bool
ScheduledTask::clear()
{
	unsigned long long c = 0;
	return cleared_at.compare_exchange_strong(c, time_point_to_ullong(std::chrono::system_clock::now()));
}


std::string
ScheduledTask::__repr__(const std::string& name) const
{
	return string::format("<%s at %p>", name, static_cast<const void *>(this));
}


SchedulerQueue::SchedulerQueue()
	: ctx(now()),
	  cctx(now()) { }


TaskType
SchedulerQueue::peep(unsigned long long current_key)
{
	ctx.op = StashContext::Operation::peep;
	ctx.cur_key = ctx.atom_first_key.load();
	ctx.current_key = current_key;
	TaskType task;
	queue.next(ctx, &task);
	return task;
}


TaskType
SchedulerQueue::walk()
{
	ctx.op = StashContext::Operation::walk;
	ctx.cur_key = ctx.atom_first_key.load();
	ctx.current_key = time_point_to_ullong(std::chrono::system_clock::now());
	TaskType task;
	queue.next(ctx, &task);
	return task;
}


void
SchedulerQueue::clean_checkpoint()
{
	auto cur_key = ctx.atom_first_key.load();
	if (cur_key < cctx.atom_first_key.load()) {
		cctx.atom_first_key = cur_key;
	}
	cctx.atom_last_key = ctx.atom_last_key.load();
}


void
SchedulerQueue::clean()
{
	cctx.op = StashContext::Operation::clean;
	cctx.cur_key = cctx.atom_first_key.load();
	cctx.current_key = time_point_to_ullong(std::chrono::system_clock::now() - 1s);
	TaskType task;
	queue.next(cctx, &task);
}


void
SchedulerQueue::add(const TaskType& task, unsigned long long key)
{
	try {
		queue.add(ctx, key, task);
	} catch (const std::out_of_range&) {
		fprintf(stderr, "%s", (BROWN + "Stash overflow!" + CLEAR_COLOR + "\n").c_str());
	}
}


Scheduler::Scheduler(std::string  name_) :
	atom_next_wakeup_time(0),
	name(std::move(name_)),
	ending(-1)
{
	start();
}


Scheduler::Scheduler(std::string name_, const char* format, size_t num_threads) :
	thread_pool(std::make_unique<ThreadPool>(format, num_threads)),
	atom_next_wakeup_time(0),
	name(std::move(name_)),
	ending(-1)
{
	start();
}


Scheduler::~Scheduler()
{
	finish(1);
}


size_t
Scheduler::threadpool_capacity()
{
	if (thread_pool) {
		return thread_pool->threadpool_capacity();
	}
	return 0;
}


size_t
Scheduler::threadpool_size()
{
	if (thread_pool) {
		return thread_pool->threadpool_size();
	}
	return 0;
}


size_t
Scheduler::running_size()
{
	if (thread_pool) {
		return thread_pool->running_size();
	}
	return 0;
}


size_t
Scheduler::size()
{
	if (thread_pool) {
		return thread_pool->size();
	}
	return 0;
}


bool
Scheduler::finish(int wait)
{
	ending = wait;

	{
		std::lock_guard<std::mutex> lk(mtx);
	}
	wakeup_signal.notify_all();

	if (thread_pool) {
		thread_pool->finish();
	}

	if (wait != 0) {
		return join(2 * wait * 100ms);
	}

	return true;
}


bool
Scheduler::join(std::chrono::milliseconds timeout)
{
	auto threadpool_workers = thread_pool->threadpool_workers() + 1;
	auto single_timeout = timeout / threadpool_workers;

	if (!Thread::join(single_timeout)) {
		return false;
	}
	if (thread_pool) {
		if (!thread_pool->join(single_timeout * threadpool_workers)) {
			return false;
		}
		thread_pool.reset();
	}
	return true;
}


void
Scheduler::add(const TaskType& task, unsigned long long wakeup_time)
{
	if (ending != 0) {
		auto now = time_point_to_ullong(std::chrono::system_clock::now());
		if (wakeup_time < now) {
			wakeup_time = now;
		}

		task->wakeup_time = wakeup_time;
		scheduler_queue.add(task, wakeup_time);

		auto next_wakeup_time = atom_next_wakeup_time.load();
		while (next_wakeup_time > wakeup_time && !atom_next_wakeup_time.compare_exchange_weak(next_wakeup_time, wakeup_time)) { }

		if (next_wakeup_time >= wakeup_time || next_wakeup_time <= now) {
			{
				std::lock_guard<std::mutex> lk(mtx);
			}
			wakeup_signal.notify_one();
			L_SCHEDULER("Scheduler::" + LIGHT_GREEN + "ADDED_NOTIFY" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu, wakeup_time:%llu - %s", now, atom_next_wakeup_time.load(), wakeup_time, task ? task->__repr__() : "");
		} else {
			L_SCHEDULER("Scheduler::" + FOREST_GREEN + "ADDED" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu, wakeup_time:%llu - %s", now, atom_next_wakeup_time.load(), wakeup_time, task ? task->__repr__() : "");
		}
	}
}


void
Scheduler::add(const TaskType& task, const std::chrono::time_point<std::chrono::system_clock>& wakeup)
{
	add(task, time_point_to_ullong(wakeup));
}


void
Scheduler::run_one(TaskType& task)
{
	if (*task) {
		if (task->clear()) {
			L_SCHEDULER("Scheduler::" + STEEL_BLUE + "RUNNING" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
			if (thread_pool) {
				try {
					thread_pool->enqueue([task]{
						task->run();
					});
				} catch (const std::logic_error&) { }
			} else {
				task->run();
			}
			return;
		}
	}
	L_SCHEDULER("Scheduler::" + BROWN + "ABORTED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), task->wakeup_time);
}


void
Scheduler::run()
{
	L_SCHEDULER("Scheduler::" + LIGHT_SKY_BLUE + "STARTED" + CLEAR_COLOR);

	set_thread_name(name);

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
		L_SCHEDULER("Scheduler::" + DIM_GREY + "PEEPING" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(now), wakeup_time);
		if ((task = scheduler_queue.peep(wakeup_time))) {
			if (task) {
				pending = true;  // flag there are still scheduled things pending.
				if (wakeup_time > task->wakeup_time) {
					wakeup_time = task->wakeup_time;
					L_SCHEDULER("Scheduler::" + PURPLE + "PEEP_UPDATED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu  (%s)", time_point_to_ullong(now), wakeup_time, *task ? "valid" : "cleared");
				} else {
					L_SCHEDULER("Scheduler::" + DIM_GREY + "PEEPED" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu  (%s)", time_point_to_ullong(now), wakeup_time, *task ? "valid" : "cleared");
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
		L_DEBUG_HOOK("Scheduler::LOOP", "Scheduler::" + STEEL_BLUE + "LOOP" + CLEAR_COLOR + " - now:%llu, next_wakeup_time:%llu", time_point_to_ullong(now), atom_next_wakeup_time.load());
		lk.lock();
		next_wakeup_time = atom_next_wakeup_time.load();
		auto next_wakeup_time_point = time_point_from_ullong(next_wakeup_time);
		if (next_wakeup_time_point > now) {
			wakeup_signal.wait_until(lk, next_wakeup_time_point);
		}
		lk.unlock();
		L_SCHEDULER("Scheduler::" + DODGER_BLUE + "WAKEUP" + CLEAR_COLOR + " - now:%llu, wakeup_time:%llu", time_point_to_ullong(std::chrono::system_clock::now()), wakeup_time);

		// Start walking the queue and running still pending tasks.
		scheduler_queue.clean_checkpoint();
		while ((task = scheduler_queue.walk())) {
			if (task) {
				run_one(task);
			}
		}
		scheduler_queue.clean();
	}
}


#ifdef L_SCHEDULER_DEFINED
#undef L_SCHEDULER_DEFINED
#undef L_SCHEDULER
#endif
