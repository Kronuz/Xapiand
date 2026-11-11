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

// The index registry (docs/design/x-pool.md). DatabasePool is one strata catalog over the indexes of this
// process, keyed by path; its payload, ShardEndpoint, is the entry of one index: its engine shard over the
// catalog's index, the turn that serialises every write on it, and the WorkGroup its device work runs in. The
// catalog opens, wakes, replaces and closes; a request names an index only through an IndexPin. Nothing in it
// makes a thread wait for another: a pin is awaited as a task, and a turn is an async mutex with a deadline.

#include "config.h"               // for XAPIAND_DATA_STORAGE

#include <atomic>               // for std::atomic
#include <chrono>               // for std::chrono, std::chrono::steady_clock
#include <cstddef>              // for size_t
#include <cstdint>              // for std::uint64_t
#include <exception>            // for std::exception_ptr
#include <functional>           // for std::function
#include <memory>               // for std::shared_ptr
#include <optional>             // for std::optional
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
#include "engine/open.h"        // for engine::default_readers_per_index
#include "engine/shard.h"       // for engine::Shard
#include "engine/types.h"       // for engine::rev, docid
#include "prospect/index.hh"    // for prospect::index
#include "strata/core/catalog.hh"          // for strata::catalog
#include "strata/core/resource_budget.hh"  // for strata::resource_budget
#include "task.hh"              // for task::task, task::async_mutex, task::event

// <sys/sysctl.h> (strata's resource budget, outside Linux) defines MIN and MAX, which Xapiand's enums name.
#undef MIN
#undef MAX


using namespace std::chrono_literals;


// How long a request waits for an index's turn before it gives up with a 503 (engine::errc::busy).
constexpr double DB_TIMEOUT = 30.0;

class Shard;
class Locator;
class DatabaseWALWriter;
class ReplayTarget;
class DataStorage;
class DatabasePool;
class IndexPin;

class WorkPool;
class WorkGroup;
namespace engine {
class AsioHost;
}
namespace strata {
class executor;
}


// The device work of one index: each chore is an item of the index's own WorkGroup, so indexes take
// fair turns on the host's pool and a query-class chore runs ahead of a flush.
class IndexChores final : public engine::Chores {
public:
	explicit IndexChores(WorkPool& pool);
	~IndexChores() override;   // debug builds abort when a chore is still queued or running (D14)

	// Whether the group has nothing queued and nothing running. It never waits.
	bool idle() noexcept;

	// Teardown, once the loops have stopped (task's cancellation D8, phase 1): drops every index's queued chores,
	// whose awaiters resume with task::cancelled, then waits until no chore runs, at most until `limit`; false
	// when one still runs then. A task left suspended on a stopped loop is destroyed afterwards, by its scheduler.
	static bool cancel_all(std::chrono::steady_clock::time_point limit) noexcept;

	IndexChores(const IndexChores&) = delete;
	IndexChores& operator=(const IndexChores&) = delete;

	task::task<void> run(engine::ChoreClass cls, std::function<void()> fn) override;

	WorkGroup& group() noexcept {
		return *group_;
	}

private:
	WorkPool& pool_;
	std::unique_ptr<WorkGroup> group_;
	IndexChores* prev_ = nullptr;   // the list of every IndexChores alive, under its lock
	IndexChores* next_ = nullptr;
	std::atomic<std::size_t> awaited_{0};   // chores whose awaiter has not resumed yet
};


// Holds an index's turn. The turn covers an operation's apply, its revision update and its WAL
// enqueue as one unit, so the WAL's order is the apply's order. A task holds one turn at most
// (checked in debug builds when its host counts them). Its release checks the index in first
// (strata's checkout contract): finished merges are published and released views freed (x-pool D5).
class Turn {
public:
	Turn() noexcept = default;
	Turn(task::async_mutex::guard guard, int* held, prospect::index* index = nullptr) noexcept;
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
	prospect::index* index_ = nullptr;
};


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 */

// The largest blob a streamed upload stores: a record of the data storage is at most 4 GiB - 1 bytes, framing included.
inline constexpr std::uint64_t STORED_UPLOAD_LIMIT = 0xFFFFFFFFull - 1024;

// A blob being written to a volume of its own, chunk by chunk (docs/design/phase7c.md D9). Each step is a chore of
// the group it is given, so the request that streams the body waits for none of them on its reactor. The record is
// uncompressed and has the framing of any `stored` record (the content type, then the blob), so it reads back like one.
class StoredUpload {
public:
	~StoredUpload();
	StoredUpload(const StoredUpload&) = delete;
	StoredUpload& operator=(const StoredUpload&) = delete;

	// Writes a chunk of the blob. The chunks add up to the size the upload was begun with.
	task::task<void> append(std::string chunk);

	// Ends the record and syncs the volume: the blob is then durable and `locator()` names it.
	task::task<void> end();

	// Abandons the upload and removes its volume. Awaited on every path that does not end().
	task::task<void> abort() noexcept;

	// Where the record is, once end() ran.
	ssize_t volume() const noexcept;
	size_t offset() const noexcept;

private:
	friend class ShardEndpoint;
	struct Impl;
	StoredUpload(std::unique_ptr<Impl> impl, engine::Chores& chores);

	std::unique_ptr<Impl> impl;
	engine::Chores& chores;
};


// One index's entry (docs/design/x-pool.md D1): the catalog's payload, built in place by the open (open_payload,
// which replays the WAL) and destroyed by the close, before the index. It holds what the catalog does not: the
// turn, the device chores, the engine shard over the borrowed index, the revision, the commit's flight and the
// blob storage. It is valid only under a pin.
class ShardEndpoint : public Endpoint
{
	friend Shard;
	friend ReplayTarget;
	friend DatabasePool;
	friend struct PoolTestAccess;  // tests/test_pool.cc

	DatabasePool& database_pool;

	std::atomic<engine::rev> revision;

	mutable std::mutex mtx;  // commit_flight_, uuid_ (O(1) sections, no I/O)

	// Every write on the index takes this turn: a write's apply, revision update and WAL enqueue, a refresh,
	// a commit. A turn is held only under a pin, so with no pin it is free (the catalog's commit chore).
	task::async_mutex turn;
	std::unique_ptr<engine::Chores> chores;  // an IndexChores

	// The engine shard over the catalog's index and its views' handle, made by the open and never replaced: a
	// fenced index is replaced by the catalog with a new entry (x-pool D2).
	prospect::index::reader_handle readers_;
	std::unique_ptr<engine::Shard> engine_;
	prospect::index* index_ = nullptr;  // the catalog's, for the turn's check-in

	// Written only under the turn (or by the catalog's commit chore, which no pin can overlap).
	std::atomic_bool modified;              // applied since the last commit
	std::atomic<std::uint64_t> applied_;    // the writes applied so far (a position of the commits)
	std::uint64_t committed_ = 0;           // the writes the last commit covered
#ifdef XAPIAND_DATA_STORAGE
	std::unique_ptr<DataStorage> writable_storage;
	// The next volume number nobody has: the shared writer's rollover and each streamed upload take theirs from
	// here, so no volume has two writers (docs/design/phase7c.md D9).
	std::atomic<std::uint64_t> next_volume_{1};
#endif /* XAPIAND_DATA_STORAGE */

	// The commit in flight, if any: a write at or below `covers` joins it.
	struct CommitFlight;
	std::shared_ptr<CommitFlight> commit_flight_;  // under mtx

	std::atomic<int> last_flags_;            // the flags of the last write: the catalog's commit follows its WAL setting

	// A write failed as only a fresh open fixes (fenced, corrupt): every use that ends hands its pin back, so the
	// catalog replaces the index once its holders let go (x-pool D2).
	std::atomic_bool stale_{false};
	void note_failure(const engine::Error& error) noexcept;

public:
	ShardEndpoint(DatabasePool& database_pool, std::string_view path);
	ShardEndpoint(const ShardEndpoint&) = delete;
	ShardEndpoint& operator=(const ShardEndpoint&) = delete;

	~ShardEndpoint();   // debug builds abort when `outstanding()` is not empty (D14)

	// What still uses the entry, or empty: the turn (held or awaited).
	std::string outstanding();

	// The index's turn, under the caller's pin: awaits the turn with a deadline of DB_TIMEOUT and answers
	// engine::errc::busy when it does not come, or task::cancelled when `cancel` (the request's token) is requested first.
	task::task<Turn> take_turn(int flags, task::cancel_token cancel = {});

	// Awaits a refresh when something was applied since the last one, so a read sees it (a turn and a
	// query chore); nothing otherwise.
	task::task<void> fresh(int flags, task::cancel_token cancel = {});

	// The engine, for a caller that holds a pin.
	engine::Shard& engine();

	// Commits what is applied: the blob storage, the engine, then the WAL line, all in one turn. A write
	// that a commit already in flight covers joins it. True when a commit made a new revision. `cancel` ends the
	// wait for the turn (a FLUSH's request).
	task::task<bool> commit(int flags, bool wal, task::cancel_token cancel = {});

	// In a turn of its own: publishes the merges that finished and syncs their output when nothing else is
	// uncommitted (no revision, no WAL line). True while a merge is still pending or its output unsynced (x-pool D8).
	task::task<bool> settle_merges(int flags);

	engine::rev get_revision() const;
	void set_revision(engine::rev revision);

	// Applied since the last commit.
	bool is_modified() const {
		return modified.load(std::memory_order_relaxed);
	}

	// The engine's uuid; the data storage's volumes carry it.
	std::string uuid() const {
		std::lock_guard<std::mutex> lk(mtx);
		return uuid_;
	}

	std::string __repr__() const;

private:
	std::string uuid_;  // set by the open, under mtx

	DatabaseWALWriter& wal_writer() const;

	// The open's work on the catalog's index (open_payload, x-pool D6): the engine shard over it, the blob
	// storage, and the WAL replayed with chores inline. The slot is `opening`, so no pin sees any of it first.
	void open_blocking(prospect::index& index);

	// The catalog's commit chore (x-pool D7, C24): runs with no pin held, under the chore's lease, at the bound
	// close, at shutdown, in a replace close and in a demotion that only syncs a merge's output. With nothing
	// applied since the last commit it writes nothing, no WAL line included (C25).
	void commit_blocking();

	// The operations of a write, which run under the index's turn (or alone, while the index is opened and
	// its log replayed) on the engine and the chores they are given. A write is applied, counted and logged here.
	task::task<engine::WriteResult> do_add(engine::Shard& eng, engine::Chores& chores, int flags, engine::Document&& doc, bool wal_, bool version_);
	task::task<engine::WriteResult> do_replace(engine::Shard& eng, engine::Chores& chores, int flags, engine::docid shard_did, engine::Document&& doc, bool wal_, bool version_);
	task::task<engine::WriteResult> do_replace_term(engine::Shard& eng, engine::Chores& chores, int flags, const std::string& term, engine::Document&& doc, bool wal_, bool version_);
	task::task<void> do_delete(engine::Shard& eng, int flags, engine::docid shard_did, bool wal_, std::optional<engine::rev> expected_ = std::nullopt);
	task::task<void> do_delete_term(engine::Shard& eng, int flags, const std::string& term, bool wal_, std::optional<engine::rev> expected_ = std::nullopt);
	task::task<void> do_set_metadata(engine::Shard& eng, int flags, const std::string& key, const std::string& value, bool wal_);
	task::task<void> do_add_spelling(engine::Shard& eng, int flags, const std::string& word, engine::termcount freqinc, bool wal_);
	task::task<engine::termcount> do_remove_spelling(engine::Shard& eng, int flags, const std::string& word, engine::termcount freqdec, bool wal_);
	task::task<bool> do_commit(engine::Shard& eng, engine::Chores& chores, int flags, bool wal_);

#ifdef XAPIAND_DATA_STORAGE
	// Blobs of the data storage: the writes run in flush chores under the turn; a read opens a reader of its own.
	std::pair<std::string, std::string> storage_push_blobs_blocking(std::string&& doc_data);
	task::task<std::pair<std::string, std::string>> storage_push_blobs(engine::Chores& chores, std::string doc_data);
	task::task<void> storage_commit(engine::Chores& chores);

	// Begins a streamed upload of a blob of `size` bytes with content type `ct_type` into a volume of its own. Raises
	// ClientError when the record could not fit a volume (the request's 413).
	task::task<std::unique_ptr<StoredUpload>> upload_begin(engine::Chores& chores, std::string ct_type, std::uint64_t size);
	std::string storage_read_blocking(const Locator& locator);
	std::string storage_read_blocking(std::string_view key);
#endif /* XAPIAND_DATA_STORAGE */
};


// The catalog of this process's indexes (strata's, docs/mechanisms/catalog.md), keyed by path.
using IndexCatalog = strata::catalog<prospect::index, ShardEndpoint>;


// A pin on an index (docs/design/x-pool.md D3): while held, the index stays open and its entry valid. Taken with
// `co_await pool.pin(...)`, before any view the request takes (D4). Outside the registry, it is the only way
// Xapiand names an index.
class IndexPin {
	IndexCatalog::pin pin_;
	DatabasePool* pool_ = nullptr;

	friend DatabasePool;
	friend struct PoolTestAccess;  // tests/test_pool.cc
	IndexPin(IndexCatalog::pin pin, DatabasePool& pool) noexcept : pin_(std::move(pin)), pool_(&pool) { }

public:
	IndexPin() noexcept = default;
	IndexPin(IndexPin&&) noexcept = default;
	IndexPin& operator=(IndexPin&&) noexcept = default;

	explicit operator bool() const noexcept {
		return static_cast<bool>(pin_);
	}

	// The index's entry, valid for the pin's life only.
	ShardEndpoint& entry() const noexcept {
		return pin_.payload();
	}
	ShardEndpoint* operator->() const noexcept {
		return &pin_.payload();
	}

	// The registry the pin is of; null for an empty pin.
	DatabasePool* pool() const noexcept {
		return pin_ ? pool_ : nullptr;
	}

	// The holder of an index that cannot go on (fenced, or failing as only a fresh open fixes) hands its pin back:
	// the catalog replaces the index once its other holders let go, and the holder pins again by key (x-pool D2).
	void reopen() noexcept;

	void release() noexcept {
		pin_.release();
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

// The catalog's sweep (x-pool D8, D13): how often it runs, how long an unpinned index idles before it is demoted, then
// closed, and how long a missing index's negative entry stays. Zero keeps strata's default (1 s, 30 s, 10 min, 10 s).
struct IndexSweep {
	std::chrono::nanoseconds period{0};
	std::chrono::nanoseconds demote_idle{0};
	std::chrono::nanoseconds close_idle{0};
	std::chrono::nanoseconds negative_ttl{0};
};


class DatabasePool {
	friend ShardEndpoint;
	friend IndexPin;
	friend struct PoolTestAccess;  // tests/test_pool.cc

	std::atomic_bool finished_;

	// The host's workers: one pool for every index, each index a group on it; the catalog's chores and the
	// indexes' background work run on the executor over it.
	std::unique_ptr<WorkPool> work_pool_;
	std::unique_ptr<strata::executor> executor_;

	// Where tasks started by a timer run (the control loop); null runs them to their end on the calling thread.
	std::atomic<engine::Host*> control_;

	std::atomic<double> turn_timeout_;                  // seconds a turn may be awaited before the request is told `busy`
	std::atomic<std::uint64_t> buffered_docs_cap_;      // documents an index buffers before a write owes a flush; 0: the index's own bound
	std::atomic<std::size_t> readers_per_index_;        // the views of one index held at once (strata R4.1's reader_records)
	std::atomic<DatabaseWALWriter*> wal_writer_;        // the log's writer; null: the manager's
	std::atomic<std::size_t> detached_;                 // tasks posted to the control loop that have not ended
	std::size_t soft_open_ = 0;                         // the catalog's soft_open (D11)
	std::chrono::milliseconds patience_{30s};           // how long the destructor waits for the shutdown before debug builds abort
	// The IO of an index's files, by the index's path; empty: a PosixIO on <path>/strata. Set before the first pin.
	std::function<std::unique_ptr<kronuz::journal::IO>(const std::string& path)> index_io_;

	// The catalog's timer (on the control loop), its budget, its shutdown, and the catalog, in the order they are
	// made: the catalog goes first, once the timer has run what is still armed (its destructor waits for them).
	struct Timer;
	std::shared_ptr<Timer> timer_;
	std::unique_ptr<strata::resource_budget> budget_;
	struct Shutdown;
	std::mutex shutdown_mtx_;
	std::shared_ptr<Shutdown> shutdown_;  // under shutdown_mtx_
	std::unique_ptr<IndexCatalog> catalog_;

	IndexCatalog::host make_host(std::size_t database_pool_size, const IndexSweep& sweep);

	// Starts the catalog's shutdown once; what it awaits ends on the thread that releases the last pin or chore.
	Shutdown& begin_shutdown();

public:
	// `database_pool_size` is the open indexes the catalog's sweep keeps (soft_open). `readers_per_index` is the
	// views of one index the host's threads hold at once (a search holds one per index it reads); a read past it
	// answers busy (503). The catalog's timers run on `timers`, the control loop, which outlives the pool.
	DatabasePool(engine::AsioHost& timers, size_t database_pool_size, unsigned work_threads = 0, size_t readers_per_index = engine::default_readers_per_index, IndexSweep sweep = {});
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
	IndexCatalog& catalog() noexcept {
		return *catalog_;
	}

	// The open indexes the sweep keeps (soft_open): the pool size, at most what the descriptors' soft cap holds (D11).
	std::size_t soft_open() const noexcept {
		return soft_open_;
	}

	void set_control(engine::Host* control) noexcept {
		control_.store(control, std::memory_order_release);
	}

	void set_turn_timeout(double seconds) noexcept {
		turn_timeout_.store(seconds, std::memory_order_relaxed);
	}

	// Resizes the records of the indexes opened from now on (a test sizing them small).
	void set_readers_per_index(size_t records) noexcept {
		readers_per_index_.store(records, std::memory_order_relaxed);
	}

	void set_buffered_docs_cap(std::uint64_t cap) noexcept {
		buffered_docs_cap_.store(cap, std::memory_order_relaxed);
	}

	void set_wal_writer(DatabaseWALWriter* writer) noexcept {
		wal_writer_.store(writer, std::memory_order_release);
	}

	// Gives the indexes' files another IO (a test's strata::faultfs); set before the first pin. An IO with no index
	// in it is a missing index, created by a create pin.
	void set_index_io(std::function<std::unique_ptr<kronuz::journal::IO>(const std::string& path)> io) {
		index_io_ = std::move(io);
	}

	// Runs a task that no request awaits (a timer's commit): on the control loop when there is one, else to its
	// end on this thread.
	void run_detached(task::task<void> t);

	// The tasks run_detached posted to the control loop that have not ended. A shutdown that stops the loop
	// clears the control with set_control(nullptr), then runs the loop until this is zero.
	std::size_t detached() const noexcept {
		return detached_.load();
	}

	// A request's pin on the index of this path (D3, D4): it opens the index, or wakes it, and a missing one is
	// created when `flags` ask for DB_CREATE_OR_OPEN (the create intent, F1); otherwise it raises not_found. A task
	// that holds a view and pins fails the view rule (debug builds): a pin may open or wake its index, which a held
	// view could be keeping from happening. A requested `cancel` ends the wait with task::cancelled (the open it
	// started goes on); `use` is false for a re-pin within the request, which the closing policy does not count (CT-R31); a pin of a key the task already pins waits for itself while that index is replaced.
	task::task<IndexPin> pin(const Endpoint& endpoint, int flags, task::cancel_token cancel = {}, bool use = true);

	// A chore's resident pin (the autocommit, FLUSH): it opens nothing, and an index that is not open gives an
	// empty pin (one that closed was committed by its close), as does one whose open failed (C25). A requested
	// `cancel` ends the wait with task::cancelled.
	task::task<IndexPin> pin_chore(const Endpoint& endpoint, task::cancel_token cancel = {});

	// The indexes something still holds (a pin, a transition), one line each. For the shutdown's report of what
	// it abandoned.
	std::vector<std::string> busy();

	// Commits every open index (FLUSH, x-pool D8): each key of the catalog's snapshot is pinned resident and
	// committed; one closed in between was committed by its close. A requested `cancel` ends it at its next pin.
	task::task<void> flush(task::cancel_token cancel = {});

	// Shutdown: no new use. join() is called until it returns true; it runs on a thread that may wait (the main
	// thread), never on a loop. Every root that holds a pin is destroyed before (C24).
	void finish();

	// Awaits the catalog's shutdown until `wakeup`: every index committed and closed. True once it ended.
	bool join(std::chrono::steady_clock::time_point wakeup);

	bool join(std::chrono::milliseconds timeout = 60s) {
		return join(std::chrono::steady_clock::now() + timeout);
	}

	// The indexes the catalog has (any state but free), and of them the ones open (open or owed).
	std::pair<size_t, size_t> count();

	std::string __repr__() const;
	std::string dump_databases(int level = 1) const;
};
