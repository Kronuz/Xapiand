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

#include <chrono>                 // for system_clock, time_point, duration, millise...
#include <mutex>                  // for condition_variable, mutex

#include "stash.h"
#include "thread.hh"              // for Thread
#include "threadpool.hh"          // for ThreadPool
#include "time_point.hh"          // for time_point_to_ullong


using namespace std::chrono_literals;


class ScheduledTask;
class Scheduler;


using TaskType = std::shared_ptr<ScheduledTask>;


class ScheduledTask : public std::enable_shared_from_this<ScheduledTask> {
	friend class Scheduler;

protected:
	unsigned long long wakeup_time;
	std::atomic_ullong created_at;
	std::atomic_ullong cleared_at;

public:
	explicit ScheduledTask(const std::chrono::time_point<std::chrono::system_clock>& created_at_ = std::chrono::system_clock::now());

	virtual ~ScheduledTask() = default;

	virtual void run() = 0;

	explicit operator bool() const noexcept {
		return !cleared_at;
	}

	bool clear();

	std::string __repr__(const std::string& name) const;

	virtual std::string __repr__() const {
		return __repr__("ScheduledTask");
	}
};


#define MS 1000000ULL


class SchedulerQueue {
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
	SchedulerQueue();

	TaskType peep(unsigned long long current_key);
	TaskType walk();
	void clean_checkpoint();
	void clean();
	void add(const TaskType& task, unsigned long long key=0);
};


class Scheduler : public Thread {
	std::mutex mtx;

	std::unique_ptr<ThreadPool> thread_pool;

	std::condition_variable wakeup_signal;
	std::atomic_ullong atom_next_wakeup_time;

	SchedulerQueue scheduler_queue;

	const std::string name;
	std::atomic_int ending;

	void run_one(TaskType& task);
	void run();

	void operator()() override {
		run();
	}

public:
	explicit Scheduler(std::string  name_);
	Scheduler(std::string  name_, const char* format, size_t num_threads);
	~Scheduler();

	size_t threadpool_capacity();
	size_t threadpool_size();
	size_t running_size();
	size_t size();

	bool finish(int wait);

	bool join(std::chrono::milliseconds timeout);
	bool join(int timeout = 60000) {
		return join(std::chrono::milliseconds(timeout));
	}

	void add(const TaskType& task, unsigned long long wakeup_time);
	void add(const TaskType& task, const std::chrono::time_point<std::chrono::system_clock>& wakeup);
};
