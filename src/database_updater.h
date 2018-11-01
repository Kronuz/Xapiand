/*
 * Copyright (C) 2018 Dubalu LLC. All rights reserved.
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

#include "config.h"                           // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include <cstddef>                            // for std::size_t
#include <functional>                         // for std::hash
#include <memory>                             // for std::shared_ptr
#include <mutex>                              // for std::mutex
#include <string>                             // for std::string
#include <unordered_map>                      // for std::unordered_map
#include <xapian.h>                           // for Xapian::rev

#include "endpoint.h"                         // for Endpoint
#include "scheduler.h"                        // for SchedulerTask, SchedulerThread


struct DatabaseUpdate {
	Endpoint endpoint;
	std::string uuid;
	Xapian::rev revision;

	DatabaseUpdate() = default;

	DatabaseUpdate(Endpoint endpoint_, const std::string& uuid_, Xapian::rev revision_)
		: endpoint(endpoint_), uuid(uuid_), revision(revision_) { }

	bool operator==(const DatabaseUpdate &other) const {
		return endpoint == other.endpoint;
	}
};


namespace std {
	template <>
	struct hash<DatabaseUpdate> {
		std::size_t operator()(const DatabaseUpdate& k) const {
			return std::hash<Endpoint>()(k.endpoint);
		};
	};
}


class DatabaseUpdater : public ScheduledTask {
	struct Status {
		std::shared_ptr<DatabaseUpdater> task;
		unsigned long long max_wakeup_time;
	};

	static std::mutex statuses_mtx;
	static std::unordered_map<DatabaseUpdate, Status> statuses;

	bool forced;
	DatabaseUpdate update;

public:
	static Scheduler& scheduler(std::size_t num_threads = 0) {
		static Scheduler scheduler("U--", "U%02zu", num_threads);
		return scheduler;
	}

	static void finish(int wait=10) {
		scheduler().finish(wait);
	}

	static void join() {
		scheduler().join();
	}

	static std::size_t threadpool_capacity() {
		return scheduler().threadpool_capacity();
	}

	static std::size_t threadpool_size() {
		return scheduler().threadpool_size();
	}

	static std::size_t running_size() {
		return scheduler().running_size();
	}

	static std::size_t size() {
		return scheduler().size();
	}

	DatabaseUpdater(bool forced_, DatabaseUpdate&& update_);
	void run() override;

	static void send(DatabaseUpdate&& update);

	std::string __repr__() const override {
		return ScheduledTask::__repr__("DatabaseUpdater");
	}
};

#endif
