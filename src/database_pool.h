/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "config.h"             // for XAPIAND_REMOTE_SERVERPORT

#include <atomic>               // for std::atomic_bool
#include <chrono>               // for std::chrono, std::chrono::system_clock
#include <cstring>              // for size_t
#include <list>                 // for std::list
#include <memory>               // for std::shared_ptr
#include <mutex>                // for std::mutex, std::condition_variable, std::unique_lock
#include <set>                  // for std::set
#include <string>               // for std::string
#include <utility>              // for std::pair
#include <vector>               // for std::vector
#include <xapian.h>             // for Xapian::rev

#include "threadpool.hh"        // for TaskQueue
#include "endpoint.h"           // for Endpoints, Endpoint
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...


using namespace std::chrono_literals;


constexpr double DB_TIMEOUT = 60.0;

class Database;
class DatabasePool;
class ReferencedDatabaseEndpoint;


//  ____        _        _                    _____           _             _       _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_ ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __/ __|
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_\__ \
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|___/
//                                                             |_|
class DatabaseEndpoint : public Endpoints
{
	friend Database;
	friend DatabasePool;
	friend ReferencedDatabaseEndpoint;

	mutable std::mutex mtx;

	DatabasePool& database_pool;

	std::atomic_int refs;

	std::atomic_bool finished;

	std::atomic_bool locked;
	std::atomic<Xapian::rev> local_revision;
	std::chrono::time_point<std::chrono::system_clock> renew_time;

	std::shared_ptr<Database> writable;
	std::list<std::shared_ptr<Database>> readables;

	std::atomic_size_t readables_available;
	std::condition_variable writable_cond;
	std::condition_variable readables_cond;

	std::condition_variable lockable_cond;

	TaskQueue<void()> callbacks;  // callbacks waiting for database to be ready

	std::shared_ptr<Database>& _writable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::system_clock>& now, std::unique_lock<std::mutex>& lk);
	std::shared_ptr<Database>& _readable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::system_clock>& now, std::unique_lock<std::mutex>& lk);

public:
	DatabaseEndpoint(DatabasePool& database_pool, const Endpoints& endpoints);

	~DatabaseEndpoint();

	std::shared_ptr<Database> checkout(int flags, double timeout, std::packaged_task<void()>* callback);

	void checkin(std::shared_ptr<Database>& database) noexcept;

	void finish();

	std::pair<size_t, size_t> clear();

	std::pair<size_t, size_t> count();

	bool is_locked() const {
		return locked.load(std::memory_order_relaxed);
	}

	bool is_finished() const {
		return finished.load(std::memory_order_relaxed);
	}

	bool empty() const;

	std::string __repr__() const;
	std::string dump_databases(int level) const;
};


//  ____        _        _                    ____             _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
//
class DatabasePool : lru::LRU<Endpoints, std::unique_ptr<DatabaseEndpoint>> {
	friend DatabaseEndpoint;

	mutable std::mutex mtx;

	std::atomic_int locks;

	std::condition_variable checkin_clears_cond;

	std::unordered_map<Endpoint, std::set<Endpoints>> endpoints_map;

	size_t max_databases;

	void _lot_endpoints(const Endpoints& endpoints);
	void _drop_endpoints(const Endpoints& endpoints);

	ReferencedDatabaseEndpoint _spawn(const Endpoints& endpoints);
	ReferencedDatabaseEndpoint spawn(const Endpoints& endpoints);

	ReferencedDatabaseEndpoint _get(const Endpoints& endpoints) const;
	ReferencedDatabaseEndpoint get(const Endpoints& endpoints) const;

	bool notify_lockable(const Endpoints& endpoints);

public:
	DatabasePool(size_t dbpool_size, size_t max_databases);

	DatabasePool(const DatabasePool&) = delete;
	DatabasePool(DatabasePool&&) = delete;
	DatabasePool& operator=(const DatabasePool&) = delete;
	DatabasePool& operator=(DatabasePool&&) = delete;

	std::vector<ReferencedDatabaseEndpoint> endpoints() const;
	std::vector<ReferencedDatabaseEndpoint> endpoints(const Endpoint& endpoint) const;

	void lock(const std::shared_ptr<Database>& database, double timeout = DB_TIMEOUT);
	void unlock(const std::shared_ptr<Database>& database);

	bool is_locked(const Endpoints& endpoints) const;

	template <typename Func>
	std::shared_ptr<Database> checkout(const Endpoints& endpoints, int flags, double timeout, Func&& func) {
		std::packaged_task<void()> callback(std::forward<Func>(func));
		return checkout(endpoints, flags, timeout, &callback);
	}

	std::shared_ptr<Database> checkout(const Endpoints& endpoints, int flags, double timeout = DB_TIMEOUT, std::packaged_task<void()>* callback = nullptr);

	void checkin(std::shared_ptr<Database>& database);

	void finish();

	bool join(const std::chrono::time_point<std::chrono::system_clock>& wakeup);

	bool join(std::chrono::milliseconds timeout = 60s) {
		return join(std::chrono::system_clock::now() + timeout);
	}

	void cleanup(bool immediate = false);

	bool clear();

	std::pair<size_t, size_t> count();

	std::string __repr__() const;
	std::string dump_databases(int level = 1) const;
};
