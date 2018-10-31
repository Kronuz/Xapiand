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

#include "config.h"             // for XAPIAND_BINARY_SERVERPORT, XAPIAND_BINARY_PROXY

#include <atomic>               // for std::atomic_bool
#include <chrono>               // for std::chrono, std::chrono::system_clock
#include <cstring>              // for size_t
#include <memory>               // for std::shared_ptr, std::enable_shared_from_this, std::make_shared
#include <mutex>                // for std::mutex, std::condition_variable, std::unique_lock
#include <string>               // for std::string
#include <sys/types.h>          // for uint32_t, uint8_t, ssize_t
#include <type_traits>          // for std::forward
#include <unordered_map>        // for std::unordered_map
#include <unordered_set>        // for std::unordered_set
#include <utility>              // for std::pair, std::make_pair
#include <vector>               // for std::vector
#include <xapian.h>             // for Xapian::docid, Xapian::termcount, Xapian::Document, Xapian::ExpandDecider

#include "cuuid/uuid.h"         // for UUID, UUID_LENGTH
#include "endpoint.h"           // for Endpoints, Endpoint
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...
#include "queue.h"              // for Queue, QueueSet
#include "storage.h"            // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...


constexpr int RECOVER_REMOVE_WRITABLE         = 0x01; // Remove endpoint from writable database
constexpr int RECOVER_REMOVE_DATABASE         = 0x02; // Remove endpoint from database
constexpr int RECOVER_REMOVE_ALL              = 0x04; // Remove endpoint from writable database and database
constexpr int RECOVER_DECREMENT_COUNT         = 0x08; // Decrement count queue


struct DatabaseCount {
	size_t count;
	size_t queues;
	size_t enqueued;
};

class Database;
class DatabasePool;
class DatabasesLRU;


//  ____        _        _                     ___
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___ / _ \ _   _  ___ _   _  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ | | | | | |/ _ \ | | |/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |_| | |_| |  __/ |_| |  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|\__\_\\__,_|\___|\__,_|\___|
//
class DatabaseQueue :
	public queue::Queue<std::shared_ptr<Database>>,
	public std::enable_shared_from_this<DatabaseQueue> {

	friend Database;
	friend DatabasePool;
	friend DatabasesLRU;

private:
	bool locked;
	std::atomic<Xapian::rev> local_revision;
	std::chrono::time_point<std::chrono::system_clock> renew_time;

	size_t count;

	std::condition_variable unlock_cond;
	std::condition_variable exclusive_cond;

	Endpoints endpoints;

protected:
	template <typename... Args>
	DatabaseQueue(const Endpoints& endpoints, Args&&... args);

public:
	DatabaseQueue(const DatabaseQueue&) = delete;
	DatabaseQueue(DatabaseQueue&&) = delete;
	DatabaseQueue& operator=(const DatabaseQueue&) = delete;
	DatabaseQueue& operator=(DatabaseQueue&&) = delete;
	~DatabaseQueue();

	size_t inc_count();
	size_t dec_count();

	template <typename... Args>
	static std::shared_ptr<DatabaseQueue> make_shared(Args&&... args) {
		// std::make_shared only can call a public constructor, for this reason
		// it is neccesary wrap the constructor in a struct.
		struct enable_make_shared : DatabaseQueue {
			enable_make_shared(Args&&... args_) : DatabaseQueue(std::forward<Args>(args_)...) { }
		};

		return std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
	}
};


//  ____        _        _                    _     ____  _   _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| |   |  _ \| | | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |   | |_) | | | |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___|  _ <| |_| |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| \_\\___/
//
class DatabasesLRU : public lru::LRU<size_t, std::shared_ptr<DatabaseQueue>> {

	const std::shared_ptr<queue::QueueState> _queue_state;

	DatabasePool& database_pool;

public:
	DatabasesLRU(DatabasePool& database_pool_, size_t dbpool_size, std::shared_ptr<queue::QueueState> queue_state);

	std::shared_ptr<DatabaseQueue> get(size_t hash);
	std::pair<std::shared_ptr<DatabaseQueue>, bool> get(size_t hash, const Endpoints& endpoints);

	void cleanup(const std::chrono::time_point<std::chrono::system_clock>& now);

	void finish();
};


#ifdef XAPIAND_CLUSTERING
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
#endif


//  ____        _        _                    ____             _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
//
class DatabasePool {
	// FIXME: Add maximum number of databases available for the queue
	// FIXME: Add cleanup for removing old database queues
	friend DatabaseQueue;
	friend DatabasesLRU;

	std::mutex qmtx;
	bool finished;
	size_t locks;

	const std::shared_ptr<queue::QueueState> queue_state;

	std::unordered_map<size_t, std::unordered_set<std::shared_ptr<DatabaseQueue>>> queues;

	DatabasesLRU databases;
	DatabasesLRU writable_databases;

	std::chrono::time_point<std::chrono::system_clock> cleanup_readable_time;
	std::chrono::time_point<std::chrono::system_clock> cleanup_writable_time;

	std::condition_variable checkin_cond;

	void _cleanup(bool writable, bool readable);

	void _drop_queue(const std::shared_ptr<DatabaseQueue>& queue);

public:
	void checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags);
	void checkin(std::shared_ptr<Database>& database);

#ifdef XAPIAND_CLUSTERING
	queue::QueueSet<DatabaseUpdate> updated_databases;
#endif

	DatabasePool(size_t dbpool_size, size_t max_databases);
	DatabasePool(const DatabasePool&) = delete;
	DatabasePool(DatabasePool&&) = delete;
	DatabasePool& operator=(const DatabasePool&) = delete;
	DatabasePool& operator=(DatabasePool&&) = delete;
	~DatabasePool();

	void finish();
	void switch_db(const std::string& tmp, const std::string& endpoint_path);
	void cleanup();

	void clear();

	DatabaseCount total_writable_databases();
	DatabaseCount total_readable_databases();
};
