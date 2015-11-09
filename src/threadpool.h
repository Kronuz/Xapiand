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

#include <thread>
#include <future>
#include <cassert>
#include <vector>


//
//   Base task for Tasks
//   run() should be overloaded and expensive calculations done there
//
class Task {
	virtual void run() = 0;
	friend class ThreadPool;
};


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


class ThreadPool {
	std::vector<std::thread> threads;
	queue::Queue<function_mo<void()>> tasks;

	// Function that retrieves a task from a fifo queue, runs it and deletes it
	void worker() {
		function_mo<void()> task;
		while (tasks.pop(task)) {
			task();
		}
	}

public:
	// Allocate a thread pool and set them to work trying to get tasks
	ThreadPool(size_t num_threads) {
		threads.reserve(num_threads);
		for (size_t idx = 0; idx < num_threads; ++idx) {
			threads.emplace_back(&ThreadPool::worker, this);
		}
	}

	// Wait for the threads to finish
	~ThreadPool() {
		finish();
		join();
	}

	// Enqueues any function to be executed
	template<typename F, typename... Args>
	auto enqueue(F&& f, Args&&... args) -> std::future<std::result_of_t<F(Args...)>> {
		auto t = std::make_tuple(std::forward<Args>(args)...);
		auto task = std::packaged_task<std::result_of_t<F(Args...)>()>(
			[f = std::forward<F>(f), t = std::move(t)]() mutable {
				return apply(std::move(f), std::move(t));
			});
		auto res = task.get_future();
		tasks.push(std::move(task));
		return res;
	}

	// Enqueues a Task object to be executed
	decltype(auto) enqueue(std::shared_ptr<Task>&& nt) {
		return enqueue([nt = std::move(nt)]() {
			nt->run();
		});
	}

	// Tell the tasks to finish so all threads exit as soon as possible
	void finish() {
		tasks.finish();
	}

	// Flag the pool as ending, so all threads exit as soon as all queued tasks end
	void end() {
		tasks.end();
	}

	// Wait for all threads
	void join() {
		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}
	}

	// Return size of the tasks queue
	size_t size() {
		return tasks.size();
	}
};
