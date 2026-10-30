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

#include "database/pool.h"

#include <algorithm>              // for std::find, std::move
#include <cassert>                // for assert
#include <thread>                 // for std::this_thread

#include "database/flags.h"       // for readable_flags
#include "database/shard.h"       // for Shard
#include "engine/error.h"         // for engine::Error, engine::errc
#include "engine/sync.h"          // for engine::check_may_block, engine::sync_wait
#include "log.h"                  // for L_CALL
#include "strata/core/work_pool_executor.hh"  // for strata::work_pool_executor
#include "work_pool.hh"           // for WorkPool, WorkGroup, WorkItem
#include "xapiand_exception.h"    // for THROW, Error, MSG_Error, Exception, DocNot...


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE


/*
 *  ___           _             ____ _
 * |_ _|_ __   __| | _____  __ / ___| |__   ___  _ __ ___  ___
 *  | || '_ \ / _` |/ _ \ \/ /| |   | '_ \ / _ \| '__/ _ \/ __|
 *  | || | | | (_| |  __/>  < | |___| | | | (_) | | |  __/\__ \.
 * |___|_| |_|\__,_|\___/_/\_\ \____|_| |_|\___/|_|  \___||___/
 */

namespace {

// One chore: an item of the index's group that runs `fn` on a worker and resumes the awaiting task
// through the scheduler it suspended on. It lives in the awaiting frame, which the resume keeps
// alive; nothing touches it after the resume.
struct ChoreAwaiter final : WorkItem {
	WorkGroup& group;
	std::function<void()> fn;
	std::exception_ptr error;
	std::coroutine_handle<> handle;
	task::scheduler_ref sched;

	ChoreAwaiter(WorkGroup& group_, engine::ChoreClass chore_class, std::function<void()> fn_) : group(group_), fn(std::move(fn_)) {
		run = &ChoreAwaiter::invoke;
		switch (chore_class) {
			case engine::ChoreClass::query: cls = WorkClass::query; break;
			case engine::ChoreClass::flush: cls = WorkClass::flush; break;
			case engine::ChoreClass::merge: cls = WorkClass::merge; break;
		}
	}

	bool await_ready() const noexcept { return false; }

	void await_suspend(std::coroutine_handle<> h) {
		handle = h;
		sched = task::current();
		group.post(*this);  // the chore may finish, and resume h, before this returns
	}

	void await_resume() {
		if (error) {
			std::rethrow_exception(error);
		}
	}

	static void invoke(WorkItem* item) noexcept {
		auto* self = static_cast<ChoreAwaiter*>(item);
		try {
			self->fn();
		} catch (...) {
			self->error = std::current_exception();
		}
		const auto h = self->handle;
		const auto s = self->sched;
		if (s) {
			s.post(h);
		} else {
			h.resume();
		}
	}
};

}  // namespace


IndexChores::IndexChores(WorkPool& pool) : group_(std::make_unique<WorkGroup>(pool))
{
}


IndexChores::~IndexChores() = default;


task::task<void>
IndexChores::run(engine::ChoreClass cls, std::function<void()> fn)
{
	co_await ChoreAwaiter{*group_, cls, std::move(fn)};
}


/*
 *  _____
 * |_   _|   _ _ __ _ __
 *   | || | | | '__| '_ \.
 *   | || |_| | |  | | | |
 *   |_| \__,_|_|  |_| |_|
 */

Turn::Turn(task::async_mutex::guard guard, int* held) noexcept
	: guard_(std::move(guard)), held_(held)
{
}


Turn::Turn(Turn&& other) noexcept
	: guard_(std::move(other.guard_)), held_(std::exchange(other.held_, nullptr))
{
}


Turn&
Turn::operator=(Turn&& other) noexcept
{
	if (this != &other) {
		release();
		guard_ = std::move(other.guard_);
		held_ = std::exchange(other.held_, nullptr);
	}
	return *this;
}


Turn::~Turn()
{
	release();
}


void
Turn::release() noexcept
{
	if (held_) {
		assert(*held_ > 0);
		--*held_;
		held_ = nullptr;
	}
	guard_.unlock();
}


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 */

// What the users of an opening index await: the leader's outcome.
struct ShardEndpoint::OpenFlight {
	task::event done;
	std::exception_ptr error;
};


ShardEndpoint::State
ShardEndpoint::state() const
{
	std::lock_guard<std::mutex> lk(mtx);
	return state_;
}


void
ShardEndpoint::settle_open(State state, const std::shared_ptr<OpenFlight>& flight, std::exception_ptr error) noexcept
{
	{
		std::lock_guard<std::mutex> lk(mtx);
		state_ = state;
		flight_.reset();
	}
	flight->error = std::move(error);
	flight->done.set();
}


task::task<void>
ShardEndpoint::ensure_open(int flags)
{
	L_CALL("ShardEndpoint::ensure_open(({}))", readable_flags(flags));

	for (;;) {
		std::shared_ptr<OpenFlight> flight;
		std::shared_ptr<task::event> closed;
		bool leader = false;
		bool reset = false;
		{
			std::lock_guard<std::mutex> lk(mtx);
			if (is_finished()) {
				throw engine::Error(engine::errc::not_available, "Shard is not available");
			}
			switch (state_) {
				case State::open:
					if (!stale_.load(std::memory_order_acquire)) {
						co_return;
					}
					// A failure told us the engine cannot go on: this task closes it and the loop opens it again.
					state_ = State::closing;
					closed_ = std::make_shared<task::event>();
					reset = true;
					break;
				case State::opening:
					flight = flight_;
					break;
				case State::closing:
					closed = closed_;
					break;
				case State::empty:
					state_ = State::opening;
					flight_ = flight = std::make_shared<OpenFlight>();
					leader = true;
					break;
			}
		}
		if (reset) {
			co_await close_marked(false);
			continue;
		}
		if (closed) {
			co_await *closed;
			continue;
		}
		if (!leader) {
			co_await flight->done;
			if (flight->error) {
				std::rethrow_exception(flight->error);
			}
			continue;
		}
		std::exception_ptr error;
		try {
			co_await open_index(flags);
		} catch (...) {
			error = std::current_exception();
		}
		settle_open(error ? State::empty : State::open, flight, error);
		if (error) {
			std::rethrow_exception(error);
		}
	}
}


task::task<Turn>
ShardEndpoint::take_turn(int flags)
{
	L_CALL("ShardEndpoint::take_turn(({}))", readable_flags(flags));

	for (;;) {
		co_await ensure_open(flags);

		task::async_mutex::guard guard;
		int* held = nullptr;
		if (auto* host = engine::Host::current()) {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(database_pool.turn_timeout_.load(std::memory_order_relaxed)));
			guard = co_await turn.lock_until(*host, deadline);
			held = host->turns_held();
		} else {
			guard = co_await turn.lock();
		}
		if (!guard) {
			throw engine::Error(engine::errc::busy, "The index is busy: its turn did not come in time");
		}
		if (held) {
			assert(*held == 0 && "a task holds one index's turn at most");
			++*held;
		}
		Turn mine(std::move(guard), held);

		bool usable;
		{
			std::lock_guard<std::mutex> lk(mtx);
			usable = state_ == State::open && !stale_.load(std::memory_order_acquire) && engine_;
		}
		if (usable) {
			co_return std::move(mine);
		}
		// Closed or replaced while the turn was queued: give it back and look again.
	}
}


task::task<void>
ShardEndpoint::fresh(int flags)
{
	co_await ensure_open(flags);
	std::shared_ptr<engine::Shard> eng;
	{
		std::lock_guard<std::mutex> lk(mtx);
		eng = engine_;
	}
	if (!eng || !eng->dirty()) {
		co_return;
	}
	auto mine = co_await take_turn(flags);
	{
		std::lock_guard<std::mutex> lk(mtx);
		eng = engine_;
	}
	if (eng->dirty()) {
		co_await eng->refresh();
	}
}


engine::Shard&
ShardEndpoint::engine()
{
	std::lock_guard<std::mutex> lk(mtx);
	if (!engine_) {
		throw engine::Error(engine::errc::closed, "Database has been closed");
	}
	return *engine_;
}


bool
ShardEndpoint::is_used() const
{
	std::lock_guard<std::mutex> lk(mtx);
	return refs != 0 || state_ != State::empty;
}


engine::rev
ShardEndpoint::get_revision() const
{
	return revision.load(std::memory_order_relaxed);
}


void
ShardEndpoint::set_revision(engine::rev revision_)
{
	revision.store(revision_, std::memory_order_relaxed);
}


std::string
ShardEndpoint::__repr__() const
{
	static const char* const names[] = {"empty", "opening", "open", "closing"};
	std::lock_guard<std::mutex> lk(mtx);
	return strings::format(STEEL_BLUE + "<ShardEndpoint {{refs:{}}} {} ({}){}{}>",
		refs.load(),
		repr(to_string()),
		names[static_cast<int>(state_)],
		stale_.load() ? " " + RED + "(stale)" + STEEL_BLUE : "",
		is_finished() ? " " + ORANGE + "(finished)" + STEEL_BLUE : "");
}


std::string
ShardEndpoint::dump_databases(int level) const
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}
	return indent + __repr__() + "\n";
}


/*
 *  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */

DatabasePool::DatabasePool(size_t database_pool_size, unsigned work_threads) :
	lru::lru(database_pool_size),
	finished_(false),
	control_(nullptr),
	turn_timeout_(DB_TIMEOUT),
	buffered_docs_cap_(0),
	wal_writer_(nullptr)
{
	WorkPool::Options options;
	options.threads = work_threads;
	options.block_guard = &engine::check_may_block;
	work_pool_ = std::make_unique<WorkPool>(options);
	executor_ = std::make_unique<strata::work_pool_executor>(*work_pool_, 1);
}


DatabasePool::~DatabasePool()
{
	// The entries (their groups, their engines and the merges those run) end before the pool they run on.
	std::lock_guard<std::mutex> lk(mtx);
	lru::lru::clear();
}


void
DatabasePool::run_detached(task::task<void> t)
{
	if (auto* control = control_.load(std::memory_order_acquire)) {
		control->spawn(std::move(t));
	} else {
		try {
			engine::sync_wait(std::move(t));
		} catch (...) {
			L_EXC("A detached task failed");
		}
	}
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
		return GetAction::renew;
	}, endpoint);
	if (it == end()) {
		auto emplaced = emplace_and([&](const std::pair<const Endpoint, std::unique_ptr<ShardEndpoint>>&, bool, bool) {
			return DropAction::stop;
		}, endpoint, std::make_unique<ShardEndpoint>(*this, endpoint));
		database_endpoint = emplaced.first->second.get();
		if (finished_.load()) {
			database_endpoint->finished.store(true);  // an entry made during the shutdown opens nothing
		}
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


// The close of an idle entry, which begin_idle_close marked: a failure is logged, not thrown.
static task::task<void>
close_idle(ReferencedShardEndpoint entry)
{
	try {
		co_await entry->finish_idle_close();
	} catch (const engine::Error& error) {
		L_WARNING("Closing idle {} failed: {}", repr(entry->to_string()), error.description());
	} catch (const std::exception& exc) {
		L_WARNING("Closing idle {} failed: {}", repr(entry->to_string()), exc.what());
	}
}


void
DatabasePool::cleanup(bool immediate)
{
	L_CALL("DatabasePool::cleanup()");

	auto now = std::chrono::steady_clock::now();

	std::vector<ReferencedShardEndpoint> closing;

	{
		std::lock_guard<std::mutex> lk(mtx);

		trim_and([&](const std::pair<const Endpoint, std::unique_ptr<ShardEndpoint>>& endpoint_database_endpoint, bool overflowed, bool) {
			auto& entry = *endpoint_database_endpoint.second;
			const bool idle = immediate || entry.renew_time < now - (overflowed ? 60s : 3600s);
			if (!idle) {
				return overflowed ? DropAction::leave : DropAction::stop;
			}
			// Marking is under the map's lock, as every use of the entry is: a use that came first is seen
			// in refs, and one that comes later awaits the close.
			if (entry.begin_idle_close()) {
				closing.emplace_back(&entry);
			}
			if (entry.is_used()) {
				L_DATABASE("Leave used endpoint: {}", repr(entry.to_string()));
				return DropAction::leave;
			}
			L_DATABASE("Evict endpoint: {}", repr(entry.to_string()));
			return DropAction::evict;
		});
	}

	if (immediate) {
		// An immediate cleanup (FLUSH) returns once the indexes are closed: one close task per open index, awaited together.
		engine::check_may_block("DatabasePool::cleanup");
		std::vector<task::task<void>> tasks;
		tasks.reserve(closing.size());
		for (auto& entry : closing) {
			tasks.push_back(close_idle(std::move(entry)));
		}
		closing.clear();
		engine::sync_wait(task::when_all(std::move(tasks)));
		return;
	}

	// Each close is a task of its own; this returns without waiting for it.
	for (auto& entry : closing) {
		run_detached(close_idle(std::move(entry)));
	}
}


void
DatabasePool::housekeeping()
{
	for (auto& entry : endpoints()) {
		if (entry->state() != ShardEndpoint::State::open) {
			continue;
		}
		run_detached([](ReferencedShardEndpoint entry) -> task::task<void> {
			try {
				co_await entry->housekeeping();
			} catch (const engine::Error& error) {
				L_WARNING("Housekeeping of {} failed: {}", repr(entry->to_string()), error.description());
			} catch (const std::exception& exc) {
				L_WARNING("Housekeeping of {} failed: {}", repr(entry->to_string()), exc.what());
			}
		}(std::move(entry)));
	}
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	finished_.store(true);
	control_.store(nullptr);  // the loop that ran the detached tasks stops: from now on they run where they are started
	std::lock_guard<std::mutex> lk(mtx);

	for (auto& endpoint_database_endpoint : *this) {
		endpoint_database_endpoint.second->finished.store(true);
	}
}


bool
DatabasePool::join(std::chrono::steady_clock::time_point wakeup)
{
	L_CALL("DatabasePool::join(<timeout>)");

	engine::check_may_block("DatabasePool::join");

	for (;;) {
		if (clear()) {
			return true;
		}
		if (std::chrono::steady_clock::now() >= wakeup) {
			return false;
		}
		std::this_thread::sleep_for(5ms);
	}
}


bool
DatabasePool::clear()
{
	L_CALL("DatabasePool::clear()");

	engine::check_may_block("DatabasePool::clear");

	bool cleared = true;

	for (auto& entry : endpoints()) {
		// This reference is one of the entry's refs.
		bool marked;
		{
			std::lock_guard<std::mutex> lk(mtx);
			marked = entry->begin_idle_close(1);
		}
		if (marked) {
			try {
				engine::sync_wait(entry->finish_idle_close());
			} catch (const engine::Error& error) {
				L_WARNING("Closing {} failed: {}", repr(entry->to_string()), error.description());
			} catch (const std::exception& exc) {
				L_WARNING("Closing {} failed: {}", repr(entry->to_string()), exc.what());
			}
		}
		if (entry->state() != ShardEndpoint::State::empty || entry->refs.load() > 1) {
			cleared = false;
		}
		entry.reset();
	}

	if (!cleared) {
		return false;
	}

	// Now lock to double-check and really clear the LRU:
	std::lock_guard<std::mutex> lk(mtx);

	for (auto& endpoint_database_endpoint : *this) {
		if (endpoint_database_endpoint.second->is_used()) {
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
	for (auto& entry : endpoints()) {
		++endpoints_count;
		if (entry->state() == ShardEndpoint::State::open) {
			++databases_count;
		}
	}
	return std::make_pair(endpoints_count, databases_count);
}


std::string
DatabasePool::__repr__() const
{
	return strings::format(STEEL_BLUE + "<DatabasePool {{finished:{}}}>", finished_.load());
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
		referenced_database_endpoint.reset();
	}
	return ret;
}
