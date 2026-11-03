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


// A use of an index: it pins the index (docs/design/x-pool.md D3) and makes the Shard through which the index is
// read and written. `co_await pin()` takes the pin, before any view the request takes (D4); the index is opened,
// and its turn taken, by the operations that need them.
class lock_shard {
	IndexPin _pin;
	std::optional<Shard> _shard;

	lock_shard(const lock_shard&) = delete;
	lock_shard& operator=(const lock_shard&) = delete;

public:
	const int flags;
	const Endpoint endpoint;
	const task::cancel_token cancel;   // the request's token, for the Shard's turn waits

	lock_shard(lock_shard&& other) noexcept :
		_pin(std::move(other._pin)),
		_shard(std::move(other._shard)),
		flags(other.flags),
		endpoint(other.endpoint),
		cancel(other.cancel) {
		other._shard.reset();
	}

	lock_shard(const Endpoint& endpoint, int flags, task::cancel_token cancel = {}) :
		flags(flags),
		endpoint(endpoint),
		cancel(std::move(cancel)) { }

	~lock_shard() noexcept
	{
		unlock();
	}

	// Pins the index and makes its Shard; a second call does nothing.
	task::task<void> pin()
	{
		if (!_shard) {
			auto manager = XapiandManager::manager();
			if (!manager) {
				throw engine::Error(engine::errc::internal, "No manager");
			}
			_pin = co_await manager->database_pool->pin(endpoint);
			_shard.emplace(_pin.entry(), flags, cancel);
		}
	}

	void unlock() noexcept
	{
		if (_shard) {
			_shard->release();
			_shard.reset();
		}
		_pin.release();
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
