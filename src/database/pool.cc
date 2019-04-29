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

#include "database/pool.h"

#include <algorithm>              // for std::find, std::move
#include <cassert>                // for assert

#include "database/flags.h"       // for readable_flags
#include "database/shard.h"       // for Shard
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "log.h"                  // for L_CALL
#include "logger.h"               // for Logging (database->log)

#define L_POOL_TIMED L_NOTHING
#define L_POOL_TIMED_CLEAR L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_POOL_TIMED_CLEAR
// #define L_POOL_TIMED_CLEAR() { \
// 	if (shard->log) { \
// 		shard->log->clear(); \
// 		shard->log.reset(); \
// 	} \
// }
// #undef L_POOL_TIMED
// #define L_POOL_TIMED(delay, format_timeout, format_done, ...) { \
// 	if (shard->log) { \
// 		shard->log->clear(); \
// 		shard->log.reset(); \
// 	} \
// 	auto __log_timed = L_DELAYED(true, (delay), LOG_WARNING, WARNING_COL, (format_timeout), ##__VA_ARGS__); \
// 	__log_timed.L_DELAYED_UNLOG(LOG_NOTICE, NOTICE_COL, (format_done), ##__VA_ARGS__); \
// 	shard->log = __log_timed.release(); \
// }


#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10


class ReferencedShardEndpoint {
	ShardEndpoint* ptr;

public:
	ReferencedShardEndpoint(ShardEndpoint* ptr) : ptr(ptr) {
		if (ptr) {
			++ptr->refs;
		}
	}

	ReferencedShardEndpoint(const ReferencedShardEndpoint& other) = delete;
	ReferencedShardEndpoint& operator=(const ReferencedShardEndpoint& other) = delete;

	ReferencedShardEndpoint(ReferencedShardEndpoint&& other) : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	ReferencedShardEndpoint& operator=(ReferencedShardEndpoint&& other) {
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	~ReferencedShardEndpoint() noexcept {
		if (ptr) {
			assert(ptr->refs > 0);
			--ptr->refs;
		}
	}

	void reset() {
		if (ptr) {
			assert(ptr->refs > 0);
			--ptr->refs;
			ptr = nullptr;
		}
	}

	operator bool() const {
		return !!ptr;
	}

	ShardEndpoint* operator->() const {
		return ptr;
	}
};


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 */

ShardEndpoint::ShardEndpoint(DatabasePool& database_pool, const Endpoint& endpoint) :
	Endpoint(endpoint),
	database_pool(database_pool),
	refs(0),
	finished(false),
	locked(false),
	local_revision(0),
	renew_time(std::chrono::steady_clock::now()),
	readables_available(0)
{
}


ShardEndpoint::~ShardEndpoint()
{
	assert(refs == 0);
}


std::shared_ptr<Shard>&
ShardEndpoint::_writable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::steady_clock>& now, std::unique_lock<std::mutex>& lk)
{
	L_CALL("ShardEndpoint::_writable_checkout(({}), {}, {})", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	do {
		if (is_finished()) {
			if (callback) {
				callbacks.enqueue(std::move(*callback));
			}
			throw Xapian::DatabaseNotAvailableError("Shard is not available");
		}
		if (!writable) {
			writable = std::make_shared<Shard>(*this, flags);
		}
		if (!is_locked() && !writable->busy.exchange(true)) {
			writable->flags = flags;  // update shard flags
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
					throw Xapian::DatabaseNotAvailableError("Shard is not available");
				}
			} else {
				while (!writable_cond.wait_for(lk, 1s, wait_pred)) {}
			}
		} else {
			if (!wait_pred()) {
				if (callback) {
					callbacks.enqueue(std::move(*callback));
				}
				throw Xapian::DatabaseNotAvailableError("Shard is not available");
			}
		}
	} while (true);
}


std::shared_ptr<Shard>&
ShardEndpoint::_readable_checkout(int flags, double timeout, std::packaged_task<void()>* callback, const std::chrono::time_point<std::chrono::steady_clock>& now, std::unique_lock<std::mutex>& lk)
{
	L_CALL("ShardEndpoint::_readable_checkout(({}), {}, {})", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	do {
		if (is_finished()) {
			if (callback) {
				callbacks.enqueue(std::move(*callback));
			}
			throw Xapian::DatabaseNotAvailableError("Shard is not available");
		}
		if (readables_available > 0) {
			for (auto& readable : readables) {
				if (!readable) {
					readable = std::make_shared<Shard>(*this, flags);
				}
				if (!is_locked() && !readable->busy.exchange(true)) {
					readable->flags = flags;  // update shard flags
					--readables_available;
					return readable;
				}
			}
		}
		if (readables.size() < database_pool.max_database_readers) {
			auto new_database = std::make_shared<Shard>(*this, flags);
			auto& readable = *readables.insert(readables.end(), new_database);
			++readables_available;
			if (!is_locked() && !readable->busy.exchange(true)) {
				readable->flags = flags;  // update shard flags
				--readables_available;
				return readable;
			}
		}
		auto wait_pred = [&]() {
			return is_finished() || ((readables_available > 0 || readables.size() < database_pool.max_database_readers) && !is_locked() && !database_pool.is_locked(*this));
		};
		if (timeout) {
			if (timeout > 0.0) {
				auto timeout_tp = now + std::chrono::duration<double>(timeout);
				if (!readables_cond.wait_until(lk, timeout_tp, wait_pred)) {
					if (callback) {
						callbacks.enqueue(std::move(*callback));
					}
					throw Xapian::DatabaseNotAvailableError("Shard is not available");
				}
			} else {
				while (!readables_cond.wait_for(lk, 1s, wait_pred)) {}
			}
		} else {
			if (!wait_pred()) {
				if (callback) {
					callbacks.enqueue(std::move(*callback));
				}
				throw Xapian::DatabaseNotAvailableError("Shard is not available");
			}
		}
	} while (true);
}


std::shared_ptr<Shard>
ShardEndpoint::checkout(int flags, double timeout, std::packaged_task<void()>* callback)
{
	L_CALL("ShardEndpoint::checkout(({}), {}, {})", readable_flags(flags), timeout, callback ? "<callback>" : "null");

	auto now = std::chrono::steady_clock::now();

	std::unique_lock<std::mutex> lk(mtx);

	if ((flags & DB_WRITABLE) == DB_WRITABLE) {
		return _writable_checkout(flags, timeout, callback, now, lk);
	} else {
		auto& shard = _readable_checkout(flags, timeout, callback, now, lk);
		lk.unlock();
		try {
			// Reopening of old/outdated (readable) databases:
			bool reopen = false;
			auto reopen_age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - shard->reopen_time).count();
			if (reopen_age >= LOCAL_DATABASE_UPDATE_TIME) {
				L_DATABASE("Shard is just too old, reopen");
				reopen = true;
			} else {
				if (shard->is_local()) {
					auto referenced_database_endpoint = database_pool.get(*this);
					if (referenced_database_endpoint) {
						auto revision = referenced_database_endpoint->local_revision.load();
						referenced_database_endpoint.reset();
						if (revision && revision != shard->db()->get_revision()) {
							L_DATABASE("Local writable shard has changed revision");
							reopen = true;
						}
					}
				} else {
					if (reopen_age >= REMOTE_DATABASE_UPDATE_TIME) {
						L_DATABASE("Remote shard is too old, reopen");
						reopen = true;
					}
				}
			}
			if (reopen) {
				// Create a new shard and discard old one
				auto new_database = std::make_shared<Shard>(*this, flags);
				new_database->busy.store(true);
				lk.lock();
				shard = new_database;
			}
		} catch (...) {
			L_WARNING("WARNING: Readable shard reopening failed: {}", to_string());
		}
		return shard;
	}
}


void
ShardEndpoint::checkin(std::shared_ptr<Shard>& shard) noexcept
{
	L_CALL("ShardEndpoint::checkin({})", shard ? shard->__repr__() : "null");

	assert(shard);
	assert(shard->is_busy());
	assert(&shard->endpoint == this);

	TaskQueue<void()> pending_callbacks;
	{
		std::lock_guard<std::mutex> lk(mtx);
		std::swap(pending_callbacks, callbacks);
	}

	if (shard->is_writable()) {
		if (is_finished() || database_pool.notify_lockable(*this) || shard->is_closed()) {
			std::lock_guard<std::mutex> lk(mtx);
			writable.reset();
			database_pool.checkin_clears_cond.notify_all();
		} else {
			Shard::autocommit(shard);
		}
		L_POOL_TIMED_CLEAR();
		shard->busy.store(false);
		writable_cond.notify_one();
	} else {
		if (is_finished() || database_pool.notify_lockable(*this) || shard->is_closed()) {
			std::lock_guard<std::mutex> lk(mtx);
			auto it = std::find(readables.begin(), readables.end(), shard);
			if (it != readables.end()) {
				readables.erase(it);
				database_pool.checkin_clears_cond.notify_all();
			}
		} else {
			++readables_available;
		}
		L_POOL_TIMED_CLEAR();
		shard->busy.store(false);
		readables_cond.notify_one();
	}

	shard.reset();

	while (pending_callbacks.call()) {};
}


void
ShardEndpoint::finish()
{
	L_CALL("ShardEndpoint::finish()");

	finished = true;
	writable_cond.notify_all();
	readables_cond.notify_all();
}


std::pair<size_t, size_t>
ShardEndpoint::clear()
{
	L_CALL("ShardEndpoint::clear()");

	std::unique_lock<std::mutex> lk(mtx);

	if (writable) {
		if (!writable->busy.exchange(true)) {
			lk.unlock();
			// First try closing internal shard:
			writable->do_close(true, writable->is_closed(), writable->transaction, false);
			lk.lock();
			auto shared_writable = writable;
			std::weak_ptr<Shard> weak_writable = shared_writable;
			writable.reset();
			lk.unlock();
			try {
				// If it's the last one, reset() will delete the shard object:
				shared_writable.reset();
			} catch (...) {
				L_WARNING("WARNING: Writable shard deletion failed: {}", to_string());
			}
			lk.lock();
			if ((shared_writable = weak_writable.lock())) {
				// It wasn't the last one,
				// add it back:
				writable = shared_writable;
				writable->busy.store(false);
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
				lk.unlock();
				// First try closing internal shard:
				readable->do_close(true, readable->is_closed(), readable->transaction, false);
				lk.lock();
				auto shared_readable = readable;
				std::weak_ptr<Shard> weak_readable = shared_readable;
				readable.reset();
				lk.unlock();
				try {
					// If it's the last one, reset() will delete the shard object:
					shared_readable.reset();
				} catch (...) {
					L_WARNING("WARNING: Readable shard deletion failed: {}", to_string());
				}
				lk.lock();
				if ((shared_readable = weak_readable.lock())) {
					// It wasn't the last one,
					// add it back:
					readable = shared_readable;
					readable->busy.store(false);
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
ShardEndpoint::count()
{
	L_CALL("ShardEndpoint::count()");

	std::lock_guard<std::mutex> lk(mtx);
	return std::make_pair(writable ? 1 : 0, readables.size());
}


bool
ShardEndpoint::is_used() const
{
	L_CALL("ShardEndpoint::is_used()");

	std::lock_guard<std::mutex> lk(mtx);

	return (
		refs != 0 ||
		is_locked() ||
		writable ||
		!readables.empty()
	);
}


std::string
ShardEndpoint::__repr__() const
{
	return strings::format(STEEL_BLUE + "<ShardEndpoint {{refs:{}}} {}{}{}>",
		refs.load(),
		repr(to_string()),
		is_locked() ? " " + RED + "(locked)" + STEEL_BLUE : "",
		is_finished() ? " " + ORANGE + "(finished)" + STEEL_BLUE : "");
}


std::string
ShardEndpoint::dump_databases(int level) const
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


/*
 *  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */

DatabasePool::DatabasePool(size_t database_pool_size, size_t max_database_readers) :
	lru::lru(database_pool_size),
	locks(0),
	max_database_readers(max_database_readers)
{
}


std::vector<ReferencedShardEndpoint>
DatabasePool::endpoints() const
{
	std::vector<ReferencedShardEndpoint> database_endpoints;

	std::lock_guard<std::mutex> lk(mtx);
	database_endpoints.reserve(size());
	for (auto& endpoint_database_endpoint : *this) {
		database_endpoints.emplace_back(endpoint_database_endpoint.second.get());
	}
	return database_endpoints;
}


void
DatabasePool::lock(const std::shared_ptr<Shard>& shard, double timeout)
{
	L_CALL("DatabasePool::lock({}, {})", shard ? shard->__repr__() : "null", timeout);

	assert(shard);

	if (!shard->is_writable() || !shard->is_local()) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for local writable databases");
		THROW(Error, "Cannot grant exclusive lock shard");
	}

	++locks;  // This needs to be done before locking
	if (shard->endpoint.locked.exchange(true)) {
		assert(locks > 0);
		--locks;  // revert if failed.
		L_DEBUG("ERROR: Exclusive lock can be granted only to non-locked databases");
		THROW(Error, "Cannot grant exclusive lock shard");
	}

	std::unique_lock<std::mutex> lk(mtx);

	auto is_ready_to_lock = [&] {
		bool is_ready = true;
		lk.unlock();
		auto referenced_database_endpoint = get(shard->endpoint);
		if (referenced_database_endpoint->clear().second) {
			is_ready = false;
		}
		lk.lock();
		return is_ready;
	};
	if (timeout > 0.0) {
		auto timeout_tp = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout);
		if (!shard->endpoint.lockable_cond.wait_until(lk, timeout_tp, is_ready_to_lock)) {
			throw Xapian::DatabaseNotAvailableError("Cannot grant exclusive lock shard");
		}
	} else {
		while (!shard->endpoint.lockable_cond.wait_for(lk, 1s, is_ready_to_lock)) {
			if (shard->endpoint.is_finished()) {
				throw Xapian::DatabaseNotAvailableError("Cannot grant exclusive lock shard");
			}
		}
	}
}


void
DatabasePool::unlock(const std::shared_ptr<Shard>& shard)
{
	L_CALL("DatabasePool::unlock({})", shard ? shard->__repr__() : "null");

	assert(shard);

	if (!shard->is_writable() || !shard->is_local()) {
		L_DEBUG("ERROR: Exclusive lock can be granted only for local writable databases");
		THROW(Error, "Cannot grant exclusive lock shard");
	}

	if (!shard->endpoint.locked.exchange(false)) {
		L_DEBUG("ERROR: Exclusive lock can be released only from locked databases");
		THROW(Error, "Cannot release exclusive lock shard");
	}

	assert(locks > 0);
	--locks;

	auto referenced_database_endpoint = get(shard->endpoint);
	referenced_database_endpoint->readables_cond.notify_all();
	referenced_database_endpoint.reset();
}


bool
DatabasePool::notify_lockable(const Endpoint& endpoint)
{
	L_CALL("DatabasePool::notify_lockable({})", repr(endpoint.to_string()));

	bool locked = false;

	if (locks) {
		std::lock_guard<std::mutex> lk(mtx);

		auto it = find_and_leave(endpoint);
		if (it != end()) {
			auto& database_endpoint = it->second;
			if (database_endpoint->is_locked()) {
				database_endpoint->lockable_cond.notify_one();
				locked = true;
			}
		}
	}

	return locked;
}


bool
DatabasePool::is_locked(const Endpoint& endpoint) const
{
	L_CALL("DatabasePool::is_locked({})", repr(endpoint.to_string()));

	if (locks) {
		std::lock_guard<std::mutex> lk(mtx);

		auto it = find_and_leave(endpoint);
		if (it != end()) {
			if (it->second->is_locked()) {
				return true;
			}
		}
	}

	return false;
}


ReferencedShardEndpoint
DatabasePool::_spawn(const Endpoint& endpoint)
{
	L_CALL("DatabasePool::_spawn({})", repr(endpoint.to_string()));

	ShardEndpoint* database_endpoint;

	// Find or spawn the shard endpoint
	auto it = find_and([&](const std::pair<const Endpoint, std::unique_ptr<ShardEndpoint>>& endpoint_database_endpoint, bool, bool) {
		assert(endpoint_database_endpoint.second);
		endpoint_database_endpoint.second->renew_time = std::chrono::steady_clock::now();
		return lru::GetAction::renew;
	}, endpoint);
	if (it == end()) {
		auto emplaced = emplace_and([&](const std::pair<const Endpoint, std::unique_ptr<ShardEndpoint>>&, bool, bool) {
			return lru::DropAction::stop;
		}, endpoint, std::make_unique<ShardEndpoint>(*this, endpoint));
		database_endpoint = emplaced.first->second.get();
	} else {
		database_endpoint = it->second.get();
	}

	// Return a referenced shard endpoint so it cannot get deleted while the object exists
	return ReferencedShardEndpoint(database_endpoint);
}


ReferencedShardEndpoint
DatabasePool::spawn(const Endpoint& endpoint)
{
	L_CALL("DatabasePool::spawn({})", repr(endpoint.to_string()));

	std::lock_guard<std::mutex> lk(mtx);
	return _spawn(endpoint);
}


ReferencedShardEndpoint
DatabasePool::_get(const Endpoint& endpoint) const
{
	L_CALL("DatabasePool::_get({})", repr(endpoint.to_string()));

	ShardEndpoint* database_endpoint = nullptr;

	auto it = find_and_leave(endpoint);
	if (it != end()) {
		database_endpoint = it->second.get();
	}

	// Return a referenced shard endpoint so it cannot get deleted while the object exists
	return ReferencedShardEndpoint(database_endpoint);
}


ReferencedShardEndpoint
DatabasePool::get(const Endpoint& endpoint) const
{
	L_CALL("DatabasePool::get({})", repr(endpoint.to_string()));

	std::lock_guard<std::mutex> lk(mtx);
	return _get(endpoint);
}


std::shared_ptr<Shard>
DatabasePool::checkout(const Endpoint& endpoint, int flags, double timeout, std::packaged_task<void()>* callback)
{
	L_CALL("DatabasePool::checkout({}, ({}), {})", repr(endpoint.to_string()), readable_flags(flags), timeout);

	auto shard = spawn(endpoint)->checkout(flags, timeout, callback);
	assert(shard);

	L_POOL_TIMED(200ms,
		"Shard checkout is taking too long: {} ({})",
		"Shard checked out for too long: {} ({})",
		repr(shard->to_string()),
		readable_flags(shard->flags));

	return shard;
}


void
DatabasePool::checkin(std::shared_ptr<Shard>& shard)
{
	L_CALL("DatabasePool::checkin({})", shard ? shard->__repr__() : "null");

	assert(shard);
	shard->endpoint.checkin(shard);
	shard.reset();
}


std::vector<std::shared_ptr<Shard>>
DatabasePool::checkout(const Endpoints& endpoints, int flags, double timeout)
{
	L_CALL("DatabasePool::checkout({}, ({}), {})", repr(endpoints.to_string()), readable_flags(flags), timeout);

	if (endpoints.empty()) {
		L_DEBUG("ERROR: Expecting at least one database, {} requested: {}", endpoints.size(), repr(endpoints.to_string()));
		throw Xapian::DatabaseOpeningError("Cannot checkout empty database");
	}

	std::vector<std::shared_ptr<Shard>> shards;
	shards.reserve(endpoints.size());
	try {
		for (auto& endpoint : endpoints) {
			auto shard = spawn(endpoint)->checkout(flags, timeout);
			assert(shard);
			shards.emplace_back(std::move(shard));
		}
		return shards;
	} catch (...) {
		// Unable to checkout all requested shards, checkin
		checkin(shards);
		throw;
	}
}


void
DatabasePool::checkin(std::vector<std::shared_ptr<Shard>>& shards)
{
	L_CALL("DatabasePool::checkin(<shards>)");

	for (auto& shard : shards) {
		if (shard) {
			try {
				shard->endpoint.checkin(shard);
			} catch (...) {
				L_EXC("Unable to checkin shard: {}", shard->endpoint.to_string());
			}
		}
	}
	shards.clear();
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	std::lock_guard<std::mutex> lk(mtx);

	for (auto& endpoint_database_endpoint : *this) {
		endpoint_database_endpoint.second->finish();
	}
}


bool
DatabasePool::join(const std::chrono::time_point<std::chrono::steady_clock>& wakeup)
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

	auto now = std::chrono::steady_clock::now();

	std::unique_lock<std::mutex> lk(mtx);

	trim_and([&](const std::pair<const Endpoint, std::unique_ptr<ShardEndpoint>>& endpoint_database_endpoint, bool overflowed, bool) {
		assert(endpoint_database_endpoint.second);
		if (overflowed) {
			if (immediate || endpoint_database_endpoint.second->renew_time < now - 60s) {
				ReferencedShardEndpoint referenced_database_endpoint(endpoint_database_endpoint.second.get());
				lk.unlock();
				referenced_database_endpoint->clear();
				lk.lock();
				referenced_database_endpoint.reset();
				if (endpoint_database_endpoint.second->is_used()) {
					L_DATABASE("Leave used endpoint: {}", repr(endpoint_database_endpoint.second->to_string()));
					return lru::DropAction::leave;
				}
				L_DATABASE("Evict endpoint from full LRU: {}", repr(endpoint_database_endpoint.second->to_string()));
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used endpoint: {}", repr(endpoint_database_endpoint.second->to_string()));
			return lru::DropAction::leave;
		}
		if (immediate || endpoint_database_endpoint.second->renew_time < now - 3600s) {
			ReferencedShardEndpoint referenced_database_endpoint(endpoint_database_endpoint.second.get());
			lk.unlock();
			referenced_database_endpoint->clear();
			lk.lock();
			referenced_database_endpoint.reset();
			if (endpoint_database_endpoint.second->is_used()) {
				L_DATABASE("Leave used endpoint: {}", repr(endpoint_database_endpoint.second->to_string()));
				return lru::DropAction::leave;
			}
			L_DATABASE("Evict endpoint: {}", repr(endpoint_database_endpoint.second->to_string()));
			return lru::DropAction::evict;
		}
		L_DATABASE("Stop at endpoint: {}", repr(endpoint_database_endpoint.second->to_string()));
		return lru::DropAction::stop;
	});
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

	for (auto& endpoint_database_endpoint : *this) {
		auto count = endpoint_database_endpoint.second->count();
		if (count.first || count.second) {
			return false;
		}
	}

	lru::lru::clear();
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
	return strings::format(STEEL_BLUE + "<DatabasePool {{locks:{}}}>",
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
