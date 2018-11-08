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

#include "threadpool.hh"

#include "exception.h"    // for BaseException
#include "likely.h"       // for likely
#include "log.h"          // for L_EXC
#include "string.hh"      // for string::format
#include "thread.hh"      // for Thread, set_thread_name



ThreadPoolThread::ThreadPoolThread() noexcept :
	_pool(nullptr),
	_idx(0)
{}

ThreadPoolThread::ThreadPoolThread(std::size_t idx, ThreadPool* pool) noexcept :
	_pool(pool),
	_idx(idx)
{}


ThreadPool::ThreadPool(const char* format, std::size_t num_threads, std::size_t queue_size)
	: _threads(num_threads),
	  _queue(queue_size),
	  _format(format),
	  _ending(false),
	  _finished(false),
	  _enqueued(0),
	  _running(0),
	  _workers(0)
{
	for (std::size_t idx = 0; idx < num_threads; ++idx) {
		_threads[idx] = ThreadPoolThread(idx, this);
		_threads[idx].start();
	}
}

void
ThreadPoolThread::operator()()
{
	set_thread_name(string::format(_pool->_format, _idx));

	_pool->_workers.fetch_add(1, std::memory_order_relaxed);
	while (!_pool->_finished.load(std::memory_order_acquire)) {
		std::function<void()> task;
		_pool->_queue.wait_dequeue(task);
		if likely(task != nullptr) {
			_pool->_running.fetch_add(1, std::memory_order_relaxed);
			_pool->_enqueued.fetch_sub(1, std::memory_order_release);
			try {
				task();
			} catch (const BaseException& exc) {
				L_EXC("Task died with an unhandled exception: %s", *exc.get_context() ? exc.get_context() : "Unkown BaseException!");
			} catch (const Xapian::Error& exc) {
				L_EXC("Task died with an unhandled exception: %s", exc.get_description());
			} catch (const std::exception& exc) {
				L_EXC("Task died with an unhandled exception: %s", *exc.what() != 0 ? exc.what() : "Unkown std::exception!");
			} catch (...) {
				std::exception exc;
				L_EXC("Task died with an unhandled exception: Unkown exception!");
			}
			_pool->_running.fetch_sub(1, std::memory_order_release);
		} else if (_pool->_ending.load(std::memory_order_acquire)) {
			break;
		}
	}
	_pool->_workers.fetch_sub(1, std::memory_order_relaxed);
}


ThreadPool::~ThreadPool()
{
	finish();
	join();
}

void
ThreadPool::clear() {
	std::function<void()> task;
	while (_queue.try_dequeue(task)) {
		if likely(task != nullptr) {
			_enqueued.fetch_sub(1, std::memory_order_relaxed);
		}
	}
}

// Return size of the tasks queue
std::size_t
ThreadPool::size()
{
	return _enqueued.load(std::memory_order_relaxed);
}

std::size_t
ThreadPool::running_size()
{
	return _running.load(std::memory_order_relaxed);
}

std::size_t
ThreadPool::threadpool_capacity()
{
	return _threads.capacity();
}

std::size_t
ThreadPool::threadpool_size()
{
	return _threads.size();
}

std::size_t
ThreadPool::threadpool_workers()
{
	return _workers.load(std::memory_order_relaxed);
}

bool
ThreadPool::join(std::chrono::milliseconds timeout)
{
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


void
ThreadPool::end()
{
	if (!_ending.exchange(true, std::memory_order_release)) {
		for (std::size_t idx = 0; idx < _threads.size(); ++idx) {
			_queue.enqueue(nullptr);
		}
	}
}

void
ThreadPool::finish()
{
	if (!_finished.exchange(true, std::memory_order_release)) {
		for (std::size_t idx = 0; idx < _threads.size(); ++idx) {
			_queue.enqueue(nullptr);
		}
	}
}

bool
ThreadPool::finished()
{
	return _finished.load(std::memory_order_relaxed);
}
