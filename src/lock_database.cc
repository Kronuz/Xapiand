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

#include "lock_database.h"

#include "database.h"           // for Database
#include "database_pool.h"      // for DatabasePool
#include "manager.h"            // for XapiandManager::manager


LockableDatabase::LockableDatabase() :
	_database_locks(0),
	flags(DB_OPEN)
{
}


LockableDatabase::LockableDatabase(const Endpoints& endpoints_, int flags_) :
	_database_locks(0),
	flags(flags_),
	endpoints(endpoints_)
{
}


const std::shared_ptr<Database>&
LockableDatabase::database() const noexcept
{
	assert(_locked_database);
	return _locked_database;
}


Xapian::Database*
LockableDatabase::db() const noexcept
{
	assert(_locked_database);
	return _locked_database ? _locked_database->db.get() : nullptr;
}


lock_database::lock_database(LockableDatabase* lockable) :
	lockable(lockable),
	locks(0)
{
	lock();
}


lock_database::~lock_database()
{
	while (locks) {
		unlock();
	}
}


void
lock_database::lock()
{
	if (lockable != nullptr) {
		if (lockable->endpoints.empty()) {
			return;
		}
		if (!lockable->_locked_database) {
			assert(locks == 0 && lockable->_database_locks == 0);
			assert(XapiandManager::manager);
			XapiandManager::manager->database_pool.checkout(lockable->_locked_database, lockable->endpoints, lockable->flags);
		}
		if (locks++ == 0) {
			++lockable->_database_locks;
		}
	}
}


void
lock_database::unlock()
{
	if (lockable != nullptr) {
		if (locks > 0 && --locks == 0) {
			if (lockable->_database_locks > 0 && --lockable->_database_locks == 0) {
				assert(lockable->_locked_database);
				assert(XapiandManager::manager);
				XapiandManager::manager->database_pool.checkin(lockable->_locked_database);
			}
		}
	}
}
