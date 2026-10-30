/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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
#include <optional>             // for std::optional
#include <utility>              // for std::exchange, std::move

#include "database/pool.h"      // for DatabasePool (database_pool)
#include "database/shard.h"     // for Shard
#include "endpoint.h"           // for Endpoints
#include "manager.h"            // for XapiandManager
#include "engine/error.h"       // for engine::Error


// A use of an index: it keeps the registry entry from being closed as idle, and makes the Shard through
// which the index is read and written. Taking it never waits: the index is opened, and its turn taken,
// by the operations that need them.
class lock_shard {
	ReferencedShardEndpoint _ref;
	std::optional<Shard> _shard;
	int _locks;

	lock_shard(const lock_shard&) = delete;
	lock_shard& operator=(const lock_shard&) = delete;

public:
	const int flags;
	const Endpoint endpoint;

	lock_shard(lock_shard&& other) noexcept :
		_ref(std::move(other._ref)),
		_shard(std::move(other._shard)),
		_locks(std::exchange(other._locks, 0)),
		flags(other.flags),
		endpoint(other.endpoint) { }

	lock_shard(const Endpoint& endpoint, int flags, bool do_lock = true) :
		_ref(nullptr),
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

	Shard& lock()
	{
		if (_locks == 0) {
			auto manager = XapiandManager::manager();
			if (!manager) {
				throw engine::Error(engine::errc::internal, "No manager");
			}
			_ref = manager->database_pool->spawn(endpoint);
			_shard.emplace(*_ref.get(), flags);
		}
		++_locks;
		return *_shard;
	}

	int unlock() noexcept
	{
		if (_locks > 0 && --_locks == 0) {
			_shard->release();
			_shard.reset();
			_ref.reset();
		}
		return _locks;
	}

	Shard& operator*() const noexcept
	{
		assert(_shard);
		return const_cast<Shard&>(*_shard);
	}

	Shard* operator->() const noexcept
	{
		assert(_shard);
		return const_cast<Shard*>(&*_shard);
	}
};
