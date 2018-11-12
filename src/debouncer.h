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

#include <chrono>                            // for std::chrono
#include <functional>                        // for std::function
#include <mutex>                             // for std::mutex
#include <unordered_map>                     // for std::unordered_map
#include <utility>                           // for std::move

#include "callable_traits.hh"                // for callable_traits
#include "log.h"                             // for L_OBJ, L_CALL, L_DEBUG, L_WARNING
#include "repr.hh"                           // for repr
#include "scheduler.h"                       // for SchedulerTask, SchedulerThread
#include "string.hh"                         // for string::from_delta
#include "time_point.hh"                     // for time_point_to_ullong


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
class DebouncerTask;


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
class Debouncer : public Scheduler {
	friend DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>;

	static constexpr auto debounce_timeout = std::chrono::milliseconds(DT);
	static constexpr auto debounce_busy_timeout = std::chrono::milliseconds(DBT);
	static constexpr auto debounce_force_timeout = std::chrono::milliseconds(DFT);

	struct Status {
		std::shared_ptr<DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>> task;
		unsigned long long max_wakeup_time;
	};

	std::mutex statuses_mtx;
	std::unordered_map<Key, Status> statuses;

	Func func;

	void release(Key key);

public:
	Debouncer(std::string name, const char* format, size_t num_threads, Func func) :
		Scheduler(name, format, num_threads),
		func(std::move(func)) {}

	template <typename... Args>
	void debounce(Key key, Args&&... args);

	template <typename... Args>
	void delayed_debounce(std::chrono::milliseconds delay, Key key, Args&&... args);
};


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
class DebouncerTask : public ScheduledTask {
	friend Debouncer<Key, DT, DBT, DFT, Func, Tuple>;

	Debouncer<Key, DT, DBT, DFT, Func, Tuple>& debouncer;

	bool forced;
	Key key;
	Tuple args;

public:
	DebouncerTask(Debouncer<Key, DT, DBT, DFT, Func, Tuple>& debouncer, bool forced, Key key, Tuple args);

	void run() override;

	std::string __repr__() const override {
		return ScheduledTask::__repr__("DebouncerTask");
	}
};


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
inline
DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>::DebouncerTask(Debouncer<Key, DT, DBT, DFT, Func, Tuple>& debouncer, bool forced, Key key, Tuple args) :
	debouncer(debouncer),
	forced(forced),
	key(std::move(key)),
	args(std::move(args))
{
}


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
inline void
DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>::run()
{
	L_CALL("DebouncerTask::run()");
	L_DEBUG_HOOK("DebouncerTask::run", "DebouncerTask::run()");

	debouncer.release(key);

	std::apply(debouncer.func, args);
}


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
inline void
Debouncer<Key, DT, DBT, DFT, Func, Tuple>::release(Key key)
{
	std::lock_guard<std::mutex> statuses_lk(statuses_mtx);
	statuses.erase(key);
}


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
template <typename... Args>
inline void
Debouncer<Key, DT, DBT, DFT, Func, Tuple>::debounce(Key key, Args&&... args)
{
	L_CALL("Debouncer::debounce(<key>, ...)");

	delayed_debounce(0ms, key, std::forward<Args>(args)...);
}


template <typename Key, unsigned long long DT, unsigned long long DBT, unsigned long long DFT, typename Func, typename Tuple>
template <typename... Args>
inline void
Debouncer<Key, DT, DBT, DFT, Func, Tuple>::delayed_debounce(std::chrono::milliseconds delay, Key key, Args&&... args)
{
	L_CALL("Debouncer::delayed_debounce(<delay>, <key>, ...)");

	std::shared_ptr<DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>> task;
	unsigned long long next_wakeup_time;

	{
		auto now = std::chrono::system_clock::now();

		std::lock_guard<std::mutex> statuses_lk(statuses_mtx);
		auto it = statuses.find(key);

		Status* status;
		if (it == statuses.end()) {
			auto& status_ref = statuses[key] = {
				nullptr,
				time_point_to_ullong(now + debounce_force_timeout + delay)
			};
			status = &status_ref;
			next_wakeup_time = time_point_to_ullong(now + debounce_timeout + delay);
		} else {
			status = &(it->second);
			next_wakeup_time = time_point_to_ullong(now + debounce_busy_timeout + delay);
		}

		bool forced;
		if (next_wakeup_time > status->max_wakeup_time) {
			next_wakeup_time = status->max_wakeup_time;
			forced = true;
		} else {
			forced = false;
		}

		if (status->task) {
			if (status->task->wakeup_time == next_wakeup_time) {
				return;
			}
			status->task->clear();
		}
		status->task = std::make_shared<DebouncerTask<Key, DT, DBT, DFT, Func, Tuple>>(*this, forced, key, std::make_tuple(std::forward<Args>(args)...));
		task = status->task;
	}

	add(task, next_wakeup_time);
}


template <typename Key, unsigned long long DT = 1000, unsigned long long DBT = 3000, unsigned long long DFT = 9000, typename Func>
inline auto
make_debouncer(std::string name, const char* format, size_t num_threads, Func func)
{
	return Debouncer<Key, DT, DBT, DFT, decltype(func), typename callable_traits<decltype(func)>::arguments_type>(name, format, num_threads, func);
}
