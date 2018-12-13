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

#include <algorithm>              // for std::find, std::move

#include "cassert.h"              // for ASSERT
#include "database.h"             // for Database
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "log.h"                  // for L_CALL
#include "logger.h"               // for Logging (database->log)


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_TIMED_VAR
// #define L_TIMED_VAR _L_TIMED_VAR


#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10


class ReferencedDatabaseEndpoint {
	DatabaseEndpoint* ptr;

public:
	ReferencedDatabaseEndpoint(DatabaseEndpoint* ptr) : ptr(ptr) {
		if (ptr) {
			++ptr->refs;
		}
	}

	ReferencedDatabaseEndpoint(const ReferencedDatabaseEndpoint& other) = delete;
	ReferencedDatabaseEndpoint& operator=(const ReferencedDatabaseEndpoint& other) = delete;

	ReferencedDatabaseEndpoint(ReferencedDatabaseEndpoint&& other) : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	ReferencedDatabaseEndpoint& operator=(ReferencedDatabaseEndpoint&& other) {
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	~ReferencedDatabaseEndpoint() noexcept {
		if (ptr) {
			ASSERT(ptr->refs > 0);
			--ptr->refs;
		}
	}

	void reset() {
		if (ptr) {
			ASSERT(ptr->refs > 0);
			--ptr->refs;
			ptr = nullptr;
		}
	}

	operator bool() const {
		return !!ptr;
	}

	DatabaseEndpoint* operator->() const {
		return ptr;
	}
};


//  ____        _        _                    _____           _             _       _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
//                                                             |_|

DatabaseEndpoint::DatabaseEndpoint(DatabasePool& database_pool, const Endpoints& endpoints) :
	Endpoints(endpoints),
	database_pool(database_pool),
	refs(0),
	finished(false),
	locked(false),
	local_revision(0),
	renew_time(std::chrono::system_clock::now()),
	readables_available(0)
{
}


std::shared_ptr<Database>&
DatabaseEndpoint::_writable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::system_clock>& now, std::unique_lock<std::mutex>& lk)
{
	L_CALL("DatabaseEndpoint::writable_checkout((%s), %f, %s)", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	do {
		if (is_finished()) {
			if (callback) {
				callbacks.enqueue(std::move(*callback));
			}
			THROW(TimeOutError, "Database is not available");
		}
		if (!writable) {
			writable = std::make_shared<Database>(*this, flags);
		}
		if (!is_locked() && !writable->busy.exchange(true)) {
			return writable;
		}
		auto wait_pred = [&]() {
			return is_finished() || (!writable->is_busy() && !is_locked() && !database_pool.is_locked(*this));
		};
		if (timeout) {
			if (timeout > 0.0) {
				auto timeout_tp = now + std::chrono::duration<double>(timeout);
				if (!writable_cond.wait_until(lk, timeout_tp, wait_pred)) {
					if (callback) {
						callbacks.enqueue(std::move(*callback));
					}
					THROW(TimeOutError, "Database is not available");
				}
			} else {
				while (!writable_cond.wait_for(lk, 1s, wait_pred)) {}
			}
		} else {
			if (!wait_pred()) {
				if (callback) {
					callbacks.enqueue(std::move(*callback));
				}
				THROW(TimeOutError, "Database is not available");
			}
		}
	} while (true);
}


std::shared_ptr<Database>&
DatabaseEndpoint::_readable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::system_clock>& now, std::unique_lock<std::mutex>& lk)
{
	L_CALL("DatabaseEndpoint::readable_checkout((%s), %f, %s)", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	do {
		if (is_finished()) {
			if (callback) {
				callbacks.enqueue(std::move(*callback));
			}
			THROW(TimeOutError, "Database is not available");
		}
		if (readables_available > 0) {
			for (auto& readable : readables) {
				if (!readable) {
					readable = std::make_shared<Database>(*this, flags);
				}
				if (!is_locked() && !readable->busy.exchange(true)) {
					--readables_available;
					return readable;
				}
			}
		}
		if (readables.size() < database_pool.max_databases) {
			auto new_database = std::make_shared<Database>(*this, flags);
			auto& readable = *readables.insert(readables.end(), new_database);
			++readables_available;
			if (!is_locked() && !readable->busy.exchange(true)) {
				--readables_available;
				return readable;
			}
		}
		auto wait_pred = [&]() {
			return is_finished() || ((readables_available > 0 || readables.size() < database_pool.max_databases) && !is_locked() && !database_pool.is_locked(*this));
		};
		if (timeout) {
			if (timeout > 0.0) {
				auto timeout_tp = now + std::chrono::duration<double>(timeout);
				if (!readables_cond.wait_until(lk, timeout_tp, wait_pred)) {
					if (callback) {
						callbacks.enqueue(std::move(*callback));
					}
					THROW(TimeOutError, "Database is not available");
				}
			} else {
				while (!readables_cond.wait_for(lk, 1s, wait_pred)) {}
			}
		} else {
			if (!wait_pred()) {
				if (callback) {
					callbacks.enqueue(std::move(*callback));
				}
				THROW(TimeOutError, "Database is not available");
			}
		}
	} while (true);
}


std::shared_ptr<Database>
DatabaseEndpoint::checkout(int flags, double timeout, std::packaged_task<void()>* callback)
{
	L_CALL("DatabaseEndpoint::checkout((%s), %f, %s)", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	auto now = std::chrono::system_clock::now();

	std::unique_lock<std::mutex> lk(mtx);

	if ((flags & DB_WRITABLE) == DB_WRITABLE) {
		return _writable_checkout(flags, timeout, callback, now, lk);
	} else {
		auto& database = _readable_checkout(flags, timeout, callback, now, lk);
		try {
			// Reopening of old/outdated (readable) databases:
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
						auto referenced_database_endpoint = database_pool.get(Endpoints{(*this)[i]});
						if (referenced_database_endpoint) {
							auto revision = referenced_database_endpoint->local_revision.load();
							referenced_database_endpoint.reset();
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
				// Discard old database and create a new one
				auto new_database = std::make_shared<Database>(*this, flags);
				new_database->busy = true;
				database = new_database;
			}
		} catch (...) {}
		return database;
	}
}


void
DatabaseEndpoint::checkin(std::shared_ptr<Database>& database) noexcept
{
	L_CALL("DatabaseEndpoint::checkin(%s)", database ? database->__repr__() : "null");

	ASSERT(database);
	ASSERT(database->is_busy());
	ASSERT(&database->endpoints == this);

	if (database->log) {
		database->log->clear();
		database->log.reset();
	}

	TaskQueue<void()> pending_callbacks;
	{
		std::lock_guard<std::mutex> lk(mtx);
		std::swap(pending_callbacks, callbacks);
	}

	if (database->is_writable()) {
		if (is_finished() || database_pool.notify_lockable(*this) || database->is_closed()) {
			std::lock_guard<std::mutex> lk(mtx);
			writable = nullptr;
			database_pool.checkin_clears_cond.notify_all();
		} else {
			Database::autocommit(database);
		}
		database->busy = false;
		writable_cond.notify_one();
	} else {
		if (is_finished() || database_pool.notify_lockable(*this) || database->is_closed()) {
			std::lock_guard<std::mutex> lk(mtx);
			auto it = std::find(readables.begin(), readables.end(), database);
			if (it != readables.end()) {
				readables.erase(it);
				database_pool.checkin_clears_cond.notify_all();
			}
		} else {
			++readables_available;
		}
		database->busy = false;
		readables_cond.notify_one();
	}

	database.reset();

	while (pending_callbacks.call()) {};
}


void
DatabaseEndpoint::finish()
{
	L_CALL("DatabaseEndpoint::finish()");

	finished = true;
	writable_cond.notify_all();
	readables_cond.notify_all();
}


std::pair<size_t, size_t>
DatabaseEndpoint::clear()
{
	L_CALL("DatabaseEndpoint::clear()");

	std::unique_lock<std::mutex> lk(mtx);

	if (writable) {
		if (!writable->busy.exchange(true)) {
			auto shared_writable = writable;
			std::weak_ptr<Database> weak_writable = shared_writable;
			writable.reset();
			lk.unlock();
			try {
				// If it's the last one,
				// reset() will close and delete the database object:
				shared_writable.reset();
			} catch (...) {
				L_WARNING("WARNING: Writable database deletion failed!");
			}
			lk.lock();
			if ((shared_writable = weak_writable.lock())) {
				// It wasn't the last one,
				// add it back:
				writable = shared_writable;
				writable->busy = false;
			}
		}
	}

	if (readables_available > 0) {
		for (auto it = readables.begin(); it != readables.end();) {
			auto& readable = *it;
			if (!readable) {
				--readables_available;
				it = readables.erase(it);
			} else if (!readable->busy.exchange(true)) {
				auto shared_readable = readable;
				std::weak_ptr<Database> weak_readable = shared_readable;
				readable.reset();
				lk.unlock();
				try {
					// If it's the last one,
					// reset() will close and delete the database object:
					shared_readable.reset();
				} catch (...) {
					L_WARNING("WARNING: Readable database deletion failed!");
				}
				lk.lock();
				if ((shared_readable = weak_readable.lock())) {
					// It wasn't the last one,
					// add it back:
					readable = shared_readable;
					readable->busy = false;
					++it;
				} else {
					// It was the last one,
					// erase it:
					--readables_available;
					it = readables.erase(it);
				}
			} else {
				++it;
			}
		}
	}

	return std::make_pair(writable ? 1 : 0, readables.size());
}


std::pair<size_t, size_t>
DatabaseEndpoint::count()
{
	L_CALL("DatabaseEndpoint::count()");

	std::lock_guard<std::mutex> lk(mtx);
	return std::make_pair(writable ? 1 : 0, readables.size());
}


bool
DatabaseEndpoint::empty() const
{
	L_CALL("DatabaseEndpoint::empty()");

	std::lock_guard<std::mutex> lk(mtx);

	return (
		refs == 0 &&
		!is_locked() &&
		!writable &&
		readables.empty()
	);
}


std::string
DatabaseEndpoint::__repr__() const
{
	return string::format("<DatabaseEndpoint {refs:%d} %s%s%s>",
		refs.load(),
		repr(to_string()),
		is_locked() ? " (locked)" : "",
		is_finished() ? " (finished)" : "");
}


std::string
DatabaseEndpoint::dump_databases(int level) const
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::lock_guard<std::mutex> lk(mtx);

	std::string ret;
	if (writable) {
		ret += indent;
		ret += writable->__repr__();
		ret.push_back('\n');
	}

	for (const auto& readable : readables) {
		ret += indent;
		ret += readable->__repr__();
		ret.push_back('\n');
	}
	return ret;
}


//  ____        _        _                    ____             _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
//

DatabasePool::DatabasePool(size_t dbpool_size, size_t max_databases) :
	LRU(dbpool_size),
	locks(0),
	max_databases(max_databases)
{
}


std::vector<ReferencedDatabaseEndpoint>
DatabasePool::endpoints() const
{
	std::vector<ReferencedDatabaseEndpoint> database_endpoints;

	std::lock_guard<std::mutex> lk(mtx);
	database_endpoints.reserve(size());
	for (auto& database_endpoint : *this) {
		database_endpoints.emplace_back(database_endpoint.second.get());
	}
	return database_endpoints;
}


std::vector<ReferencedDatabaseEndpoint>
DatabasePool::endpoints(const Endpoint& endpoint) const
{
	std::vector<ReferencedDatabaseEndpoint> database_endpoints;

	std::lock_guard<std::mutex> lk(mtx);
	auto it = endpoints_map.find(endpoint);
	if (it != endpoints_map.end()) {
		database_endpoints.reserve(it->second.size());
		for (auto& endpoints : it->second) {
			auto referenced_database_endpoint = _get(endpoints);
			if (referenced_database_endpoint) {
				database_endpoints.push_back(std::move(referenced_database_endpoint));
			}
		}
	}
	return database_endpoints;
}


void
DatabasePool::lock(const std::shared_ptr<Database>& database, double timeout)
{
	L_CALL("DatabasePool::lock(%s, %f)", database ? database->__repr__() : "null", timeout);

	ASSERT(database);

	if (!database->is_writable() || !database->is_local()) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for local writable databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	++locks;  // This needs to be done before locking
	if (database->endpoints.locked.exchange(true)) {
		ASSERT(locks > 0);
		--locks;  // revert if failed.
		L_DEBUG("ERROR: Exclusive lock can be granted only to non-locked databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	std::unique_lock<std::mutex> lk(mtx);

	auto is_ready_to_lock = [&] {
		bool is_ready = true;
		lk.unlock();
		for (auto& referenced_database_endpoint : endpoints(database->endpoints[0])) {
			if (referenced_database_endpoint->clear().second) {
				is_ready = false;
			}
		}
		lk.lock();
		return is_ready;
	};
	if (timeout > 0.0) {
		auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
		if (!database->endpoints.lockable_cond.wait_until(lk, timeout_tp, is_ready_to_lock)) {
			THROW(TimeOutError, "Cannot grant exclusive lock database");
		}
	} else {
		while (!database->endpoints.lockable_cond.wait_for(lk, 1s, is_ready_to_lock)) {
			if (database->endpoints.is_finished()) {
				THROW(TimeOutError, "Cannot grant exclusive lock database");
			}
		}
	}
}


void
DatabasePool::unlock(const std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::unlock(%s)", database ? database->__repr__() : "null");

	ASSERT(database);

	if (!database->is_writable() || !database->is_local()) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for local writable databases");
		THROW(Error, "Cannot grant exclusive lock database");
	}

	if (!database->endpoints.locked.exchange(false)) {
		L_DEBUG("ERROR: Exclusive lock can be released only from locked databases");
		THROW(Error, "Cannot release exclusive lock database");
	}

	ASSERT(locks > 0);
	--locks;

	for (auto& referenced_database_endpoint : endpoints(database->endpoints[0])) {
		referenced_database_endpoint->readables_cond.notify_all();
		referenced_database_endpoint.reset();
	}
}


bool
DatabasePool::notify_lockable(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::notify_lockable(%s)", repr(endpoints.to_string()));

	bool locked = false;

	if (locks) {
		std::lock_guard<std::mutex> lk(mtx);

		for (auto& endpoint : endpoints) {
			auto it = find(Endpoints{endpoint});
			if (it != end()) {
				auto& database_endpoint = it->second;
				if (database_endpoint->is_locked()) {
					database_endpoint->lockable_cond.notify_one();
					locked = true;
				}
			}
		}
	}

	return locked;
}


bool
DatabasePool::is_locked(const Endpoints& endpoints) const
{
	L_CALL("DatabasePool::is_locked(%s)", repr(endpoints.to_string()));

	if (locks) {
		std::lock_guard<std::mutex> lk(mtx);

		for (auto& endpoint : endpoints) {
			auto it = find(Endpoints{endpoint});
			if (it != end()) {
				if (it->second->is_locked()) {
					return true;
				}
			}
		}
	}

	return false;
}


void
DatabasePool::_lot_endpoints(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::_lot_endpoints(%s)", repr(endpoints.to_string()));

	for (auto& endpoint : endpoints) {
		auto& endpoints_set = endpoints_map[endpoint];
		endpoints_set.insert(endpoints);
	}
}


void
DatabasePool::_drop_endpoints(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::_drop_endpoints(%s)", repr(endpoints.to_string()));

	for (auto& endpoint : endpoints) {
		auto &endpoints_set = endpoints_map[endpoint];
		endpoints_set.erase(endpoints);
		if (endpoints_set.empty()) {
			endpoints_map.erase(endpoint);
		}
	}
}


ReferencedDatabaseEndpoint
DatabasePool::_spawn(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::_spawn(%s)", repr(endpoints.to_string()));

	DatabaseEndpoint* database_endpoint;

	// Find or spawn the database endpoint
	auto it = find_and([&](const std::unique_ptr<DatabaseEndpoint>& database_endpoint) {
		database_endpoint->renew_time = std::chrono::system_clock::now();
		return lru::GetAction::renew;
	}, endpoints);
	if (it == end()) {
		auto emplaced = emplace_and([&](const std::unique_ptr<DatabaseEndpoint>&, ssize_t, ssize_t) {
			return lru::DropAction::stop;
		}, endpoints, std::make_unique<DatabaseEndpoint>(*this, endpoints));
		database_endpoint = emplaced.first->second.get();
		_lot_endpoints(*database_endpoint);
	} else {
		database_endpoint = it->second.get();
	}

	// Return a busy database endpoint so it cannot get deleted while the object exists
	return ReferencedDatabaseEndpoint(database_endpoint);
}


ReferencedDatabaseEndpoint
DatabasePool::spawn(const Endpoints& endpoints)
{
	L_CALL("DatabasePool::spawn(%s)", repr(endpoints.to_string()));

	std::lock_guard<std::mutex> lk(mtx);
	return _spawn(endpoints);
}


ReferencedDatabaseEndpoint
DatabasePool::_get(const Endpoints& endpoints) const
{
	L_CALL("DatabasePool::_get(%s)", repr(endpoints.to_string()));

	DatabaseEndpoint* database_endpoint = nullptr;

	auto it = find(endpoints);
	if (it != end()) {
		database_endpoint = it->second.get();
	}

	// Return a busy database endpoint so it cannot get deleted while the object exists
	return ReferencedDatabaseEndpoint(database_endpoint);
}


ReferencedDatabaseEndpoint
DatabasePool::get(const Endpoints& endpoints) const
{
	L_CALL("DatabasePool::get(%s)", repr(endpoints.to_string()));

	std::lock_guard<std::mutex> lk(mtx);
	return _get(endpoints);
}


std::shared_ptr<Database>
DatabasePool::checkout(const Endpoints& endpoints, int flags, double timeout, std::packaged_task<void()>* callback)
{
	L_CALL("DatabasePool::checkout(%s, (%s), %f)", repr(endpoints.to_string()), readable_flags(flags), timeout);

	if (endpoints.empty()) {
		L_DEBUG("ERROR: Expecting at least one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout empty database");
	}

	bool is_writable = (flags & DB_WRITABLE) == DB_WRITABLE;

	if (is_writable && endpoints.size() != 1) {
		L_DEBUG("ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout writable multi-database");
	}

	auto database = spawn(endpoints)->checkout(flags, timeout, callback);
	ASSERT(database);

	L_TIMED_VAR(database->log, 200ms,
		"Database checkout is taking too long: %s (%s)%s%s%s",
		"Database checked out for too long: %s (%s)%s%s%s",
		repr(database->endpoints.to_string()),
		readable_flags(database->flags),
		database->is_writable() ? " (writable)" : "",
		database->is_wal_active() ? " (active WAL)" : "",
		database->is_local() ? " (local)" : "");

	return database;
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::checkin(%s)", database ? database->__repr__() : "null");

	return database->endpoints.checkin(database);
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	std::lock_guard<std::mutex> lk(mtx);

	for (auto& database_endpoint : *this) {
		database_endpoint.second->finish();
	}
}


bool
DatabasePool::join(const std::chrono::time_point<std::chrono::system_clock>& wakeup)
{
	L_CALL("DatabasePool::join(<timeout>)");

	std::unique_lock<std::mutex> lk(mtx);

	auto is_cleared = [&] {
		lk.unlock();
		auto cleared = clear();
		lk.lock();
		return cleared;
	};

	if (!checkin_clears_cond.wait_until(lk, wakeup, is_cleared)) {
		return false;
	}

	return true;
}


void
DatabasePool::cleanup(bool immediate)
{
	L_CALL("DatabasePool::cleanup()");

	auto now = std::chrono::system_clock::now();

	std::lock_guard<std::mutex> cleanup_lk(cleanup_mtx);

	std::unique_lock<std::mutex> lk(mtx);

	const auto on_drop = [&](const std::unique_ptr<DatabaseEndpoint>& database_endpoint, ssize_t size, ssize_t max_size) {
		if (size > max_size) {
			if (database_endpoint->renew_time < now - (immediate ? 10s : 60s)) {
				ReferencedDatabaseEndpoint referenced_database_endpoint(database_endpoint.get());
				lk.unlock();
				referenced_database_endpoint->clear();
				lk.lock();
				referenced_database_endpoint.reset();
				if (!database_endpoint->empty()) {
					L_DATABASE("Leave used endpoint: %s", repr(database_endpoint->to_string()));
					return lru::DropAction::leave;
				}
				_drop_endpoints(*database_endpoint);
				L_DATABASE("Evict endpoint from full LRU: %s", repr(database_endpoint->to_string()));
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used endpoint: %s", repr(database_endpoint->to_string()));
			return lru::DropAction::leave;
		}
		if (database_endpoint->renew_time < now - (immediate ? 10s : 3600s)) {
			ReferencedDatabaseEndpoint referenced_database_endpoint(database_endpoint.get());
			lk.unlock();
			referenced_database_endpoint->clear();
			lk.lock();
			referenced_database_endpoint.reset();
			if (!database_endpoint->empty()) {
				L_DATABASE("Leave used endpoint: %s", repr(database_endpoint->to_string()));
				return lru::DropAction::leave;
			}
			_drop_endpoints(*database_endpoint);
			L_DATABASE("Evict endpoint: %s", repr(database_endpoint->to_string()));
			return lru::DropAction::evict;
		}
		L_DATABASE("Stop at endpoint: %s", repr(database_endpoint->to_string()));
		return lru::DropAction::stop;
	};
	trim(on_drop);
}


bool
DatabasePool::clear()
{
	L_CALL("DatabasePool::clear()");

	bool cleared = true;

	for (auto& referenced_database_endpoint : endpoints()) {
		auto count = referenced_database_endpoint->clear();
		referenced_database_endpoint.reset();
		if (count.first || count.second) {
			cleared = false;
		}
	}

	if (!cleared) {
		return false;
	}

	// Now lock to double-check and really clear the LRU:
	std::lock_guard<std::mutex> lk(mtx);

	for (auto& database_endpoint : *this) {
		auto count = database_endpoint.second->count();
		if (count.first || count.second) {
			return false;
		}
	}

	LRU::clear();
	return true;
}


std::pair<size_t, size_t>
DatabasePool::count()
{
	L_CALL("DatabasePool::count()");

	size_t endpoints_count = 0;
	size_t databases_count = 0;
	for (auto& referenced_database_endpoint : endpoints()) {
		++endpoints_count;
		auto count = referenced_database_endpoint->count();
		databases_count += count.first + count.second;
	}
	return std::make_pair(endpoints_count, databases_count);
}


std::string
DatabasePool::__repr__() const
{
	return string::format("<DatabasePool {locks:%d}>",
		locks.load());
}


std::string
DatabasePool::dump_databases(int level) const
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret;
	ret += indent;
	ret += __repr__();
	ret.push_back('\n');

	for (auto& referenced_database_endpoint : endpoints()) {
		ret += indent + indent;
		ret += referenced_database_endpoint->__repr__();
		ret.push_back('\n');
		ret += referenced_database_endpoint->dump_databases(level + 2);
		referenced_database_endpoint.reset();
	}
	return ret;
}
