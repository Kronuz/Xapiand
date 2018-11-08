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

#include <chrono>         // for std::chrono
#include <cstddef>        // for std::size_t
#include <functional>     // for std::function
#include <future>         // for std::packaged_task
#include <tuple>          // for std::make_tuple, std::apply
#include <vector>         // for std::vector

#include "cassert.hh"     // for assert

#include "blocking_concurrent_queue.h"
#include "likely.h"       // for unlikely
#include "thread.hh"      // for Thread


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
		assert(false);  // but should never be called!
	}

	PackagedTask(PackagedTask&& other) noexcept = default;

	PackagedTask& operator=(PackagedTask&& other) noexcept = default;
};


class ThreadPool;
class ThreadPoolThread : public Thread {
	ThreadPool* _pool;
	std::size_t _idx;

public:
	ThreadPoolThread() noexcept;

	ThreadPoolThread(std::size_t idx, ThreadPool* pool) noexcept;

	void operator()() override;
};


class ThreadPool {
	friend ThreadPoolThread;

	std::vector<ThreadPoolThread> _threads;
	BlockingConcurrentQueue<std::function<void()>> _queue;

	const char* _format;

	std::atomic_bool _ending;
	std::atomic_bool _finished;
	std::atomic_size_t _enqueued;
	std::atomic_size_t _running;
	std::atomic_size_t _workers;

public:
	ThreadPool(const char* format, std::size_t num_threads, std::size_t queue_size = 1000);

	~ThreadPool();

	void clear();
	std::size_t size();
	std::size_t running_size();
	std::size_t threadpool_capacity();
	std::size_t threadpool_size();

	bool join(const std::chrono::time_point<std::chrono::system_clock>& wakeup);

	template <typename T, typename R>
	bool join(std::chrono::duration<T, R> timeout) {
		return join(std::chrono::system_clock::now() + timeout);
	}

	bool join(int timeout = 60000) {
		return join(std::chrono::milliseconds(timeout));
	}

	// Flag the pool as ending, so all threads exit as soon as all queued tasks end
	void end();

	// Tell the tasks to finish so all threads exit as soon as possible
	void finish();

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

	bool finished();
};


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
