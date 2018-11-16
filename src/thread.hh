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

#include <atomic>                // for std::atomic_bool
#include <chrono>                // for std::chrono
#include <future>                // for std::future, std::promise
#include <thread>                // for std::thread


using namespace std::chrono_literals;


////////////////////////////////////////////////////////////////////////////////


void set_thread_afinity(uint64_t afinity_map);

void set_thread_name(const std::string& name);

const std::string& get_thread_name(std::thread::id thread_id);

const std::string& get_thread_name();


////////////////////////////////////////////////////////////////////////////////


template <typename ThreadImpl, uint64_t Affinity>
class Thread {
	std::thread _thread;
	std::promise<void> _promise;
	std::future<void> _future;

	std::atomic_bool _started;
	std::atomic_bool _joined;

	void _runner() {
		set_thread_afinity(Affinity);
		try {
			static_cast<ThreadImpl*>(this)->operator()();
			_promise.set_value();
		} catch (...) {
			try {
				// store anything thrown in the promise
				_promise.set_exception(std::current_exception());
			} catch(...) {} // set_exception() may throw too
		}
	}

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

	void start() {
		if (!_started.exchange(true)) {
			_thread = std::thread(&Thread::_runner, this);
			_thread.detach();
		}
	}

	bool join(const std::chrono::time_point<std::chrono::system_clock>& wakeup) {
		if (_started && !_joined) {
			std::future_status status;
			do {
				status = _future.wait_until(wakeup);
				if (status == std::future_status::timeout) {
					return false;
				}
			} while (status != std::future_status::ready);
			if (!_joined.exchange(true)) {
				_future.get(); // rethrow any exceptions
			}
		}
		return true;
	}

	bool join(std::chrono::milliseconds timeout = 60s) {
		return join(std::chrono::system_clock::now() + timeout);
	}
};
