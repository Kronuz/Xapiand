/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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


#include "database/wide_search.h"

#include <algorithm>               // for std::sort, std::unique, std::min, std::max

#include "database/pool.h"         // for DatabasePool::soft_open
#include "endpoint.h"              // for Endpoint
#include "engine/error.h"          // for engine::Error, engine::errc
#include "manager.h"               // for XapiandManager::resolve_index_endpoints
#include "opts.h"                  // for opts::database_pool_size


namespace wide_search {

Pins::Pins(std::size_t capacity) noexcept
	: capacity_(std::max<std::size_t>(1, capacity)),
	  free_(capacity_)
{
}


task::task<Pins::Hold>
Pins::take(std::size_t n, task::cancel_token token)
{
	n = std::min(std::max<std::size_t>(1, n), capacity_);
	auto head = co_await line_.lock(std::move(token));
	if (!head) {
		throw task::cancelled{};
	}
	// At the head of the line: wait for room. Room comes back as running batches end, which needs no one ahead.
	while (true) {
		std::shared_ptr<task::event> room;
		{
			std::lock_guard<std::mutex> lk(mutex_);
			if (free_ >= n) {
				free_ -= n;
				break;
			}
			room_ = std::make_shared<task::event>();
			room = room_;
		}
		co_await *room;
	}
	co_return Hold(this, n);
}


Pins::Hold
Pins::try_take(std::size_t n) noexcept
{
	n = std::min(std::max<std::size_t>(1, n), capacity_);
	if (!line_.try_lock()) {
		return {};
	}
	Hold hold;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (free_ >= n) {
			free_ -= n;
			hold = Hold(this, n);
		}
	}
	line_.unlock();
	return hold;
}


std::size_t
Pins::held() const noexcept
{
	std::lock_guard<std::mutex> lk(mutex_);
	return capacity_ - free_;
}


void
Pins::give(std::size_t n) noexcept
{
	std::shared_ptr<task::event> room;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		free_ += n;
		room = std::move(room_);
	}
	if (room) {
		room->set();
	}
}


Pins&
pins()
{
	static Pins node([] {
		auto manager = XapiandManager::manager();
		if (manager && manager->database_pool) {
			return manager->database_pool->soft_open();
		}
		return static_cast<std::size_t>(std::max<ssize_t>(1, opts.database_pool_size));
	}());
	return node;
}


task::task<std::vector<Endpoint>>
resolve(const std::vector<std::string>& paths, bool writable, const MsgPack* settings, task::budget& budget)
{
	std::vector<Endpoint> resolved;
	resolved.reserve(paths.size());
	const bool wide = paths.size() > 1;
	for (const auto& path : paths) {
		if (wide) {
			co_await budget.checkpoint();
		}
		auto endpoints = co_await XapiandManager::resolve_index_endpoints(Endpoint(path), writable, settings);
		if (endpoints.empty()) {
			throw engine::Error(engine::errc::network, "Endpoint node not available");
		}
		for (auto& endpoint : endpoints) {
			resolved.push_back(std::move(endpoint));
		}
	}
	// Sorted and without repeats, as Endpoints::add keeps them, in one sort.
	std::sort(resolved.begin(), resolved.end());
	resolved.erase(std::unique(resolved.begin(), resolved.end()), resolved.end());
	co_return resolved;
}

}  // namespace wide_search
