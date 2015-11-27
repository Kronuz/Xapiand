/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "queue.h"
#include "utils.h"

#include <iostream>
#include <string>
#include <tuple>
#include <thread>
#include <future>
#include <cassert>
#include <vector>
#include <vector>


template<typename F, typename Tuple, std::size_t... I>
static inline constexpr decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
	return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}


template<typename F, typename Tuple>
static inline constexpr decltype(auto) apply(F&& f, Tuple&& t) {
	using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
	return apply_impl(std::forward<F>(f), std::forward<Tuple>(t), Indices{});
}


// Custom function wrapper that can handle move-only types like std::packaged_task.
template<typename F>
class function_mo : private std::function<F> {
	template<typename Fun>
	struct impl_move {
		Fun f;

		impl_move(Fun f_) : f(std::move(f_)) { }
		impl_move(impl_move&&) = default;
		impl_move& operator =(impl_move&&) = default;
		impl_move(const impl_move&) { assert(false); };
		impl_move& operator =(const impl_move&) { assert(false); };

		template<typename... Args>
		decltype(auto) operator()(Args&&... args) {
			return f(std::forward<Args>(args)...);
		}
	};

public:
	template<typename Fun, typename = std::enable_if_t<!std::is_copy_constructible<Fun>::value &&
	!std::is_copy_assignable<Fun>::value>>
	function_mo(Fun fun) : std::function<F>(impl_move<Fun>(std::move(fun))) { }

	function_mo() = default;
	function_mo(function_mo&&) = default;
	function_mo& operator =(function_mo&&) = default;
	function_mo(const function_mo&) = delete;
	function_mo& operator =(const function_mo&) = delete;

	using std::function<F>::operator();
};


//
//   Base task for Tasks
//   run() should be overloaded and expensive calculations done there
//

template<typename... Params>
class TaskQueue;


template<typename... Params>
class Task {
	friend TaskQueue<Params...>;

	virtual void run(Params...) = 0;
};


template<typename... Params>
class TaskQueue {
protected:
	queue::Queue<function_mo<void(Params...)>> tasks;

public:
	// Wait for the threads to finish
	virtual ~TaskQueue() {
		finish();
	}

	// Function that retrieves a task from a fifo queue, runs it and deletes it
	template<typename... Params_>
	bool call(Params_&&... params) {
		function_mo<void(Params...)> task;
		if (TaskQueue::tasks.pop(task, 0)) {
			task(std::forward<Params_>(params)...);
			return true;
		} else {
			return false;
		}
	}

	// Enqueues any function to be executed
	template<typename F, typename... Args>
	auto enqueue(F&& f, Args&&... args) -> std::future<std::result_of_t<F(Params..., Args...)>> {
		auto t = std::make_tuple(std::forward<Args>(args)...);
		auto task = std::packaged_task<std::result_of_t<F(Params..., Args...)>(Params...)>
		(
			[f = std::forward<F>(f), t = std::move(t)] (Params... params) mutable {
				return apply(std::move(f), std::tuple_cat(std::make_tuple(std::move(params)...), std::move(t)));
			}
		 );
		auto res = task.get_future();
		bool pushed = tasks.push(std::move(task));
		assert(pushed);
		return res;
	}

	// Enqueues a Task object to be executed
	decltype(auto) enqueue(std::shared_ptr<Task<Params...>> nt) {
		return enqueue([nt = std::move(nt)](Params... params) {
			nt->run(std::move(params)...);
		});
	}

	void clear() {
		tasks.clear();
	}

	// Tell the tasks to finish so all threads exit as soon as possible
	void finish() {
		tasks.finish();
	}

	// Flag the pool as ending, so all threads exit as soon as all queued tasks end
	void end() {
		tasks.end();
	}

	// Return size of the tasks queue
	size_t size() {
		return tasks.size();
	}

};


template<typename... Params>
class ThreadPool : public TaskQueue<Params...> {
	std::vector<std::thread> threads;

	// Function that retrieves a task from a fifo queue, runs it and deletes it
	template<typename... Params_>
	void worker(const std::string& format, size_t idx, Params_&&... params) {
		char name[100];
		snprintf(name, sizeof(name), format.c_str(), idx);
		set_thread_name(std::string(name));
		function_mo<void(Params...)> task;
		while (TaskQueue<Params...>::tasks.pop(task)) {
			task(std::forward<Params_>(params)...);
		}
	}

public:
	// Allocate a thread pool and set them to work trying to get tasks
	template<typename... Params_>
	ThreadPool(const std::string& format, size_t num_threads, Params_&&... params) {
		threads.reserve(num_threads);
		for (size_t idx = 0; idx < num_threads; ++idx) {
			threads.emplace_back(&ThreadPool::worker<Params_...>, this, format, idx, std::forward<Params_>(params)...);
		}
	}

	// Wait for the threads to finish
	~ThreadPool() {
		join();
	}

	// Wait for all threads
	void join() {
		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}
	}
};
