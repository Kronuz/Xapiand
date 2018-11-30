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

#include <chrono>                // for std::chrono
#include <cstddef>               // for std::size_t
#include <functional>            // for std::function
#include <future>                // for std::future, std::packaged_task
#include <memory>                // for std::shared_ptr, std::unique_ptr
#include <tuple>                 // for std::make_tuple, std::apply
#include <vector>                // for std::vector

#include "blocking_concurrent_queue.h"
#include "cassert.h"             // for ASSERT
#include "likely.h"              // for likely, unlikely
#include "log.h"                 // for L_EXC
#include "string.hh"             // for string::format
#include "thread.hh"             // for Thread


using namespace std::chrono_literals;


////////////////////////////////////////////////////////////////////////////////

/* Since std::packaged_task cannot be copied, and std::function requires it can,
 * we add a dummy copy constructor to std::packaged_task. (We need to make sure
 * we actually really never copy the object though!)
 * [https://stackoverflow.com/q/39996132/167522]
 */
template <typename Result>
class PackagedTask : public std::packaged_task<Result> {
  public:
	PackagedTask() noexcept
	  : std::packaged_task<Result>() {}

	template <typename F>
	explicit PackagedTask(F&& f)
	  : std::packaged_task<Result>(std::forward<F>(f)) {}

	PackagedTask(const PackagedTask& /*unused*/) noexcept {
		// Adding this borks the compile
		ASSERT(false);  // but should never be called!
	}

	PackagedTask(PackagedTask&& other) noexcept = default;

	PackagedTask& operator=(PackagedTask&& other) noexcept = default;
};


////////////////////////////////////////////////////////////////////////////////

template <typename TaskType, ThreadPolicyType thread_policy>
class ThreadPool;

template <typename TaskType, ThreadPolicyType thread_policy>
class ThreadPoolThread : public Thread<ThreadPoolThread<TaskType, thread_policy>, thread_policy> {
	ThreadPool<TaskType, thread_policy>* _pool;
	std::string _name;

public:
	ThreadPoolThread() noexcept :
		_pool(nullptr) {}

	ThreadPoolThread(std::size_t idx, ThreadPool<TaskType, thread_policy>* pool) noexcept :
		_pool(pool),
		_name(string::format(pool->_format, idx)) {}

	void operator()();

	const std::string& name() const noexcept;
};


template <typename>
struct TaskWrapper;


template <>
struct TaskWrapper<std::function<void()>> {
	std::function<void()> t;

	TaskWrapper() {}

	template <typename T>
	TaskWrapper(T&& t) :
		t(std::forward<T>(t)) {}

	void operator()() {
		t.operator()();
	}

	operator bool() const noexcept {
		return !!t;
	}
};


template <typename P>
struct TaskWrapper<std::shared_ptr<P>> {
	std::shared_ptr<P> t;

	TaskWrapper() {}

	template <typename T>
	TaskWrapper(T&& t) :
		t(std::forward<T>(t)) {}

	void operator()() {
		if (t) {
			t->operator()();
		}
	}

	operator bool() const noexcept {
		return !!t;
	}
};


template <typename P>
struct TaskWrapper<std::unique_ptr<P>> {
	std::unique_ptr<P> t;

	TaskWrapper() {}

	template <typename T>
	TaskWrapper(T&& t) :
		t(std::forward<T>(t)) {}

	void operator()() {
		if (t) {
			t->operator()();
		}
	}

	operator bool() const noexcept {
		return !!t;
	}
};


template <typename TaskType = std::function<void()>, ThreadPolicyType thread_policy = ThreadPolicyType::regular>
class ThreadPool {
	friend ThreadPoolThread<TaskType, thread_policy>;

	std::vector<ThreadPoolThread<TaskType, thread_policy>> _threads;
	BlockingConcurrentQueue<TaskWrapper<TaskType>> _queue;

	const char* _format;

	std::atomic_bool _ending;
	std::atomic_bool _finished;
	std::atomic_size_t _enqueued;
	std::atomic_size_t _running;
	std::atomic_size_t _workers;

public:
	ThreadPool(const char* format, std::size_t num_threads, std::size_t queue_size = 1000) :
		_threads(num_threads),
		_queue(queue_size),
		_format(format),
		_ending(false),
		_finished(false),
		_enqueued(0),
		_running(0),
		_workers(0) {
		for (std::size_t idx = 0; idx < num_threads; ++idx) {
			_threads[idx] = ThreadPoolThread<TaskType, thread_policy>(idx, this);
			_threads[idx].run();
		}
	}

	~ThreadPool() {
		finish();
		join();
	}


	void clear() {
		TaskWrapper<TaskType> task;
		while (_queue.try_dequeue(task)) {
			if likely(task) {
				_enqueued.fetch_sub(1, std::memory_order_relaxed);
			}
		}
	}

	std::size_t size() {
		return _enqueued.load(std::memory_order_relaxed);
	}

	std::size_t running_size() {
		return _running.load(std::memory_order_relaxed);
	}

	std::size_t threadpool_capacity() {
		return _threads.capacity();
	}

	std::size_t threadpool_size() {
		return _threads.size();
	}

	std::size_t threadpool_workers() {
		return _workers.load(std::memory_order_relaxed);
	}

	bool join(std::chrono::milliseconds timeout = 60s) {
		bool ret = true;
		// Divide timeout among number of running worker threads
		// to give each thread the chance to "join".
		auto threadpool_workers = _workers.load(std::memory_order_relaxed);
		if (!threadpool_workers) {
			threadpool_workers = 1;
		}
		auto single_timeout = timeout / threadpool_workers;
		for (auto& _thread : _threads) {
			auto wakeup = std::chrono::system_clock::now() + single_timeout;
			if (!_thread.join(wakeup)) {
				ret = false;
			}
		}
		return ret;
	}

	// Flag the pool as ending, so all threads exit as soon as all queued tasks end
	void end() {
		if (!_ending.exchange(true, std::memory_order_release)) {
			for (std::size_t idx = 0; idx < _threads.size(); ++idx) {
				_queue.enqueue(nullptr);
			}
		}
	}

	// Tell the tasks to finish so all threads exit as soon as possible
	void finish() {
		if (!_finished.exchange(true, std::memory_order_release)) {
			for (std::size_t idx = 0; idx < _threads.size(); ++idx) {
				_queue.enqueue(nullptr);
			}
		}
	}

	template <typename Func, typename... Args>
	auto package(Func&& func, Args&&... args) {
		auto packaged_task = PackagedTask<std::result_of_t<Func(Args...)>()>([
			func = std::forward<Func>(func),
			args = std::make_tuple(std::forward<Args>(args)...)
		]() mutable {
			return std::apply(std::move(func), std::move(args));
		});
		return packaged_task;
	}

	template <typename It>
	auto enqueue_bulk(It itemFirst, std::size_t count) {
		_enqueued.fetch_add(count, std::memory_order_release);
		if unlikely(!_queue.enqueue_bulk(std::forward<It>(itemFirst), count)) {
			_enqueued.fetch_sub(count, std::memory_order_release);
			return false;
		}
		return true;
	}

	template <typename Func>
	auto enqueue(Func&& func) {
		_enqueued.fetch_add(1, std::memory_order_release);
		if unlikely(!_queue.enqueue(std::forward<Func>(func))) {
			_enqueued.fetch_sub(1, std::memory_order_release);
			return false;
		}
		return true;
	}

	template <typename Func, typename... Args>
	auto async(Func&& func, Args&&... args) {
		auto packaged_task = package(std::forward<Func>(func), std::forward<Args>(args)...);
		auto future = packaged_task.get_future();
		if unlikely(!enqueue([packaged_task = std::move(packaged_task)]() mutable {
			packaged_task();
		})) {
			throw std::runtime_error("Cannot enqueue task to threadpool");
		}
		return future;
	}

	bool finished() {
		return _finished.load(std::memory_order_relaxed);
	}
};


template <typename TaskType, ThreadPolicyType thread_policy>
inline const std::string&
ThreadPoolThread<TaskType, thread_policy>::name() const noexcept
{
	return _name;
}


template <typename TaskType, ThreadPolicyType thread_policy>
inline void
ThreadPoolThread<TaskType, thread_policy>::operator()()
{
	_pool->_workers.fetch_add(1, std::memory_order_relaxed);
	while (!_pool->_finished.load(std::memory_order_acquire)) {
		TaskWrapper<TaskType> task;
		_pool->_queue.wait_dequeue(task);
		if likely(task) {
			_pool->_running.fetch_add(1, std::memory_order_relaxed);
			_pool->_enqueued.fetch_sub(1, std::memory_order_release);
			try {
				task();
			} catch (...) {
				L_EXC("ERROR: Task died with an unhandled exception");
			}
			_pool->_running.fetch_sub(1, std::memory_order_release);
		} else if (_pool->_ending.load(std::memory_order_acquire)) {
			break;
		}
	}
	_pool->_workers.fetch_sub(1, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////////////////////////

template <typename>
class TaskQueue;

template <typename R, typename... Args>
class TaskQueue<R(Args...)> {
	using Queue = ConcurrentQueue<std::packaged_task<R(Args...)>>;
	Queue _queue;

public:
	template <typename Func>
	auto enqueue(Func&& func) {
		auto packaged_task = std::packaged_task<R(Args...)>(std::forward<Func>(func));
		auto future = packaged_task.get_future();
		_queue.enqueue(std::move(packaged_task));
		return future;
	}

	bool call(Args&&... args) {
		std::packaged_task<R(Args...)> task;
		if (_queue.try_dequeue(task)) {
			task(std::forward<Args>(args)...);
			return true;
		}
		return false;
	}

	std::size_t clear() {
		std::array<std::packaged_task<R(Args...)>, Queue::BLOCK_SIZE> tasks;
		std::size_t cleared = 0;
		while (auto dequeued = _queue.try_dequeue_bulk(tasks.begin(), tasks.size())) {
			cleared += dequeued;
		}
		return cleared;
	}
};
