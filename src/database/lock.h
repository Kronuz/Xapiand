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

#include <cassert>              // for assert

#include "database/pool.h"      // for DatabasePool (database_pool)
#include "endpoint.h"           // for Endpoints
#include "manager.h"            // for XapiandManager
#include "xapian.h"             // for Xapian::Database


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
			assert(_locks == 0);
			_locked = XapiandManager::database_pool()->checkout(endpoint, flags, std::forward<Args>(args)...);
		}
		++_locks;
		return _locked;
	}

	int unlock() noexcept
	{
		if (_locks > 0 && --_locks == 0) {
			assert(_locked);
			XapiandManager::database_pool()->checkin(_locked);
		}
		return _locks;
	}

	Shard& operator*() const noexcept
	{
		assert(_locked);
		return *_locked;
	}

	Shard* operator->() const noexcept
	{
		assert(_locked);
		return _locked.get();
	}

	const std::shared_ptr<Shard> locked()
	{
		return _locked;
	}
};
