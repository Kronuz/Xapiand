/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <cassert>                           // for std::chrono
#include <chrono>                            // for std::chrono
#include <mutex>                             // for std::mutex
#include <unordered_map>                     // for std::unordered_map
#include <utility>                           // for std::move, std::forward

#include "callable_traits.hh"                // for callable_traits
#include "log.h"                             // for L_CALL, L_DEBUG_HOOK
#include "scheduler.h"                       // for ScheduledTask, ThreadedScheduler
#include "time_point.hh"                     // for time_point_to_ullong


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
class DebouncerTask;


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
class Debouncer : public ThreadedScheduler<DebouncerTask<Key, Func, Tuple, thread_policy>> {
	friend DebouncerTask<Key, Func, Tuple, thread_policy>;

	struct Status {
		std::shared_ptr<DebouncerTask<Key, Func, Tuple, thread_policy>> task;
		unsigned long long max_wakeup_time;
	};

	std::mutex statuses_mtx;
	std::unordered_map<Key, Status> statuses;

	Func func;
	std::chrono::milliseconds throttle_time;
	std::chrono::milliseconds debounce_timeout;
	std::chrono::milliseconds debounce_busy_timeout;
	std::chrono::milliseconds debounce_force_timeout;

	void release(const Key& key);
	void throttle(const Key& key);

public:
	Debouncer(std::string name, const char* format, size_t num_threads, Func func, std::chrono::milliseconds throttle_time, std::chrono::milliseconds debounce_timeout, std::chrono::milliseconds debounce_busy_timeout, std::chrono::milliseconds debounce_force_timeout) :
		ThreadedScheduler<DebouncerTask<Key, Func, Tuple, thread_policy>>(name, format, num_threads),
		func(std::move(func)),
		throttle_time(throttle_time),
		debounce_timeout(debounce_timeout),
		debounce_busy_timeout(debounce_busy_timeout),
		debounce_force_timeout(debounce_force_timeout)
	{
		assert(throttle_time == std::chrono::milliseconds(0) || throttle_time >= debounce_timeout);
		assert(debounce_busy_timeout >= debounce_timeout);
		assert(debounce_force_timeout >= debounce_busy_timeout);
	}

	template <typename... Args>
	void debounce(const Key& key, Args&&... args);

	template <typename... Args>
	void delayed_debounce(std::chrono::milliseconds delay, const Key& key, Args&&... args);
};


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
class DebouncerTask : public ScheduledTask<ThreadedScheduler<DebouncerTask<Key, Func, Tuple, thread_policy>>, DebouncerTask<Key, Func, Tuple, thread_policy>> {
	friend Debouncer<Key, Func, Tuple, thread_policy>;

	Debouncer<Key, Func, Tuple, thread_policy>& debouncer;

	bool throttler;

	Key key;
	Tuple args;

public:
	DebouncerTask(Debouncer<Key, Func, Tuple, thread_policy>& debouncer, const Key& key);
	DebouncerTask(Debouncer<Key, Func, Tuple, thread_policy>& debouncer, const Key& key, Tuple args);

	void operator()();
};


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
inline
DebouncerTask<Key, Func, Tuple, thread_policy>::DebouncerTask(Debouncer<Key, Func, Tuple, thread_policy>& debouncer, const Key& key, Tuple args) :
	debouncer(debouncer),
	throttler(false),
	key(std::move(key)),
	args(std::move(args))
{
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
inline
DebouncerTask<Key, Func, Tuple, thread_policy>::DebouncerTask(Debouncer<Key, Func, Tuple, thread_policy>& debouncer, const Key& key) :
	debouncer(debouncer),
	throttler(true),
	key(std::move(key))
{
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
inline void
DebouncerTask<Key, Func, Tuple, thread_policy>::operator()()
{
	L_CALL("DebouncerTask::operator()()");
	L_DEBUG_HOOK("DebouncerTask::operator()", "DebouncerTask::operator()()");

	if (throttler) {
		debouncer.release(key);
	} else {
		debouncer.throttle(key);
		std::apply(debouncer.func, args);
	}
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
inline void
Debouncer<Key, Func, Tuple, thread_policy>::release(const Key& key)
{
	std::lock_guard<std::mutex> statuses_lk(statuses_mtx);
	statuses.erase(key);
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
inline void
Debouncer<Key, Func, Tuple, thread_policy>::throttle(const Key& key)
{
	if (throttle_time <= debounce_timeout) {
		// No throttler, just release:
		release(key);
	} else {
		// Throttling, configure:
		std::shared_ptr<DebouncerTask<Key, Func, Tuple, thread_policy>> task;

		{
			auto now = std::chrono::steady_clock::now();

			std::lock_guard<std::mutex> statuses_lk(statuses_mtx);
			auto it = statuses.find(key);
			if (it != statuses.end()) {
				auto status = &(it->second);

				status->max_wakeup_time = 0;  // flag status as throttler

				if (status->task) {
					status->task->clear();
				}
				task = std::make_shared<DebouncerTask<Key, Func, Tuple, thread_policy>>(*this, key);
				task->wakeup_time = time_point_to_ullong(now + throttle_time);
				status->task = task;
			}
		}

		this->add(task);
	}
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
template <typename... Args>
inline void
Debouncer<Key, Func, Tuple, thread_policy>::debounce(const Key& key, Args&&... args)
{
	L_CALL("Debouncer::debounce(<key>, ...)");

	delayed_debounce(0ms, key, std::forward<Args>(args)...);
}


template <typename Key, typename Func, typename Tuple, ThreadPolicyType thread_policy>
template <typename... Args>
inline void
Debouncer<Key, Func, Tuple, thread_policy>::delayed_debounce(std::chrono::milliseconds delay, const Key& key, Args&&... args)
{
	L_CALL("Debouncer::delayed_debounce(<delay>, <key>, ...)");

	std::shared_ptr<DebouncerTask<Key, Func, Tuple, thread_policy>> task;

	{
		unsigned long long next_wakeup_time;

		auto now = std::chrono::steady_clock::now();

		std::lock_guard<std::mutex> statuses_lk(statuses_mtx);
		auto it = statuses.find(key);

		Status* status;
		if (it == statuses.end()) {
			// status doesn't exists, initialize:
			auto& status_ref = statuses[key] = {
				nullptr,
				time_point_to_ullong(now + debounce_force_timeout + delay)
			};
			status = &status_ref;
			next_wakeup_time = time_point_to_ullong(now + debounce_timeout + delay);
		} else {
			status = &(it->second);
			if (status->max_wakeup_time) {
				// status exists, so new timeout is of busy type:
				next_wakeup_time = time_point_to_ullong(now + debounce_busy_timeout + delay);
			} else {
				// status exists but is a throttler (max_wakeup_time == 0), initialize:
				status->max_wakeup_time = time_point_to_ullong(now + debounce_force_timeout + delay);
				next_wakeup_time = time_point_to_ullong(now + debounce_timeout + delay);
			}
		}

		if (next_wakeup_time > status->max_wakeup_time) {
			next_wakeup_time = status->max_wakeup_time;
		}

		if (status->task) {
			if (status->task->throttler) {
				// if throttling, use throttler wakeup time:
				if (next_wakeup_time < status->task->wakeup_time) {
					next_wakeup_time = status->task->wakeup_time;
				}
			} else {
				// if task is already waking up some time in the future, do nothing:
				if (next_wakeup_time <= status->task->wakeup_time) {
					return;
				}
			}
			status->task->clear();
		}
		task = std::make_shared<DebouncerTask<Key, Func, Tuple, thread_policy>>(*this, key, std::make_tuple(std::forward<Args>(args)...));
		task->wakeup_time = next_wakeup_time;
		status->task = task;
	}

	this->add(task);
}


template <typename Key, ThreadPolicyType thread_policy = ThreadPolicyType::regular, typename Func>
inline auto
make_debouncer(std::string name, const char* format, size_t num_threads, Func func, std::chrono::milliseconds throttle_time, std::chrono::milliseconds debounce_timeout, std::chrono::milliseconds debounce_busy_timeout, std::chrono::milliseconds debounce_force_timeout)
{
	return Debouncer<Key, decltype(func), typename callable_traits<decltype(func)>::arguments_type, thread_policy>(name, format, num_threads, func, throttle_time, debounce_timeout, debounce_busy_timeout, debounce_force_timeout);
}

template <typename Key, ThreadPolicyType thread_policy = ThreadPolicyType::regular, typename Func>
inline auto
make_unique_debouncer(std::string name, const char* format, size_t num_threads, Func func, std::chrono::milliseconds throttle_time, std::chrono::milliseconds debounce_timeout, std::chrono::milliseconds debounce_busy_timeout, std::chrono::milliseconds debounce_force_timeout)
{
	return std::make_unique<Debouncer<Key, decltype(func), typename callable_traits<decltype(func)>::arguments_type, thread_policy>>(name, format, num_threads, func, throttle_time, debounce_timeout, debounce_busy_timeout, debounce_force_timeout);
}


template <typename Key, ThreadPolicyType thread_policy = ThreadPolicyType::regular, typename Func>
inline auto
make_shared_debouncer(std::string name, const char* format, size_t num_threads, Func func, std::chrono::milliseconds throttle_time, std::chrono::milliseconds debounce_timeout, std::chrono::milliseconds debounce_busy_timeout, std::chrono::milliseconds debounce_force_timeout)
{
	return std::make_shared<Debouncer<Key, decltype(func), typename callable_traits<decltype(func)>::arguments_type, thread_policy>>(name, format, num_threads, func, throttle_time, debounce_timeout, debounce_busy_timeout, debounce_force_timeout);
}
