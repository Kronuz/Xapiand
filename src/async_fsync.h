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

#include "xapiand.h"

#include "worker.h"
#include "threadpool.h"

#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <condition_variable>


class XapiandManager;


class AsyncFsync : public Task<>, public Worker {
	struct Status {
		int mode;
		std::chrono::time_point<std::chrono::system_clock> max_commit_time;
		std::chrono::time_point<std::chrono::system_clock> commit_time;
		std::chrono::time_point<std::chrono::system_clock> next_wakeup_time();
		Status() : mode(0) {}
	};

	static std::mutex mtx;
	static std::mutex statuses_mtx;
	static std::condition_variable wakeup_signal;
	static std::unordered_map<int, Status> statuses;
	static std::atomic<std::time_t> next_wakeup_time;

	std::atomic_bool running;

	void destroy_impl() override;
	void shutdown_impl(time_t asap, time_t now) override;

	static int _fsync(int fd, bool full_fsync);

public:
	AsyncFsync(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_);
	~AsyncFsync();

	void run() override;

	static inline int fsync(int fd) {
		return _fsync(fd, false);
	}

	static inline int full_fsync(int fd) {
		return _fsync(fd, true);
	}
};
