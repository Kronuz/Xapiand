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

#include "xapiand.h"

#include <mutex>          // for mutex
#include <unordered_map>  // for unordered_map

#include "endpoint.h"     // for Endpoints
#include "scheduler.h"    // for SchedulerTask, SchedulerThread


class Database;


class DatabaseAutocommit : public ScheduledTask {
	struct Status {
		std::shared_ptr<DatabaseAutocommit> task;
		unsigned long long max_wakeup_time;
	};

	static std::mutex statuses_mtx;
	static std::unordered_map<Endpoints, Status> statuses;

	bool forced;
	Endpoints endpoints;
	std::weak_ptr<const Database> weak_database;

public:
	static Scheduler& scheduler(size_t num_threads=0) {
		static Scheduler scheduler("A--", "A%02zu", num_threads);
		return scheduler;
	}

	static void finish(int wait=10) {
		scheduler().finish(wait);
	}

	static void join() {
		scheduler().join();
	}

	static size_t threadpool_capacity() {
		return scheduler().threadpool_capacity();
	}

	static size_t threadpool_size() {
		return scheduler().threadpool_size();
	}

	static size_t running_size() {
		return scheduler().running_size();
	}

	static size_t size() {
		return scheduler().size();
	}

	DatabaseAutocommit(bool forced_, Endpoints endpoints_, std::weak_ptr<const Database> weak_database_);
	void run() override;

	static void commit(const std::shared_ptr<Database>& database);

	std::string __repr__() const override {
		return ScheduledTask::__repr__("DatabaseAutocommit");
	}
};
