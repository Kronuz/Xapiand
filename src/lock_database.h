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

#include <xapian.h>             // for Xapian::Database

#include "endpoint.h"           // for Endpoints
#include "manager.h"            // for XapiandManager::manager


class Database;
class LockableDatabase;


class lock_database {
	LockableDatabase* lockable;
	int locks;

	lock_database(const lock_database&) = delete;
	lock_database& operator=(const lock_database&) = delete;

public:
	lock_database(LockableDatabase* lockable);
	~lock_database();

	void unlock();

	template <typename... Args>
	void lock(Args&&... args);
};


class LockableDatabase {
	friend lock_database;

	std::shared_ptr<Database> _locked_database;
	int _database_locks;

protected:
	int flags;
	Endpoints endpoints;

	const std::shared_ptr<Database>& database() const noexcept;
	Xapian::Database* db() const noexcept;

public:
	LockableDatabase();
	LockableDatabase(const Endpoints& endpoints_, int flags_);
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
			assert(locks == 0 && lockable->_database_locks == 0);
			assert(XapiandManager::manager);
			XapiandManager::manager->database_pool.checkout(lockable->_locked_database, lockable->endpoints, lockable->flags, std::forward<Args>(args)...);
		}
		if (locks++ == 0) {
			++lockable->_database_locks;
		}
	}
}
