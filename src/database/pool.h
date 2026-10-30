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

// The index registry (docs/design/phase7c.md D5). DatabasePool maps a path to a ShardEndpoint, the
// entry of one index: its engine shard, the turn that serialises every operation on it, the
// WorkGroup its device work runs in, and the state of its open and close. Nothing in it makes a
// thread wait for another: opening is a single flight that concurrent users await as a task, a
// close is announced by an event, and a turn is an async mutex with a deadline.

#include "config.h"               // for XAPIAND_DATA_STORAGE

#include <atomic>               // for std::atomic
#include <chrono>               // for std::chrono, std::chrono::steady_clock
#include <cstddef>              // for size_t
#include <cstdint>              // for std::uint64_t
#include <exception>            // for std::exception_ptr
#include <functional>           // for std::function
#include <memory>               // for std::shared_ptr
#include <mutex>                // for std::mutex
#include <string>               // for std::string
#include <utility>              // for std::pair
#include <vector>               // for std::vector

#include "endpoint.h"           // for Endpoints, Endpoint
#include <string_view>         // for std::string_view

#include "engine/chores.h"      // for Chores
#include "engine/document.h"    // for engine::Document
#include "engine/error.h"       // for engine::Error
#include "engine/host.h"        // for Host
#include "engine/shard.h"       // for engine::Shard
#include "engine/types.h"       // for engine::rev, docid
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...
#include "task.hh"              // for task::task, task::async_mutex, task::event


using namespace std::chrono_literals;


// How long a request waits for an index's turn before it gives up with a 503 (engine::errc::busy).
constexpr double DB_TIMEOUT = 30.0;

class Shard;
class Locator;
class DatabaseWALWriter;
class ReplayTarget;
class DataStorage;
class DatabasePool;
class ReferencedShardEndpoint;

class WorkPool;
class WorkGroup;
namespace strata {
class executor;
}


// The device work of one index: each chore is an item of the index's own WorkGroup, so indexes take
// fair turns on the host's pool and a query-class chore runs ahead of a flush.
class IndexChores final : public engine::Chores {
public:
	explicit IndexChores(WorkPool& pool);
	~IndexChores() override;

	IndexChores(const IndexChores&) = delete;
	IndexChores& operator=(const IndexChores&) = delete;

	task::task<void> run(engine::ChoreClass cls, std::function<void()> fn) override;

	WorkGroup& group() noexcept {
		return *group_;
	}

private:
	std::unique_ptr<WorkGroup> group_;
};


// Holds an index's turn. The turn covers an operation's apply, its revision update and its WAL
// enqueue as one unit, so the WAL's order is the apply's order. A task holds one turn at most
// (checked in debug builds when its host counts them).
class Turn {
public:
	Turn() noexcept = default;
	Turn(task::async_mutex::guard guard, int* held) noexcept;
	Turn(Turn&& other) noexcept;
	Turn& operator=(Turn&& other) noexcept;
	Turn(const Turn&) = delete;
	Turn& operator=(const Turn&) = delete;
	~Turn();

	explicit operator bool() const noexcept {
		return static_cast<bool>(guard_);
	}
	void release() noexcept;

private:
	task::async_mutex::guard guard_;
	int* held_ = nullptr;
};


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 */

// One registry entry: an index by path.
class ShardEndpoint : public Endpoint
{
	friend Shard;
	friend ReplayTarget;
	friend DatabasePool;
	friend ReferencedShardEndpoint;
	friend struct PoolTestAccess;  // tests/test_pool.cc

public:
	enum class State : std::uint8_t {
		empty,    // no engine: the next user opens it
		opening,  // one task is opening it; the others await it
		open,
		closing,  // one task is closing it; the others await `closed`
	};

private:
	DatabasePool& database_pool;

	std::atomic_int refs;           // the lock_shards and tasks that name this entry: it is not idle while any
	std::atomic_bool finished;      // the node is shutting down

	std::atomic<engine::rev> revision;
	std::chrono::steady_clock::time_point renew_time;  // under the pool's mtx

	// The state machine and what its waiters await, all under mtx (O(1) sections, no I/O).
	mutable std::mutex mtx;
	State state_ = State::empty;
	struct OpenFlight;
	std::shared_ptr<OpenFlight> flight_;
	std::shared_ptr<task::event> closed_;

	// A failure told the entry its engine cannot go on: the next user closes it and opens it again.
	std::atomic_bool stale_;

	// Every operation on the index takes this turn: a write's apply, revision update and WAL enqueue,
	// a refresh, a commit, a close, the idle housekeeping.
	task::async_mutex turn;
	std::unique_ptr<engine::Chores> chores;  // an IndexChores; tests put a wrapper in its place before the first open

	// Set while open. Replaced only by a task that holds the turn.
	std::shared_ptr<engine::Shard> engine_;

	// Written only under the turn.
	std::atomic_bool modified;              // applied since the last commit
	std::atomic<std::uint64_t> applied_;    // the writes applied so far (a position of the commits)
	std::uint64_t committed_ = 0;           // the writes the last commit covered
#ifdef XAPIAND_DATA_STORAGE
	std::unique_ptr<DataStorage> writable_storage;
#endif /* XAPIAND_DATA_STORAGE */

	// The commit in flight, if any: a write at or below `covers` joins it.
	struct CommitFlight;
	std::shared_ptr<CommitFlight> commit_flight_;  // under mtx

	std::atomic<int> last_flags_;            // the flags of the last write: the commit of a close follows its WAL setting
	std::atomic<std::uint64_t> opens_;       // how many times the index was opened (the single flight keeps it at one per close)

public:
	ShardEndpoint(DatabasePool& database_pool, const Endpoint& endpoint);

	~ShardEndpoint();

	State state() const;
	bool is_open() const {
		return state() == State::open;
	}

	// The state is `open` when this returns, or it throws: it opens the index when it is empty (the
	// first caller does, the others await it), awaits a close in progress, and replaces an engine a
	// failure marked stale. `flags` decide whether a missing index is created.
	task::task<void> ensure_open(int flags);

	// The index's turn, with the index open: awaits the turn with a deadline of DB_TIMEOUT and answers
	// engine::errc::busy when it does not come. A turn that came for an entry that closed meanwhile is
	// given back and the open awaited again.
	task::task<Turn> take_turn(int flags);

	// Awaits a refresh when something was applied since the last one, so a read sees it (a turn and a
	// query chore); nothing otherwise.
	task::task<void> fresh(int flags);

	// The engine, for a caller that has seen the index open.
	engine::Shard& engine();

	// Commits what is applied: the blob storage, the engine, then the WAL line, all in one turn. A write
	// that a commit already in flight covers joins it. True when a commit made a new revision.
	task::task<bool> commit(int flags, bool wal);

	// Marks the index closing if it is open and no more than `own_refs` name it. Called under the pool's
	// lock, which every use of the entry also takes: a use that came first is seen in refs, and one that
	// comes after awaits the close. True when it marked.
	bool begin_idle_close(int own_refs = 0);

	// The close that begin_idle_close started: the turn, a commit of what is pending, the close chore.
	task::task<void> finish_idle_close();

	// The idle housekeeping: publishes finished merges and frees what is queued for removal.
	task::task<void> housekeeping();

	// Tells the entry the engine cannot go on; the next user closes it and opens it again.
	void mark_stale() noexcept {
		stale_.store(true, std::memory_order_release);
	}

	engine::rev get_revision() const;
	void set_revision(engine::rev revision);

	bool is_finished() const {
		return finished.load(std::memory_order_relaxed);
	}

	// How many times the index has been opened.
	std::uint64_t opens() const {
		return opens_.load(std::memory_order_relaxed);
	}

	// In use or not empty: the pool keeps the entry.
	bool is_used() const;

	// Applied since the last commit.
	bool is_modified() const {
		return modified.load(std::memory_order_relaxed);
	}

	// The engine's uuid, once open; the data storage's volumes carry it.
	const std::string& uuid() const {
		return uuid_;
	}

	std::string __repr__() const;
	std::string dump_databases(int level) const;

private:
	std::string uuid_;  // set when opened; the data storage's volumes carry it

	DatabaseWALWriter& wal_writer() const;

	std::shared_ptr<engine::Shard> engine_ptr() const;

	// A failure the engine reports: one it cannot go on from marks the entry stale.
	void note_failure(const engine::Error& error) noexcept;

	// The operations of a write, which run under the index's turn (or alone, while the index is opened and
	// its log replayed) on the engine and the chores they are given. A write is applied, counted and logged here.
	task::task<engine::WriteResult> do_add(engine::Shard& eng, engine::Chores& chores, int flags, engine::Document&& doc, bool wal_, bool version_);
	task::task<engine::WriteResult> do_replace(engine::Shard& eng, engine::Chores& chores, int flags, engine::docid shard_did, engine::Document&& doc, bool wal_, bool version_);
	task::task<engine::WriteResult> do_replace_term(engine::Shard& eng, engine::Chores& chores, int flags, const std::string& term, engine::Document&& doc, bool wal_, bool version_);
	task::task<void> do_delete(engine::Shard& eng, int flags, engine::docid shard_did, bool wal_);
	task::task<void> do_delete_term(engine::Shard& eng, int flags, const std::string& term, bool wal_);
	task::task<void> do_set_metadata(engine::Shard& eng, int flags, const std::string& key, const std::string& value, bool wal_);
	task::task<void> do_add_spelling(engine::Shard& eng, int flags, const std::string& word, engine::termcount freqinc, bool wal_);
	task::task<engine::termcount> do_remove_spelling(engine::Shard& eng, int flags, const std::string& word, engine::termcount freqdec, bool wal_);
	task::task<bool> do_commit(engine::Shard& eng, engine::Chores& chores, int flags, bool wal_);

#ifdef XAPIAND_DATA_STORAGE
	// Blobs of the data storage: the writes run in flush chores under the turn; a read opens a reader of its own.
	std::pair<std::string, std::string> storage_push_blobs_blocking(std::string&& doc_data);
	task::task<std::pair<std::string, std::string>> storage_push_blobs(engine::Chores& chores, std::string doc_data);
	task::task<void> storage_commit(engine::Chores& chores);
	std::string storage_read_blocking(const Locator& locator);
	std::string storage_read_blocking(std::string_view key);
#endif /* XAPIAND_DATA_STORAGE */

	// Opens the engine and the blob storage, and replays the WAL (shard.cc).
	void open_index_blocking(int flags);
	task::task<void> open_index(int flags);
	task::task<void> close_marked(bool commit_);
	void settle_open(State state, const std::shared_ptr<OpenFlight>& flight, std::exception_ptr error) noexcept;
};


class ReferencedShardEndpoint {
	ShardEndpoint* ptr;

public:
	ReferencedShardEndpoint(ShardEndpoint* ptr) : ptr(ptr) {
		if (ptr) {
			++ptr->refs;
		}
	}

	ReferencedShardEndpoint(const ReferencedShardEndpoint& other) = delete;
	ReferencedShardEndpoint& operator=(const ReferencedShardEndpoint& other) = delete;

	ReferencedShardEndpoint(ReferencedShardEndpoint&& other) : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	ReferencedShardEndpoint& operator=(ReferencedShardEndpoint&& other) {
		reset();
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	~ReferencedShardEndpoint() noexcept {
		reset();
	}

	void reset() noexcept {
		if (ptr) {
			ptr->refs.fetch_sub(1);
			ptr = nullptr;
		}
	}

	explicit operator bool() const {
		return !!ptr;
	}

	ShardEndpoint* operator->() const {
		return ptr;
	}

	ShardEndpoint* get() const {
		return ptr;
	}
};


/*
 *  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */

class DatabasePool : lru::lru<Endpoint, std::unique_ptr<ShardEndpoint>> {
	friend ShardEndpoint;
	friend struct PoolTestAccess;  // tests/test_pool.cc

	mutable std::mutex mtx;        // the map: O(log n) work, no I/O

	std::atomic_bool finished_;

	// The host's workers: one pool for every index, each index a group on it.
	std::unique_ptr<WorkPool> work_pool_;
	std::unique_ptr<strata::executor> executor_;

	// Where tasks started by a timer run (the control loop); null runs them to their end on the calling thread.
	std::atomic<engine::Host*> control_;

	std::atomic<double> turn_timeout_;                  // seconds a turn may be awaited before the request is told `busy`
	std::atomic<std::uint64_t> buffered_docs_cap_;      // documents an index buffers before a write owes a flush; 0: the index's own bound
	std::atomic<DatabaseWALWriter*> wal_writer_;        // the log's writer; null: the manager's
	std::atomic<std::size_t> detached_;                 // tasks posted to the control loop that have not ended

	ReferencedShardEndpoint _spawn(const Endpoint& endpoint);
	ReferencedShardEndpoint _get(const Endpoint& endpoint) const;

public:
	explicit DatabasePool(size_t database_pool_size, unsigned work_threads = 0);
	~DatabasePool();

	DatabasePool(const DatabasePool&) = delete;
	DatabasePool(DatabasePool&&) = delete;
	DatabasePool& operator=(const DatabasePool&) = delete;
	DatabasePool& operator=(DatabasePool&&) = delete;

	WorkPool& work_pool() noexcept {
		return *work_pool_;
	}
	strata::executor& executor() noexcept {
		return *executor_;
	}

	void set_control(engine::Host* control) noexcept {
		control_.store(control, std::memory_order_release);
	}

	void set_turn_timeout(double seconds) noexcept {
		turn_timeout_.store(seconds, std::memory_order_relaxed);
	}

	void set_buffered_docs_cap(std::uint64_t cap) noexcept {
		buffered_docs_cap_.store(cap, std::memory_order_relaxed);
	}

	void set_wal_writer(DatabaseWALWriter* writer) noexcept {
		wal_writer_.store(writer, std::memory_order_release);
	}

	// Runs a task that no request awaits (a timer's commit, an idle close): on the control loop when
	// there is one, else to its end on this thread.
	void run_detached(task::task<void> t);

	// The tasks run_detached posted to the control loop that have not ended. A shutdown that stops the loop
	// clears the control with set_control(nullptr), then runs the loop until this is zero.
	std::size_t detached() const noexcept {
		return detached_.load();
	}

	// The entry of this path, referenced so that it stays until the reference ends. Never waits.
	ReferencedShardEndpoint spawn(const Endpoint& endpoint);
	ReferencedShardEndpoint get(const Endpoint& endpoint) const;

	std::vector<ReferencedShardEndpoint> endpoints() const;

	// Closes the entries nobody uses that have been idle long, and the ones past the pool's size.
	// A close is a task of its own: this returns without waiting for it.
	void cleanup(bool immediate = false);

	// The idle housekeeping of every open index: publishes finished merges and frees queued files.
	void housekeeping();

	// Shutdown: no new use. join() is called until it returns true; it closes the entries nobody
	// uses and runs on a thread that may wait (the main thread), never on a loop.
	void finish();

	bool join(std::chrono::steady_clock::time_point wakeup);

	bool join(std::chrono::milliseconds timeout = 60s) {
		return join(std::chrono::steady_clock::now() + timeout);
	}

	// Closes every entry nobody uses; false while any is still open or in use.
	bool clear();

	std::pair<size_t, size_t> count();

	std::string __repr__() const;
	std::string dump_databases(int level = 1) const;
};
