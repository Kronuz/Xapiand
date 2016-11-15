/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include <chrono>             // for system_clock, time_point, duration, millise...
#include <mutex>              // for condition_variable, mutex
#include <thread>             // for thread, thread::id

#include "xapiand.h"
#include "stash.h"
#include "threadpool.h"


using namespace std::chrono_literals;


class ScheduledTask;
class Scheduler;


using TaskType = std::shared_ptr<ScheduledTask>;


class ScheduledTask : public Task<>, public std::enable_shared_from_this<ScheduledTask> {
	friend class Scheduler;

	unsigned long long wakeup_time;

protected:
	std::atomic_ullong created_at;
	std::atomic_ullong cleared_at;

public:
	ScheduledTask(std::chrono::time_point<std::chrono::system_clock> created_at_=std::chrono::system_clock::now());
	~ScheduledTask();

	bool clear();
};


#define MUL 1000000ULL

class SchedulerQueue {
public:
	template <typename T>
	static inline uint64_t time_point_to_key(std::chrono::time_point<T> n) {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(n.time_since_epoch()).count();
	}

	static inline uint64_t now() {
		return time_point_to_key(std::chrono::system_clock::now());
	}

private:
	using _tasks =        StashValues<TaskType,       10ULL>;
	using _50_1ms =       StashSlots<_tasks,          10ULL,   &now, 1ULL * MUL / 2ULL, 1ULL * MUL,       50ULL,   false>;
	using _10_50ms =      StashSlots<_50_1ms,         10ULL,   &now, 0ULL,              50ULL * MUL,      10ULL,   false>;
	using _3600_500ms =   StashSlots<_10_50ms,        600ULL,  &now, 0ULL,              500ULL * MUL,     3600ULL, false>;
	using _48_1800s =     StashSlots<_3600_500ms,     48ULL,   &now, 0ULL,              1800000ULL * MUL, 48ULL,   true>;
	_48_1800s queue;

public:
	SchedulerQueue();

	TaskType& peep();
	TaskType& next(bool final=true, uint64_t final_key=0, bool keep_going=true);
	void add(const TaskType& task, uint64_t key=0);
};


class Scheduler {
	ThreadPool<>* thread_pool;

	std::condition_variable wakeup_signal;
	std::atomic_ullong next_wakeup_time;

	SchedulerQueue scheduler_queue;

	const std::string name;
	std::atomic_int running;
	std::thread inner_thread;

	void run_one(TaskType& task);
	void run();

public:
	Scheduler(const std::string& name_, ThreadPool<>* thread_pool_=nullptr);
	~Scheduler();

	void finish(int wait=10);
	void join();
	void add(const TaskType& task, std::chrono::time_point<std::chrono::system_clock> wakeup);
};


class ThreadPoolScheduler : public Scheduler {
	ThreadPool<> thread_pool;

public:
	template<typename... Params_>
	ThreadPoolScheduler(const std::string& name, const std::string format, size_t num_threads, Params_&&... params)
		: Scheduler(name, &thread_pool),
		  thread_pool(format, num_threads, std::forward<Params_>(params)...) { }  // o.num_committers
};
