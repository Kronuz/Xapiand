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
// #undef L_DATABASE_WRAP_BEGIN
// #define L_DATABASE_WRAP_BEGIN L_DELAYED_100
// #undef L_DATABASE_WRAP_END
// #define L_DATABASE_WRAP_END L_DELAYED_N_UNLOG


#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10


//  ____        _        _                     ___
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___ / _ \ _   _  ___ _   _  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ | | | | | |/ _ \ | | |/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |_| | |_| |  __/ |_| |  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|\__\_\\__,_|\___|\__,_|\___|
//

template <typename... Args>
DatabaseQueue::DatabaseQueue(const std::shared_ptr<DatabasePool>& database_pool, const Endpoints& endpoints_, int flags_, Args&&... args)
	: Queue(std::forward<Args>(args)...),
	  locked(false),
	  renew_time(std::chrono::system_clock::now()),
	  count(0),
	  endpoints(endpoints_),
	  flags(flags_),
	  weak_database_pool(database_pool)
{
}


DatabaseQueue::~DatabaseQueue()
{
	if (auto database_pool = weak_database_pool.lock()) {
		database_pool->release_queue(endpoints, flags);
	}
}


void
DatabaseQueue::clear()
{
	std::shared_ptr<Database> database;
	while (pop(database, 0)) {
		database->weak_queue.reset();
		database.reset();
	}
	if (auto database_pool = weak_database_pool.lock()) {
		database_pool->_drop_endpoints(endpoints);
	}
	weak_database_pool.reset();
}


size_t
DatabaseQueue::inc_count()
{
	L_CALL("DatabaseQueue::inc_count()");

	auto current_count = count++;

	L_DATABASE("++%zu (inc_count) %s [%s]", current_count, repr(endpoints.to_string()), readable_flags(flags));

	++current_count;
	return current_count;
}


size_t
DatabaseQueue::dec_count()
{
	L_CALL("DatabaseQueue::dec_count()");

	auto current_count = count--;

	L_DATABASE("--%zu (dec_count) %s [%s]", current_count, repr(endpoints.to_string()), readable_flags(flags));

	if (current_count == 0) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}
	if (--current_count == 0) {
		if (auto database_pool = weak_database_pool.lock()) {
			database_pool->release_queue(endpoints, flags);
		}
	}
	return current_count;
}


std::string
DatabaseQueue::__repr__() const
{
	return string::format("<%s at %p: %s [%s]>",
		((flags & DB_WRITABLE) == DB_WRITABLE) ? "WritableQueue" : "Queue",
		static_cast<const void*>(this),
		repr(endpoints.to_string()),
		readable_flags(flags));
}


std::string
DatabaseQueue::dump_databases(int level) const
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret;
	size_t current_count;
	{
		std::lock_guard<std::mutex> lk(_state->_mutex);
		current_count = count;
		for (const auto& database : _items_queue) {
			ret += indent;
			ret += database->__repr__();
			ret.push_back('\n');
			--current_count;
		}
	}
	while (current_count) {
		ret += indent;
		ret += string::format("<%s (checked out): %s [%s]>",
			((flags & DB_WRITABLE) == DB_WRITABLE) ? "WritableDatabase" : "Database",
			repr(endpoints.to_string()),
			readable_flags(flags));
		ret.push_back('\n');
		--current_count;
	}
	return ret;
}


//  ____        _        _                         _     ____  _   _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___  ___| |   |  _ \| | | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \/ __| |   | |_) | | | |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/\__ \ |___|  _ <| |_| |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___||___/_____|_| \_\\___/
//

DatabasesLRU::DatabasesLRU(size_t dbpool_size)
	: LRU(dbpool_size) {}


std::shared_ptr<DatabaseQueue>
DatabasesLRU::get(const Endpoints& endpoints)
{
	L_CALL("DatabasesLRU::get(%s)", endpoints.to_string());

	auto it = find(endpoints);
	if (it != end()) {
		return it->second;
	}
	return nullptr;
}


std::pair<std::shared_ptr<DatabaseQueue>, bool>
DatabasesLRU::get(const std::shared_ptr<DatabasePool>& database_pool, const Endpoints& endpoints, int flags)
{
	L_CALL("DatabasesLRU::get(%s, [%s])", repr(endpoints.to_string()), readable_flags(flags));

	const auto now = std::chrono::system_clock::now();

	const auto on_get = [&](std::shared_ptr<DatabaseQueue>& queue) {
		queue->renew_time = now;
		return lru::GetAction::renew;
	};

	auto it = find_and(on_get, endpoints);
	if (it != end()) {
		return std::make_pair(it->second, false);
	}

	const auto on_drop = [&](std::shared_ptr<DatabaseQueue>&, ssize_t, ssize_t) {
		return lru::DropAction::stop;
	};

	auto emplaced = emplace_and(on_drop, endpoints, DatabaseQueue::make_shared(database_pool, endpoints, flags, database_pool->queue_state));
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
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used queue: %s", repr(queue->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (queue->renew_time < now - 3600s) {
			L_DATABASE("Evict queue: %s", repr(queue->endpoints.to_string()));
			queue->clear();
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


void
DatabasesLRU::clear()
{
	for (auto& queue : *this) {
		queue.second->clear();
	}
	lru::LRU<Endpoints, std::shared_ptr<DatabaseQueue>>::clear();
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
	  databases(dbpool_size),
	  writable_databases(dbpool_size)
{
}


DatabasePool::~DatabasePool()
{
	finish();
}


std::shared_ptr<DatabaseQueue>
DatabasePool::_spawn_queue(const Endpoints& endpoints, int flags)
{
	bool db_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	L_CALL("DatabasePool::_spawn_queue(<%s: %s>)", db_writable ? "WritableDatabase" : "Database", repr(endpoints.to_string()));

	auto queue_pair = db_writable
		? writable_databases.get(shared_from_this(), endpoints, flags)
		: databases.get(shared_from_this(), endpoints, flags);

	auto queue = queue_pair.first;

	if (queue_pair.second) {
		// Queue was just created, add as used queue to the endpoint -> used endpoints map
		for (const auto& endpoint : endpoints) {
			auto& used_endpoints_set = used_endpoints_map[endpoint];
			used_endpoints_set.insert(endpoints);
		}
	}

	return queue;
}


void
DatabasePool::_drop_endpoints(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::_drop_endpoints(<endpoints>)");

	// Queue was just erased, remove as used queue to the endpoint -> used endpoints map
	for (const auto& endpoint : endpoints) {
		auto& used_endpoints_set = used_endpoints_map[endpoint];
		used_endpoints_set.erase(endpoints);
		if (used_endpoints_set.empty()) {
			used_endpoints_map.erase(endpoint);
		}
	}
}


void
DatabasePool::_unlocked_notify(const std::shared_ptr<DatabaseQueue>& queue)
{
	if (queue->locked) {
		ASSERT(locks > 0);
		--locks;
		queue->locked = false;
		queue->unlocked_cond.notify_all();
	}
}


bool
DatabasePool::_lockable_notify(const Endpoints& endpoints)
{
	bool was_locked = false;
	for (auto& endpoint : endpoints) {
		auto writable_queue = writable_databases.get(Endpoints{endpoint});
		if (writable_queue && writable_queue->locked) {
			bool unlock = true;
			for (auto& used_endpoints : used_endpoints_map[endpoint]) {
				auto endpoint_queue = databases.get(used_endpoints);
				if (endpoint_queue && endpoint_queue->size() < endpoint_queue->count) {
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
	return was_locked;
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
		if (!timeout) {
			THROW(TimeOutError, "Cannot grant exclusive lock database");
		}
		auto is_ready_to_lock = [&] {
			for (auto& used_endpoints : used_endpoints_map[database->endpoints[0]]) {
				auto endpoint_queue = databases.get(used_endpoints);
				if (endpoint_queue && endpoint_queue->size() < endpoint_queue->count) {
					return false;
				}
			}
			return true;
		};
		if (timeout > 0.0) {
			auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
			if (!queue->lockable_cond.wait_until(lk, timeout_tp, is_ready_to_lock)) {
				THROW(TimeOutError, "Cannot grant exclusive lock database");
			}
		} else {
			while (!queue->lockable_cond.wait_for(lk, 1s, is_ready_to_lock)) {
				if (finished) {
					THROW(TimeOutError, "Cannot grant exclusive lock database");
				}
			}
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
	L_CALL("DatabasePool::checkout(%s, [%s])", repr(endpoints.to_string()), readable_flags(flags));

	bool db_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	L_DATABASE_BEGIN("++ CHECKING OUT DB: %s [%s] ...", repr(endpoints.to_string()), readable_flags(flags));
	L_DATABASE_END("!! FAILED CHECKOUT DB: %s [%s] ...", repr(endpoints.to_string()), readable_flags(flags));

	ASSERT(!database);

	if (db_writable && endpoints.size() != 1) {
		L_DEBUG("ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout writable multi-database");
	}

	if (!finished) {
		std::unique_lock<std::mutex> lk(qmtx);
		auto queue = _spawn_queue(endpoints, flags);

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
						if (!timeout) {
							THROW(TimeOutError, "Database is not available");
						}
						if (!queue->pop(database, timeout)) {
							THROW(TimeOutError, "Database is not available");
						}
					} else {
						database = std::make_shared<Database>(queue);
					}
					lk.lock();
				} catch (...) {
					queue->dec_count();
					database.reset();
					throw;
				}
				// Decrement, count should have been already incremented if Database was created
				queue->dec_count();
			}

			if (locks) {
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
						for (auto& endpoint : database->endpoints) {
							auto wq = writable_databases.get(Endpoints{endpoint});
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
					if (timeout > 0.0) {
						auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
						do {
							if (wq->unlocked_cond.wait_until(lk, timeout_tp) == std::cv_status::timeout) {
								if (!has_locked_endpoints()) {
									break;
								}
								THROW(TimeOutError, "Locked database is not available");
							}
						} while ((wq = has_locked_endpoints()));
					} else {
						do {
							if (wq->unlocked_cond.wait_for(lk, 1s) ==  std::cv_status::timeout) {
								if (!has_locked_endpoints()) {
									break;
								}
								if (finished) {
									THROW(TimeOutError, "Locked database is not available");
								}
							}
						} while ((wq = has_locked_endpoints()));
					}
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
			for (size_t i = 0; i < database->_databases.size(); ++i) {
				const auto& db_pair = database->_databases[i];
				bool local = db_pair.second;
				if (local) {
					std::unique_lock<std::mutex> lk(qmtx);
					auto queue = writable_databases.get(Endpoints{endpoints[i]});
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
			L_DATABASE("== REOPEN DB: %s [%s]", repr(database->endpoints.to_string()), readable_flags(flags));
		}
	}

	L_TIMED_VAR(database->log, 200ms,
		"%s checkout is taking too long: %s",
		"%s checked out for too long: %s",
		database->is_writable_and_local_with_wal ? "LocalWritableDatabaseWithWAL" : database->is_writable_and_local ? "LocalWritableDatabase" : database->is_writable ? "WritableDatabase" : "Database",
		repr(endpoints.to_string()));
	L_DATABASE_END("++ CHECKED OUT DB: %s [%s] (rev:%llu)", repr(endpoints.to_string()), readable_flags(flags), database->reopen_revision);
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::checkin(%s)", repr(database->to_string()));

	ASSERT(database);
	auto endpoints = database->endpoints;
	int flags = database->flags;
	bool db_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	L_DATABASE_BEGIN("-- CHECKING IN DB: %s [%s] ...", repr(endpoints.to_string()), readable_flags(flags));
	if (database->log) {
		database->log->clear();
		database->log.reset();
	}

	Database::autocommit(database);

	TaskQueue<void()> callbacks;

	if (auto queue = database->weak_queue.lock()) {
		std::lock_guard<std::mutex> lk(qmtx);

		if (db_writable) {
			_unlocked_notify(queue);
		} else if (locks) {
			if (_lockable_notify(endpoints)) {
				database->close();
			}
		}

		if (!database->closed) {
			if (!queue->push(database)) {
				_cleanup(true, true);
				queue->push(database);
			}
		}

		std::swap(callbacks, queue->callbacks);
	}

	database.reset();

	while (callbacks.call()) {};

	L_DATABASE_END("-- CHECKED IN DB: %s [%s]", repr(endpoints.to_string()), readable_flags(flags));
}


void
DatabasePool::release_queue(const Endpoints& endpoints, int flags)
{
	L_CALL("DatabasePool::release_queue(<endpoints>, <flags>)");

	// This releases a queue:
	// drops endpoints, notifies locks engine, signals callbacks
	// But first, double check queue was released (race condition)
	// by checking count == 0 after mutex lock.

	TaskQueue<void()> callbacks;

	{
		std::lock_guard<std::mutex> lk(qmtx);
		if ((flags & DB_WRITABLE) == DB_WRITABLE) {
			auto queue = writable_databases.get(endpoints);
			if (queue && queue->count == 0) {
				std::swap(callbacks, queue->callbacks);
				queue->clear();
				writable_databases.erase(endpoints);
				_unlocked_notify(queue);
			} else {
				_drop_endpoints(endpoints);
			}
		} else {
			auto queue = databases.get(endpoints);
			if (queue && queue->count == 0) {
				std::swap(callbacks, queue->callbacks);
				queue->clear();
				databases.erase(endpoints);
				_lockable_notify(endpoints);
			} else {
				_drop_endpoints(endpoints);
			}
		}
	}

	while (callbacks.call()) {};
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

	used_endpoints_map.clear();
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


std::string
DatabasePool::dump_databases(int level)
{
	std::lock_guard<std::mutex> lk(qmtx);

	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret = "\n";

	for (const auto& queue_database : writable_databases) {
		ret += indent;
		ret += queue_database.second->__repr__();
		ret.push_back('\n');
		ret += queue_database.second->dump_databases(level + 1);
	}
	for (const auto& queue_database : databases) {
		ret += indent;
		ret += queue_database.second->__repr__();
		ret.push_back('\n');
		ret += queue_database.second->dump_databases(level + 1);
	}
	return ret;
}
