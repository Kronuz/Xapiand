/*
 * Copyright (c) 2018 Dubalu LLC
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

#include <atomic>                             // for std::atomic_bool
#include <condition_variable>                 // for std::condition_variable
#include <mutex>                              // for std::mutex

#include "thread.hh"                          // for Thread, ThreadPolicyType::*


// Periodic database/schema cleanup on its own background thread.
//
// A single self-contained job: every 60 seconds run database_pool->cleanup()
// and schemas->cleanup(). It never shared the manager's event loop -- it only
// ever needed "do this every 60s on a separate thread" -- so it drops the
// Worker/libev machinery (its own ev::loop + ev::timer) in favour of the
// ev-free Thread<> base and a plain condition-variable wait. The manager
// drives its lifecycle explicitly: run() -> finish() -> join().
class DatabaseCleanup : public Thread<DatabaseCleanup, ThreadPolicyType::regular> {
	std::mutex mtx;
	std::condition_variable wakeup;
	std::atomic_bool finished;

public:
	DatabaseCleanup();

	~DatabaseCleanup() noexcept;

	const char* name() const noexcept {
		return "DBCL";
	}

	void operator()();

	void finish();
};
