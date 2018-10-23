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

#include "database.h"
#include "endpoint.h"
#include "manager.h"


class LockableDatabase;


class lock_database {
	LockableDatabase* lockable;
	int locks;

	lock_database(const lock_database&) = delete;
	lock_database& operator=(const lock_database&) = delete;

	template <bool internal>
	void _lock();

	template <bool internal>
	void _unlock();

public:
	lock_database(LockableDatabase* lockable)
		: lockable(lockable),
		  locks(0) {
			_lock<true>();
		}

	~lock_database() {
		while (locks) {
			_unlock<true>();
		}
	}

	void lock() {
		_lock<false>();
	}
	void unlock() {
		_unlock<false>();
	}
};


class LockableDatabase {
	friend lock_database;

protected:
	Endpoints endpoints;
	std::shared_ptr<Database> database;
	int database_locks;
	int flags;

public:
	LockableDatabase()
		: database_locks(0),
		  flags(DB_OPEN) { }

	LockableDatabase(const Endpoints& endpoints_, int flags_)
		: endpoints(endpoints_),
		  database_locks(0),
		  flags(flags_) { }
};


template <bool internal>
inline void
lock_database::_lock()
{
	if (lockable != nullptr) {
		if constexpr (!internal) {
			if (lockable->endpoints.empty()) {
				// internal never throws, just ignores
				THROW(Error, "lock_database cannot lock empty endpoints");
			}
		}
		if (!lockable->database) {
			assert(locks == 0 && lockable->database_locks == 0);
			assert(XapiandManager::manager);
			XapiandManager::manager->database_pool.checkout(lockable->database, lockable->endpoints, lockable->flags);
		}
		if (locks++ == 0) {
			++lockable->database_locks;
		}
	}
}


template <bool internal>
inline void
lock_database::_unlock()
{
	if (lockable != nullptr) {
		if (locks > 0 && --locks == 0) {
			if (lockable->database_locks > 0 && --lockable->database_locks == 0) {
				assert(lockable->database);
				assert(XapiandManager::manager);
				XapiandManager::manager->database_pool.checkin(lockable->database);
			}
		}
	}
}
