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

// A request is a task on a reactor (docs/design/phase7c.md D2, D3, D6): a search that has a budget gives
// its loop back at window boundaries and stops when cancelled, the index admits only so many readers, a
// task that holds a view waits for no turn, open or drain (the N2 rule), and the views' handle is read
// with no lock while the index is replaced. Each test runs real tasks on a real reactor loop.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "engine/callbacks.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/host_asio.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/strata/shard.h"
#include "engine/strata/viewed.h"
#include "engine/sync.h"
#include "reactor.h"
#include "task.hh"

using namespace std::chrono_literals;

namespace {

constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT;

std::string fresh(const char* name) {
	std::string path = std::string("reactor_requests_") + name + ".db";
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.set_data(key);
	return doc;
}

// Fills an index from this (a plain) thread.
void fill(Shard& shard, int documents) {
	for (int i = 0; i < documents; ++i) {
		engine::sync_wait(shard.add_document(make("K" + std::to_string(i))));
	}
	engine::sync_wait(shard.commit());
}

// A loop with its host, bound to the loop's thread as the manager binds its reactors.
struct Loop {
	reactor::Reactor reactor;
	engine::AsioHost host{reactor.io().get_executor()};
	Loop() {
		asio::post(reactor.io(), [this] { host.bind_loop_thread(); });
	}
	~Loop() { engine::Host::bind_thread(nullptr); }   // the test thread ran the loop

	// Runs a request's task on the loop until it ends; what it throws is rethrown here.
	template <typename F>
	void run(F&& body) {
		std::exception_ptr error;
		engine::Run run(host);
		asio::co_spawn(reactor.io(), [&]() -> asio::awaitable<void> {
			try {
				co_await run.await(body());
			} catch (...) {
				error = std::current_exception();
			}
			reactor.io().stop();
		}, asio::detached);
		reactor.run();
		if (error) {
			std::rethrow_exception(error);
		}
	}
};

engine::SearchRequest everything(const engine::PreparedQuery& query, engine::MatchSpy* spy = nullptr) {
	engine::SearchRequest request;
	request.query = &query;
	request.maxitems = 10;
	request.check_at_least = engine::CHECK_ALL;
	request.spy = spy;
	return request;
}

// Counts the matches it sees, and notes whether `other` had run by the time the last one came.
class CountingSpy final : public engine::MatchSpy {
public:
	std::atomic<std::size_t> seen{0};
	void see(const engine::DocView&, double) override { ++seen; }
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountingSpy>(); }
	std::string name() const override { return "CountingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "CountingSpy"; }
};

}  // namespace


// A search with a budget yields at window boundaries: a task queued behind it finishes before it does. With no
// budget the search runs to its end first.
static void test_a_search_with_a_budget_shares_its_loop() {
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("share")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 12000);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto prepared = engine::prepare(engine::Query::term("f", "body"));

	for (const bool budgeted : {true, false}) {
		Loop loop;
		std::atomic<bool> light_done{false};
		std::size_t seen_when_light_finished = 0;
		CountingSpy spy;
		task::cancel_source cancel;
		task::budget budget(1us, cancel.token());
		auto request = everything(*prepared, &spy);
		request.budget = budgeted ? &budget : nullptr;
		loop.run([&]() -> task::task<void> {
			// A task queued as the search starts: it runs between the search's slices, or after it.
			loop.host.spawn([](std::atomic<bool>& done, CountingSpy& s, std::size_t& at) -> task::task<void> {
				at = s.seen.load();
				done = true;
				co_return;
			}(light_done, spy, seen_when_light_finished));
			auto result = co_await engine::search(std::vector<engine::Shard*>{eng}, request);
			co_await task::yield();   // the light task, if it has not run, runs before this resumes
			CHECK_EQ(result.estimated, 12000u);
			CHECK_TRUE(light_done.load());
		});
		CHECK_EQ(spy.seen.load(), 12000u);
		if (budgeted) {
			CHECK_LT(seen_when_light_finished, 12000u);   // it ran between the search's slices
		} else {
			CHECK_EQ(seen_when_light_finished, 12000u);   // it waited for the whole search
		}
	}
}

// A request cancelled while its search is queued stops at the next checkpoint, and the search's views and its
// admissions are returned (the next search is served).
static void test_a_cancelled_search_stops_and_lets_go() {
	DatabasePool pool(8);
	pool.set_readers_per_index(2);
	auto entry = pool.spawn(Endpoint(fresh("cancel")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 12000);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto prepared = engine::prepare(engine::Query::term("f", "body"));

	for (int i = 0; i < 4; ++i) {   // more cancelled searches than the index admits at once
		Loop loop;
		task::cancel_source cancel;
		task::budget budget(1us, cancel.token());
		CountingSpy spy;
		auto request = everything(*prepared, &spy);
		request.budget = &budget;
		bool cancelled = false;
		cancel.request();
		try {
			loop.run([&]() -> task::task<void> {
				co_await engine::search(std::vector<engine::Shard*>{eng}, request);
			});
		} catch (const task::cancelled&) {
			cancelled = true;
		}
		CHECK_TRUE(cancelled);
		CHECK_LT(spy.seen.load(), 12000u);
	}
	// Nothing is still held: a search runs to its end.
	engine::SearchRequest request = everything(*prepared);
	CHECK_EQ(engine::sync_wait(engine::search(std::vector<engine::Shard*>{eng}, request)).estimated, 12000u);
	auto* strata = dynamic_cast<engine::strata::Shard*>(eng);
	REQUIRE(strata != nullptr);
	CHECK_EQ(strata->views_refused(), 0u);
}

// The index's cap is an admission: a view past it is refused and counted, before it takes a record.
static void test_the_cap_is_an_admission_that_counts() {
	DatabasePool pool(8);
	pool.set_readers_per_index(3);
	auto entry = pool.spawn(Endpoint(fresh("admission")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 5);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto* strata = dynamic_cast<engine::strata::Shard*>(eng);
	REQUIRE(strata != nullptr);
	std::vector<engine::strata::Viewed> held;
	for (int i = 0; i < 3; ++i) {
		held.push_back(strata->view());
	}
	CHECK_EQ(strata->views_refused(), 0u);
	engine::errc kind = engine::errc::internal;
	try {
		(void)strata->view();
	} catch (const engine::Error& error) {
		kind = error.kind;
	}
	CHECK_TRUE(kind == engine::errc::busy);
	CHECK_EQ(strata->views_refused(), 1u);
	held.pop_back();   // a view released returns its admission
	auto again = strata->view();
	CHECK_TRUE(static_cast<bool>(again));
	CHECK_EQ(strata->views_refused(), 1u);
}

// The views' handle is read with no lock and no count while the index is replaced under the readers: every
// reader gets a view or `closed`, never a freed handle (run under ASan and TSan).
static void test_views_are_acquired_while_the_index_is_replaced() {
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("replace")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 50);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto* strata = dynamic_cast<engine::strata::Shard*>(eng);
	REQUIRE(strata != nullptr);
	std::atomic<bool> stop{false};
	std::atomic<std::size_t> served{0};
	std::vector<std::thread> readers;
	for (int t = 0; t < 4; ++t) {
		readers.emplace_back([&] {
			while (!stop) {
				try {
					auto view = strata->view();
					++served;
				} catch (const engine::Error&) { }   // closed or reopening: an answer, not a crash
			}
		});
	}
	for (int i = 0; i < 20; ++i) {
		engine::sync_wait(strata->reopen());
		std::this_thread::sleep_for(2ms);
	}
	stop = true;
	for (auto& reader : readers) {
		reader.join();
	}
	CHECK_GT(served.load(), 0u);
}

#ifndef NDEBUG
struct RuleBroken {
	std::string waits_for;
};

void throw_rule_broken(const char* waits_for) {
	throw RuleBroken{waits_for};
}

// What a task that holds a view waits for, when it breaks the rule: the name of the wait, or empty.
std::string broken_by(Loop& loop, const std::function<task::task<void>()>& body) {
	engine::view_rule_handler() = &throw_rule_broken;
	std::string waits_for;
	try {
		loop.run(body);
	} catch (const RuleBroken& broken) {
		waits_for = broken.waits_for;
	}
	engine::view_rule_handler() = nullptr;
	return waits_for;
}

// The N2 rule: a task that holds a view waits for no turn, no open and no drain, for the view could be what
// the wait waits for. The check stops the task that breaks it (debug builds).
static void test_a_task_that_holds_a_view_waits_for_nothing() {
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("rules")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 5);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto* strata = dynamic_cast<engine::strata::Shard*>(eng);
	REQUIRE(strata != nullptr);

	{
		Loop loop;
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			auto view = strata->view();
			co_await shard.add_document(make("Kx"));
		}), std::string("a turn"));
	}
	{
		Loop loop;
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			auto view = strata->view();
			co_await shard.reopen();
		}), std::string("an open"));
	}
	{
		// Closing the index waits for its readers, and this task is one.
		Loop loop;
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			auto view = strata->view();
			co_await strata->close();
		}), std::string("a drain"));
	}
}

// The same waits are fine once the view is let go, and a search that yields while it holds views is fine.
static void test_the_rule_allows_a_yield_and_a_wait_after_the_view() {
	DatabasePool pool(8);
	auto entry = pool.spawn(Endpoint(fresh("rules_ok")));
	Shard shard(*entry.get(), WRITE);
	fill(shard, 5);
	auto* eng = engine::sync_wait(shard.engine_for_read());
	auto* strata = dynamic_cast<engine::strata::Shard*>(eng);
	REQUIRE(strata != nullptr);
	Loop loop;
	CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
		{
			auto view = strata->view();
			co_await task::yield();
			co_await task::yield();
		}
		co_await shard.add_document(make("Ky"));
	}), std::string());
}
#endif

#ifndef NDEBUG
TEST_MAIN(
	test_a_search_with_a_budget_shares_its_loop,
	test_a_cancelled_search_stops_and_lets_go,
	test_the_cap_is_an_admission_that_counts,
	test_views_are_acquired_while_the_index_is_replaced,
	test_a_task_that_holds_a_view_waits_for_nothing,
	test_the_rule_allows_a_yield_and_a_wait_after_the_view
)
#else
TEST_MAIN(
	test_a_search_with_a_budget_shares_its_loop,
	test_a_cancelled_search_stops_and_lets_go,
	test_the_cap_is_an_admission_that_counts,
	test_views_are_acquired_while_the_index_is_replaced
)
#endif
