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

// The index registry (docs/design/phase7c.md D5, D6, D7): single-flight opens that never block a
// thread, the turn every operation on an index takes (timers included), a deadline that answers
// `busy`, the close protocol and a request racing it, one turn per task, a commit that joins the one in
// flight, the WAL's order across writers, and the engine in host mode behind all of it. Each test opens
// real indexes in the working directory.

#include "test_harness.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <deque>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "database/data.h"
#include "database/flags.h"
#include "database/handler.h"
#include "database/lock.h"
#include "database/pool.h"
#include "pool_support.h"
#include "database/shard.h"
#include "database/wal.h"
#include "engine/callbacks.h"
#include "engine/chores.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/host_asio.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "engine/strata/owed.h"
#include "compressor_lz4.h"
#include "length.h"
#include "node.h"
#include "reactor.h"
#include "strata/core/faultfs.hh"
#include "task.hh"
#include "utype.hh"
#include "work_pool.hh"

using namespace std::chrono_literals;

struct PoolTestAccess {
	// Swaps an open entry's chores: the engine runs its own on the new ones from here on.
	static void set_chores(ShardEndpoint& entry, std::unique_ptr<engine::Chores> chores) {
		entry.chores = std::move(chores);
		entry.engine_->use_chores(entry.chores.get());
	}
	static std::unique_ptr<engine::Chores> take_chores(ShardEndpoint& entry) { return std::move(entry.chores); }
	// The catalog's commit chore, as the catalog runs it (no pin held by the caller's turn).
	static void commit_blocking(ShardEndpoint& entry) { entry.commit_blocking(); }
	static prospect::index& index(ShardEndpoint& entry) { return *entry.index_; }
	static IndexCatalog::handle where(const IndexPin& pin) { return pin.pin_.where(); }
	static void patience(DatabasePool& pool, std::chrono::milliseconds wait) { pool.patience_ = wait; }
	// Whether nobody holds the index's turn right now.
	static bool turn_is_free(ShardEndpoint& entry) {
		if (!entry.turn.try_lock()) { return false; }
		entry.turn.unlock();
		return true;
	}
	static std::uint64_t applied(ShardEndpoint& entry) { return entry.applied_.load(); }
	static engine::Chores& chores(ShardEndpoint& entry) { return *entry.chores; }
	// As if a task posted to the control loop had not ended.
	static void count_detached(DatabasePool& pool, int by) {
		if (by > 0) { pool.detached_.fetch_add(static_cast<std::size_t>(by)); } else { pool.detached_.fetch_sub(static_cast<std::size_t>(-by)); }
	}
	// As if a write landed after a commit began: one more write applied, and not yet committed.
	static void write_lands(ShardEndpoint& entry) {
		entry.applied_.fetch_add(1);
		entry.modified.store(true);
	}
	// Wraps the pool's executor, which the indexes opened from here on run their merges on; call it before any pin.
	// The catalog keeps the inner one, which the wrapper owns, so it lives as long as before.
	template <typename Wrapper>
	static Wrapper& wrap_executor(DatabasePool& pool) {
		auto wrapper = std::make_unique<Wrapper>(std::move(pool.executor_));
		auto* raw = wrapper.get();
		pool.executor_ = std::move(wrapper);
		return *raw;
	}
};

namespace {

using A = PoolTestAccess;

std::string fresh(const char* name) {
	std::string path = std::string("pool_test_") + name + ".db";
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.set_data(data);
	return doc;
}

template <typename F>
engine::errc error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

// Writes without a WAL or an autocommit (those need a manager): the registry's own behaviour.
constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT;

// A request's pin on the index at `path` (it opens it, and creates it with WRITE's flags).
IndexPin pin(DatabasePool& pool, const std::string& path, int flags = WRITE) {
	return engine::sync_wait(pool.pin(Endpoint(path), flags));
}

// A use of the index `entry` pins: a pin of its own, as lock_shard makes one.
Shard use(const IndexPin& entry, int flags) {
	return Shard(engine::sync_wait(entry.pool()->pin(Endpoint(entry->path), flags)), flags);
}

// The slot of the index a pin holds, and its state in the catalog's snapshot (free when it has none).
std::uint32_t slot_of(const IndexPin& pin) {
	return A::where(pin).slot;
}

strata::slot_state state_of(DatabasePool& pool, const std::string& path) {
	for (const auto& slot : pool.catalog().snapshot()) {
		if (slot.key == path) {
			return slot.state;
		}
	}
	return strata::slot_state::free;
}

// Waits for `flag`, up to a few seconds.
bool eventually(const std::atomic<bool>& flag, std::chrono::milliseconds limit = 5s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!flag && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	return flag.load();
}

// Holds an index's turn from a thread of its own until released: the tests then run an operation and see whether it waits.
class HoldTurn {
public:
	HoldTurn(ShardEndpoint& entry, int flags) {
		thread_ = std::thread([this, &entry, flags] {
			engine::sync_wait(hold(entry, flags));
		});
		wait_until_held();
	}
	~HoldTurn() { release(); }

	void release() {
		release_.set();
		if (thread_.joinable()) {
			thread_.join();
		}
	}

private:
	void wait_until_held() { eventually(held_); }

	task::task<void> hold(ShardEndpoint& entry, int flags) {
		auto turn = co_await entry.take_turn(flags);
		held_ = true;
		co_await release_;
	}

	std::atomic<bool> held_{false};
	task::event release_;
	std::thread thread_;
};

// Runs `op` on a thread of its own, and reports whether it ended: an operation that takes the turn does not end while it is held.
class Running {
public:
	explicit Running(std::function<void()> op) {
		thread_ = std::thread([this, op = std::move(op)] {
			try {
				op();
			} catch (const engine::Error&) {
				error_ = std::current_exception();   // read by kind() after the join, where TSan sees the order
				threw_ = true;
			}
			done_ = true;
		});
	}
	~Running() { join(); }
	bool done() const { return done_; }
	bool threw() const { return threw_; }
	// What the operation threw, read here and not in the thread: waiters that share one exception object release it
	// through a refcount in libstdc++, which TSan does not see, so a read in the thread looks like a race with the free.
	engine::errc kind() const {
		if (!error_) {
			return engine::errc::internal;
		}
		try {
			std::rethrow_exception(error_);
		} catch (const engine::Error& error) {
			return error.kind;
		}
	}
	void join() {
		if (thread_.joinable()) {
			thread_.join();
		}
	}

private:
	std::atomic<bool> done_{false};
	bool threw_ = false;
	std::exception_ptr error_;
	std::thread thread_;
};

// A runner of chores that can hold or fail the next flush-class one: how the tests make a commit slow or broken.
class GateChores final : public engine::Chores {
public:
	explicit GateChores(std::unique_ptr<engine::Chores> inner) : inner_(std::move(inner)) { }

	// The next flush chore waits for release(), then fails when `fail`.
	void arm(bool fail = false) {
		std::lock_guard<std::mutex> lk(m_);
		armed_ = true;
		fail_ = fail;
		gate_ = std::make_shared<task::event>();
		entered_ = false;
	}
	const std::atomic<bool>& entered_flag() const { return entered_; }
	void release() {
		std::shared_ptr<task::event> gate;
		{
			std::lock_guard<std::mutex> lk(m_);
			gate = gate_;
		}
		if (gate) {
			gate->set();
		}
	}

	task::task<void> run(engine::ChoreClass cls, std::function<void()> fn) override {
		if (cls == engine::ChoreClass::flush) {
			std::shared_ptr<task::event> gate;
			bool fail = false;
			{
				std::lock_guard<std::mutex> lk(m_);
				if (armed_) {
					armed_ = false;
					gate = gate_;
					fail = fail_;
				}
			}
			if (gate) {
				entered_ = true;
				co_await *gate;
				if (fail) {
					throw engine::Error(engine::errc::database, "injected failure of a commit");
				}
			}
		}
		co_await inner_->run(cls, std::move(fn));
	}

private:
	std::unique_ptr<engine::Chores> inner_;
	std::mutex m_;
	bool armed_ = false;
	bool fail_ = false;
	std::shared_ptr<task::event> gate_;
	std::atomic<bool> entered_{false};
};

// Counts the chores of each class that an index runs.
class CountingChores final : public engine::Chores {
public:
	explicit CountingChores(std::unique_ptr<engine::Chores> inner) : inner_(std::move(inner)) { }
	task::task<void> run(engine::ChoreClass cls, std::function<void()> fn) override {
		++counts_[static_cast<int>(cls)];
		co_await inner_->run(cls, std::move(fn));
	}
	std::size_t count(engine::ChoreClass cls) const { return counts_[static_cast<int>(cls)].load(); }

private:
	std::unique_ptr<engine::Chores> inner_;
	std::atomic<std::size_t> counts_[3] = {};
};

CountingChores& count_chores(ShardEndpoint& entry) {
	auto counting = std::make_unique<CountingChores>(A::take_chores(entry));
	auto* raw = counting.get();
	A::set_chores(entry, std::move(counting));
	return *raw;
}

GateChores& gate_chores(ShardEndpoint& entry) {
	auto gate = std::make_unique<GateChores>(A::take_chores(entry));
	auto* raw = gate.get();
	A::set_chores(entry, std::move(gate));
	return *raw;
}


// An executor whose work stops at its next checkpoint while held: a merge then stays pending until let go.
class GatedExecutor final : public strata::executor {
public:
	explicit GatedExecutor(std::unique_ptr<strata::executor> inner) : inner_(std::move(inner)) { }
	~GatedExecutor() override { let_go(); }

	void hold() {
		std::lock_guard<std::mutex> lk(m_);
		held_ = true;
	}
	void let_go() {
		{
			std::lock_guard<std::mutex> lk(m_);
			held_ = false;
		}
		cv_.notify_all();
	}

	std::unique_ptr<group> open() override { return inner_->open(); }
	std::uint64_t now() override { return inner_->now(); }
	bool sleep_until(std::uint64_t t, const task::cancel_token& stop) override { return inner_->sleep_until(t, stop); }
	bool checkpoint(const task::cancel_token& stop) override {
		std::unique_lock<std::mutex> lk(m_);
		while (held_ && !stop.requested()) {
			cv_.wait_for(lk, 1ms);
		}
		lk.unlock();
		return inner_->checkpoint(stop);
	}

private:
	std::unique_ptr<strata::executor> inner_;
	std::mutex m_;
	std::condition_variable cv_;
	bool held_ = false;
};

// Waits for `pred`, up to a few seconds.
template <typename Pred>
bool until(Pred&& pred, std::chrono::milliseconds limit = 5s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!pred() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(1ms);
	}
	return pred();
}

// A sweep at 1 kHz that demotes and closes an index idle for a millisecond.
IndexSweep fast_sweep() {
	return {1ms, 1ms, 1ms};
}

// Writes one-document segments until the index has a merge pending (the policy merges them).
void make_a_merge(Shard& shard, prospect::index& index) {
	for (int i = 0; i < 200 && index.merges_pending() == 0; ++i) {
		engine::sync_wait(shard.add_document(make("K" + std::to_string(i), "d")));
		engine::sync_wait(shard.engine_for_read());
	}
}


// A spy that stops inside its first see() until released: the search that runs it holds its views of the index.
class BlockingSpy final : public engine::MatchSpy {
public:
	std::atomic<bool> inside{false};
	std::shared_future<void> gate;

	void see(const engine::DocView&, double) override {
		if (!inside.exchange(true)) {
			gate.wait();
		}
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<BlockingSpy>(); }
	std::string name() const override { return "BlockingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<BlockingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "BlockingSpy"; }
};

// A search held open on a thread of its own, between the first match it sees and release(). It ended at once when
// it threw (kind() says why).
class HeldSearch {
public:
	explicit HeldSearch(Shard& shard) {
		spy_.gate = release_.get_future().share();
		thread_ = std::thread([this, &shard] {
			try {
				auto* eng = engine::sync_wait(shard.engine_for_read());
				auto prepared = engine::prepare(engine::Query::term("f", "body"));
				engine::SearchRequest request;
				request.query = prepared.get();
				request.maxitems = 10;
				request.spy = &spy_;
				hits_ = engine::sync_wait(engine::search(std::vector<engine::Shard*>{eng}, request)).hits.size();
			} catch (const engine::Error& error) {
				kind_ = error.kind;
				threw_ = true;
			}
			done_ = true;
		});
		const auto end = std::chrono::steady_clock::now() + 5s;
		while (!spy_.inside && !done_ && std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(1ms);
		}
	}
	~HeldSearch() { release(); }

	bool holding() const { return spy_.inside && !done_; }
	bool threw() const { return threw_; }
	engine::errc kind() const { return kind_; }
	std::size_t hits() const { return hits_; }

	void release() {
		if (!released_) {
			released_ = true;
			release_.set_value();
		}
		if (thread_.joinable()) {
			thread_.join();
		}
	}

private:
	BlockingSpy spy_;
	std::promise<void> release_;
	std::thread thread_;
	std::atomic<bool> done_{false};
	std::atomic<bool> released_{false};
	bool threw_ = false;
	engine::errc kind_ = engine::errc::internal;
	std::size_t hits_ = 0;
};

std::size_t count_files(const std::string& path) {
	std::size_t files = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
		files += entry.is_regular_file() ? 1 : 0;
	}
	return files;
}


// Kills the process when a test hangs: a task that blocks a thread it needed shows as a hang.
class Watchdog {
public:
	explicit Watchdog(const char* name, std::chrono::seconds limit = 60s) : name_(name) {
		thread_ = std::thread([this, limit] {
			const auto end = std::chrono::steady_clock::now() + limit;
			while (!done_ && std::chrono::steady_clock::now() < end) {
				std::this_thread::sleep_for(20ms);
			}
			if (!done_) {
				std::fprintf(stderr, "  FAIL %s: hung\n", name_);
				std::_Exit(1);
			}
		});
	}
	~Watchdog() {
		done_ = true;
		thread_.join();
	}

private:
	const char* name_;
	std::atomic<bool> done_{false};
	std::thread thread_;
};

// The committed revision of the index a Shard uses.
engine::rev revision_of(Shard& shard) {
	engine::sync_wait(shard.ready());
	return shard.engine().revision();
}

// A write, a read, and a commit through a Shard.
void test_smoke() {
	DatabasePool pool(control_loop(), 8);
	Endpoint endpoint(fresh("smoke"));
	auto entry = pin(pool, endpoint.path);
	Shard shard = use(entry, WRITE);
	auto written = engine::sync_wait(shard.add_document(make("Ka", "a")));
	CHECK_EQ(written.did, 1u);
	auto view = engine::sync_wait(shard.get_view(1));  // the read refreshes first: the write is seen
	CHECK_EQ(view->data(), "a");
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_EQ(pool.catalog().stats().opens, 1u);
}

// Opening never waits on a thread: two uses of one index on one thread open it once and finish, though
// the second must wait for the first's open. A thread that blocked on the open could not run the chore's
// completion that the first is waiting for.
void test_opening_never_waits_on_a_thread() {
	Watchdog watchdog("opening never waits on a thread");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("nowait"));
	Shard a = use(entry, WRITE);
	Shard b = use(entry, WRITE);
	std::vector<task::task<void>> both;
	both.push_back(a.ready());
	both.push_back(b.ready());
	engine::sync_wait(task::when_all(std::move(both)));
	CHECK_EQ(pool.catalog().stats().opens, 1u);
}

// Many users of a new index at once: one open (the single flight), and every user gets the engine.
void test_concurrent_opens_are_one_flight() {
	Watchdog watchdog("concurrent opens are one flight");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("flight"));
	std::vector<std::thread> threads;
	std::atomic<int> opened{0};
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([&] {
			Shard shard = use(entry, WRITE);
			engine::sync_wait(shard.ready());
			if (!shard.engine().uuid().empty()) {
				++opened;
			}
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_EQ(opened.load(), 8);
	CHECK_EQ(pool.catalog().stats().opens, 1u);
}

// Every operation on an index takes its turn: it does not end while another task holds the turn, and it
// ends once the turn is released.
template <typename Op>
void waits_for_the_turn(const char* name, Op op) {
	Watchdog watchdog(name);
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh(name));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Kseed", "seed")));  // something to replace, delete and commit
	{
		HoldTurn hold(entry.entry(), WRITE);
		Running running([&] { op(shard); });
		std::this_thread::sleep_for(150ms);
		CHECK_FALSE(running.done());
		hold.release();
		running.join();
		CHECK_TRUE(running.done());
		CHECK_FALSE(running.threw());
	}
}

void test_every_operation_takes_the_turn() {
	waits_for_the_turn("turn_add", [](Shard& s) { engine::sync_wait(s.add_document(make("Kb", "b"))); });
	waits_for_the_turn("turn_replace", [](Shard& s) { engine::sync_wait(s.replace_document(1, make("Kseed", "again"))); });
	waits_for_the_turn("turn_replace_term", [](Shard& s) { engine::sync_wait(s.replace_document_term("Kseed", make("Kseed", "term"))); });
	waits_for_the_turn("turn_delete", [](Shard& s) { engine::sync_wait(s.delete_document(1)); });
	waits_for_the_turn("turn_delete_term", [](Shard& s) { engine::sync_wait(s.delete_document_term("Kseed")); });
	waits_for_the_turn("turn_metadata", [](Shard& s) { engine::sync_wait(s.set_metadata("name", "value")); });
	waits_for_the_turn("turn_commit", [](Shard& s) { engine::sync_wait(s.commit()); });
	// A read that finds something applied since the last refresh refreshes: a lease operation.
	waits_for_the_turn("turn_refresh_for_read", [](Shard& s) { engine::sync_wait(s.get_view(1)); });
}

// A read that has nothing to refresh takes no turn: it does not wait behind a writer.
void test_a_read_with_nothing_to_refresh_takes_no_turn() {
	Watchdog watchdog("a read takes no turn");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("readsnoturn"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.engine_for_read());  // refreshes: nothing is owed now
	HoldTurn hold(entry.entry(), WRITE);
	std::string data;
	Running running([&] { data = engine::sync_wait(shard.get_view(1))->data(); });
	std::this_thread::sleep_for(150ms);
	CHECK_TRUE(running.done());
	hold.release();
	running.join();
	CHECK_EQ(data, "a");
}

// The debounced autocommit is a task that takes the turn like any request, and commits what is pending.
void test_the_autocommit_task_takes_the_turn() {
	Watchdog watchdog("the autocommit task");
	DatabasePool pool(control_loop(), 8);
	Endpoint endpoint(fresh("autocommit"));
	auto entry = pin(pool, endpoint.path);
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	const auto revision = revision_of(shard);
	CHECK_TRUE(entry->is_modified());
	{
		HoldTurn hold(entry.entry(), WRITE);
		Running running([&] { engine::sync_wait(autocommit(pool, endpoint)); });
		std::this_thread::sleep_for(150ms);
		CHECK_FALSE(running.done());
		hold.release();
		running.join();
		CHECK_FALSE(running.threw());
	}
	CHECK_FALSE(entry->is_modified());
	CHECK_EQ(revision_of(shard), revision + 1);
}

// A turn that does not come in time answers `busy` (a 503), and the queue it left is whole.
void test_a_turn_wait_times_out_with_busy() {
	Watchdog watchdog("a turn wait times out");
	DatabasePool pool(control_loop(), 8);
	pool.set_turn_timeout(0.3);
	auto entry = pin(pool, fresh("busy"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.ready());
	{
		HoldTurn hold(entry.entry(), WRITE);
		const auto start = std::chrono::steady_clock::now();
		Running running([&] { engine::sync_wait(shard.add_document(make("Ka", "a"))); });
		running.join();
		const auto waited = std::chrono::steady_clock::now() - start;
		CHECK_TRUE(running.threw());
		CHECK_TRUE(running.kind() == engine::errc::busy);
		CHECK_GE(waited, 250ms);
		CHECK_LT(waited, 5s);
	}
	// The timed-out waiter left nothing behind: the next one is served.
	CHECK_EQ(engine::sync_wait(shard.add_document(make("Kb", "b"))).did, 1u);
}

// A task that takes a second index's turn while it holds the first would deadlock; debug builds abort.
task::task<void> two_turns(ShardEndpoint& first, ShardEndpoint& second) {
	auto one = co_await first.take_turn(WRITE);
	auto two = co_await second.take_turn(WRITE);
}

void test_a_task_holds_one_turn_at_most() {
#ifndef NDEBUG
	Watchdog watchdog("one turn per task");
	DatabasePool pool(control_loop(), 8);
	auto first = pin(pool, fresh("one_a"));
	auto second = pin(pool, fresh("one_b"));
	Shard a = use(first, WRITE);
	Shard b = use(second, WRITE);
	engine::sync_wait(a.ready());
	engine::sync_wait(b.ready());
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		engine::sync_wait(two_turns(first.entry(), second.entry()));
		::_exit(0);  // not reached when the assertion is on
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	CHECK_TRUE(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#endif
}

// Commits coalesce by position (C7). A commit in flight is joined by a commit whose writes it covers, which
// then waits for no commit of its own; a write that arrives while it runs waits for the turn and for the next
// commit.
void test_a_commit_joins_the_one_in_flight() {
	Watchdog watchdog("a commit joins the one in flight");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("coalesce"));
	auto& gate = gate_chores(entry.entry());
	Shard s1 = use(entry, WRITE), s2 = use(entry, WRITE), s3 = use(entry, WRITE);
	engine::sync_wait(s1.add_document(make("Ka", "a")));
	const auto revision = revision_of(s1);
	gate.arm();
	std::atomic<int> changed{0};
	Running first([&] { changed += engine::sync_wait(s1.commit()) ? 1 : 0; });
	CHECK_TRUE(eventually(gate.entered_flag()));
	Running joined([&] { changed += engine::sync_wait(s2.commit()) ? 1 : 0; });  // its write is covered: no turn of its own
	Running later([&] { engine::sync_wait(s3.add_document(make("Kb", "b"))); });  // needs the turn: waits
	std::this_thread::sleep_for(150ms);
	CHECK_FALSE(first.done());
	CHECK_FALSE(joined.done());
	CHECK_FALSE(later.done());
	gate.release();
	first.join();
	joined.join();
	later.join();
	CHECK_EQ(changed.load(), 2);                  // both saw the one commit
	CHECK_EQ(revision_of(s1), revision + 1);  // and there was one
	// The write that waited is not covered: its own commit is the next one.
	CHECK_TRUE(engine::sync_wait(s3.commit()));
	CHECK_EQ(revision_of(s1), revision + 2);
}

// The rule behind the join is a position: a commit joins the one in flight only if that commit covers its writes. A write
// that lands after the commit began is not covered, so it does not share the commit's outcome, failure included.
void test_a_commit_not_covered_does_not_share_the_outcome() {
	Watchdog watchdog("a commit not covered");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("covered"));
	auto& gate = gate_chores(entry.entry());
	Shard s1 = use(entry, WRITE), s2 = use(entry, WRITE);
	engine::sync_wait(s1.add_document(make("Ka", "a")));
	gate.arm(true);  // the commit in flight will fail
	Running first([&] { engine::sync_wait(s1.commit()); });
	CHECK_TRUE(eventually(gate.entered_flag()));
	A::write_lands(entry.entry());
	Running second([&] { engine::sync_wait(s2.commit()); });  // its position is past the commit's: it waits for its own turn
	std::this_thread::sleep_for(150ms);
	CHECK_FALSE(second.done());
	gate.release();
	first.join();
	second.join();
	CHECK_TRUE(first.threw());
	CHECK_FALSE(second.threw());  // it ran its own commit
}

// A failed commit reaches the commit that ran it and the waiters that joined it, and no one else: the write
// that waited for the turn is served on the replacement engine.
void test_a_failed_commit_reaches_only_the_joined_waiters() {
	Watchdog watchdog("a failed commit");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("failed"));
	auto& gate = gate_chores(entry.entry());
	Shard s1 = use(entry, WRITE), s2 = use(entry, WRITE), s3 = use(entry, WRITE);
	engine::sync_wait(s1.add_document(make("Ka", "a")));
	gate.arm(true);
	Running first([&] { engine::sync_wait(s1.commit()); });
	CHECK_TRUE(eventually(gate.entered_flag()));
	Running joined([&] { engine::sync_wait(s2.commit()); });
	Running later([&] { engine::sync_wait(s3.add_document(make("Kb", "b"))); });
	std::this_thread::sleep_for(150ms);
	gate.release();
	first.join();
	joined.join();
	later.join();
	CHECK_TRUE(first.threw());
	CHECK_TRUE(first.kind() == engine::errc::database);
	CHECK_TRUE(joined.threw());
	CHECK_TRUE(joined.kind() == engine::errc::database);
	CHECK_FALSE(later.threw());
	CHECK_EQ(engine::sync_wait(s3.get_view(2))->data(), "b");
}

// Writers on many threads with a buffer of a few documents: the engine owes a flush all the time, each
// flush runs as a chore while the writer holds the turn, and the same write is applied after it before any
// later write: ids are contiguous, none repeats, and each thread's ids rise.
void test_owed_flushes_do_not_reorder_writers() {
	Watchdog watchdog("owed flushes", 120s);
	DatabasePool pool(control_loop(), 8);
	pool.set_buffered_docs_cap(5);
	auto entry = pin(pool, fresh("owed"));
	constexpr int threads_n = 4;
	constexpr int per_thread = 60;
	std::vector<std::vector<engine::docid>> got(threads_n);
	std::vector<std::thread> threads;
	for (int t = 0; t < threads_n; ++t) {
		threads.emplace_back([&, t] {
			Shard shard = use(entry, WRITE);
			for (int i = 0; i < per_thread; ++i) {
				got[t].push_back(engine::sync_wait(shard.add_document(make("K" + std::to_string(t) + "-" + std::to_string(i), "d"))).did);
			}
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
	std::set<engine::docid> all;
	for (int t = 0; t < threads_n; ++t) {
		CHECK_TRUE(std::is_sorted(got[t].begin(), got[t].end()));
		all.insert(got[t].begin(), got[t].end());
	}
	CHECK_EQ(all.size(), static_cast<std::size_t>(threads_n * per_thread));
	CHECK_EQ(*all.begin(), 1u);
	CHECK_EQ(*all.rbegin(), static_cast<engine::docid>(threads_n * per_thread));
	Shard reader = use(entry, WRITE);
	CHECK_EQ(engine::sync_wait(reader.engine_for_read())->doc_count(), static_cast<engine::doccount>(threads_n * per_thread));
}

// Blob I/O is device work, so it runs as chores: the blob's write is a flush chore (inside the document's turn, before
// the apply), its commit another, and its read a query chore.
void test_blob_io_runs_in_chores() {
	Watchdog watchdog("blob I/O");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("blobs"));
	auto& counting = count_chores(entry.entry());
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.ready());
	Data data;
	data.update(ct_type_t("application/octet-stream"), -1, 0, 0, std::string("blobbytes"));
	data.flush();
	auto doc = make("Kblob", "");
	doc.set_data(data.serialise());
	const auto flushes = counting.count(engine::ChoreClass::flush);
	const auto written = engine::sync_wait(shard.add_document(std::move(doc)));
	CHECK_GT(counting.count(engine::ChoreClass::flush), flushes);  // the blob was pushed in a chore
	const auto commits = counting.count(engine::ChoreClass::flush);
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_GE(counting.count(engine::ChoreClass::flush), commits + 2);  // the storage's fsync and the engine's commit
	auto view = engine::sync_wait(shard.get_view(written.did));
	Data stored(view->data());
	const Locator* locator = nullptr;
	for (const auto& candidate : stored) {
		if (candidate.volume != -1) {
			locator = &candidate;
		}
	}
	REQUIRE(locator != nullptr);
	const auto queries = counting.count(engine::ChoreClass::query);
	const auto blob = engine::sync_wait(shard.storage_get_stored(*locator));
	CHECK_GT(counting.count(engine::ChoreClass::query), queries);  // the read was a chore
	CHECK_TRUE(blob.find("blobbytes") != std::string::npos);
}

// The WAL's order is the apply order across writers (C6): the turn covers the apply, the revision update and the WAL
// enqueue, so lines reach the log in the order the documents were assigned their ids.
void test_the_wal_order_is_the_apply_order() {
	Watchdog watchdog("the WAL order", 120s);
	DatabasePool pool(control_loop(), 8);
	DatabaseWALWriter writer("WL{:02}", 1);
	pool.set_wal_writer(&writer);
	Endpoint endpoint(fresh("walorder"));
	constexpr int threads_n = 4;
	constexpr int per_thread = 50;
	{
		auto entry = pin(pool, endpoint.path);
		std::vector<std::thread> threads;
		for (int t = 0; t < threads_n; ++t) {
			threads.emplace_back([&, t] {
				Shard shard = use(entry, DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT);
				for (int i = 0; i < per_thread; ++i) {
					engine::sync_wait(shard.add_document(make("K" + std::to_string(t) + "-" + std::to_string(i), "d")));
				}
			});
		}
		for (auto& thread : threads) {
			thread.join();
		}
	}
	pool.finish();
	CHECK_TRUE(pool.join());  // the shutdown's commit writes its line before the writer ends
	writer.end();  // the queue drains, then the thread ends
	writer.join(std::chrono::milliseconds(20000));
	// The log, line by line: the ids of the replaced documents, in the order they reached it.
	std::vector<engine::docid> logged;
	DatabaseWAL wal(endpoint.path);
	for (auto it = wal.begin(); it != wal.end(); ++it) {
		const std::string line = *it;
		const char* p = line.data();
		const char* p_end = p + line.size();
		(void)unserialise_length(&p, p_end);  // the revision
		const auto type = static_cast<DatabaseWAL::Type>(unserialise_length(&p, p_end));
		if (type == DatabaseWAL::Type::REPLACE_DOCUMENT) {
			const auto data = decompress_lz4(std::string_view(p, p_end - p));
			const char* q = data.data();
			logged.push_back(static_cast<engine::docid>(unserialise_length(&q, q + data.size())));
		}
	}
	CHECK_EQ(logged.size(), static_cast<std::size_t>(threads_n * per_thread));
	for (std::size_t i = 0; i < logged.size(); ++i) {
		if (logged[i] != i + 1) {
			CHECK_EQ(logged[i], i + 1);
			break;
		}
	}
}


// A search held for a second delays neither a write, a refresh nor a commit of the index it reads: it holds a view
// of the index, not a lock (D6). The reads that tasks on the control loop make (the work owed, the revision) do
// not wait for it either.
void test_a_held_search_delays_no_refresh_or_commit() {
	Watchdog watchdog("a held search");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("held"));
	Shard shard = use(entry, WRITE);
	for (int i = 1; i <= 3; ++i) {
		engine::sync_wait(shard.add_document(make("K" + std::to_string(i), "d")));
	}
	engine::sync_wait(shard.commit());
	HeldSearch held(shard);
	CHECK_TRUE(held.holding());
	const auto begin = std::chrono::steady_clock::now();

	Running writer([&] {
		engine::sync_wait(shard.add_document(make("Kw", "w")));
		auto* eng = engine::sync_wait(shard.engine_for_read());  // a refresh
		CHECK_EQ(eng->doc_count(), 4u);
		CHECK_TRUE(engine::sync_wait(shard.commit()));
	});
	Running loop([&] {
		(void)shard.engine().revision();
		(void)shard.engine().merges_pending();
	});
	const auto end = std::chrono::steady_clock::now() + 3s;
	while ((!writer.done() || !loop.done()) && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	const bool delayed = !writer.done() || !loop.done();
	CHECK_FALSE(delayed);
	CHECK_FALSE(writer.threw());
	// The search is held for a second in all.
	const auto elapsed = std::chrono::steady_clock::now() - begin;
	if (elapsed < 1s) {
		std::this_thread::sleep_for(1s - elapsed);
	}
	CHECK_TRUE(held.holding());
	held.release();
	writer.join();
	loop.join();
	CHECK_EQ(held.hits(), 3u);  // the search read the view it held: the write came after
	CHECK_EQ(engine::sync_wait(shard.engine_for_read())->doc_count(), 4u);
}

// Every reader record is claimed by a search that is running: the next one is refused with busy (a 503), the
// writes go on, and a record freed serves the next search (C9, T8).
void test_readers_cap() {
	Watchdog watchdog("the readers cap");
	DatabasePool pool(control_loop(), 8);
	pool.set_readers_per_index(16);
	auto entry = pin(pool, fresh("readers"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	std::vector<std::unique_ptr<HeldSearch>> held;
	for (int i = 0; i < 16; ++i) {
		held.push_back(std::make_unique<HeldSearch>(shard));
		CHECK_TRUE(held.back()->holding());
	}
	HeldSearch refused(shard);
	CHECK_FALSE(refused.holding());
	CHECK_TRUE(refused.threw());
	CHECK_TRUE(refused.kind() == engine::errc::busy);
	// A write and a commit need no record.
	engine::sync_wait(shard.add_document(make("Kb", "b")));
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	held.pop_back();  // frees a record
	HeldSearch served(shard);
	CHECK_TRUE(served.holding());
	served.release();
	CHECK_EQ(served.hits(), 2u);
}

// The records belong to the index: one index with all of its records held refuses its next reader, and a
// neighbour is not affected (C9).
void test_readers_cap_is_per_index() {
	Watchdog watchdog("the readers cap per index");
	DatabasePool pool(control_loop(), 8);
	pool.set_readers_per_index(4);
	auto first = pin(pool, fresh("readers_first"));
	auto second = pin(pool, fresh("readers_second"));
	Shard a = use(first, WRITE);
	Shard b = use(second, WRITE);
	for (auto* shard : {&a, &b}) {
		engine::sync_wait(shard->add_document(make("Ka", "a")));
		engine::sync_wait(shard->commit());
	}
	std::vector<std::unique_ptr<HeldSearch>> held;
	for (int i = 0; i < 4; ++i) {
		held.push_back(std::make_unique<HeldSearch>(a));
		CHECK_TRUE(held.back()->holding());
	}
	HeldSearch refused(a);
	CHECK_TRUE(refused.threw());
	CHECK_TRUE(refused.kind() == engine::errc::busy);
	HeldSearch neighbour(b);
	CHECK_TRUE(neighbour.holding());
	neighbour.release();
	CHECK_EQ(neighbour.hits(), 1u);
}

// A search over more indexes than one index has records is served: it takes one record in each (X2b-B1).
void test_a_search_over_more_indexes_than_records_is_served() {
	Watchdog watchdog("a wide search");
	constexpr std::size_t indexes = 40;
	DatabasePool pool(control_loop(), indexes + 8);
	pool.set_readers_per_index(2);
	std::vector<std::unique_ptr<Shard>> shards;
	for (std::size_t i = 0; i < indexes; ++i) {
		shards.push_back(std::make_unique<Shard>(pin(pool, fresh(("wide" + std::to_string(i)).c_str())), WRITE));
		engine::sync_wait(shards.back()->add_document(make("Ka", "a")));
		engine::sync_wait(shards.back()->commit());
	}
	std::vector<engine::Shard*> engines;
	for (auto& shard : shards) {
		engines.push_back(engine::sync_wait(shard->engine_for_read()));
	}
	auto prepared = engine::prepare(engine::Query::term("f", "body"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100;
	CHECK_EQ(engine::sync_wait(engine::search(engines, request)).hits.size(), indexes);
}

// A search names at most max_search_indexes shards (the runaway cap): the limit is served, one more is the client's
// error (a 400). A search under the cap reads any number of indexes, in batches (test_wide_search).
void test_search_fanout_limit() {
	CHECK_TRUE(error_kind([] { check_search_fanout(100000, 100000); }) == engine::errc::internal);  // no error
	CHECK_TRUE(error_kind([] { check_search_fanout(1, 100000); }) == engine::errc::internal);
	CHECK_TRUE(error_kind([] { check_search_fanout(100001, 100000); }) == engine::errc::invalid);
	CHECK_TRUE(error_kind([] { check_search_fanout(1010, 1000); }) == engine::errc::invalid);
	CHECK_TRUE(error_kind([] { check_search_fanout(1, 0); }) == engine::errc::invalid);  // 0 is not unlimited (the option refuses it)
	try {
		check_search_fanout(1001, 1000);
	} catch (const engine::Error& error) {
		const std::string message = error.description();
		CHECK_TRUE(message.find("1000") != std::string::npos);
		CHECK_TRUE(message.find("--max-search-indexes") != std::string::npos);
	}
}

// A commit whose turn times out after the write was applied does not fail the write: a client that retried it would
// create a second document. The write stands, and a later commit makes it durable (N2).
void test_a_commit_that_times_out_leaves_the_write_standing() {
	Watchdog watchdog("a late commit");
	DatabasePool pool(control_loop(), 8);
	pool.set_turn_timeout(0.3);
	auto entry = pin(pool, fresh("late_commit"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	{
		HoldTurn hold(entry.entry(), WRITE);
		engine::sync_wait(shard.finish_write(true, false));  // the commit's turn does not come: no error
	}
	CHECK_TRUE(entry->is_modified());
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_FALSE(entry->is_modified());
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
}

// A node that is finishing serves the indexes it has open (an autocommit, a request in flight) and opens no more (N3).
void test_a_finishing_pool_serves_an_open_index() {
	Watchdog watchdog("a finishing pool");
	DatabasePool pool(control_loop(), 8);
	const auto open_path = fresh("finishing_open"), closed_path = fresh("finishing_closed");
	auto open = pin(pool, open_path);
	Shard shard = use(open, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	{
		auto closed = pin(pool, closed_path);
		const auto slot = slot_of(closed);
		closed.release();
		CHECK_TRUE(pool.catalog().begin_demote(slot));
		pool.catalog().wait_chores();
		CHECK_TRUE(pool.catalog().begin_close(slot));
		pool.catalog().wait_chores();
		CHECK_TRUE(state_of(pool, closed_path) == strata::slot_state::free);
	}
	pool.finish();
	engine::sync_wait(shard.add_document(make("Kb", "b")));
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	Shard again(pin(pool, open_path), WRITE);   // a new use of an open index is served
	CHECK_EQ(engine::sync_wait(again.get_view(1))->data(), "a");
	CHECK_TRUE(error_kind([&] { (void)pin(pool, closed_path); }) == engine::errc::not_available);
	CHECK_EQ(pool.catalog().stats().opens, 2u);
}

// The engine's integrity check changes nothing but runs against the engine the node has open, so it takes the turn
// like any other operation on the index.
void test_the_integrity_check_takes_the_turn() {
	Watchdog watchdog("the check takes the turn");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("check"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	{
		HoldTurn hold(entry.entry(), WRITE);
		Running running([&] { (void)engine::sync_wait(shard.check()); });
		std::this_thread::sleep_for(150ms);
		CHECK_FALSE(running.done());
		hold.release();
		running.join();
		CHECK_TRUE(running.done());
		CHECK_FALSE(running.threw());
	}
	const auto result = engine::sync_wait(shard.check());
	CHECK_EQ(result.errors, 0u);
	CHECK_EQ(result.failure, "");
}


// A chore is awaited by a task that runs on a scheduler, which the chore resumes it on; one that runs on none (task.hh's
// own sync_wait) would continue on a pool worker, so debug builds abort (X2-N8).
void test_a_chore_needs_a_scheduler() {
#ifndef NDEBUG
	Watchdog watchdog("a chore needs a scheduler");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("scheduler"));
	Shard shard = use(entry, WRITE);
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		task::sync_wait(A::chores(entry.entry()).run(engine::ChoreClass::query, [] { }));
		::_exit(0);  // not reached when the assertion is on
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	CHECK_TRUE(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#endif
}

// A scheduler that records how it was handed work: a chore finishes on a pool worker, and only post() is thread-safe
// on a loop's scheduler, so the resume must come through it and never through post_demoted (review C10).
class SpyScheduler {
public:
	void post(std::coroutine_handle<> h) {
		std::lock_guard<std::mutex> lk(m_);
		ready_.push_back(h);
		++posts_;
		cv_.notify_one();
	}
	void post_demoted(std::coroutine_handle<> h) {
		std::lock_guard<std::mutex> lk(m_);
		ready_.push_back(h);
		++demoted_;
		cv_.notify_one();
	}
	std::uint64_t now() const noexcept { return 0; }
	int posts() { std::lock_guard<std::mutex> lk(m_); return posts_; }
	int demoted() { std::lock_guard<std::mutex> lk(m_); return demoted_; }

	// Runs the queued handles on this thread until `done`.
	void drive(const std::atomic<bool>& done) {
		task::running_on scope(task::scheduler_ref::of(*this));
		while (!done) {
			std::coroutine_handle<> h;
			{
				std::unique_lock<std::mutex> lk(m_);
				cv_.wait_for(lk, 5ms, [this] { return !ready_.empty(); });
				if (ready_.empty()) {
					continue;
				}
				h = ready_.front();
				ready_.pop_front();
			}
			h.resume();
		}
	}

private:
	std::mutex m_;
	std::condition_variable cv_;
	std::deque<std::coroutine_handle<>> ready_;
	int posts_ = 0;
	int demoted_ = 0;
};

// A chore resumes the task that awaits it through the scheduler's post(), from the worker that ran it, even when
// the task had used up its slice.
void test_a_chore_resumes_its_awaiter_with_post() {
	Watchdog watchdog("a chore resumes with post");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("chorepost"));
	auto chores = A::take_chores(entry.entry());
	SpyScheduler spy;
	std::atomic<bool> done{false};
	std::atomic<int> ran{0};
	std::thread::id worker;
	auto body = [&]() -> task::task<void> {
		for (int i = 0; i < 5; ++i) {
			co_await chores->run(engine::ChoreClass::query, [&] { worker = std::this_thread::get_id(); ++ran; });
		}
		done = true;
	};
	auto t = body();
	auto handle = t.release();
	spy.post(handle);
	spy.drive(done);
	CHECK_EQ(ran.load(), 5);
	CHECK_NE(worker, std::this_thread::get_id());   // the chore ran on a worker
	CHECK_EQ(spy.demoted(), 0);                     // and resumed its awaiter with post, five times
	CHECK_GE(spy.posts(), 6);                       // the first post starts the task
	handle.destroy();
}

#ifndef NDEBUG
// The registry, the lease and the chores end with nothing outstanding (D14): a debug build aborts and says what is left.
// Each scenario runs in a child, once with the outstanding work (it aborts with the message) and once without (it ends clean).
struct ChildEnd {
	bool aborted = false;
	bool clean = false;
	std::string log;
};

template <typename F>
ChildEnd in_child(const char* name, F&& body) {
	const std::string log_path = std::string("at_end_") + name + ".log";
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		std::freopen(log_path.c_str(), "w", stderr);
		std::setvbuf(stderr, nullptr, _IONBF, 0);   // freopen to a file leaves stderr buffered, and abort() does not flush it
		::alarm(30);   // a child that waits for what it should have refused to destroy ends here
		body();
		std::fflush(nullptr);
		::_exit(0);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	ChildEnd end;
	end.aborted = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
	end.clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	if (FILE* f = std::fopen(log_path.c_str(), "r")) {
		char buf[512];
		std::size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
			end.log.append(buf, n);
		}
		std::fclose(f);
	}
	return end;
}

task::task<void> take_and_drop(ShardEndpoint& entry) {
	auto turn = co_await entry.take_turn(WRITE);
}

// The lease: the index's turn is held, and a task waits for it, under a pin that outlives the pool.
void at_end_turn(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	A::patience(*pool, 1s);
	auto entry = pin(*pool, fresh("at_end_turn"));
	ShardEndpoint* raw = &entry.entry();
	auto hold = std::make_unique<HoldTurn>(*raw, WRITE);
	auto waiter = std::make_unique<Running>([raw] { engine::sync_wait(take_and_drop(*raw)); });
	std::this_thread::sleep_for(100ms);
	if (!outstanding) {
		hold->release();
		waiter->join();
		entry.release();
	}
	pool.reset();
}

// The registry: a view a reader holds when the index ends.
void at_end_views(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	A::patience(*pool, 1s);
	std::unique_ptr<HeldSearch> held;
	{
		auto entry = pin(*pool, fresh("at_end_views"));
		Shard shard = use(entry, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
		held = std::make_unique<HeldSearch>(shard);
		if (!held->holding()) {
			std::_Exit(3);
		}
	}
	if (!outstanding) {
		held->release();
	}
	pool.reset();
}

// The chores: a chore of the index is still running.
void at_end_chores(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	A::patience(*pool, 1s);
	std::atomic<bool> go{false};
	std::atomic<bool> started{false};
	ShardEndpoint* raw = nullptr;
	std::thread runner;   // awaits the chore; with work outstanding the process aborts before it ends
	{
		auto entry = pin(*pool, fresh("at_end_chores"));
		raw = &entry.entry();
		runner = std::thread([&go, &started, raw] {
			engine::sync_wait(A::chores(*raw).run(engine::ChoreClass::flush, [&go, &started] {
				started = true;
				while (!go) {
					std::this_thread::sleep_for(1ms);
				}
			}));
		});
		if (!eventually(started)) {
			std::_Exit(3);
		}
	}
	if (!outstanding) {
		go = true;
		runner.join();
	}
	pool.reset();
}

// The registry: a task posted to the control loop has not ended.
void at_end_detached(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	A::count_detached(*pool, 1);
	if (!outstanding) {
		A::count_detached(*pool, -1);
	}
	pool.reset();
}

// The registry: a reference to an entry outlives the pool.
void at_end_references(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	A::patience(*pool, 1s);
	std::optional<IndexPin> entry;
	entry.emplace(pin(*pool, fresh("at_end_refs")));
	if (!outstanding) {
		entry.reset();
	}
	pool.reset();
}

template <typename F>
void check_at_end(const char* name, F&& scenario, const char* message) {
	Watchdog watchdog(name);
	const auto with = in_child(name, [&] { scenario(true); });
	CHECK_TRUE(with.aborted);
	CHECK_TRUE(with.log.find(message) != std::string::npos);
	if (!with.aborted || with.log.find(message) == std::string::npos) {
		std::fprintf(stderr, "  %s with work outstanding: %s\n", name, with.log.c_str());
	}
	const auto without = in_child(name, [&] { scenario(false); });
	CHECK_TRUE(without.clean);
	if (!without.clean) {
		std::fprintf(stderr, "  %s without: %s\n", name, without.log.c_str());
	}
}
#endif

void test_a_registry_that_ends_with_work_outstanding_aborts() {
#ifndef NDEBUG
	check_at_end("turn", at_end_turn, "pinned");
	check_at_end("views", at_end_views, "owner_break");   // the catalog's close finds a view held past its pin
	check_at_end("chores", at_end_chores, "a chore is queued or running");
	check_at_end("detached", at_end_detached, "detached tasks have not ended");
	check_at_end("references", at_end_references, "pinned");
#endif
}

#ifndef NDEBUG
// A host that resumes each task on a thread of its own and lingers in post(): the worker that posts a chore's
// resume is still inside the chore when the task, and its owner, have gone on.
class LingeringHost final : public engine::Host {
public:
	void post(std::coroutine_handle<> h) override {
		std::thread([this, h] {
			task::running_on scope(task::scheduler_ref::of(*this));
			h.resume();
		}).detach();
		std::this_thread::sleep_for(150ms);
	}

	void post_call(void (*fn)(void*), void* arg) override {
		std::thread([fn, arg] { fn(arg); }).detach();
		std::this_thread::sleep_for(150ms);
	}

protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point, std::unique_ptr<TimerNode>) override { std::abort(); }
};

// A host that resumes each task on the thread that posts it: a task scheduler's thread that is not a waiting one.
class InlineHost final : public engine::Host {
public:
	void post(std::coroutine_handle<> h) override {
		task::running_on scope(task::scheduler_ref::of(*this));
		h.resume();
	}

	void post_call(void (*fn)(void*), void* arg) override { fn(arg); }

protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point, std::unique_ptr<TimerNode>) override { std::abort(); }
};

task::task<void> nothing() { co_return; }

task::task<void> detach_from_a_task(DatabasePool& pool) {
	pool.run_detached(nothing());
	co_return;
}

// The shutdown clears the control and drains the loop on its own thread; a debounced commit that fires there starts
// its task with no control left, from a scheduler's thread.
void detached_after_the_control_is_gone() {
	DatabasePool pool(control_loop(), 8);
	InlineHost host;
	host.spawn(detach_from_a_task(pool));
}

task::task<void> run_chore_and_drop(std::unique_ptr<IndexChores>& chores, std::atomic<bool>& done) {
	co_await chores->run(engine::ChoreClass::query, [] { });
	engine::BlockingAllowed blocking("the test drops the chores where the join waits");
	chores.reset();   // the chore has resumed its task: nothing is outstanding
	done = true;
}

void chore_dropped_by_its_resume() {
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	auto chores = std::make_unique<IndexChores>(pool->work_pool());
	LingeringHost host;
	std::atomic<bool> done{false};
	host.spawn(run_chore_and_drop(chores, done));
	if (!eventually(done, 10s)) {
		std::_Exit(3);
	}
	std::this_thread::sleep_for(300ms);   // the lingering post ends before the host goes
	pool.reset();
}
#endif

// A chore is over for its owner when its task resumes: the owner may drop the chores then, while the worker is still leaving.
void test_a_detached_task_may_block_once_the_control_is_gone() {
#ifndef NDEBUG
	Watchdog watchdog("detached without control");
	const auto end = in_child("detached_no_control", [] { detached_after_the_control_is_gone(); });
	CHECK_TRUE(end.clean);
	if (!end.clean) {
		std::fprintf(stderr, "  a detached task without a control: %s\n", end.log.c_str());
	}
#endif
}

void test_a_chore_is_over_when_its_task_resumes() {
#ifndef NDEBUG
	Watchdog watchdog("chore resume");
	const auto end = in_child("chore_resume", [] { chore_dropped_by_its_resume(); });
	CHECK_TRUE(end.clean);
	if (!end.clean) {
		std::fprintf(stderr, "  a chore dropped by its resume: %s\n", end.log.c_str());
	}
#endif
}


// A missing index is a negative entry (x-pool D3, strata F1): pins that may not create fail not_found at once, every
// one, and a create pin passes the entry and makes the index; the reads after it see it.
void test_a_create_pin_passes_a_missing_index() {
	Watchdog watchdog("a create pin passes a missing index");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("missing");
	std::atomic<int> not_found{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&] {
			try {
				(void)pin(pool, path, DB_OPEN);
			} catch (const engine::Error& error) {
				if (error.kind == engine::errc::not_found) {
					++not_found;
				}
			}
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_EQ(not_found.load(), 4);
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::failed);
	CHECK_FALSE(std::filesystem::exists(path));
	{
		Shard create(pin(pool, path, WRITE), WRITE);   // inside the backoff: a create pin passes it
		engine::sync_wait(create.add_document(make("Ka", "a")));
		engine::sync_wait(create.commit());
	}
	Shard read(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(read.get_view(1))->data(), "a");
	CHECK_EQ(pool.catalog().stats().opens, 1u);
}

// A pin keeps its index open (XP3): with a pin held the sweep's demotion and close are refused; once it goes, the
// demotion and the close take, and the next pin opens it again with the writes in it.
void test_a_pin_keeps_its_index_open() {
	Watchdog watchdog("a pin keeps its index open");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("pinned");
	auto held = pin(pool, path);
	const auto slot = slot_of(held);
	{
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());   // an idle close takes a committed index only (the autocommit's job)
	}
	CHECK_FALSE(pool.catalog().begin_demote(slot));
	CHECK_FALSE(pool.catalog().begin_close(slot));
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::open);
	held.release();
	CHECK_TRUE(pool.catalog().begin_demote(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(pool.catalog().begin_close(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::free);
	Shard again(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(again.get_view(1))->data(), "a");
	CHECK_EQ(pool.catalog().stats().opens, 2u);
}

// A chore's pin is resident (D3): it never opens. A key the catalog does not have gives an empty pin and makes no
// slot; an open index gives its entry; a closed one gives an empty pin and is not reopened.
void test_a_chore_pin_is_resident() {
	Watchdog watchdog("a chore pin is resident");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("chorepin");
	CHECK_FALSE(static_cast<bool>(engine::sync_wait(pool.pin_chore(Endpoint(path)))));
	CHECK_EQ(pool.catalog().table_size(), 0u);
	std::uint32_t slot;
	{
		auto held = pin(pool, path);
		slot = slot_of(held);
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
		auto chore = engine::sync_wait(pool.pin_chore(Endpoint(path)));
		CHECK_TRUE(static_cast<bool>(chore));
		CHECK_TRUE(&chore.entry() == &held.entry());
	}
	CHECK_TRUE(pool.catalog().begin_demote(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(pool.catalog().begin_close(slot));
	pool.catalog().wait_chores();
	const auto opens = pool.catalog().stats().opens;
	CHECK_FALSE(static_cast<bool>(engine::sync_wait(pool.pin_chore(Endpoint(path)))));
	CHECK_EQ(pool.catalog().stats().opens, opens);
	CHECK_EQ(pool.count().second, 0u);
}

// FLUSH commits every index the catalog has (D8): each is pinned resident and committed, and stays open.
void test_flush_commits_every_index() {
	Watchdog watchdog("flush commits every index");
	DatabasePool pool(control_loop(), 8);
	const auto a_path = fresh("flush_a"), b_path = fresh("flush_b");
	engine::rev a_before, b_before;
	{
		Shard a(pin(pool, a_path), WRITE), b(pin(pool, b_path), WRITE);
		engine::sync_wait(a.add_document(make("Ka", "a")));
		engine::sync_wait(b.add_document(make("Kb", "b")));
		a_before = a.engine().revision();
		b_before = b.engine().revision();
		CHECK_TRUE(a.endpoint().is_modified());
	}
	engine::sync_wait(pool.flush());
	Shard a(pin(pool, a_path), WRITE), b(pin(pool, b_path), WRITE);
	CHECK_EQ(a.engine().revision(), a_before + 1);
	CHECK_EQ(b.engine().revision(), b_before + 1);
	CHECK_FALSE(a.endpoint().is_modified());
	CHECK_EQ(pool.count().second, 2u);
	CHECK_EQ(pool.catalog().stats().opens, 2u);
}

// The shutdown commits what is applied with the entry's commit chore (D7, D10), and a pool made later opens the
// index with the writes in it.
void test_the_shutdown_commits_through_the_entry() {
	Watchdog watchdog("the shutdown commits");
	const auto path = fresh("shutdown_commit");
	engine::rev before;
	{
		DatabasePool pool(control_loop(), 8);
		Shard shard(pin(pool, path), WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		before = shard.engine().revision();
	}
	DatabasePool pool(control_loop(), 8);
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(shard.engine().revision(), before + 1);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
}

// The catalog's commit chore with nothing applied makes no revision (C25), and a resident pin's commit on an `owed`
// index applies nothing, so the index still closes when idle (the item 5b review's map_asked_ note).
void test_a_resident_commit_on_an_owed_index_lets_it_close() {
	Watchdog watchdog("a resident commit on an owed index");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("owed");
	std::uint32_t slot;
	engine::rev revision;
	{
		auto held = pin(pool, path);
		slot = slot_of(held);
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
		revision = shard.engine().revision();
		A::commit_blocking(held.entry());   // nothing applied since: no revision of its own
		CHECK_EQ(shard.engine().revision(), revision);
	}
	CHECK_TRUE(pool.catalog().begin_demote(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::owed);
	engine::sync_wait(autocommit(pool, Endpoint(path)));   // a resident pin on `owed`, granted without a wake
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::owed);
	CHECK_TRUE(pool.catalog().begin_close(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::free);
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(shard.engine().revision(), revision);
}

// The shutdown waits for every pin (C24): join() does not end while a pin is held, and ends once it goes. A root
// that holds a pin is therefore destroyed before join() is awaited, and the pool after it ended.
void test_join_waits_for_every_pin() {
	Watchdog watchdog("join waits for every pin");
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	const auto path = fresh("join_pin");
	auto held = pin(*pool, path);
	{
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
	}
	pool->finish();
	CHECK_FALSE(pool->join(100ms));
	const auto busy = pool->busy();
	REQUIRE_EQ(busy.size(), 1u);
	CHECK_TRUE(busy[0].find("pinned") != std::string::npos);
	CHECK_TRUE(error_kind([&] { (void)pin(*pool, fresh("join_late")); }) == engine::errc::not_available);
	held.release();
	CHECK_TRUE(pool->join(5s));
	pool.reset();
	DatabasePool after(control_loop(), 8);
	Shard shard(pin(after, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
}

// A use whose index failed hands its pin back (D2, F4): the catalog replaces the index once no other holder is left,
// and the use pins again by key and reads what was committed.
void test_a_failed_use_hands_its_pin_back() {
	Watchdog watchdog("a failed use hands its pin back");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("reopen");
	Shard shard(pin(pool, path), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	const auto* before = &shard.endpoint();
	try {
		throw engine::Error(engine::errc::database, "an injected failure of the index");
	} catch (const engine::Error& error) {
		try {
			shard.retry_read(error, 1);
		} catch (const engine::Error&) { }
	}
	CHECK_TRUE(engine::sync_wait(shard.reopen()));
	CHECK_EQ(pool.catalog().stats().replaces, 1u);
	CHECK_EQ(pool.catalog().stats().opens, 2u);
	CHECK_TRUE(&shard.endpoint() == before);   // reopened in place: the same slot, a new entry in it
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
	CHECK_FALSE(engine::sync_wait(shard.reopen()));   // nothing marked it again: no replacement
	CHECK_EQ(pool.catalog().stats().replaces, 1u);
}

// A pin handed back by a reopen is taken again within the request, and the re-pin is not a use for the closing
// policy (CT-R31): the slot's frequency counts the request once.
void test_a_repin_after_a_reopen_is_not_a_use() {
	Watchdog watchdog("a re-pin is not a use");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("repin_use");
	Shard shard(pin(pool, path), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	const auto freq = [&] { return IndexCatalog::freq_of(pool.catalog().word_of(0)); };
	const unsigned used = freq();
	try {
		throw engine::Error(engine::errc::database, "an injected failure of the index");
	} catch (const engine::Error& error) {
		try {
			shard.retry_read(error, 1);
		} catch (const engine::Error&) { }
	}
	CHECK_TRUE(engine::sync_wait(shard.reopen()));
	CHECK_EQ(freq(), used);   // the re-pin adds no use
	Shard next(pin(pool, path), WRITE);
	CHECK_EQ(freq(), used + 1);   // a request's own pin does
}

// Indexes whose files are on `fs` (strata's faultfs), their logs and blobs on the disk at their path, and the log's
// writer for them: a test can then fail the index's syncs, and see what the log replays.
struct FaultyIndexes {
	strata::faultfs::filesystem& fs;
	DatabasePool pool{control_loop(), 8};
	DatabaseWALWriter writer{"WF{:02}", 1};

	explicit FaultyIndexes(strata::faultfs::filesystem& fs_) : fs(fs_) {
		pool.set_wal_writer(&writer);
		pool.set_index_io([this](const std::string& path) -> std::unique_ptr<kronuz::journal::IO> {
			std::filesystem::create_directories(path);
			return std::make_unique<strata::faultfs::io>(fs);
		});
	}
	~FaultyIndexes() {
		pool.finish();
		(void)pool.join();
		writer.end();
		writer.join(std::chrono::milliseconds(20000));
	}
};

constexpr int LOGGED = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_SYNCHRONOUS_WAL | DB_DISABLE_AUTOCOMMIT;

// A cold key's request pin opens the index and replays its log before the pin is handed (XP2, D6): a shutdown whose
// commit failed leaves a write in the log only, and the next process's first pin reads it.
void test_a_cold_pin_reads_what_the_log_replays() {
	Watchdog watchdog("a cold pin replays the log", 60s);
	strata::faultfs::filesystem fs;
	const auto path = fresh("cold_replay");
	{
		FaultyIndexes indexes(fs);
		Shard shard(pin(indexes.pool, path, LOGGED), LOGGED);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		CHECK_TRUE(engine::sync_wait(shard.commit()));
		engine::sync_wait(shard.add_document(make("Kb", "b")));   // logged, not committed
		fs.add_fault({strata::faultfs::op::sync});                 // the shutdown's commit fails
	}
	fs.clear_faults();
	FaultyIndexes indexes(fs);
	Shard shard(pin(indexes.pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	CHECK_EQ(indexes.pool.catalog().stats().opens, 1u);
}

// A fenced index is replaced by the catalog (XP2, D2, C23): the use whose commit met the fence hands its pin back,
// the catalog closes the index once no other holder is left, and the next pin opens it again and reads the write
// the failed commit left in the log.
void test_a_fenced_index_is_replaced_and_replays_its_log() {
	Watchdog watchdog("a fenced index is replaced", 60s);
	strata::faultfs::filesystem fs;
	const auto path = fresh("fenced");
	FaultyIndexes indexes(fs);
	{
		Shard shard(pin(indexes.pool, path, LOGGED), LOGGED);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		CHECK_TRUE(engine::sync_wait(shard.commit()));
		engine::sync_wait(shard.add_document(make("Kb", "b")));
		fs.add_fault({strata::faultfs::op::sync});
		CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.commit()); }) == engine::errc::database);
		fs.clear_faults();
		shard.release();   // the use ends: it hands its pin back
	}
	Shard shard(pin(indexes.pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	CHECK_EQ(indexes.pool.catalog().stats().replaces, 1u);
	CHECK_EQ(indexes.pool.catalog().stats().opens, 2u);
}

// The turn's end checks the index in (D5, F6): a background merge that finished is published when the next turn is
// let go, with no write and no commit after it.
void test_the_turns_end_publishes_a_finished_merge() {
	Watchdog watchdog("the turn's end publishes a merge", 60s);
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("checkin"));
	Shard shard = use(entry, WRITE);
	auto& index = A::index(entry.entry());
	for (int i = 0; i < 200 && index.merges_pending() == 0; ++i) {   // one-document segments, which the policy merges
		engine::sync_wait(shard.add_document(make("K" + std::to_string(i), "d")));
		engine::sync_wait(shard.engine_for_read());
	}
	REQUIRE(index.merges_pending() > 0);
	const auto end = std::chrono::steady_clock::now() + 10s;
	while (index.merges_pending() != 0 && std::chrono::steady_clock::now() < end) {
		{ auto turn = engine::sync_wait(entry->take_turn(WRITE)); }
		std::this_thread::sleep_for(5ms);
	}
	CHECK_EQ(index.merges_pending(), 0u);
}

// The catalog's commit chore with nothing applied writes no WAL line of its own (C25): the log has the commit of the
// write and nothing for the chore's commits, the shutdown's included.
void test_the_commit_chore_logs_nothing_when_nothing_was_applied() {
	Watchdog watchdog("the commit chore logs nothing", 60s);
	DatabasePool pool(control_loop(), 8);
	DatabaseWALWriter writer("WC{:02}", 1);
	pool.set_wal_writer(&writer);
	const auto path = fresh("chore_wal");
	constexpr int flags = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT;
	engine::rev revision;
	{
		auto held = pin(pool, path, flags);
		Shard shard = use(held, flags);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		CHECK_TRUE(engine::sync_wait(shard.commit()));
		revision = shard.engine().revision();
		A::commit_blocking(held.entry());
		A::commit_blocking(held.entry());
		CHECK_EQ(shard.engine().revision(), revision);
	}
	pool.finish();
	CHECK_TRUE(pool.join());
	writer.end();
	writer.join(std::chrono::milliseconds(20000));
	std::size_t commits = 0, replaces = 0;
	DatabaseWAL wal(path);
	for (auto it = wal.begin(); it != wal.end(); ++it) {
		const std::string line = *it;
		const char* p = line.data();
		const char* p_end = p + line.size();
		(void)unserialise_length(&p, p_end);  // the revision
		const auto type = static_cast<DatabaseWAL::Type>(unserialise_length(&p, p_end));
		commits += type == DatabaseWAL::Type::COMMIT ? 1 : 0;
		replaces += type == DatabaseWAL::Type::REPLACE_DOCUMENT ? 1 : 0;
	}
	CHECK_EQ(replaces, 1u);
	CHECK_EQ(commits, 1u);
}

// The catalog is destroyed only once its shutdown ended (C24): a pin released while the pool's destructor waits is
// released into a live catalog (ASan sees no use after free), and the destructor ends after it.
void test_the_pool_waits_for_a_pin_released_during_its_end() {
	Watchdog watchdog("the pool waits for a late pin");
	auto pool = std::make_unique<DatabasePool>(control_loop(), 8);
	std::optional<IndexPin> held;
	held.emplace(pin(*pool, fresh("late_pin")));
	std::atomic<bool> released{false};
	std::thread late([&] {
		std::this_thread::sleep_for(200ms);
		released = true;
		held.reset();
	});
	pool.reset();
	CHECK_TRUE(released.load());
	late.join();
}


// A reactor loop with a host bound to it, torn down the way the manager stops its loops.
struct Loop {
	reactor::Reactor reactor;
	engine::AsioHost host{reactor.scheduler()};
	Loop() { asio::post(reactor.io(), [this] { host.bind_loop_thread(); }); }
	~Loop() { engine::Host::bind_thread(nullptr); }
	// The manager's stop: the loop stops, the chores end, the roots still suspended are destroyed.
	void tear_down() {
		reactor.io().stop();
		IndexChores::cancel_all(std::chrono::steady_clock::now() + 5s);
		reactor.scheduler().close();
		reactor.scheduler().destroy_roots();
	}
};

// A root destroyed while it waits for a pin leaves no waiter behind (the X-POOL step 2 review's CR1): the open's
// delivery finds the waiter abandoned and releases its pin, so the pool's shutdown ends. Each round destroys the roots
// at a later point of a cold open: a create pin dropped at once, one held across a wait, a chore pin and its commit.
void test_a_root_destroyed_while_it_waits_for_a_pin_releases_it() {
	Watchdog watchdog("a destroyed waiter releases its pin", 240s);
	const auto base = fresh("destroyed_waiter");
	std::filesystem::create_directories(base);
	DatabasePool pool(control_loop(), 4);
	for (int k = 0; k < 160; ++k) {
		Loop loop;
		auto work = asio::make_work_guard(loop.reactor.io());
		const std::string path = base + "/c" + std::to_string(k);
		task::event never;
		loop.host.spawn([](DatabasePool& pool, std::string path) -> task::task<void> {
			auto pinned = co_await pool.pin(Endpoint(path), WRITE);
		}(pool, path));
		loop.host.spawn([](DatabasePool& pool, std::string path, task::event& never) -> task::task<void> {
			auto pinned = co_await pool.pin(Endpoint(path), WRITE);
			co_await never;
		}(pool, path, never));
		loop.host.spawn([](DatabasePool& pool, std::string path) -> task::task<void> {
			auto pinned = co_await pool.pin_chore(Endpoint(path));
			if (pinned) {
				co_await pinned->commit(DB_OPEN | DB_WRITABLE, true);
			}
		}(pool, path));
		loop.reactor.io().run_for(std::chrono::microseconds(k * 40));
		loop.tear_down();
	}
	pool.finish();
	// A stranded pin never lets the shutdown end; the closes of 160 indexes take seconds, more on a loaded disk.
	const bool joined = pool.join(120s);
	CHECK_TRUE(joined);
	if (!joined) {
		for (const auto& line : pool.busy()) {
			std::fprintf(stderr, "    busy: %s\n", line.c_str());
		}
	}
}

// A destroyed root that held a pin lets the pool join (CR3, the property C24's order exists for): the manager destroys
// its control roots before it awaits the catalog's shutdown, and a destroyed root's pin is released with its frame.
void test_a_destroyed_root_that_held_a_pin_lets_the_pool_join() {
	Watchdog watchdog("a destroyed root's pin is released");
	DatabasePool pool(control_loop(), 4);
	const auto path = fresh("destroyed_holder");
	std::atomic<bool> holding{false};
	{
		Loop loop;
		auto work = asio::make_work_guard(loop.reactor.io());
		task::event never;
		loop.host.spawn([](DatabasePool& pool, std::string path, task::event& never, std::atomic<bool>& holding) -> task::task<void> {
			auto pinned = co_await pool.pin(Endpoint(path), WRITE);
			holding = true;
			co_await never;
		}(pool, path, never, holding));
		const auto end = std::chrono::steady_clock::now() + 5s;
		while (!holding && std::chrono::steady_clock::now() < end) {
			loop.reactor.io().run_for(1ms);
		}
		REQUIRE(holding.load());
		CHECK_EQ(pool.busy().size(), 1u);
		loop.tear_down();
	}
	pool.finish();
	CHECK_TRUE(pool.join(5s));
	CHECK_TRUE(pool.busy().empty());
}

// A request's token ends its wait for a pin (x-pool, strata's pin review): a cancelled pin, chore pin or Shard's pin
// throws task::cancelled, and FLUSH ends with it. Here the index's open is held inside its IO's making. A free pin is
// taken whatever the token, and a requested token still starts the open it asked for.
void test_a_cancelled_pin_stops_waiting() {
	Watchdog watchdog("a cancelled pin stops waiting");
	std::mutex fs_mutex;
	std::map<std::string, std::unique_ptr<strata::faultfs::filesystem>> fs;   // one per index: each locks its own
	DatabasePool pool(control_loop(), 8);
	std::promise<void> gate;
	std::shared_future<void> opens = gate.get_future().share();
	std::atomic<bool> opening{false};
	pool.set_index_io([&](const std::string& at) -> std::unique_ptr<kronuz::journal::IO> {
		opening = true;
		opens.wait();
		std::filesystem::create_directories(at);
		std::lock_guard<std::mutex> lk(fs_mutex);
		auto& one = fs[at];
		if (!one) {
			one = std::make_unique<strata::faultfs::filesystem>();
		}
		return std::make_unique<strata::faultfs::io>(*one);
	});
	const auto path = fresh("cancelled_pin");
	std::thread opener([&] { (void)pin(pool, path); });
	REQUIRE(until([&] { return opening.load(); }));
	const auto waits = [&](auto call) {
		task::cancel_source cancel;
		std::atomic<bool> ended{false}, cancelled{false};
		std::thread waiter([&] {
			try {
				call(cancel.token());
			} catch (const task::cancelled&) {
				cancelled = true;
			}
			ended = true;
		});
		std::this_thread::sleep_for(100ms);
		const bool waited = !ended.load();
		cancel.request();
		waiter.join();
		return waited && cancelled.load();
	};
	CHECK_TRUE(waits([&](task::cancel_token token) { engine::sync_wait(pool.pin(Endpoint(path), WRITE, token)); }));
	CHECK_TRUE(waits([&](task::cancel_token token) { engine::sync_wait(pool.pin_chore(Endpoint(path), token)); }));
	CHECK_TRUE(waits([&](task::cancel_token token) { engine::sync_wait(pool.flush(token)); }));
	CHECK_TRUE(waits([&](task::cancel_token token) {
		Shard unpinned(pool, path, WRITE, token);   // as lock_shard makes one; its use pins with the token
		engine::sync_wait(unpinned.ready());
	}));
	gate.set_value();
	opener.join();
	task::cancel_source requested;
	requested.request();
	CHECK_TRUE(static_cast<bool>(engine::sync_wait(pool.pin(Endpoint(path), WRITE, requested.token()))));
	const auto cold = fresh("cancelled_cold");
	bool cancelled = false;
	try {
		engine::sync_wait(pool.pin(Endpoint(cold), WRITE, requested.token()));
	} catch (const task::cancelled&) {
		cancelled = true;
	}
	CHECK_TRUE(cancelled);
	CHECK_TRUE(until([&] { return state_of(pool, cold) == strata::slot_state::open; }));
	CHECK_TRUE(static_cast<bool>(pin(pool, cold, DB_OPEN)));
}

// A cancelled FLUSH ends while an index's turn is held (review C2): its commit's wait for the turn takes the
// request's token, so the FLUSH throws task::cancelled at once rather than after the turn's timeout.
void test_a_cancelled_flush_stops_waiting_for_a_held_turn() {
	Watchdog watchdog("a cancelled flush stops waiting for a held turn");
	DatabasePool pool(control_loop(), 8);
	auto entry = pin(pool, fresh("flush_cancel"));
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));   // uncommitted: the FLUSH must commit it
	HoldTurn hold(entry.entry(), WRITE);
	task::cancel_source cancel;
	std::atomic<bool> ended{false}, cancelled{false};
	std::thread flusher([&] {
		try {
			engine::sync_wait(pool.flush(cancel.token()));
		} catch (const task::cancelled&) {
			cancelled = true;
		}
		ended = true;
	});
	std::this_thread::sleep_for(150ms);
	CHECK_FALSE(ended.load());   // it waits for the held turn
	cancel.request();
	CHECK_TRUE(until([&] { return ended.load(); }, 5s));   // well before the turn's 30 s timeout
	hold.release();
	flusher.join();
	CHECK_TRUE(cancelled.load());
	CHECK_TRUE(entry->is_modified());   // the cancelled FLUSH committed nothing
}

// The autocommit settles what its commit leaves (D8): while a merge is pending it asks to come back, and once the
// merges finish it publishes them and syncs their output with no revision of its own.
void test_the_autocommit_comes_back_while_a_merge_is_pending() {
	Watchdog watchdog("the autocommit comes back", 60s);
	DatabasePool pool(control_loop(), 8);
	DatabaseWALWriter writer("WR{:02}", 1);   // the autocommit logs its commit
	pool.set_wal_writer(&writer);
	auto& gate = A::wrap_executor<GatedExecutor>(pool);
	gate.hold();
	Endpoint endpoint(fresh("rearm"));
	auto entry = pin(pool, endpoint.path);
	Shard shard = use(entry, WRITE);
	auto& index = A::index(entry.entry());
	make_a_merge(shard, index);
	REQUIRE(index.merges_pending() > 0);
	CHECK_TRUE(engine::sync_wait(autocommit(pool, endpoint)));   // the merge is held: come back for it
	CHECK_FALSE(entry->is_modified());
	const auto committed = revision_of(shard);
	gate.let_go();
	bool pending = true;
	for (int i = 0; i < 1000 && pending; ++i) {
		pending = engine::sync_wait(autocommit(pool, endpoint));
		if (pending) {
			std::this_thread::sleep_for(5ms);
		}
	}
	CHECK_FALSE(pending);
	{
		auto turn = engine::sync_wait(entry->take_turn(WRITE));
		CHECK_EQ(index.merges_pending(), 0u);
		CHECK_FALSE(index.merge_output_unsynced());
	}
	CHECK_EQ(revision_of(shard), committed);   // the merges' sync made no revision
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "d");
	writer.end();
	writer.join(std::chrono::milliseconds(20000));
}

// An autocommit whose turn does not come asks to come back (D8), so a busy index's write is not left uncommitted.
void test_a_busy_autocommit_comes_back() {
	Watchdog watchdog("a busy autocommit comes back");
	DatabasePool pool(control_loop(), 8);
	DatabaseWALWriter writer("WB{:02}", 1);   // the autocommit logs its commit
	pool.set_wal_writer(&writer);
	pool.set_turn_timeout(0.1);
	Endpoint endpoint(fresh("busy_autocommit"));
	auto entry = pin(pool, endpoint.path);
	Shard shard = use(entry, WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	{
		HoldTurn hold(entry.entry(), WRITE);
		CHECK_TRUE(engine::sync_wait(autocommit(pool, endpoint)));
		CHECK_TRUE(entry->is_modified());
	}
	const auto revision = revision_of(shard);
	CHECK_FALSE(engine::sync_wait(autocommit(pool, endpoint)));
	CHECK_FALSE(entry->is_modified());
	CHECK_EQ(revision_of(shard), revision + 1);
	writer.end();
	writer.join(std::chrono::milliseconds(20000));
}

// The sweep is the only idle close (XP3): at 1 kHz it closes an unpinned idle index, and never one a use pins; with
// strata's default times nothing closes an index idle for a second.
void test_the_sweep_closes_only_an_unpinned_index() {
	Watchdog watchdog("the sweep closes only an unpinned index");
	const auto held_path = fresh("sweep_held"), idle_path = fresh("sweep_idle");
	{
		DatabasePool pool(control_loop(), 8);
		{
			Shard shard(pin(pool, idle_path), WRITE);
			engine::sync_wait(shard.add_document(make("Kb", "b")));
			engine::sync_wait(shard.commit());
		}
		std::this_thread::sleep_for(1500ms);   // a pass of the default sweep, or more
		CHECK_TRUE(state_of(pool, idle_path) == strata::slot_state::open);
	}
	DatabasePool pool(control_loop(), 8, 0, engine::default_readers_per_index, fast_sweep());
	{
		Shard shard(pin(pool, held_path), WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
	}
	std::optional<Shard> held;
	held.emplace(pin(pool, held_path, DB_OPEN), DB_OPEN);   // a use that holds its pin
	{
		Shard shard(pin(pool, idle_path), WRITE);
		CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "b");
	}
	CHECK_TRUE(until([&] { return state_of(pool, idle_path) == strata::slot_state::free; }));
	const auto stats = pool.catalog().stats();
	CHECK_GE(stats.pass_closes, 1u);
	std::this_thread::sleep_for(300ms);
	CHECK_GE(pool.catalog().stats().passes, stats.passes + 10);   // the sweep went on passing over it
	CHECK_TRUE(state_of(pool, held_path) == strata::slot_state::open);
	held.reset();   // the use ends: its pin goes
	CHECK_TRUE(until([&] { return state_of(pool, held_path) == strata::slot_state::free; }));
}

// The shutdown commits a write left uncommitted with the entry's commit chore (XP4): its WAL COMMIT line, a revision
// more, and the write read back by the next pool.
void test_the_shutdown_commits_and_logs_an_uncommitted_write() {
	Watchdog watchdog("the shutdown commits and logs", 60s);
	const auto path = fresh("shutdown_wal");
	constexpr int flags = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT;
	engine::rev before;
	{
		DatabasePool pool(control_loop(), 8);
		DatabaseWALWriter writer("WS{:02}", 1);
		pool.set_wal_writer(&writer);
		{
			Shard shard(pin(pool, path, flags), flags);
			engine::sync_wait(shard.add_document(make("Ka", "a")));
			before = shard.engine().revision();
			CHECK_TRUE(shard.endpoint().is_modified());
		}
		pool.finish();
		CHECK_TRUE(pool.join());
		CHECK_EQ(pool.catalog().stats().shutdown_commits, 1u);
		writer.end();
		writer.join(std::chrono::milliseconds(20000));
	}
	std::size_t commits = 0, replaces = 0;
	{
		DatabaseWAL wal(path);
		for (auto it = wal.begin(); it != wal.end(); ++it) {
			const std::string line = *it;
			const char* p = line.data();
			const char* p_end = p + line.size();
			(void)unserialise_length(&p, p_end);  // the revision
			const auto type = static_cast<DatabaseWAL::Type>(unserialise_length(&p, p_end));
			commits += type == DatabaseWAL::Type::COMMIT ? 1 : 0;
			replaces += type == DatabaseWAL::Type::REPLACE_DOCUMENT ? 1 : 0;
		}
	}
	CHECK_EQ(replaces, 1u);
	CHECK_EQ(commits, 1u);
	DatabasePool pool(control_loop(), 8);
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(shard.engine().revision(), before + 1);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
}

// The sweep's close syncs a finished merge's output through the entry's commit chore (XP4, S14): the index demotes
// and closes with no revision of its own, and opens again with every document.
void test_the_sweeps_close_syncs_a_merges_output() {
	Watchdog watchdog("the sweep's close syncs a merge", 60s);
	DatabasePool pool(control_loop(), 8, 0, engine::default_readers_per_index, fast_sweep());
	const auto path = fresh("sweep_merge");
	engine::rev revision;
	std::size_t written = 0;
	{
		auto entry = pin(pool, path);
		Shard shard = use(entry, WRITE);
		auto& index = A::index(entry.entry());
		make_a_merge(shard, index);
		REQUIRE(index.merges_pending() > 0);
		written = static_cast<std::size_t>(engine::sync_wait(shard.engine_for_read())->doc_count());
		engine::sync_wait(shard.commit());
		revision = shard.engine().revision();
		bool unsynced = false;
		const auto end = std::chrono::steady_clock::now() + 10s;
		while (!unsynced && std::chrono::steady_clock::now() < end) {
			auto turn = engine::sync_wait(entry->take_turn(WRITE));
			index.check_in();
			unsynced = index.merges_pending() == 0 && index.merge_output_unsynced();
		}
		REQUIRE(unsynced);
	}
	CHECK_TRUE(until([&] { return state_of(pool, path) == strata::slot_state::free; }, 10s));
	CHECK_GE(pool.catalog().stats().merge_commits, 1u);
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(shard.engine().revision(), revision);
	CHECK_EQ(static_cast<std::size_t>(engine::sync_wait(shard.engine_for_read())->doc_count()), written);
}

// A write to a demoted defer_io index applies after one promote() (XP5, D9): a resident pin is granted on `owed`
// without a wake, so the write's apply answers map_owed; the turn's chore maps the bodies under the lease and the
// same transaction is applied again. No map is made outside the lease (D12).
void test_a_write_to_a_demoted_index_applies_after_one_promote() {
	Watchdog watchdog("a write to a demoted index");
	DatabasePool pool(control_loop(), 8);
	const auto path = fresh("map_owed");
	std::uint32_t slot;
	{
		auto held = pin(pool, path);
		slot = slot_of(held);
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
	}
	CHECK_TRUE(pool.catalog().begin_demote(slot));
	pool.catalog().wait_chores();
	CHECK_TRUE(state_of(pool, path) == strata::slot_state::owed);
	const auto wakes = pool.catalog().stats().wakes_posted;
	{
		auto resident = engine::sync_wait(pool.pin_chore(Endpoint(path)));
		CHECK_TRUE(static_cast<bool>(resident));
		auto& index = A::index(resident.entry());
		CHECK_TRUE(index.promote_owed());
		Shard shard(std::move(resident), WRITE);
		engine::sync_wait(shard.add_document(make("Kb", "b")));
		CHECK_FALSE(index.promote_owed());   // promoted once, by the write's chore
		CHECK_EQ(index.unleased_maps(), 0u);
		engine::sync_wait(shard.commit());
		CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	}
	CHECK_EQ(pool.catalog().stats().wakes_posted, wakes);   // the write's own promote, not a wake
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(static_cast<std::size_t>(engine::sync_wait(shard.engine_for_read())->doc_count()), 2u);
}

// The owed answers' loop gives a transaction two owed answers and raises errc::internal at a third (XP5, D9), against
// a stub that answers owed forever; one that is owed twice is applied after two settles.
void test_a_third_owed_answer_is_internal() {
	int applies = 0, settles = 0;
	const auto settle = [&](strata::verdict::outcome) -> task::task<void> {
		++settles;
		co_return;
	};
	const auto forever = [&] {
		++applies;
		strata::verdict verdict;
		verdict.result = strata::verdict::map_owed;
		return verdict;
	};
	CHECK_TRUE(error_kind([&] { (void)engine::sync_wait(engine::strata::apply_settling(forever, settle)); }) == engine::errc::internal);
	CHECK_EQ(applies, 3);
	CHECK_EQ(settles, 2);
	applies = settles = 0;
	const auto twice = [&] {
		strata::verdict verdict;
		verdict.result = ++applies == 1 ? strata::verdict::map_owed : applies == 2 ? strata::verdict::drain_owed : strata::verdict::accepted;
		return verdict;
	};
	CHECK_TRUE(engine::sync_wait(engine::strata::apply_settling(twice, settle)).result == strata::verdict::accepted);
	CHECK_EQ(settles, 2);
}

// A request for a missing index leaves a negative entry under its name, and the sweep erases it once it is older than
// its TTL (D13's fourth change): a scan of missing names holds one slot per name for the TTL and the passes after it.
void test_the_sweep_erases_a_missing_index_negative_entries() {
	Watchdog watchdog("negative entries");
	IndexSweep sweep;
	sweep.period = 1ms;
	sweep.negative_ttl = 3s;
	DatabasePool pool(control_loop(), 8, 0, engine::default_readers_per_index, sweep);
	constexpr int names = 100;
	const auto failed = [&] {
		int n = 0;
		for (const auto& slot : pool.catalog().snapshot()) {
			n += slot.state == strata::slot_state::failed;
		}
		return n;
	};
	for (int i = 0; i < names; ++i) {
		const auto path = fresh(("missing_" + std::to_string(i)).c_str());
		CHECK_TRUE(error_kind([&] { (void)pin(pool, path, DB_OPEN); }) == engine::errc::not_found);
	}
	const int before = failed();
	CHECK_GE(before, 1);
	std::this_thread::sleep_for(1500ms);   // half the TTL: an erase that ignores it has already run
	CHECK_EQ(failed(), before);
	CHECK_TRUE(until([&] { return failed() == 0; }, 7s));   // under strata's 10 s default, which a TTL not applied would give
	CHECK_GE(pool.catalog().stats().erased, static_cast<std::uint64_t>(names));
}

// The hostless path (engine::open_local, the adapter's owned Core) is not the catalog's (D2 as built, CR6): an index is
// held by one of them at a time, since the index's directory lock refuses the second holder either way.
void test_the_hostless_path_and_the_catalog_never_share_an_index() {
	Watchdog watchdog("hostless and catalog");
	const auto path = fresh("hostless");
	const auto refused = [](auto&& f) {
		try {
			f();
		} catch (...) {
			return true;
		}
		return false;
	};
	{
		DatabasePool pool(control_loop(), 8);
		auto held = pin(pool, path);
		Shard shard = use(held, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
		engine::sync_wait(shard.commit());
		CHECK_TRUE(refused([&] { (void)engine::sync_wait(engine::open_local(path, true, false)); }));
	}
	{
		auto opened = engine::sync_wait(engine::open_local(path, true, false));
		CHECK_EQ(static_cast<std::size_t>(opened.shard->doc_count()), 1u);
		DatabasePool pool(control_loop(), 8);
		CHECK_TRUE(refused([&] { (void)pin(pool, path, DB_OPEN); }));
	}
	DatabasePool pool(control_loop(), 8);
	Shard shard(pin(pool, path, DB_OPEN), DB_OPEN);
	CHECK_EQ(static_cast<std::size_t>(engine::sync_wait(shard.engine_for_read())->doc_count()), 1u);
}

// The pool keeps open at most what the descriptors' soft cap holds, whatever size it is given (D11).
void test_soft_open_is_at_most_the_descriptor_cap() {
	DatabasePool small(control_loop(), 8);
	CHECK_EQ(small.soft_open(), 8u);
	DatabasePool huge(control_loop(), std::size_t(1) << 40);
	CHECK_GE(huge.soft_open(), 64u);   // strata's floor: room for 64 indexes
	CHECK_TRUE(huge.soft_open() < (std::size_t(1) << 40));
}

}  // namespace

int main(int argc, char** argv) {
	struct Case { const char* name; void (*fn)(); };
	const Case cases[] = {
		{"test_smoke", test_smoke},
		{"test_opening_never_waits_on_a_thread", test_opening_never_waits_on_a_thread},
		{"test_concurrent_opens_are_one_flight", test_concurrent_opens_are_one_flight},
		{"test_every_operation_takes_the_turn", test_every_operation_takes_the_turn},
		{"test_a_read_with_nothing_to_refresh_takes_no_turn", test_a_read_with_nothing_to_refresh_takes_no_turn},
		{"test_the_autocommit_task_takes_the_turn", test_the_autocommit_task_takes_the_turn},
		{"test_a_turn_wait_times_out_with_busy", test_a_turn_wait_times_out_with_busy},
		{"test_a_task_holds_one_turn_at_most", test_a_task_holds_one_turn_at_most},
		{"test_a_commit_joins_the_one_in_flight", test_a_commit_joins_the_one_in_flight},
		{"test_a_commit_not_covered_does_not_share_the_outcome", test_a_commit_not_covered_does_not_share_the_outcome},
		{"test_a_failed_commit_reaches_only_the_joined_waiters", test_a_failed_commit_reaches_only_the_joined_waiters},
		{"test_owed_flushes_do_not_reorder_writers", test_owed_flushes_do_not_reorder_writers},
		{"test_blob_io_runs_in_chores", test_blob_io_runs_in_chores},
		{"test_the_wal_order_is_the_apply_order", test_the_wal_order_is_the_apply_order},
		{"test_a_held_search_delays_no_refresh_or_commit", test_a_held_search_delays_no_refresh_or_commit},
		{"test_readers_cap", test_readers_cap},
		{"test_readers_cap_is_per_index", test_readers_cap_is_per_index},
		{"test_a_search_over_more_indexes_than_records_is_served", test_a_search_over_more_indexes_than_records_is_served},
		{"test_search_fanout_limit", test_search_fanout_limit},
		{"test_a_commit_that_times_out_leaves_the_write_standing", test_a_commit_that_times_out_leaves_the_write_standing},
		{"test_a_finishing_pool_serves_an_open_index", test_a_finishing_pool_serves_an_open_index},
		{"test_the_integrity_check_takes_the_turn", test_the_integrity_check_takes_the_turn},
		{"test_a_chore_needs_a_scheduler", test_a_chore_needs_a_scheduler},
		{"test_a_chore_resumes_its_awaiter_with_post", test_a_chore_resumes_its_awaiter_with_post},
		{"test_a_registry_that_ends_with_work_outstanding_aborts", test_a_registry_that_ends_with_work_outstanding_aborts},
		{"test_a_chore_is_over_when_its_task_resumes", test_a_chore_is_over_when_its_task_resumes},
		{"test_a_create_pin_passes_a_missing_index", test_a_create_pin_passes_a_missing_index},
		{"test_a_pin_keeps_its_index_open", test_a_pin_keeps_its_index_open},
		{"test_a_chore_pin_is_resident", test_a_chore_pin_is_resident},
		{"test_flush_commits_every_index", test_flush_commits_every_index},
		{"test_the_shutdown_commits_through_the_entry", test_the_shutdown_commits_through_the_entry},
		{"test_a_resident_commit_on_an_owed_index_lets_it_close", test_a_resident_commit_on_an_owed_index_lets_it_close},
		{"test_join_waits_for_every_pin", test_join_waits_for_every_pin},
		{"test_a_failed_use_hands_its_pin_back", test_a_failed_use_hands_its_pin_back},
		{"test_a_repin_after_a_reopen_is_not_a_use", test_a_repin_after_a_reopen_is_not_a_use},
		{"test_a_cold_pin_reads_what_the_log_replays", test_a_cold_pin_reads_what_the_log_replays},
		{"test_a_fenced_index_is_replaced_and_replays_its_log", test_a_fenced_index_is_replaced_and_replays_its_log},
		{"test_the_turns_end_publishes_a_finished_merge", test_the_turns_end_publishes_a_finished_merge},
		{"test_the_commit_chore_logs_nothing_when_nothing_was_applied", test_the_commit_chore_logs_nothing_when_nothing_was_applied},
		{"test_the_pool_waits_for_a_pin_released_during_its_end", test_the_pool_waits_for_a_pin_released_during_its_end},
		{"test_a_root_destroyed_while_it_waits_for_a_pin_releases_it", test_a_root_destroyed_while_it_waits_for_a_pin_releases_it},
		{"test_a_destroyed_root_that_held_a_pin_lets_the_pool_join", test_a_destroyed_root_that_held_a_pin_lets_the_pool_join},
		{"test_a_cancelled_pin_stops_waiting", test_a_cancelled_pin_stops_waiting},
		{"test_a_cancelled_flush_stops_waiting_for_a_held_turn", test_a_cancelled_flush_stops_waiting_for_a_held_turn},
		{"test_a_detached_task_may_block_once_the_control_is_gone", test_a_detached_task_may_block_once_the_control_is_gone},
		{"test_the_autocommit_comes_back_while_a_merge_is_pending", test_the_autocommit_comes_back_while_a_merge_is_pending},
		{"test_a_busy_autocommit_comes_back", test_a_busy_autocommit_comes_back},
		{"test_the_sweep_closes_only_an_unpinned_index", test_the_sweep_closes_only_an_unpinned_index},
		{"test_the_shutdown_commits_and_logs_an_uncommitted_write", test_the_shutdown_commits_and_logs_an_uncommitted_write},
		{"test_the_sweeps_close_syncs_a_merges_output", test_the_sweeps_close_syncs_a_merges_output},
		{"test_a_write_to_a_demoted_index_applies_after_one_promote", test_a_write_to_a_demoted_index_applies_after_one_promote},
		{"test_a_third_owed_answer_is_internal", test_a_third_owed_answer_is_internal},
		{"test_soft_open_is_at_most_the_descriptor_cap", test_soft_open_is_at_most_the_descriptor_cap},
		{"test_the_hostless_path_and_the_catalog_never_share_an_index", test_the_hostless_path_and_the_catalog_never_share_an_index},
		{"test_the_sweep_erases_a_missing_index_negative_entries", test_the_sweep_erases_a_missing_index_negative_entries},
	};
	for (const auto& c : cases) {
		if (argc > 1 && std::string(c.name).find(argv[1]) == std::string::npos) {
			continue;  // a name filter, for running one test
		}
		const int before = testh::failures;
		try {
			c.fn();
		} catch (const engine::Error& error) {
			std::fprintf(stderr, "  FAIL %s: threw engine::Error %s: %s\n", c.name, engine::to_string(error.kind), error.description().c_str());
			++testh::failures;
		} catch (const std::exception& exc) {
			std::fprintf(stderr, "  FAIL %s: threw %s\n", c.name, exc.what());
			++testh::failures;
		}
		std::fprintf(stderr, "%s %s\n", testh::failures == before ? "ok  " : "FAIL", c.name);
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
