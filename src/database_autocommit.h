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

#include "threadpool.h"
#include "database.h"
#include "manager.h"

#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <condition_variable>


struct DatabaseCommitStatus {
	std::weak_ptr<const Database> database;
	std::chrono::time_point<std::chrono::system_clock> max_commit_time;
	std::chrono::time_point<std::chrono::system_clock> commit_time;

	std::chrono::time_point<std::chrono::system_clock> next_wakeup_time();
};


class DatabaseAutocommit : public Task<> {
	static std::mutex mtx;
	static std::condition_variable wakeup_signal;
	static std::unordered_map<Endpoints, DatabaseCommitStatus> databases;
	static std::chrono::time_point<std::chrono::system_clock> next_wakeup_time;

	std::atomic_bool running;
	std::shared_ptr<XapiandManager> manager;

public:
	DatabaseAutocommit(const std::shared_ptr<XapiandManager>& manager_);
	~DatabaseAutocommit();

	static void signal_changed(const std::shared_ptr<Database>& database);
	void shutdown();

	void run() override;
};
