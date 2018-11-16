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

#include "database_pool.h"

#include <algorithm>              // for std::move
#include <sysexits.h>             // for EX_SOFTWARE

#include "cassert.h"              // for ASSERT
#include "database.h"             // for Database
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "log.h"                  // for L_OBJ, L_CALL
#include "manager.h"              // for sig_exit
#include "repr.hh"                // for repr
#include "string.hh"              // for string::join


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_DATABASE_BEGIN
// #define L_DATABASE_BEGIN L_DELAYED_600
// #undef L_DATABASE_END
// #define L_DATABASE_END L_DELAYED_N_UNLOG


#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10


//  ____        _        _                     ___
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___ / _ \ _   _  ___ _   _  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ | | | | | |/ _ \ | | |/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |_| | |_| |  __/ |_| |  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|\__\_\\__,_|\___|\__,_|\___|
//

template <typename... Args>
DatabaseQueue::DatabaseQueue(const Endpoints& endpoints_, Args&&... args)
	: Queue(std::forward<Args>(args)...),
	  locked(false),
	  renew_time(std::chrono::system_clock::now()),
	  count(0),
	  endpoints(endpoints_)
{
}


size_t
DatabaseQueue::inc_count()
{
	L_CALL("DatabaseQueue::inc_count()");

	return ++count;
}


size_t
DatabaseQueue::dec_count()
{
	L_CALL("DatabaseQueue::dec_count()");

	auto current_count = count.fetch_sub(1);
	if (current_count == 0) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}
	return current_count - 1;
}


//  ____        _        _                         _     ____  _   _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___  ___| |   |  _ \| | | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \/ __| |   | |_) | | | |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/\__ \ |___|  _ <| |_| |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___||___/_____|_| \_\\___/
//

DatabasesLRU::DatabasesLRU(DatabasePool& database_pool_, size_t dbpool_size, std::shared_ptr<queue::QueueState> queue_state)
	: LRU(dbpool_size),
	  _queue_state(std::move(queue_state)),
	  database_pool(database_pool_) { }


std::shared_ptr<DatabaseQueue>
DatabasesLRU::get(size_t hash)
{
	L_CALL("DatabasesLRU::get(%zx)", hash);

	auto it = find(hash);
	if (it != end()) {
		return it->second;
	}
	return nullptr;
}


std::pair<std::shared_ptr<DatabaseQueue>, bool>
DatabasesLRU::get(size_t hash, const Endpoints& endpoints)
{
	L_CALL("DatabasesLRU::get(%zx, %s)", hash, repr(endpoints.to_string()));

	const auto now = std::chrono::system_clock::now();

	const auto on_get = [&](std::shared_ptr<DatabaseQueue>& queue) {
		queue->renew_time = now;
		return lru::GetAction::renew;
	};

	auto it = find_and(on_get, hash);
	if (it != end()) {
		return std::make_pair(it->second, false);
	}

	const auto on_drop = [&](std::shared_ptr<DatabaseQueue>&, ssize_t, ssize_t) {
		return lru::DropAction::stop;
	};

	auto emplaced = emplace_and(on_drop, hash, DatabaseQueue::make_shared(endpoints, _queue_state));
	return std::make_pair(emplaced.first->second, emplaced.second);
}


void
DatabasesLRU::cleanup(const std::chrono::time_point<std::chrono::system_clock>& now)
{
	L_CALL("DatabasesLRU::cleanup()");

	const auto on_drop = [&](std::shared_ptr<DatabaseQueue>& queue, ssize_t size, ssize_t max_size) {
		if (queue->locked) {
			L_DATABASE("Leave locked queue: %s", repr(queue->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (queue->size() < queue->count) {
			L_DATABASE("Leave occupied queue: %s", repr(queue->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (size > max_size) {
			if (queue->renew_time < now - 60s) {
				L_DATABASE("Evict queue from full LRU: %s", repr(queue->endpoints.to_string()));
				queue->clear();
				database_pool._drop_queue(queue);
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used queue: %s", repr(queue->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (queue->renew_time < now - 3600s) {
			L_DATABASE("Evict queue: %s", repr(queue->endpoints.to_string()));
			queue->clear();
			database_pool._drop_queue(queue);
			return lru::DropAction::evict;
		}
		L_DATABASE("Stop at queue: %s", repr(queue->endpoints.to_string()));
		return lru::DropAction::stop;
	};
	trim(on_drop);
}


void
DatabasesLRU::finish()
{
	L_CALL("DatabasesLRU::finish()");

	for (auto& queue : *this) {
		queue.second->finish();
	}
}


//  ____        _        _                    ____             _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
//

DatabasePool::DatabasePool(size_t dbpool_size, size_t max_databases)
	: finished(false),
	  locks(0),
	  queue_state(std::make_shared<queue::QueueState>(-1, max_databases, -1)),
	  databases(*this, dbpool_size, queue_state),
	  writable_databases(*this, dbpool_size, queue_state)
{
}


DatabasePool::~DatabasePool()
{
	finish();
}


std::shared_ptr<DatabaseQueue>
DatabasePool::_spawn_queue(bool db_writable, size_t hash, const Endpoints& endpoints)
{
	L_CALL("DatabasePool::_spawn_queue(<Database %s>)", repr(database->endpoints.to_string()));

	auto queue_pair = db_writable
		? writable_databases.get(hash, endpoints)
		: databases.get(hash, endpoints);

	auto queue = queue_pair.first;

	if (queue_pair.second) {
		// Queue was just created, add as used queue to the endpoint -> queues map
		for (const auto& endpoint : endpoints) {
			auto endpoint_hash = endpoint.hash();
			auto& queues_set = queues[endpoint_hash];
			queues_set.insert(queue);
		}
	}

	return queue;
}


void
DatabasePool::_drop_queue(const std::shared_ptr<DatabaseQueue>& queue)
{
	L_CALL("DatabasePool::_drop_queue(<queue>)");

	// Queue was just erased, remove as used queue to the endpoint -> queues map
	for (const auto& endpoint : queue->endpoints) {
		auto endpoint_hash = endpoint.hash();
		auto& queues_set = queues[endpoint_hash];
		queues_set.erase(queue);
		if (queues_set.empty()) {
			queues.erase(endpoint_hash);
		}
	}
}


void
DatabasePool::lock(const std::shared_ptr<Database>& database, double timeout)
{
	L_CALL("DatabasePool::lock(<Database %s>)", repr(database->endpoints.to_string()));

	if (!database->is_writable_and_local) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for local writable databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	auto queue = database->weak_queue.lock();
	if (!queue) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for valid databases in a queue");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	std::unique_lock<std::mutex> lk(qmtx);
	if (queue->locked) {
		L_DEBUG("ERROR: Exclusive lock can be granted only to non-locked databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	++locks;
	queue->locked = true;
	try {
		auto is_ready_to_lock = [&] {
			for (auto& endpoint_queue : queues[database->endpoints.hash()]) {
				if (endpoint_queue != queue && endpoint_queue->size() < endpoint_queue->count) {
					return false;
				}
			}
			return true;
		};
		auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
		if (!queue->lockable_cond.wait_until(lk, timeout_tp, is_ready_to_lock)) {
			THROW(TimeOutError, "Cannot grant exclusive lock database");
		}
	} catch(...) {
		ASSERT(locks > 0);
		--locks;
		queue->locked = false;
		queue->unlocked_cond.notify_all();
		throw;
	}
}


void
DatabasePool::unlock(const std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::unlock(<Database %s>)", repr(database->endpoints.to_string()));

	if (!database->is_writable_and_local) {
		L_DEBUG("ERROR: Exclusive lock can be released only for locked databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	auto queue = database->weak_queue.lock();
	if (!queue) {
		L_DEBUG("ERROR: Exclusive lock can be released only for locked databases in a queue");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	std::unique_lock<std::mutex> lk(qmtx);
	if (!queue->locked) {
		L_DEBUG("ERROR: Exclusive lock can be released only to locked databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	ASSERT(locks > 0);
	--locks;
	queue->locked = false;
	queue->unlocked_cond.notify_all();
}


void
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, double timeout)
{
	L_CALL("DatabasePool::checkout(%s, 0x%02x (%s))", repr(endpoints.to_string()), flags, [&flags]() {
		std::vector<std::string> values;
		if ((flags & DB_OPEN) == DB_OPEN) values.push_back("DB_OPEN");
		if ((flags & DB_WRITABLE) == DB_WRITABLE) values.push_back("DB_WRITABLE");
		if ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN) values.push_back("DB_CREATE_OR_OPEN");
		if ((flags & DB_NO_WAL) == DB_NO_WAL) values.push_back("DB_NO_WAL");
		if ((flags & DB_NOSTORAGE) == DB_NOSTORAGE) values.push_back("DB_NOSTORAGE");
		return string::join(values, " | ");
	}());

	bool db_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	L_DATABASE_BEGIN("++ CHECKING OUT DB [%s]: %s ...", db_writable ? "WR" : "RO", repr(endpoints.to_string()));
	L_DATABASE_END("!! FAILED CHECKOUT DB [%s]: %s", db_writable ? "WR" : "RO", repr(endpoints.to_string()));

	ASSERT(!database);

	if (db_writable && endpoints.size() != 1) {
		L_DEBUG("ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout writable multi-database");
	}

	if (!finished) {
		size_t hash = endpoints.hash();

		std::unique_lock<std::mutex> lk(qmtx);
		auto queue = _spawn_queue(db_writable, hash, endpoints);

		int retries = 10;
		while (true) {
			if (!queue->pop(database, 0)) {
				// Increment so other threads don't delete the queue
				auto count = queue->inc_count();
				try {
					lk.unlock();
					if (
						(db_writable && count > 1) ||
						(!db_writable && count > 1000)
					) {
						// Lock until a database is available if it can't get one.
						if (!queue->pop(database, timeout)) {
							THROW(TimeOutError, "Database is not available");
						}
					} else {
						database = std::make_shared<Database>(queue, flags);
					}
					lk.lock();
				} catch (...) {
					lk.lock();
					database.reset();
					count = queue->dec_count();
					if (count == 0) {
						_drop_queue(queue);
						// Erase queue from LRUs
						if (db_writable) {
							writable_databases.erase(hash);
						} else {
							databases.erase(hash);
						}
					}
					throw;
				}
				// Decrement, count should have been already incremented if Database was created
				queue->dec_count();
			}

			if (locks) {
				auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
				std::function<std::shared_ptr<DatabaseQueue>()> has_locked_endpoints;
				if (db_writable) {
					has_locked_endpoints = [&]() -> std::shared_ptr<DatabaseQueue> {
						if (queue->locked) {
							return queue;
						}
						return nullptr;
					};
				} else {
					has_locked_endpoints = [&]() -> std::shared_ptr<DatabaseQueue> {
						for (auto& e : database->endpoints) {
							auto wq = writable_databases.get(e.hash());
							if (wq && wq->locked) {
								return wq;
							}
						}
						return nullptr;
					};
				}
				auto wq = has_locked_endpoints();
				if (wq) {
					database.reset();
					if (--retries == 0 || !timeout) {
						THROW(TimeOutError, "Locked database is not available");
					}
					do {
						if (wq->unlocked_cond.wait_until(lk, timeout_tp) == std::cv_status::timeout) {
							if (has_locked_endpoints()) {
								THROW(TimeOutError, "Locked database is not available");
							}
							break;
						}
					} while ((wq = has_locked_endpoints()));
					continue;  // locked database has been unlocked, retry!
				}
			}

			break;
		};
	}

	if (!database) {
		THROW(DatabaseNotFoundError, "Database not found: %s", repr(endpoints.to_string()));
	}

	// Reopening of old/outdated databases:
	if (!db_writable) {
		bool reopen = false;
		auto reopen_age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - database->reopen_time).count();
		if (reopen_age >= LOCAL_DATABASE_UPDATE_TIME) {
			// Database is just too old, reopen
			reopen = true;
		} else {
			for (size_t i = 0; i < database->dbs.size(); ++i) {
				const auto& db_pair = database->dbs[i];
				bool local = db_pair.second;
				auto hash = endpoints[i].hash();
				if (local) {
					std::unique_lock<std::mutex> lk(qmtx);
					auto queue = writable_databases.get(hash);
					if (queue) {
						lk.unlock();
						auto revision = queue->local_revision.load();
						if (revision != db_pair.first.get_revision()) {
							// Local writable database has changed revision.
							reopen = true;
							break;
						}
					}
				} else {
					if (reopen_age >= REMOTE_DATABASE_UPDATE_TIME) {
						// Remote database is too old, reopen.
						reopen = true;
						break;
					}
				}
			}
		}
		if (reopen) {
			database->reopen();
			L_DATABASE("== REOPEN DB [%s]: %s", db_writable ? "WR" : "RO", repr(database->endpoints.to_string()));
		}
	}

	L_DATABASE_END("++ CHECKED OUT DB [%s]: %s (rev:%llu)", db_writable ? "WR" : "RO", repr(endpoints.to_string()), database->reopen_revision);
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::checkin(%s)", repr(database->to_string()));

	ASSERT(database);
	auto endpoints = database->endpoints;
	int flags = database->flags;
	bool db_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	L_DATABASE_BEGIN("-- CHECKING IN DB [%s]: %s ...", db_writable ? "WR" : "RO", repr(endpoints.to_string()));

	Database::autocommit(database);

	if (auto queue = database->weak_queue.lock()) {
		std::unique_lock<std::mutex> lk(qmtx);

		if (locks) {
			if (db_writable) {
				if (queue->locked) {
					ASSERT(locks > 0);
					--locks;
					queue->locked = false;
					queue->unlocked_cond.notify_all();
				}
			} else {
				bool was_locked = false;
				for (auto& e : endpoints) {
					auto hash = e.hash();
					auto writable_queue = writable_databases.get(hash);
					if (writable_queue && writable_queue->locked) {
						bool unlock = true;
						for (auto& endpoint_queue : queues[hash]) {
							if (endpoint_queue != writable_queue && endpoint_queue->size() < endpoint_queue->count) {
								unlock = false;
								break;
							}
						}
						if (unlock) {
							writable_queue->lockable_cond.notify_one();
							was_locked = true;
						}
					}
				}
				if (was_locked) {
					database->close();
				}
			}
		}

		if (!database->closed) {
			if (!queue->push(database)) {
				_cleanup(true, true);
				queue->push(database);
			}
		}

		database.reset();

		if (queue->count < queue->size()) {
			L_CRIT("Inconsistency in the number of databases in queue");
			sig_exit(-EX_SOFTWARE);
		}

		if (queue->count == 0) {
			_drop_queue(queue);
			// Erase queue from LRUs
			auto hash = endpoints.hash();
			if (db_writable) {
				writable_databases.erase(hash);
			} else {
				databases.erase(hash);
			}
		}

		TaskQueue<void()> callbacks;
		std::swap(callbacks, queue->callbacks);
		lk.unlock();

		while (callbacks.call()) {};

	} else {
		database.reset();
	}

	L_DATABASE_END("-- CHECKED IN DB [%s]: %s", db_writable ? "WR" : "RO", repr(endpoints.to_string()));
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	std::lock_guard<std::mutex> lk(qmtx);

	finished = true;

	writable_databases.finish();
	databases.finish();
}


void
DatabasePool::_cleanup(bool writable, bool readable)
{
	L_CALL("DatabasePool::_cleanup(%s, %s)", writable ? "true" : "false", readable ? "true" : "false");

	const auto now = std::chrono::system_clock::now();

	if (writable) {
		if (cleanup_writable_time < now - 60s) {
			L_DATABASE("Cleanup writable databases...");
			writable_databases.cleanup(now);
			cleanup_writable_time = now;
		}
	}

	if (readable) {
		if (cleanup_readable_time < now - 60s) {
			L_DATABASE("Cleanup readable databases...");
			databases.cleanup(now);
			cleanup_readable_time = now;
		}
	}
}


void
DatabasePool::cleanup()
{
	L_CALL("DatabasePool::cleanup()");

	std::unique_lock<std::mutex> lk(qmtx);
	_cleanup(true, true);
}


void
DatabasePool::clear()
{
	L_CALL("DatabasePool::clear()");

	std::lock_guard<std::mutex> lk(qmtx);

	writable_databases.clear();
	databases.clear();
	queues.clear();
}



DatabaseCount
DatabasePool::total_writable_databases()
{
	L_CALL("DatabasePool::total_wdatabases()");

	size_t db_count = 0;
	size_t db_queues = writable_databases.size();
	size_t db_enqueued = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & writable_database : writable_databases) {
		db_count += writable_database.second->count;
		db_enqueued += writable_database.second->size();
	}
	return {
		db_count,
		db_queues,
		db_enqueued,
	};
}


DatabaseCount
DatabasePool::total_readable_databases()
{
	L_CALL("DatabasePool::total_rdatabases()");

	size_t db_count = 0;
	size_t db_queues = databases.size();
	size_t db_enqueued = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & database : databases) {
		db_count += database.second->size();
		db_enqueued += database.second->size();
	}
	return {
		db_count,
		db_queues,
		db_enqueued,
	};
}
