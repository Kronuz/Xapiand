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

#include "thread.hh"

#include "config.h"              // for HAVE_PTHREADS, HAVE_PTHREAD_SETNAME_NP

#include <mutex>                 // for std::mutex, std::lock_guard
#include <tuple>                 // for std::forward_as_tuple
#include <unordered_map>         // for std::unordered_map

#ifdef HAVE_PTHREADS
#include <pthread.h>             // for pthread_self
#endif

#include "stringified.hh"        // for stringified

void
Thread::start()
{
	if (!_started.exchange(true)) {
		_thread = std::thread(&Thread::_runner, this);
		_thread.detach();
	}
}

bool
Thread::join(const std::chrono::time_point<std::chrono::system_clock>& wakeup)
{
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

void
Thread::_runner()
{
	try {
		(*this)();
		_promise.set_value();
	} catch (...) {
		try {
			// store anything thrown in the promise
			_promise.set_exception(std::current_exception());
		} catch(...) {} // set_exception() may throw too
	}
}


static std::unordered_map<std::thread::id, std::string> thread_names;
static std::mutex thread_names_mutex;


void set_thread_name(const std::string& name) {
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__linux__)
	pthread_setname_np(pthread_self(), stringified(name).c_str());
	// pthread_setname_np(pthread_self(), stringified(name).c_str(), nullptr);
#elif defined(HAVE_PTHREAD_SETNAME_NP)
	pthread_setname_np(stringified(name).c_str());
#elif defined(HAVE_PTHREAD_SET_NAME_NP)
	pthread_set_name_np(pthread_self(), stringified(name).c_str());
#endif
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	thread_names.emplace(std::piecewise_construct,
		std::forward_as_tuple(std::this_thread::get_id()),
		std::forward_as_tuple(name));
}


const std::string& get_thread_name(std::thread::id thread_id) {
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	auto thread = thread_names.find(thread_id);
	if (thread == thread_names.end()) {
		static std::string _ = "???";
		return _;
	}
	return thread->second;
}


const std::string& get_thread_name() {
	return get_thread_name(std::this_thread::get_id());
}
