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

#include "cassert.h"            // for ASSERT
#include "database_pool.h"      // for DatabasePool (database_pool)
#include "endpoint.h"           // for Endpoints
#include "manager.h"            // for XapiandManager
#include "xapian.h"             // for Xapian::Database


class Database;


class LockableDatabase {
	friend class lock_database;

	std::shared_ptr<Database> _locked_database;
	int _database_locks;

protected:
	int flags;
	Endpoints endpoints;

public:
	const std::shared_ptr<Database>& database() const noexcept;
	Xapian::Database* db() const noexcept;

	LockableDatabase();
	LockableDatabase(const Endpoints& endpoints_, int flags_);
};


class lock_database {
	LockableDatabase* lockable;
	int locks;

	lock_database(const lock_database&) = delete;
	lock_database& operator=(const lock_database&) = delete;

public:
	lock_database(LockableDatabase* lockable);
	~lock_database() noexcept;

	void unlock() noexcept;

	template <typename... Args>
	void lock(Args&&... args);
};


template <typename... Args>
inline void
lock_database::lock(Args&&... args)
{
	if (lockable != nullptr) {
		if (lockable->endpoints.empty()) {
			return;
		}
		if (!lockable->_locked_database) {
			ASSERT(locks == 0 && lockable->_database_locks == 0);
			lockable->_locked_database = XapiandManager::database_pool()->checkout(lockable->endpoints, lockable->flags, std::forward<Args>(args)...);
		}
		if (locks++ == 0) {
			++lockable->_database_locks;
		}
	}
}


class lock_db {
	std::shared_ptr<Database> _locked;
	int _locks;

	lock_db(const lock_db&) = delete;
	lock_db& operator=(const lock_db&) = delete;

public:
	const int flags;
	const Endpoints endpoints;

	lock_db(const Endpoints& endpoints, int flags, bool do_lock = true) :
		_locks(0),
		flags(flags),
		endpoints(endpoints)
	{
		if (do_lock) {
			lock();
		}
	}

	~lock_db() noexcept
	{
		while (_locks) {
			unlock();
		}
	}

	template <typename... Args>
	const std::shared_ptr<Database> lock(Args&&... args)
	{
		if (!_locked) {
			ASSERT(_locks == 0);
			_locked = XapiandManager::database_pool()->checkout(endpoints, flags, std::forward<Args>(args)...);
		}
		++_locks;
		return _locked;
	}

	void unlock() noexcept
	{
		if (_locks > 0 && --_locks == 0) {
			ASSERT(_locked);
			XapiandManager::database_pool()->checkin(_locked);
		}
	}

	Database& operator*() const noexcept
	{
		ASSERT(_locked);
		return *_locked;
	}

	Database* operator->() const noexcept
	{
		ASSERT(_locked);
		return _locked.get();
	}

	const std::shared_ptr<Database> locked()
	{
		return _locked;
	}
};


class lock_shard {
	std::shared_ptr<Shard> _locked;
	int _locks;

	lock_shard(const lock_shard&) = delete;
	lock_shard& operator=(const lock_shard&) = delete;

public:
	const int flags;
	const Endpoint endpoint;

	lock_shard(const Endpoint& endpoint, int flags, bool do_lock = true) :
		_locks(0),
		flags(flags),
		endpoint(endpoint)
	{
		if (do_lock) {
			lock();
		}
	}

	~lock_shard() noexcept
	{
		while (_locks) {
			unlock();
		}
	}

	template <typename... Args>
	const std::shared_ptr<Shard> lock(Args&&... args)
	{
		if (!_locked) {
			ASSERT(_locks == 0);
			_locked = XapiandManager::database_pool()->checkout(endpoint, flags, std::forward<Args>(args)...);
		}
		++_locks;
		return _locked;
	}

	void unlock() noexcept
	{
		if (_locks > 0 && --_locks == 0) {
			ASSERT(_locked);
			XapiandManager::database_pool()->checkin(_locked);
		}
	}

	Shard& operator*() const noexcept
	{
		ASSERT(_locked);
		return *_locked;
	}

	Shard* operator->() const noexcept
	{
		ASSERT(_locked);
		return _locked.get();
	}

	const std::shared_ptr<Shard> locked()
	{
		return _locked;
	}
};
