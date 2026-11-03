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
#include "database/shard.h"
#include "database/wal.h"
#include "engine/callbacks.h"
#include "engine/chores.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "compressor_lz4.h"
#include "length.h"
#include "node.h"
#include "task.hh"
#include "utype.hh"
#include "work_pool.hh"

using namespace std::chrono_literals;

struct PoolTestAccess {
	static std::uint64_t opens(ShardEndpoint& entry) { return entry.opens_.load(); }
	static ShardEndpoint::State state(ShardEndpoint& entry) { return entry.state(); }
	static void set_chores(ShardEndpoint& entry, std::unique_ptr<engine::Chores> chores) { entry.chores = std::move(chores); }
	static std::unique_ptr<engine::Chores> take_chores(ShardEndpoint& entry) { return std::move(entry.chores); }
	static void mark_stale(ShardEndpoint& entry) { entry.mark_stale(); }
	// Whether nobody holds the index's turn right now.
	static bool turn_is_free(ShardEndpoint& entry) {
		if (!entry.turn.try_lock()) { return false; }
		entry.turn.unlock();
		return true;
	}
	static std::uint64_t applied(ShardEndpoint& entry) { return entry.applied_.load(); }
	static WorkGroup& chore_group(ShardEndpoint& entry) { return static_cast<IndexChores&>(*entry.chores).group(); }
	// As if a task posted to the control loop had not ended.
	static void count_detached(DatabasePool& pool, int by) {
		if (by > 0) { pool.detached_.fetch_add(static_cast<std::size_t>(by)); } else { pool.detached_.fetch_sub(static_cast<std::size_t>(-by)); }
	}
	// As if a write landed after a commit began: one more write applied, and not yet committed.
	static void write_lands(ShardEndpoint& entry) {
		entry.applied_.fetch_add(1);
		entry.modified.store(true);
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
			} catch (const engine::Error& error) {
				kind_ = error.kind;
				threw_ = true;
			}
			done_ = true;
		});
	}
	~Running() { join(); }
	bool done() const { return done_; }
	bool threw() const { return threw_; }
	engine::errc kind() const { return kind_; }
	void join() {
		if (thread_.joinable()) {
			thread_.join();
		}
	}

private:
	std::atomic<bool> done_{false};
	bool threw_ = false;
	engine::errc kind_ = engine::errc::internal;
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
	DatabasePool pool(8);
	Endpoint endpoint(fresh("smoke"));
	auto entry = pool.spawn(endpoint);
	Shard shard(*entry.get(), WRITE);
	auto written = engine::sync_wait(shard.add_document(make("Ka", "a")));
	CHECK_EQ(written.did, 1u);
	auto view = engine::sync_wait(shard.get_view(1));  // the read refreshes first: the write is seen
	CHECK_EQ(view->data(), "a");
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_EQ(entry->opens(), 1u);
}

// Opening never waits on a thread: two uses of one index on one thread open it once and finish, though
// the second must wait for the first's open. A thread that blocked on the open could not run the chore's
// completion that the first is waiting for.
void test_opening_never_waits_on_a_thread() {
	Watchdog watchdog("opening never waits on a thread");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("nowait")));
	Shard a(*entry.get(), WRITE);
	Shard b(*entry.get(), WRITE);
	std::vector<task::task<void>> both;
	both.push_back(a.ready());
	both.push_back(b.ready());
	engine::sync_wait(task::when_all(std::move(both)));
	CHECK_EQ(entry->opens(), 1u);
	CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::open);
}

// Many users of a new index at once: one open (the single flight), and every user gets the engine.
void test_concurrent_opens_are_one_flight() {
	Watchdog watchdog("concurrent opens are one flight");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("flight")));
	std::vector<std::thread> threads;
	std::atomic<int> opened{0};
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([&] {
			Shard shard(*entry.get(), WRITE);
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
	CHECK_EQ(entry->opens(), 1u);
}

// An open that fails fails every user that awaited it, and the next user tries again.
void test_a_failed_open_reaches_every_waiter_and_is_retried() {
	Watchdog watchdog("a failed open");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("missing")));
	std::atomic<int> not_found{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&] {
			Shard shard(*entry.get(), DB_OPEN);  // the index does not exist and is not to be created
			try {
				engine::sync_wait(shard.ready());
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
	CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::empty);
	Shard create(*entry.get(), WRITE);
	engine::sync_wait(create.ready());
	CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::open);
}

// Every operation on an index takes its turn: it does not end while another task holds the turn, and it
// ends once the turn is released.
template <typename Op>
void waits_for_the_turn(const char* name, Op op) {
	Watchdog watchdog(name);
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh(name)));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Kseed", "seed")));  // something to replace, delete and commit
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("readsnoturn")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.engine_for_read());  // refreshes: nothing is owed now
	HoldTurn hold(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	Endpoint endpoint(fresh("autocommit"));
	auto entry = pool.spawn(endpoint);
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	const auto revision = revision_of(shard);
	CHECK_TRUE(entry->is_modified());
	{
		HoldTurn hold(*entry.get(), WRITE);
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

// The idle close takes the turn too: it waits for an operation that holds it, then commits what is pending, and
// the data is there when the index opens again.
void test_the_idle_close_takes_the_turn() {
	Watchdog watchdog("the idle close");
	DatabasePool pool(8);
	Endpoint endpoint(fresh("idleclose"));
	ShardEndpoint* raw;
	{
		auto entry = pool.spawn(endpoint);
		raw = entry.get();
		Shard shard(*raw, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
	}  // nobody uses the index now
	{
		HoldTurn hold(*raw, WRITE);
		Running running([&] { engine::sync_wait(pool.flush()); });
		std::this_thread::sleep_for(150ms);
		CHECK_FALSE(running.done());
		hold.release();
		running.join();
		CHECK_TRUE(running.done());
	}
	CHECK_TRUE(A::state(*raw) == ShardEndpoint::State::empty);
	auto entry = pool.spawn(endpoint);
	Shard shard(*entry.get(), WRITE);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
	CHECK_EQ(entry->opens(), 2u);
}

// A turn that does not come in time answers `busy` (a 503), and the queue it left is whole.
void test_a_turn_wait_times_out_with_busy() {
	Watchdog watchdog("a turn wait times out");
	DatabasePool pool(8);
	pool.set_turn_timeout(0.3);
	auto entry = pool.spawn(Endpoint(fresh("busy")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.ready());
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	auto first = pool.spawn(Endpoint(fresh("one_a")));
	auto second = pool.spawn(Endpoint(fresh("one_b")));
	Shard a(*first.get(), WRITE);
	Shard b(*second.get(), WRITE);
	engine::sync_wait(a.ready());
	engine::sync_wait(b.ready());
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		engine::sync_wait(two_turns(*first.get(), *second.get()));
		::_exit(0);  // not reached when the assertion is on
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	CHECK_TRUE(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#endif
}

// A request that finds the index closing awaits the close and then opens it again; it never opens a second
// engine beside the closing one. What the close committed is there.
void test_a_request_racing_an_idle_close() {
	Watchdog watchdog("a request racing an idle close");
	DatabasePool pool(8);
	Endpoint endpoint(fresh("race"));
	ShardEndpoint* raw;
	{
		auto entry = pool.spawn(endpoint);
		raw = entry.get();
		Shard shard(*raw, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));  // pending: the close commits it
	}
	HoldTurn hold(*raw, WRITE);
	Running closing([&] { engine::sync_wait(pool.flush()); });
	for (int i = 0; i < 2500 && A::state(*raw) != ShardEndpoint::State::closing; ++i) {
		std::this_thread::sleep_for(2ms);
	}
	CHECK_TRUE(A::state(*raw) == ShardEndpoint::State::closing);
	auto referenced = pool.spawn(endpoint);
	Shard late(*referenced.get(), WRITE);
	Running opening([&] { engine::sync_wait(late.ready()); });  // a lookup finds the entry closing: it awaits the close
	Running request([&] { engine::sync_wait(late.add_document(make("Kb", "b"))); });
	std::this_thread::sleep_for(150ms);
	CHECK_FALSE(opening.done());
	CHECK_FALSE(request.done());
	CHECK_EQ(raw->opens(), 1u);  // it waits for the close: no second engine
	hold.release();
	closing.join();
	opening.join();
	request.join();
	CHECK_FALSE(opening.threw());
	CHECK_FALSE(request.threw());
	CHECK_EQ(raw->opens(), 2u);
	CHECK_EQ(engine::sync_wait(late.get_view(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(late.get_view(2))->data(), "b");
}

// A writer queued on the turn when a failure marks the engine stale gets the turn of an engine that cannot go
// on: it gives the turn back, the entry replaces the engine, and the write lands on the new one.
void test_a_stale_engine_is_replaced_before_a_queued_write() {
	Watchdog watchdog("a stale engine");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("stale")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	HoldTurn hold(*entry.get(), WRITE);
	Running queued([&] { engine::sync_wait(shard.add_document(make("Kb", "b"))); });
	std::this_thread::sleep_for(100ms);
	A::mark_stale(*entry.get());  // what the holder's failing operation does
	hold.release();
	queued.join();
	CHECK_FALSE(queued.threw());
	CHECK_EQ(entry->opens(), 2u);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
}

// Commits coalesce by position (C7). A commit in flight is joined by a commit whose writes it covers, which
// then waits for no commit of its own; a write that arrives while it runs waits for the turn and for the next
// commit.
void test_a_commit_joins_the_one_in_flight() {
	Watchdog watchdog("a commit joins the one in flight");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("coalesce")));
	auto& gate = gate_chores(*entry.get());
	Shard s1(*entry.get(), WRITE), s2(*entry.get(), WRITE), s3(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("covered")));
	auto& gate = gate_chores(*entry.get());
	Shard s1(*entry.get(), WRITE), s2(*entry.get(), WRITE);
	engine::sync_wait(s1.add_document(make("Ka", "a")));
	gate.arm(true);  // the commit in flight will fail
	Running first([&] { engine::sync_wait(s1.commit()); });
	CHECK_TRUE(eventually(gate.entered_flag()));
	A::write_lands(*entry.get());
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("failed")));
	auto& gate = gate_chores(*entry.get());
	Shard s1(*entry.get(), WRITE), s2(*entry.get(), WRITE), s3(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	pool.set_buffered_docs_cap(5);
	auto entry = pool.spawn(Endpoint(fresh("owed")));
	constexpr int threads_n = 4;
	constexpr int per_thread = 60;
	std::vector<std::vector<engine::docid>> got(threads_n);
	std::vector<std::thread> threads;
	for (int t = 0; t < threads_n; ++t) {
		threads.emplace_back([&, t] {
			Shard shard(*entry.get(), WRITE);
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
	Shard reader(*entry.get(), WRITE);
	CHECK_EQ(engine::sync_wait(reader.engine_for_read())->doc_count(), static_cast<engine::doccount>(threads_n * per_thread));
}

// The idle housekeeping posts a chore only when background work is owed: a quiet index costs nothing every minute.
void test_housekeeping_costs_nothing_on_a_quiet_index() {
	Watchdog watchdog("housekeeping");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("housekeeping")));
	auto& counting = count_chores(*entry.get());
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	const auto before = counting.count(engine::ChoreClass::merge);
	pool.housekeeping();  // nothing owed: no chore
	CHECK_EQ(counting.count(engine::ChoreClass::merge), before);
	CHECK_EQ(engine::sync_wait(shard.get_view(1))->data(), "a");
}

// Blob I/O is device work, so it runs as chores: the blob's write is a flush chore (inside the document's turn, before
// the apply), its commit another, and its read a query chore.
void test_blob_io_runs_in_chores() {
	Watchdog watchdog("blob I/O");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("blobs")));
	auto& counting = count_chores(*entry.get());
	Shard shard(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	DatabaseWALWriter writer("WL{:02}", 1);
	pool.set_wal_writer(&writer);
	Endpoint endpoint(fresh("walorder"));
	constexpr int threads_n = 4;
	constexpr int per_thread = 50;
	{
		auto entry = pool.spawn(endpoint);
		std::vector<std::thread> threads;
		for (int t = 0; t < threads_n; ++t) {
			threads.emplace_back([&, t] {
				Shard shard(*entry.get(), DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT);
				for (int i = 0; i < per_thread; ++i) {
					engine::sync_wait(shard.add_document(make("K" + std::to_string(t) + "-" + std::to_string(i), "d")));
				}
			});
		}
		for (auto& thread : threads) {
			thread.join();
		}
	}
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("held")));
	Shard shard(*entry.get(), WRITE);
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
		pool.housekeeping();
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
	DatabasePool pool(8);
	pool.set_readers_per_index(16);
	auto entry = pool.spawn(Endpoint(fresh("readers")));
	Shard shard(*entry.get(), WRITE);
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

// FLUSH closes every index nobody uses and returns when they are closed (X3).
void test_flush_closes_every_idle_index_before_it_returns() {
	Watchdog watchdog("flush");
	DatabasePool pool(8);
	for (int i = 0; i < 3; ++i) {
		auto entry = pool.spawn(Endpoint(fresh(("flush" + std::to_string(i)).c_str())));
		Shard shard(*entry.get(), WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
	}   // nobody uses the three indexes now
	engine::sync_wait(pool.flush());
	std::size_t seen = 0;
	for (auto& entry : pool.endpoints()) {
		CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::empty);
		++seen;
	}
	CHECK_EQ(seen, 3u);
}

// A pin keeps its index open (docs/design/x-pool.md D3): FLUSH closes only the index nobody pins, and the pinned
// one closes once its pin is released.
void test_a_pin_keeps_its_index_open() {
	Watchdog watchdog("a pin keeps its index open");
	DatabasePool pool(8);
	Endpoint pinned_at(fresh("pinned")), loose_at(fresh("loose"));
	auto pin = engine::sync_wait(pool.pin(pinned_at));
	CHECK_TRUE(static_cast<bool>(pin));
	ShardEndpoint* pinned = &pin.entry();
	ShardEndpoint* loose;
	{
		Shard shard(*pinned, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
	}
	{
		auto other = engine::sync_wait(pool.pin(loose_at));
		loose = &other.entry();
		Shard shard(*loose, WRITE);
		engine::sync_wait(shard.add_document(make("Kb", "b")));
	}
	engine::sync_wait(pool.flush());
	CHECK_TRUE(A::state(*pinned) == ShardEndpoint::State::open);
	CHECK_TRUE(A::state(*loose) == ShardEndpoint::State::empty);
	pin.release();
	CHECK_FALSE(static_cast<bool>(pin));
	engine::sync_wait(pool.flush());
	CHECK_TRUE(A::state(*pinned) == ShardEndpoint::State::empty);
	// Two pins of one path name one entry.
	auto a = engine::sync_wait(pool.pin(pinned_at));
	auto b = engine::sync_wait(pool.pin(pinned_at));
	CHECK_TRUE(&a.entry() == &b.entry());
	CHECK_TRUE(&a.entry() == pinned);
}

// A chore's pin is resident: it opens nothing. A path the registry does not have, or an index that is not open,
// gives an empty pin and opens nothing; an open index gives its entry, kept from the idle close while pinned.
void test_a_chore_pin_opens_nothing() {
	Watchdog watchdog("a chore pin opens nothing");
	DatabasePool pool(8);
	Endpoint endpoint(fresh("chorepin"));
	{
		auto none = engine::sync_wait(pool.pin_chore(endpoint));
		CHECK_FALSE(static_cast<bool>(none));
		CHECK_EQ(pool.count().first, 0u);
	}
	ShardEndpoint* raw;
	{
		auto pin = engine::sync_wait(pool.pin(endpoint));
		raw = &pin.entry();
		CHECK_FALSE(static_cast<bool>(engine::sync_wait(pool.pin_chore(endpoint))));  // made, not open
		Shard shard(*raw, WRITE);
		engine::sync_wait(shard.add_document(make("Ka", "a")));
	}
	{
		auto chore = engine::sync_wait(pool.pin_chore(endpoint));
		CHECK_TRUE(static_cast<bool>(chore));
		CHECK_TRUE(&chore.entry() == raw);
		engine::sync_wait(pool.flush());
		CHECK_TRUE(A::state(*raw) == ShardEndpoint::State::open);  // pinned: not closed
	}
	engine::sync_wait(pool.flush());
	CHECK_TRUE(A::state(*raw) == ShardEndpoint::State::empty);
	const auto opens = raw->opens();
	CHECK_FALSE(static_cast<bool>(engine::sync_wait(pool.pin_chore(endpoint))));  // closed: empty, not reopened
	CHECK_EQ(raw->opens(), opens);
	CHECK_EQ(pool.count().second, 0u);
}

// A close waits for the readers that hold views of the index, and while it waits it holds no turn: a writer
// or a refresh that needs the turn is not behind a drain (X3-3).
void test_a_close_drains_its_readers_without_the_turn() {
	Watchdog watchdog("a close drains off the turn");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("drain_turn")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	HeldSearch held(shard);
	CHECK_TRUE(held.holding());
	std::atomic<bool> closed{false};
	CHECK_TRUE(entry->begin_idle_close(1000));   // the handles of this test count as uses: mark it anyway
	std::thread closer([&] {
		engine::sync_wait(entry->finish_idle_close());
		closed = true;
	});
	// The close cannot end while the search holds its view.
	CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::closing);
	std::this_thread::sleep_for(150ms);   // the drain polls with a growing pause: it is waiting by now
	CHECK_FALSE(closed.load());
	CHECK_TRUE(A::turn_is_free(*entry.get()));
	held.release();
	closer.join();
	CHECK_TRUE(closed.load());
	CHECK_TRUE(A::state(*entry.get()) == ShardEndpoint::State::empty);
}

// The records belong to the index: one index with all of its records held refuses its next reader, and a
// neighbour is not affected (C9).
void test_readers_cap_is_per_index() {
	Watchdog watchdog("the readers cap per index");
	DatabasePool pool(8);
	pool.set_readers_per_index(4);
	auto first = pool.spawn(Endpoint(fresh("readers_first")));
	auto second = pool.spawn(Endpoint(fresh("readers_second")));
	Shard a(*first.get(), WRITE);
	Shard b(*second.get(), WRITE);
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
	DatabasePool pool(indexes + 8);
	pool.set_readers_per_index(2);
	std::vector<ReferencedShardEndpoint> entries;
	std::vector<std::unique_ptr<Shard>> shards;
	for (std::size_t i = 0; i < indexes; ++i) {
		entries.push_back(pool.spawn(Endpoint(fresh(("wide" + std::to_string(i)).c_str()))));
		shards.push_back(std::make_unique<Shard>(*entries.back().get(), WRITE));
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

// A search names at most max_indexes_per_search shards: the limit is served, one more is the client's error (a 400).
void test_search_fanout_limit() {
	CHECK_TRUE(error_kind([] { check_search_fanout(200, 200); }) == engine::errc::internal);  // no error
	CHECK_TRUE(error_kind([] { check_search_fanout(1, 200); }) == engine::errc::internal);
	CHECK_TRUE(error_kind([] { check_search_fanout(201, 200); }) == engine::errc::invalid);
	CHECK_TRUE(error_kind([] { check_search_fanout(1010, 200); }) == engine::errc::invalid);
	CHECK_TRUE(error_kind([] { check_search_fanout(5000, 0); }) == engine::errc::internal);  // 0: unset
	try {
		check_search_fanout(201, 200);
	} catch (const engine::Error& error) {
		const std::string message = error.description();
		CHECK_TRUE(message.find("200") != std::string::npos);
		CHECK_TRUE(message.find("--max-indexes-per-search") != std::string::npos);
	}
}

// A commit whose turn times out after the write was applied does not fail the write: a client that retried it would
// create a second document. The write stands, and a later commit makes it durable (N2).
void test_a_commit_that_times_out_leaves_the_write_standing() {
	Watchdog watchdog("a late commit");
	DatabasePool pool(8);
	pool.set_turn_timeout(0.3);
	auto entry = pool.spawn(Endpoint(fresh("late_commit")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	auto open = pool.spawn(Endpoint(fresh("finishing_open")));
	auto closed = pool.spawn(Endpoint(fresh("finishing_closed")));
	Shard shard(*open.get(), WRITE);
	Shard other(*closed.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	pool.finish();
	engine::sync_wait(shard.add_document(make("Kb", "b")));
	CHECK_TRUE(engine::sync_wait(shard.commit()));
	CHECK_EQ(engine::sync_wait(shard.get_view(2))->data(), "b");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(other.ready()); }) == engine::errc::not_available);
}

// The uuid the data storage's volumes carry is read while a stale engine is replaced: under the entry's lock (N1;
// the thread sanitizer reports the race without it).
void test_the_uuid_is_read_under_the_lock_while_the_index_reopens() {
	Watchdog watchdog("the uuid");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("uuid")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.ready());
	const auto uuid = entry->uuid();
	std::atomic<bool> stop{false};
	std::atomic<int> wrong{0};
	std::thread reader([&] {
		while (!stop) {
			if (entry->uuid() != uuid) { ++wrong; }
		}
	});
	for (int i = 0; i < 10; ++i) {
		A::mark_stale(*entry.get());
		engine::sync_wait(shard.ready());
	}
	stop = true;
	reader.join();
	CHECK_EQ(wrong.load(), 0);
	CHECK_GE(entry->opens(), 11u);
}

// A view readers released stays unfreed until something frees it: the idle housekeeping does, so the files a merge
// replaced go back to the disk without another write (C16).
void test_released_views_are_reclaimed_by_the_housekeeping() {
	Watchdog watchdog("released views", 120s);
	DatabasePool pool(8);
	const auto path = fresh("views");
	auto entry = pool.spawn(Endpoint(path));
	Shard shard(*entry.get(), WRITE);
	// Many one-document segments, which the merge policy keeps merging.
	const auto write = [&](int from, int to) {
		for (int i = from; i <= to; ++i) {
			engine::sync_wait(shard.add_document(make("K" + std::to_string(i), "d")));
			engine::sync_wait(shard.engine_for_read());
		}
	};
	write(1, 40);
	std::size_t peak = 0;
	{
		HeldSearch held(shard);  // a view of the segments as they are now
		CHECK_TRUE(held.holding());
		write(41, 100);  // the merges replace segments the held view still names
		engine::sync_wait(shard.commit());
		const auto end = std::chrono::steady_clock::now() + 20s;
		while (std::chrono::steady_clock::now() < end && shard.engine().merges_pending() == 0) {
			pool.housekeeping();
			std::this_thread::sleep_for(10ms);
		}
		CHECK_GT(shard.engine().merges_pending(), 0u);  // the view a reader holds is owed
		peak = count_files(path);
		held.release();
	}
	// Nothing is written from here: only the housekeeping can free the view the search let go of.
	CHECK_GT(shard.engine().merges_pending(), 0u);
	const auto end = std::chrono::steady_clock::now() + 30s;
	while (std::chrono::steady_clock::now() < end && shard.engine().merges_pending() != 0) {
		pool.housekeeping();
		std::this_thread::sleep_for(20ms);
	}
	CHECK_EQ(shard.engine().merges_pending(), 0u);
	const auto freed = std::chrono::steady_clock::now() + 10s;
	while (std::chrono::steady_clock::now() < freed && count_files(path) >= peak) {
		std::this_thread::sleep_for(20ms);
	}
	CHECK_LT(count_files(path), peak);  // the files only the released view named went back
	CHECK_EQ(engine::sync_wait(shard.engine_for_read())->doc_count(), 100u);
}

// The housekeeping pass looks at a slice of the registry per turn, and starts a task only for an entry that owes
// work: 30 quiet indexes in slices of 4 are 8 turns and no task; one that owes a released view is found in whichever
// slice it sits in and its view is freed.
void test_the_housekeeping_pass_works_in_slices() {
	Watchdog watchdog("housekeeping slices", 120s);
	DatabasePool pool(64);
	std::vector<ReferencedShardEndpoint> quiet;
	for (int i = 0; i < 30; ++i) {
		quiet.push_back(pool.spawn(Endpoint(fresh(("slice_quiet" + std::to_string(i)).c_str()))));
		Shard shard(*quiet.back().get(), WRITE);
		engine::sync_wait(shard.add_document(make("K", "a")));
	}
	pool.housekeeping(4);
	std::uint64_t calls = 0;   // the passes that follow
	CHECK_EQ(pool.housekeeping_turns(), 8u);   // 30 entries, 4 a turn
	CHECK_EQ(pool.housekeeping_spawned(), 0u);

	const auto path = fresh("slice_views");
	auto entry = pool.spawn(Endpoint(path));
	Shard shard(*entry.get(), WRITE);
	const auto write = [&](int from, int to) {
		for (int i = from; i <= to; ++i) {
			engine::sync_wait(shard.add_document(make("K" + std::to_string(i), "d")));
			engine::sync_wait(shard.engine_for_read());
		}
	};
	write(1, 40);
	{
		HeldSearch held(shard);
		CHECK_TRUE(held.holding());
		write(41, 100);
		engine::sync_wait(shard.commit());
		const auto end = std::chrono::steady_clock::now() + 20s;
		while (std::chrono::steady_clock::now() < end && shard.engine().merges_pending() == 0) {
			std::this_thread::sleep_for(10ms);
			pool.housekeeping(4);
			++calls;
		}
		CHECK_GT(shard.engine().merges_pending(), 0u);
		held.release();
	}
	const auto spawned = pool.housekeeping_spawned();
	const auto end = std::chrono::steady_clock::now() + 30s;
	while (std::chrono::steady_clock::now() < end && shard.engine().merges_pending() != 0) {
		pool.housekeeping(4);
		++calls;
		std::this_thread::sleep_for(20ms);
	}
	CHECK_EQ(shard.engine().merges_pending(), 0u);
	CHECK_GT(pool.housekeeping_spawned(), spawned);
	// Each pass started at most the one entry that owes work, never one of the 30 quiet ones.
	CHECK_LE(pool.housekeeping_spawned(), calls);
}

// The engine's integrity check changes nothing but runs against the engine the node has open, so it takes the turn
// like any other operation on the index.
void test_the_integrity_check_takes_the_turn() {
	Watchdog watchdog("the check takes the turn");
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("check")));
	Shard shard(*entry.get(), WRITE);
	engine::sync_wait(shard.add_document(make("Ka", "a")));
	engine::sync_wait(shard.commit());
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("scheduler")));
	Shard shard(*entry.get(), WRITE);
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		task::sync_wait(shard.ready());
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
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("chorepost")));
	auto chores = A::take_chores(*entry.get());
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

// The shutdown's report of what it abandoned lists the indexes something still uses: a turn held, or a reference.
void test_the_registry_lists_what_still_uses_an_index() {
	Watchdog watchdog("what still uses an index");
	DatabasePool pool(8);
	Endpoint endpoint(fresh("listbusy"));
	ShardEndpoint* raw;
	{
		auto entry = pool.spawn(endpoint);
		raw = entry.get();
		Shard shard(*raw, WRITE);
		engine::sync_wait(shard.ready());
		const auto used = pool.busy();   // the test's own reference
		REQUIRE_EQ(used.size(), 1u);
		CHECK_TRUE(used[0].find("1 reference") != std::string::npos);
		CHECK_TRUE(used[0].find("turn held") == std::string::npos);
	}
	CHECK_TRUE(pool.busy().empty());   // nobody uses the open index now
	{
		HoldTurn hold(*raw, WRITE);
		const auto held = pool.busy();
		REQUIRE_EQ(held.size(), 1u);
		CHECK_TRUE(held[0].find("turn held") != std::string::npos);
		CHECK_TRUE(held[0].find("listbusy") != std::string::npos);
	}
	CHECK_TRUE(pool.busy().empty());   // the turn is back
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

// The lease: the index's turn is held, and a task waits for it.
void at_end_turn(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(8);
	ShardEndpoint* raw = nullptr;
	std::unique_ptr<HoldTurn> hold;
	std::unique_ptr<Running> waiter;
	{
		auto entry = pool->spawn(Endpoint(fresh("at_end_turn")));
		raw = entry.get();
		Shard shard(*raw, WRITE);
		engine::sync_wait(shard.ready());
		hold = std::make_unique<HoldTurn>(*raw, WRITE);
		waiter = std::make_unique<Running>([raw] { engine::sync_wait(take_and_drop(*raw)); });
		std::this_thread::sleep_for(100ms);
	}
	if (!outstanding) {
		hold->release();
		waiter->join();
	}
	pool.reset();
}

// The registry: a view a reader holds when the index ends.
void at_end_views(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(8);
	std::unique_ptr<HeldSearch> held;
	{
		auto entry = pool->spawn(Endpoint(fresh("at_end_views")));
		Shard shard(*entry.get(), WRITE);
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
	auto pool = std::make_unique<DatabasePool>(8);
	std::atomic<bool> go{false};
	std::atomic<bool> started{false};
	ShardEndpoint* raw = nullptr;
	{
		auto entry = pool->spawn(Endpoint(fresh("at_end_chores")));
		raw = entry.get();
		A::chore_group(*raw).enqueue([&go, &started] {
			started = true;
			while (!go) {
				std::this_thread::sleep_for(1ms);
			}
		});
		if (!eventually(started)) {
			std::_Exit(3);
		}
	}
	if (!outstanding) {
		go = true;
		A::chore_group(*raw).wait();
	}
	pool.reset();
}

// The registry: a task posted to the control loop has not ended.
void at_end_detached(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(8);
	A::count_detached(*pool, 1);
	if (!outstanding) {
		A::count_detached(*pool, -1);
	}
	pool.reset();
}

// The registry: a reference to an entry outlives the pool.
void at_end_references(bool outstanding) {
	auto pool = std::make_unique<DatabasePool>(8);
	std::optional<ReferencedShardEndpoint> entry;
	entry.emplace(pool->spawn(Endpoint(fresh("at_end_refs"))));
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
	check_at_end("turn", at_end_turn, "the turn is held or awaited");
	check_at_end("views", at_end_views, "views are claimed by readers");
	check_at_end("chores", at_end_chores, "a chore is queued or running");
	check_at_end("detached", at_end_detached, "detached tasks have not ended");
	check_at_end("references", at_end_references, "references");
#endif
}

}  // namespace

int main(int argc, char** argv) {
	struct Case { const char* name; void (*fn)(); };
	const Case cases[] = {
		{"test_smoke", test_smoke},
		{"test_opening_never_waits_on_a_thread", test_opening_never_waits_on_a_thread},
		{"test_concurrent_opens_are_one_flight", test_concurrent_opens_are_one_flight},
		{"test_a_failed_open_reaches_every_waiter_and_is_retried", test_a_failed_open_reaches_every_waiter_and_is_retried},
		{"test_every_operation_takes_the_turn", test_every_operation_takes_the_turn},
		{"test_a_read_with_nothing_to_refresh_takes_no_turn", test_a_read_with_nothing_to_refresh_takes_no_turn},
		{"test_the_autocommit_task_takes_the_turn", test_the_autocommit_task_takes_the_turn},
		{"test_the_idle_close_takes_the_turn", test_the_idle_close_takes_the_turn},
		{"test_a_turn_wait_times_out_with_busy", test_a_turn_wait_times_out_with_busy},
		{"test_a_task_holds_one_turn_at_most", test_a_task_holds_one_turn_at_most},
		{"test_a_request_racing_an_idle_close", test_a_request_racing_an_idle_close},
		{"test_a_stale_engine_is_replaced_before_a_queued_write", test_a_stale_engine_is_replaced_before_a_queued_write},
		{"test_a_commit_joins_the_one_in_flight", test_a_commit_joins_the_one_in_flight},
		{"test_a_commit_not_covered_does_not_share_the_outcome", test_a_commit_not_covered_does_not_share_the_outcome},
		{"test_a_failed_commit_reaches_only_the_joined_waiters", test_a_failed_commit_reaches_only_the_joined_waiters},
		{"test_owed_flushes_do_not_reorder_writers", test_owed_flushes_do_not_reorder_writers},
		{"test_housekeeping_costs_nothing_on_a_quiet_index", test_housekeeping_costs_nothing_on_a_quiet_index},
		{"test_blob_io_runs_in_chores", test_blob_io_runs_in_chores},
		{"test_the_wal_order_is_the_apply_order", test_the_wal_order_is_the_apply_order},
		{"test_a_held_search_delays_no_refresh_or_commit", test_a_held_search_delays_no_refresh_or_commit},
		{"test_readers_cap", test_readers_cap},
		{"test_readers_cap_is_per_index", test_readers_cap_is_per_index},
		{"test_a_close_drains_its_readers_without_the_turn", test_a_close_drains_its_readers_without_the_turn},
		{"test_flush_closes_every_idle_index_before_it_returns", test_flush_closes_every_idle_index_before_it_returns},
		{"test_a_pin_keeps_its_index_open", test_a_pin_keeps_its_index_open},
		{"test_a_chore_pin_opens_nothing", test_a_chore_pin_opens_nothing},
		{"test_a_search_over_more_indexes_than_records_is_served", test_a_search_over_more_indexes_than_records_is_served},
		{"test_search_fanout_limit", test_search_fanout_limit},
		{"test_a_commit_that_times_out_leaves_the_write_standing", test_a_commit_that_times_out_leaves_the_write_standing},
		{"test_a_finishing_pool_serves_an_open_index", test_a_finishing_pool_serves_an_open_index},
		{"test_the_uuid_is_read_under_the_lock_while_the_index_reopens", test_the_uuid_is_read_under_the_lock_while_the_index_reopens},
		{"test_released_views_are_reclaimed_by_the_housekeeping", test_released_views_are_reclaimed_by_the_housekeeping},
		{"test_the_housekeeping_pass_works_in_slices", test_the_housekeeping_pass_works_in_slices},
		{"test_the_integrity_check_takes_the_turn", test_the_integrity_check_takes_the_turn},
		{"test_a_chore_needs_a_scheduler", test_a_chore_needs_a_scheduler},
		{"test_a_chore_resumes_its_awaiter_with_post", test_a_chore_resumes_its_awaiter_with_post},
		{"test_the_registry_lists_what_still_uses_an_index", test_the_registry_lists_what_still_uses_an_index},
		{"test_a_registry_that_ends_with_work_outstanding_aborts", test_a_registry_that_ends_with_work_outstanding_aborts},
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
