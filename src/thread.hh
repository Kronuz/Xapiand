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

#include <atomic>         // for std::atomic_bool
#include <chrono>         // for std::chrono
#include <future>         // for std::future, std::promise
#include <string>         // for std::string
#include <thread>         // for std::thread


class Thread {
	std::thread _thread;
	std::promise<void> _promise;
	std::future<void> _future;

	std::atomic_bool _started;
	std::atomic_bool _joined;

	void _runner();

public:
	Thread() :
		_future{_promise.get_future()},
		_started{false},
		_joined{false} {};

	Thread(Thread&& other) :
		_thread(std::move(other._thread)),
		_promise(std::move(other._promise)),
		_future(std::move(other._future)),
		_started(other._started.load()),
		_joined(other._joined.load())
	{}

	Thread& operator=(Thread&& other) {
		_thread = std::move(other._thread);
		_promise = std::move(other._promise);
		_future = std::move(other._future);
		_started = other._started.load();
		_joined = other._joined.load();
		return *this;
	}

	virtual ~Thread() = default;

	void start();

	bool join(const std::chrono::time_point<std::chrono::system_clock>& wakeup);

	template <typename T, typename R>
	bool join(std::chrono::duration<T, R> timeout) {
		return join(std::chrono::system_clock::now() + timeout);
	}

	bool join(int timeout = 60000) {
		return join(std::chrono::milliseconds(timeout));
	}

	virtual void operator()() = 0;
};


void set_thread_name(const std::string& name);
const std::string& get_thread_name(std::thread::id thread_id);
const std::string& get_thread_name();
