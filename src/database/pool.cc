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
#include <condition_variable>     // for std::condition_variable
#include <coroutine>              // for std::suspend_never
#include <filesystem>             // for std::filesystem
#include <optional>               // for std::optional
#include <system_error>           // for std::system_error
#include <thread>                 // for std::this_thread
#include <unordered_map>          // for std::unordered_map
#include <utility>                // for std::exchange

#include "at_end.h"               // for at_end::assert_none
#include "database/flags.h"       // for readable_flags
#include "database/shard.h"       // for Shard
#include "completion/posix.h"     // for kronuz::journal::PosixIO
#include "engine/error.h"         // for engine::Error, engine::errc
#include "engine/host_asio.h"     // for engine::AsioHost
#include "engine/strata/shard.h"  // for engine::strata::index_options, engine::strata::new_uuid
#include "engine/sync.h"          // for engine::check_may_block, engine::sync_wait
#include "log.h"                  // for L_CALL
#include "reactor.h"              // for reactor::inside_run
#include "strata/core/counted_io.hh"          // for strata::counted_io
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
// alive; nothing touches it after the resume. `awaited` counts the chores not yet handed back to their
// awaiter: a chore is over for its owner when the task resumes, which can be before the worker leaves it.
struct ChoreAwaiter final : WorkItem {
	WorkGroup& group;
	std::atomic<std::size_t>& awaited;
	std::function<void()> fn;
	std::exception_ptr error;
	std::coroutine_handle<> handle;
	task::scheduler_ref sched;

	ChoreAwaiter(WorkGroup& group_, std::atomic<std::size_t>& awaited_, engine::ChoreClass chore_class, std::function<void()> fn_) : group(group_), awaited(awaited_), fn(std::move(fn_)) {
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
		assert(sched && "a chore is awaited by a task that runs on a scheduler");
		awaited.fetch_add(1, std::memory_order_relaxed);
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
		self->resume();
	}

	// A chore its group dropped unrun: the awaiter resumes with task::cancelled.
	void cancel() noexcept {
		error = std::make_exception_ptr(task::cancelled{});
		resume();
	}

	void resume() noexcept {
		const auto h = handle;
		const auto s = sched;
		awaited.fetch_sub(1, std::memory_order_acq_rel);   // the last touch of the owner's state
		if (s) {
			s.post(h);
		} else {
			h.resume();
		}
	}
};

}  // namespace


namespace {

// Destroys a WorkGroup on a worker: ~WorkGroup joins the group, which a loop thread must not do.
struct Reap final : WorkItem {
	std::unique_ptr<WorkGroup> group;

	explicit Reap(std::unique_ptr<WorkGroup> group_) : group(std::move(group_)) {
		run = &Reap::invoke;
		cls = WorkClass::flush;
	}

	static void invoke(WorkItem* item) noexcept {
		delete static_cast<Reap*>(item);
	}
};

}  // namespace


namespace {

// Every IndexChores alive, so that a teardown can cancel and drain their groups (IndexChores::cancel_all).
std::mutex chores_mtx;
IndexChores* chores_head = nullptr;

}  // namespace


IndexChores::IndexChores(WorkPool& pool) : pool_(pool), group_(std::make_unique<WorkGroup>(pool))
{
	std::lock_guard<std::mutex> lk(chores_mtx);
	next_ = chores_head;
	if (chores_head != nullptr) { chores_head->prev_ = this; }
	chores_head = this;
}


bool
IndexChores::cancel_all(std::chrono::steady_clock::time_point limit) noexcept
{
	{
		std::lock_guard<std::mutex> lk(chores_mtx);
		for (auto* chores = chores_head; chores != nullptr; chores = chores->next_) {
			if (!chores->group_) {
				continue;
			}
			for (WorkItem* item = chores->group_->cancel(); item != nullptr;) {
				WorkItem* const next = std::exchange(item->next, nullptr);
				assert(item->run == &ChoreAwaiter::invoke && "an index group holds only chores");
				if (item->run == &ChoreAwaiter::invoke) {
					static_cast<ChoreAwaiter*>(item)->cancel();
				}
				item = next;
			}
		}
	}
	// The running chores end on their own; a cancelled one's resume is queued. Polled: it runs once, at exit.
	for (;;) {
		bool idle = true;
		{
			std::lock_guard<std::mutex> lk(chores_mtx);
			for (auto* chores = chores_head; chores != nullptr && idle; chores = chores->next_) {
				idle = !chores->group_ || chores->idle();
			}
		}
		if (idle) {
			return true;
		}
		if (std::chrono::steady_clock::now() >= limit) {
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}


bool
IndexChores::idle() noexcept
{
	struct Probe : WorkIdle {
		std::atomic<bool> fired{false};
	} probe;
	probe.fn = [](WorkIdle* node) noexcept { static_cast<Probe*>(node)->fired.store(true, std::memory_order_release); };
	group_->on_idle(probe);   // runs the probe now when the group is idle
	if (probe.fired.load(std::memory_order_acquire)) {
		return true;
	}
	if (group_->cancel_idle(probe)) {
		return false;
	}
	while (!probe.fired.load(std::memory_order_acquire)) {   // it fired between the two calls: the store is next
	}
	return true;
}


IndexChores::~IndexChores()
{
	{
		std::lock_guard<std::mutex> lk(chores_mtx);
		(prev_ != nullptr ? prev_->next_ : chores_head) = next_;
		if (next_ != nullptr) { next_->prev_ = prev_; }
	}

	at_end::assert_none("IndexChores", awaited_.load(std::memory_order_acquire) == 0 ? std::string() : "a chore is queued or running");

	// Every chore has resumed its awaiter, though a worker may still be leaving the last one: joining the
	// group waits for that. Off a loop it is done here, on a loop a worker does it.
	if (group_ && reactor::inside_run()) {
		auto* reap = new Reap(std::move(group_));
		pool_.post(*reap);
	}
}


task::task<void>
IndexChores::run(engine::ChoreClass cls, std::function<void()> fn)
{
	co_await ChoreAwaiter{*group_, awaited_, cls, std::move(fn)};
}


/*
 *  _____
 * |_   _|   _ _ __ _ __
 *   | || | | | '__| '_ \.
 *   | || |_| | |  | | | |
 *   |_| \__,_|_|  |_| |_|
 */

Turn::Turn(task::async_mutex::guard guard, int* held, prospect::index* index) noexcept
	: guard_(std::move(guard)), held_(held), index_(index)
{
}


Turn::Turn(Turn&& other) noexcept
	: guard_(std::move(other.guard_)), held_(std::exchange(other.held_, nullptr)), index_(std::exchange(other.index_, nullptr))
{
}


Turn&
Turn::operator=(Turn&& other) noexcept
{
	if (this != &other) {
		release();
		guard_ = std::move(other.guard_);
		held_ = std::exchange(other.held_, nullptr);
		index_ = std::exchange(other.index_, nullptr);
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
	// The holder checks the index in while it still holds the turn: a check-in is safe only while no thread makes
	// the owner's lease-less reads, which a writer makes under its turn (strata's checkout contract, x-pool D5).
	if (auto* index = std::exchange(index_, nullptr); index && guard_) {
		index->check_in();
		// Under defer_io every map is made under the lease: a turn's apply never maps (x-pool D12).
		assert(index->unleased_maps() == 0 && "an index mapped a record outside its lease");
	}
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

task::task<Turn>
ShardEndpoint::take_turn(int flags, task::cancel_token cancel)
{
	L_CALL("ShardEndpoint::take_turn(({}))", readable_flags(flags));

	engine::check_no_view_held("a turn");

	task::async_mutex::guard guard;
	int* held = nullptr;
	if (auto* host = engine::Host::current()) {
		const auto at = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(database_pool.turn_timeout_.load(std::memory_order_relaxed)));
		task::deadline<engine::Host> expiry(*host, at, cancel);
		guard = co_await turn.lock(expiry.token());
		if (auto* counters = host->counters()) {
			held = &counters->turns;
		}
	} else {
		guard = co_await turn.lock(cancel);
	}
	if (!guard) {
		if (cancel.requested()) {
			throw task::cancelled{};
		}
		throw engine::Error(engine::errc::busy, "The index is busy: its turn did not come in time");
	}
	if (held) {
		assert(*held == 0 && "a task holds one index's turn at most");
		++*held;
	}
	co_return Turn(std::move(guard), held, index_);
}


task::task<void>
ShardEndpoint::fresh(int flags, task::cancel_token cancel)
{
	if (!engine_->dirty()) {
		co_return;
	}
	auto mine = co_await take_turn(flags, std::move(cancel));
	if (engine_->dirty()) {
		co_await engine_->refresh();
	}
}


engine::Shard&
ShardEndpoint::engine()
{
	return *engine_;
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
	return strings::format(STEEL_BLUE + "<ShardEndpoint {}{}>",
		repr(to_string()),
		is_modified() ? " " + ORANGE + "(modified)" + STEEL_BLUE : "");
}


/*
 *  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */

// The catalog's timer (strata's host::timer) on the control loop: each callback is an Asio timer armed there with
// Host::start_timer, and only posts a chore. close() runs at once what is still armed; later ones run when armed.
struct DatabasePool::Timer : std::enable_shared_from_this<Timer> {
	struct Armed {
		std::function<void()> fn;
		std::unique_ptr<engine::Host::Timer> timer;  // set and destroyed on the loop's thread only
	};

	engine::AsioHost& host;
	std::mutex mtx;
	bool closed = false;
	std::uint64_t last = 0;
	std::unordered_map<std::uint64_t, Armed> armed;

	explicit Timer(engine::AsioHost& host_) : host(host_) { }

	void after(std::uint64_t delay_ns, std::function<void()> fn) {
		const auto at = std::chrono::steady_clock::now() + std::chrono::nanoseconds(delay_ns);
		std::uint64_t id = 0;
		{
			std::lock_guard<std::mutex> lk(mtx);
			if (!closed) {
				id = ++last;
				armed.emplace(id, Armed{std::move(fn), nullptr});
			}
		}
		if (id == 0) {
			fn();
			return;
		}
		// Armed on the loop's thread: the raw executor still runs once the loop's task scheduler has closed.
		asio::post(host.scheduler().executor(), [self = shared_from_this(), id, at] {
			auto timer = self->host.start_timer(at, [weak = std::weak_ptr<Timer>(self), id] {
				if (auto self = weak.lock()) {
					self->fire(id);
				}
			});
			std::lock_guard<std::mutex> lk(self->mtx);
			if (auto it = self->armed.find(id); it != self->armed.end()) {
				it->second.timer = std::exchange(timer, nullptr);
			}
		});
	}

	// The loop's thread, at the deadline.
	void fire(std::uint64_t id) {
		Armed done;
		{
			std::lock_guard<std::mutex> lk(mtx);
			auto it = armed.find(id);
			if (it == armed.end()) {
				return;
			}
			done = std::move(it->second);
			armed.erase(it);
		}
		done.fn();
	}

	// Runs every callback still armed now; their Asio timers are destroyed on the loop's thread.
	void close() {
		std::unordered_map<std::uint64_t, Armed> left;
		{
			std::lock_guard<std::mutex> lk(mtx);
			closed = true;
			left.swap(armed);
		}
		auto timers = std::make_shared<std::vector<std::unique_ptr<engine::Host::Timer>>>();
		for (auto& [id, entry] : left) {
			entry.fn();
			timers->push_back(std::move(entry.timer));
		}
		asio::post(host.scheduler().executor(), [timers] { timers->clear(); });
	}
};


// What join() and the destructor await: the catalog's shutdown, set by the task that awaits it.
struct DatabasePool::Shutdown {
	std::mutex mtx;
	std::condition_variable cv;
	bool done = false;
	bool reported = false;
	std::vector<std::string> failed;  // the keys whose commit failed: their WAL replays them at the next open
};


namespace {

// A coroutine nobody awaits: it runs until its first suspension when called, and its frame goes when it ends.
struct Detached {
	struct promise_type {
		Detached get_return_object() noexcept { return {}; }
		std::suspend_never initial_suspend() noexcept { return {}; }
		std::suspend_never final_suspend() noexcept { return {}; }
		void return_void() noexcept { }
		void unhandled_exception() noexcept { std::terminate(); }
	};
};

// A pin's failure as the engine's error: an absent index is not_found, a closed catalog not_available, and
// strata's own failures are translated; anything else (the WAL replay's) leaves as it is.
[[noreturn]] void
rethrow_pin_failure(const std::string& path)
{
	try {
		throw;
	} catch (const strata::index_not_created&) {
		throw engine::Error(engine::errc::not_found, "No such database: " + path);
	} catch (const strata::catalog_closed&) {
		throw engine::Error(engine::errc::not_available, "Shard is not available");
	} catch (const strata::too_many_open& exc) {
		throw engine::Error(engine::errc::busy, std::string("Too many indexes are open: ") + exc.what());
	} catch (const strata::wake_failed& exc) {
		try {
			std::rethrow_exception(exc.cause());
		} catch (const strata::too_many_open& cause) {
			throw engine::Error(engine::errc::busy, std::string("Too many indexes are open: ") + cause.what());
		} catch (...) {
			engine::strata::rethrow_translated();
		}
	} catch (const strata::index_fenced&) {
		engine::strata::rethrow_translated();
	} catch (const strata::corrupt_segment&) {
		engine::strata::rethrow_translated();
	} catch (const strata::corrupt_index&) {
		engine::strata::rethrow_translated();
	}
}

}  // namespace


IndexCatalog::host
DatabasePool::make_host(std::size_t database_pool_size, const IndexSweep& sweep)
{
	IndexCatalog::host h;
	h.exec = executor_.get();
	h.budget = budget_.get();
	h.timer = [timer = timer_.get()](std::uint64_t delay_ns, std::function<void()> fn) { timer->after(delay_ns, std::move(fn)); };
	// The key is the index's path; strata's files are in its "strata" directory (x-pool D6).
	h.resolve = [this](const IndexCatalog::opening& op) -> std::unique_ptr<kronuz::journal::IO> {
		namespace fs = std::filesystem;
		const std::string path(op.key);
		if (index_io_) {
			return strata::counted_io::make(op.counts, [&] { return index_io_(path); });
		}
		const auto dir = (fs::path(path) / "strata").string();
		std::error_code ec;
		if (!fs::exists(dir, ec)) {
			if (!op.create) {
				throw strata::index_not_created("No such database: " + path);
			}
			fs::create_directories(dir);
		}
		return strata::counted_io::make(op.counts, [&] {
			return std::make_unique<kronuz::journal::PosixIO>(dir, 0600, kronuz::journal::PosixIOOptions{true, true});
		});
	};
	// The options are read at each open, so a test that resizes them affects the indexes opened from then on.
	h.open_index = [this](kronuz::journal::IO& io, const prospect::index::options&, const IndexCatalog::opening& op) {
		engine::OpenOptions host;
		host.host = true;
		host.background = executor_.get();
		host.buffered_docs_cap = buffered_docs_cap_.load(std::memory_order_relaxed);
		host.readers_per_index = readers_per_index_.load(std::memory_order_relaxed);
		const auto options = engine::strata::index_options(host);
		try {
			return prospect::index::open(io, options);
		} catch (const strata::index_not_created&) {
			if (!op.create) {
				throw;
			}
			return prospect::index::create(io, options, engine::strata::new_uuid());
		}
	};
	h.open_payload = [this](const IndexCatalog::opening& op, prospect::index& index, std::optional<ShardEndpoint>& entry) {
		entry.emplace(*this, op.key);
		entry->open_blocking(index);
	};
	h.commit = [](ShardEndpoint& entry, prospect::index&) {
		entry.commit_blocking();
	};
	// A warm index holds one descriptor, so the pool keeps no more open than the descriptors' soft cap (D11).
	soft_open_ = std::min<std::size_t>(database_pool_size, budget_->soft(strata::resource::descriptors));
	h.soft_open = soft_open_;
	const auto set = [](std::uint64_t& field, std::chrono::nanoseconds value) {
		if (value.count() > 0) {
			field = static_cast<std::uint64_t>(value.count());
		}
	};
	set(h.sweep_period_ns, sweep.period);
	set(h.demote_idle_ns, sweep.demote_idle);
	set(h.close_idle_ns, sweep.close_idle);
	set(h.negative_ttl_ns, sweep.negative_ttl);
	return h;
}


DatabasePool::DatabasePool(engine::AsioHost& timers, size_t database_pool_size, unsigned work_threads, size_t readers_per_index, IndexSweep sweep) :
	finished_(false),
	control_(nullptr),
	turn_timeout_(DB_TIMEOUT),
	buffered_docs_cap_(0),
	readers_per_index_(readers_per_index),
	wal_writer_(nullptr),
	detached_(0)
{
	WorkPool::Options options;
	options.threads = work_threads;
	options.block_guard = &engine::check_may_block;
	work_pool_ = std::make_unique<WorkPool>(options);
	executor_ = std::make_unique<strata::work_pool_executor>(*work_pool_, 1);
	timer_ = std::make_shared<Timer>(timers);
	budget_ = std::make_unique<strata::resource_budget>();
	catalog_ = std::make_unique<IndexCatalog>(make_host(database_pool_size, sweep));
	catalog_->start();
}


DatabasePool::~DatabasePool()
{
	at_end::assert_none("DatabasePool", detached_.load() == 0 ? std::string() : strings::format("{} detached tasks have not ended", detached_.load()));
	// The catalog is destroyed only after its shutdown ended: no pin is held and none can be taken (C24). Something
	// that holds an index past the pool (a root that holds a pin, a view kept past its pin) is reported, and debug
	// builds abort; release builds wait, since destroying the catalog under a pin would free what the pin names.
	auto& state = begin_shutdown();
	std::unique_lock<std::mutex> lk(state.mtx);
	if (!state.cv.wait_for(lk, patience_, [&] { return state.done; })) {
		lk.unlock();
		std::string left;
		for (const auto& line : busy()) {
			left += (left.empty() ? "" : "; ") + line;
		}
		at_end::assert_none("DatabasePool", left.empty() ? std::string("the index catalog's shutdown did not end") : left);
		lk.lock();
		state.cv.wait(lk, [&] { return state.done; });
	}
	lk.unlock();
	timer_->close();
	catalog_.reset();
}


DatabasePool::Shutdown&
DatabasePool::begin_shutdown()
{
	std::lock_guard<std::mutex> lk(shutdown_mtx_);
	if (!shutdown_) {
		shutdown_ = std::make_shared<Shutdown>();
		// Resumed inline by whoever releases the last pin or ends the last chore (no scheduler is current here).
		[](IndexCatalog& catalog, std::shared_ptr<Shutdown> state) -> Detached {
			auto result = co_await catalog.shutdown();
			std::lock_guard<std::mutex> lk(state->mtx);
			state->failed = std::move(result.uncommitted);
			state->done = true;
			state->cv.notify_all();
		}(*catalog_, shutdown_);
	}
	return *shutdown_;
}


namespace {

// Uncounts a detached task when its frame goes: it ends, or teardown destroys it suspended, or a closed
// control loop destroys it unrun. A parameter, so it lives in the frame from the call on.
struct Uncount {
	std::atomic<std::size_t>* n;
	explicit Uncount(std::atomic<std::size_t>& n_) noexcept : n(&n_) { }
	Uncount(Uncount&& other) noexcept : n(std::exchange(other.n, nullptr)) { }
	Uncount(const Uncount&) = delete;
	Uncount& operator=(const Uncount&) = delete;
	Uncount& operator=(Uncount&&) = delete;
	~Uncount() {
		if (n != nullptr) { n->fetch_sub(1); }
	}
};

// Counts a task posted to the control loop until its frame goes, however it goes.
task::task<void>
counted(task::task<void> t, Uncount done)
{
	(void)done;
	co_await std::move(t);
}

}  // namespace


void
DatabasePool::run_detached(task::task<void> t)
{
	// Counted before the control is read: a shutdown that clears the control and then sees no task counted
	// is sure that this call read the empty control.
	detached_.fetch_add(1);
	if (auto* control = control_.load()) {
		control->spawn(counted(std::move(t), Uncount(detached_)));
		return;
	}
	detached_.fetch_sub(1);
	// No control left: the task runs here, to its end, on the thread that starts it, which may be the shutdown
	// thread draining the control loop (a debounced commit fires there).
	engine::BlockingAllowed blocking("a detached task runs on the thread that starts it once the control is gone");
	try {
		engine::sync_wait(std::move(t));
	} catch (...) {
		L_EXC("A detached task failed");
	}
}


task::task<IndexPin>
DatabasePool::pin(const Endpoint& endpoint, int flags, task::cancel_token cancel, bool use)
{
	L_CALL("DatabasePool::pin({}, ({}))", repr(endpoint.to_string()), readable_flags(flags));

	engine::check_no_view_held("a pin");
	const std::string key = endpoint.path;
	const auto intent = has_db_create_or_open(flags) ? strata::open_intent::create : strata::open_intent::open;
	IndexCatalog::pin resident;
	if (finished_.load()) {
		// A finishing node serves the indexes it has open and opens no more: a resident pin finds the index open (or
		// owed) and keeps it so while the request pin is taken; it applies nothing, so it never meets a debt.
		try {
			resident = co_await catalog_->pin_chore(key, false, strata::open_intent::resident, cancel);
		} catch (...) {
			rethrow_pin_failure(key);
		}
		if (!resident) {
			throw engine::Error(engine::errc::not_available, "Shard is not available");
		}
	}
	try {
		co_return IndexPin(co_await catalog_->pin_request(key, use, intent, std::move(cancel)), *this);
	} catch (...) {
		rethrow_pin_failure(key);
	}
}


task::task<IndexPin>
DatabasePool::pin_chore(const Endpoint& endpoint, task::cancel_token cancel)
{
	L_CALL("DatabasePool::pin_chore({})", repr(endpoint.to_string()));

	const std::string key = endpoint.path;
	try {
		auto pinned = co_await catalog_->pin_chore(key, false, strata::open_intent::resident, std::move(cancel));
		if (!pinned) {
			co_return IndexPin();
		}
		co_return IndexPin(std::move(pinned), *this);
	} catch (const task::cancelled&) {
		throw;   // the caller was cancelled (a FLUSH's request): it ends, not the next key
	} catch (...) {
		// A resident pin that waited on an open that failed gets its error: there is nothing open to commit (C25).
		co_return IndexPin();
	}
}


void
IndexPin::reopen() noexcept
{
	if (pin_) {
		pool_->catalog_->reopen(std::move(pin_));
	}
}


namespace {

const char*
state_name(strata::slot_state state) noexcept
{
	switch (state) {
		case strata::slot_state::free: return "free";
		case strata::slot_state::opening: return "opening";
		case strata::slot_state::open: return "open";
		case strata::slot_state::owed: return "owed";
		case strata::slot_state::waking: return "waking";
		case strata::slot_state::demoting: return "demoting";
		case strata::slot_state::closing: return "closing";
		case strata::slot_state::failed: return "failed";
	}
	return "?";
}

const char*
hold_name(strata::shutdown_hold why) noexcept
{
	switch (why) {
		case strata::shutdown_hold::pinned: return "pinned";
		case strata::shutdown_hold::transition: return "in a transition";
		case strata::shutdown_hold::lease_busy: return "its lease is busy";
		case strata::shutdown_hold::close_throws: return "its close throws";
		case strata::shutdown_hold::view_held: return "a view is held";
		case strata::shutdown_hold::waiting: return "waiting";
	}
	return "?";
}

}  // namespace


std::vector<std::string>
DatabasePool::busy()
{
	std::vector<std::string> lines;
	if (catalog_->shutting_down()) {
		for (const auto& holder : catalog_->shutdown_report()) {
			lines.push_back(strings::format("{} ({}, {} pin{}, {})", repr(holder.key), state_name(static_cast<strata::slot_state>(holder.state)), holder.pins, holder.pins == 1 ? "" : "s", hold_name(holder.why)));
		}
		return lines;
	}
	for (const auto& slot : catalog_->snapshot()) {
		if (slot.pins != 0 || (slot.state != strata::slot_state::open && slot.state != strata::slot_state::owed && slot.state != strata::slot_state::failed)) {
			lines.push_back(strings::format("{} ({}, {} pin{})", repr(slot.key), state_name(slot.state), slot.pins, slot.pins == 1 ? "" : "s"));
		}
	}
	return lines;
}


task::task<void>
DatabasePool::flush(task::cancel_token cancel)
{
	L_CALL("DatabasePool::flush()");

	// Every key the catalog has, pinned resident and committed, one at a time: a task holds one turn at most.
	for (auto& slot : catalog_->snapshot()) {
		if (cancel.requested()) {
			throw task::cancelled{};   // the request left: the keys after it are not pinned
		}
		auto pinned = co_await pin_chore(Endpoint(slot.key), cancel);
		if (!pinned) {
			continue;   // closed since the snapshot: its close committed it
		}
		try {
			co_await pinned->commit(DB_OPEN | DB_WRITABLE, true, cancel);
		} catch (const engine::Error& error) {
			L_WARNING("Flushing {} failed: {}", repr(slot.key), error.description());
		}
	}
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	finished_.store(true);
	control_.store(nullptr);  // the loop that ran the detached tasks stops: from now on they run where they are started
}


bool
DatabasePool::join(std::chrono::steady_clock::time_point wakeup)
{
	L_CALL("DatabasePool::join(<timeout>)");

	engine::check_may_block("DatabasePool::join");

	auto& state = begin_shutdown();
	std::unique_lock<std::mutex> lk(state.mtx);
	if (!state.cv.wait_until(lk, wakeup, [&] { return state.done; })) {
		return false;
	}
	if (!std::exchange(state.reported, true)) {
		for (const auto& key : state.failed) {
			L_WARNING("The last commit of {} failed: its WAL replays it at the next open", repr(key));
		}
	}
	return true;
}


std::pair<size_t, size_t>
DatabasePool::count()
{
	L_CALL("DatabasePool::count()");

	size_t endpoints_count = 0;
	size_t databases_count = 0;
	for (const auto& slot : catalog_->snapshot()) {
		++endpoints_count;
		if (slot.state == strata::slot_state::open || slot.state == strata::slot_state::owed) {
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

	for (const auto& slot : catalog_->snapshot()) {
		ret += indent + indent;
		ret += strings::format(STEEL_BLUE + "<Index {} ({}, {} pin{})>", repr(slot.key), state_name(slot.state), slot.pins, slot.pins == 1 ? "" : "s");
		ret.push_back('\n');
	}
	return ret;
}
