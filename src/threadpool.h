/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <cassert>       // for assert
#include <future>        // for future
#include <stdexcept>     // for logic_error
#include <string>        // for string
#include <thread>        // for thread
#include <tuple>         // for tuple_size, tuple_cat
#include <vector>        // for vector

#include "exception.h"   // for Exception
#include "log.h"         // for L_DEBUG, L_EXC, L_NOTHING
#include "queue.h"       // for Queue
#include "utils.h"       // for set_thread_name


#ifndef L_THREADPOOL
#define L_THREADPOOL_DEFINED
#define L_THREADPOOL(args...)
#endif

#ifndef L_EXC
#define L_EXC_DEFINED
#define L_EXC(args...)
#endif


template<typename F, typename Tuple, std::size_t... I>
static constexpr auto apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
	return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}


template<typename F, typename Tuple>
static constexpr auto apply(F&& f, Tuple&& t) {
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
		auto operator()(Args&&... args) {
			return f(std::forward<Args>(args)...);
		}
	};

public:
	template<typename Fun, typename = std::enable_if_t<!std::is_copy_constructible<Fun>::value && !std::is_copy_assignable<Fun>::value>>
	explicit function_mo(Fun fun) : std::function<F>(impl_move<Fun>(std::move(fun))) { }

	function_mo() = default;
	function_mo(function_mo&&) = default;
	function_mo& operator =(function_mo&&) = default;
	function_mo(const function_mo&) = delete;
	function_mo& operator =(const function_mo&) = delete;

	using std::function<F>::operator();

	virtual void get() { }

	virtual ~function_mo() = default;
};


template<typename F, typename R>
class packed_function_mo : public function_mo<F> {
	R r;

public:
	template<typename Fun, typename = std::enable_if_t<!std::is_copy_constructible<Fun>::value && !std::is_copy_assignable<Fun>::value>>
	packed_function_mo(Fun fun, R res) : function_mo<F>(std::move(fun)), r(std::move(res)) { }

	packed_function_mo() = default;
	packed_function_mo(packed_function_mo&&) = default;
	packed_function_mo& operator =(packed_function_mo&&) = default;
	packed_function_mo(const packed_function_mo&) = delete;
	packed_function_mo& operator =(const packed_function_mo&) = delete;

	void get() override {
		r.get();
	}
};


template<typename... Params>
class TaskQueue;


/*
 *   Base task for Tasks
 *   run() should be overloaded and expensive calculations done there
 */

template<typename... Params>
class Task {
	friend TaskQueue<Params...>;

public:
	virtual void run(Params...) = 0;

	virtual ~Task() = default;
};


template<typename... Params>
class TaskQueue {
protected:
	queue::Queue<std::unique_ptr<function_mo<void(Params...)>>> tasks;

public:
	// Wait for the threads to finish
	virtual ~TaskQueue() {
		finish();
	}

	// Function that retrieves a task from a fifo queue, runs it and deletes it
	template<typename... Params_>
	bool call(Params_&&... params) {
		std::unique_ptr<function_mo<void(Params...)>> task;
		if (TaskQueue::tasks.pop(task, 0)) {
			(*task)(std::forward<Params_>(params)...);
			task->get();
			return true;
		} else {
			return false;
		}
	}

	// Enqueues any function to be executed
	template<typename F, typename... Args>
	auto enqueue(F&& f, Args&&... args) -> std::shared_future<std::result_of_t<F(Params..., Args...)>> {
		auto packed_task = std::packaged_task<std::result_of_t<F(Params..., Args...)>(Params...)>
		(
			[f = std::forward<F>(f), t = std::make_tuple(std::forward<Args>(args)...)] (Params... params) mutable {
				return apply(std::move(f), std::tuple_cat(std::make_tuple(std::move(params)...), std::move(t)));
			}
		);
		auto res = packed_task.get_future().share();
		if (!tasks.push(std::make_unique<packed_function_mo<void(Params...), std::shared_future<std::result_of_t<F(Params..., Args...)>>>>(std::move(packed_task), res))) {
			throw std::logic_error("Unable to enqueue task");
		}
		return res;
	}

	// Enqueues a Task object to be executed
	auto enqueue(std::shared_ptr<Task<Params...>> nt) {
		return enqueue([nt = std::move(nt)](Params... params) mutable {
			nt->run(std::move(params)...);
			nt.reset();
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
	std::function<void(size_t)> worker;
	std::atomic_size_t running_tasks;
	std::atomic_bool full_pool;
	std::string format;
	std::vector<std::thread> threads;
	std::mutex mtx;

	// Function that retrieves a task from a fifo queue, runs it and deletes it
	template<typename... Params_>
	void _worker(size_t idx, Params_&&... params) {
		char name[100];
		snprintf(name, sizeof(name), format.c_str(), idx);
		set_thread_name(std::string(name));
		std::unique_ptr<function_mo<void(Params...)>> task;

		L_THREADPOOL(this, "Worker %s started! (size: %lu, capacity: %lu)", name, threadpool_size(), threadpool_capacity());
		while (TaskQueue<Params...>::tasks.pop(task)) {
			++running_tasks;
			try {
				(*task)(std::forward<Params_>(params)...);
				task->get();
			} catch (const BaseException& exc) {
				auto exc_context = exc.get_context();
				L_EXC(this, "Task died with an unhandled exception: %s", *exc_context ? exc_context : "Unkown Exception!");
			} catch (const Xapian::Error& exc) {
				auto exc_msg = exc.get_msg().c_str();
				L_EXC(this, "Task died with an unhandled exception: %s", *exc_msg ? exc_msg : "Unkown Xapian::Error!");
			} catch (const std::exception& exc) {
				auto exc_msg = exc.what();
				L_EXC(this, "Task died with an unhandled exception: %s", *exc_msg ? exc_msg : "Unkown std::exception!");
			} catch (...) {
				std::exception exc;
				L_EXC(this, "Task died with an unhandled exception: Unkown!");
			}
			--running_tasks;
		}
		L_THREADPOOL(this, "Worker %s ended.", name);
	}

	bool spawn_workers() {
		if (full_pool) return false;
		auto enqueued = TaskQueue<Params...>::size();
		if (enqueued) {
			std::lock_guard<std::mutex> lk(mtx);
			auto threads_capacity = threads.capacity();
			auto threads_size = threads.size();
			while (enqueued-- > (threads_size - running_tasks) * 2 / 3 && threads_size < threads_capacity) {
				threads.emplace_back(worker, threads.size());
				threads_size = threads.size();
			}
			if (threads_size == threads_capacity) {
				full_pool = true;
			}
		}
		return false;
	}

public:
	// Allocate a thread pool and set them to work trying to get tasks
	template<typename... Params_>
	ThreadPool(const std::string format_, size_t num_threads, Params_&&... params)
		: worker([&](size_t idx) {
			ThreadPool::_worker<Params_...>(idx, std::forward<Params_>(params)...);
		}),
		/*  OLD BUGGY GCC COMPILERS NEED TO USE std::bind, AS IN:
		: worker(std::bind([this](size_t idx, Params_&&... params) {
			ThreadPool::_worker<Params_...>(idx, std::forward<Params_>(params)...);
		}, format, std::placeholders::_1, std::forward<Params_>(params)...)),*/
		running_tasks(0),
		full_pool(false),
		format(format_) {
		threads.reserve(num_threads);
	}

	// Wait for the threads to finish
	~ThreadPool() {
		join();
	}

	template<typename... Args>
	auto enqueue(Args&&... args) {
		auto ret = TaskQueue<Params...>::enqueue(std::forward<Args>(args)...);
		spawn_workers();
		return ret;
	}

	// Wait for all threads
	void join() {
		std::lock_guard<std::mutex> lk(mtx);
		for (auto& thread : threads) {
			try {
				if (thread.joinable()) {
					thread.join();
				}
			} catch (const std::system_error&) { }
		}
		threads.clear();
	}

	size_t threadpool_capacity() {
		std::lock_guard<std::mutex> lk(mtx);
		return threads.capacity();
	}

	size_t threadpool_size() {
		std::lock_guard<std::mutex> lk(mtx);
		return threads.size();
	}

	size_t running_size() {
		return running_tasks.load();
	}
};


#ifdef L_THREADPOOL_DEFINED
#undef L_THREADPOOL_DEFINED
#undef L_THREADPOOL
#endif

#ifdef L_EXC_DEFINED
#undef L_EXC_DEFINED
#undef L_EXC
#endif
