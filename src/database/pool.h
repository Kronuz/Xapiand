/*
 * Copyright (c) 2015-2019 Dubalu LLC
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
#include <chrono>               // for std::chrono, std::chrono::steady_clock
#include <cstring>              // for size_t
#include <list>                 // for std::list
#include <memory>               // for std::shared_ptr
#include <mutex>                // for std::mutex, std::condition_variable, std::unique_lock
#include <set>                  // for std::set
#include <string>               // for std::string
#include <utility>              // for std::pair
#include <unordered_map>        // for std::unordered_map
#include <vector>               // for std::vector

#include "threadpool.hh"        // for TaskQueue
#include "endpoint.h"           // for Endpoints, Endpoint
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...
#include "xapian.h"             // for Xapian::rev


using namespace std::chrono_literals;


constexpr double DB_TIMEOUT = 60.0;

class Shard;
class Database;
class DatabasePool;
class ReferencedShardEndpoint;


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_ ___
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __/ __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_\__ \
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|___/
 *                                                             |_|
 */

class ShardEndpoint : public Endpoint
{
	friend Shard;
	friend DatabasePool;
	friend ReferencedShardEndpoint;

	mutable std::mutex mtx;

	DatabasePool& database_pool;

	std::atomic_int refs;

	std::atomic_bool finished;

	std::atomic_bool locked;
	std::mutex revisions_mtx;
	std::unordered_map<std::string, Xapian::rev> revisions;
	std::chrono::time_point<std::chrono::steady_clock> renew_time;

	std::shared_ptr<Shard> writable;
	std::list<std::shared_ptr<Shard>> readables;

	std::atomic_size_t readables_available;
	std::condition_variable writable_cond;
	std::condition_variable readables_cond;

	std::condition_variable lockable_cond;

	TaskQueue<void()> callbacks;  // callbacks waiting for database to be ready

	std::shared_ptr<Shard>& _writable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::steady_clock>& now, std::unique_lock<std::mutex>& lk);
	std::shared_ptr<Shard>& _readable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::steady_clock>& now, std::unique_lock<std::mutex>& lk);

public:
	ShardEndpoint(DatabasePool& database_pool, const Endpoint& endpoint);

	~ShardEndpoint();

	std::shared_ptr<Shard> checkout(int flags, double timeout, std::packaged_task<void()>* callback = nullptr);

	void checkin(std::shared_ptr<Shard>& database) noexcept;

	void finish();

	std::pair<size_t, size_t> clear();

	std::pair<size_t, size_t> count();

	bool is_locked() const {
		return locked.load(std::memory_order_relaxed);
	}

	bool is_finished() const {
		return finished.load(std::memory_order_relaxed);
	}

	bool is_used() const;

	Xapian::rev revision(const std::string& lower_name);
	Xapian::rev revision();
	void revision(const std::string& lower_name, Xapian::rev revision);
	void revision(Xapian::rev revision);

	std::string __repr__() const;
	std::string dump_databases(int level) const;
};


/*
 *  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */

class DatabasePool : lru::lru<Endpoint, std::unique_ptr<ShardEndpoint>> {
	friend ShardEndpoint;

	mutable std::mutex mtx;

	std::atomic_int locks;

	std::condition_variable checkin_clears_cond;

	size_t max_database_readers;

	ReferencedShardEndpoint _spawn(const Endpoint& endpoint);
	ReferencedShardEndpoint spawn(const Endpoint& endpoint);

	ReferencedShardEndpoint _get(const Endpoint& endpoint) const;
	ReferencedShardEndpoint get(const Endpoint& endpoint) const;

	bool notify_lockable(const Endpoint& endpoint);

public:
	DatabasePool(size_t database_pool_size, size_t max_database_readers);

	DatabasePool(const DatabasePool&) = delete;
	DatabasePool(DatabasePool&&) = delete;
	DatabasePool& operator=(const DatabasePool&) = delete;
	DatabasePool& operator=(DatabasePool&&) = delete;

	std::vector<ReferencedShardEndpoint> endpoints() const;

	void lock(const std::shared_ptr<Shard>& shard, double timeout = DB_TIMEOUT);
	void unlock(const std::shared_ptr<Shard>& shard);

	bool is_locked(const Endpoint& endpoint) const;

	template <typename Func>
	std::shared_ptr<Shard> checkout(const Endpoint& endpoint, int flags, double timeout, Func&& func) {
		std::packaged_task<void()> callback(std::forward<Func>(func));
		return checkout(endpoint, flags, timeout, &callback);
	}
	std::shared_ptr<Shard> checkout(const Endpoint& endpoint, int flags, double timeout = DB_TIMEOUT, std::packaged_task<void()>* callback = nullptr);
	void checkin(std::shared_ptr<Shard>& shard);

	std::vector<std::shared_ptr<Shard>> checkout(const Endpoints& endpoints, int flags, double timeout = DB_TIMEOUT);
	void checkin(std::vector<std::shared_ptr<Shard>>& database);

	void finish();

	bool join(const std::chrono::time_point<std::chrono::steady_clock>& wakeup);

	bool join(std::chrono::milliseconds timeout = 60s) {
		return join(std::chrono::steady_clock::now() + timeout);
	}

	void cleanup(bool immediate = false);

	bool clear();

	std::pair<size_t, size_t> count();

	std::string __repr__() const;
	std::string dump_databases(int level = 1) const;
};
