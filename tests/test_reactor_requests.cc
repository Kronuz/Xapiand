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
#include <ctime>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "completion/posix.h"
#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "pool_support.h"
#include "engine/callbacks.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/host_asio.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/strata/shard.h"
#include "engine/strata/viewed.h"
#include "engine/sync.h"
#include "http_asio.h"
#include "http_body_stream.h"
#include "http_message.h"
#include "reactor.h"
#include "server/search_service.h"
#include "server/search_views.h"
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

// A loop with its host over the reactor's scheduler, bound to the loop's thread as the manager binds its reactors.
struct Loop {
	reactor::Reactor reactor;
	engine::AsioHost host{reactor.scheduler()};
	Loop() {
		asio::post(reactor.io(), [this] { host.bind_loop_thread(); });
	}
	~Loop() { engine::Host::bind_thread(nullptr); }   // the test thread ran the loop

	// Runs a request's task on the loop until it ends, as SearchService::handle awaits a view; what it throws is
	// rethrown here. A task holds no Asio work while it waits for a chore, so the loop is kept running until it ends.
	template <typename F>
	void run(F&& body) {
		auto work = asio::make_work_guard(reactor.io());
		std::exception_ptr error;
		host.spawn([](Loop& loop, F& body, std::exception_ptr& error) -> task::task<void> {
			try {
				co_await engine::counted(loop.host, body());
			} catch (...) {
				error = std::current_exception();
			}
			loop.reactor.io().stop();
		}(*this, body, error));
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
	DatabasePool pool(control_loop(), 8);
	auto entry = pin_index(pool, fresh("share"), WRITE);
	Shard shard = use_index(entry, WRITE);
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
	DatabasePool pool(control_loop(), 8);
	pool.set_readers_per_index(2);
	auto entry = pin_index(pool, fresh("cancel"), WRITE);
	Shard shard = use_index(entry, WRITE);
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
	DatabasePool pool(control_loop(), 8);
	pool.set_readers_per_index(3);
	auto entry = pin_index(pool, fresh("admission"), WRITE);
	Shard shard = use_index(entry, WRITE);
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

// A bulk body is parsed in a chore of the request's group, not on the loop (D12): the loop thread spends a fraction of
// the parse's CPU time on a body that takes `inline_ms` of CPU to parse.
static double thread_cpu_ms() {
	timespec ts;
	::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
}

static void test_a_bulk_body_is_parsed_off_the_loop() {
	DatabasePool pool(control_loop(), 8);
	const std::string line = R"({"_id": 1, "user": "kronuz", "message": "a line of the bulk body that is a little long to parse"})" "\n";
	std::string body;
	std::size_t lines = 0;
	double inline_ms = 0;
	while (inline_ms < 80 && body.size() < (96u << 20)) {
		for (std::size_t i = 0; i < 100000; ++i) {
			body += line;
			++lines;
		}
		Request probe;
		probe.mode = Request::Mode::STREAM_NDJSON;
		probe.has_content_length = true;
		const auto start = thread_cpu_ms();
		probe.append(body.data(), body.size());
		probe.append(nullptr, 0);
		inline_ms = thread_cpu_ms() - start;
	}
	CHECK_GE(inline_ms, 80.0);   // a parse long enough to tell

	Loop loop;
	auto stream = std::make_shared<http::BodyStream>(loop.reactor.scheduler(), 8);
	http::Request hreq;
	hreq.body_stream = stream;
	hreq.set_scheduler(&loop.reactor.scheduler());
	Request request;
	request.http_req = &hreq;
	request.mode = Request::Mode::STREAM_NDJSON;
	request.has_content_length = true;
	IndexChores chores(pool.work_pool());
	request.parse_chores = &chores;

	loop.host.spawn([](std::shared_ptr<http::BodyStream> stream, const std::string& body) -> task::task<void> {
		constexpr std::size_t piece = 64 * 1024;
		for (std::size_t at = 0; at < body.size(); at += piece) {
			co_await stream->push(body.substr(at, piece), {});
		}
		stream->finish();
	}(stream, body));
	std::size_t total = 0;
	const auto start = thread_cpu_ms();
	loop.run([&]() -> task::task<void> {
		std::vector<MsgPack> batch;
		while (co_await request.next_batch(batch)) {
			total += batch.size();
		}
	});
	const double loop_ms = thread_cpu_ms() - start;
	CHECK_EQ(total, lines);
	CHECK_LT(loop_ms, inline_ms / 3);   // the parse ran on the pool's workers
}

// A loop with more demoted handles than its cap refuses the next request before its body is read (D2): 503 with Retry-After.
static void test_a_loop_past_its_demoted_cap_refuses_new_requests() {
	SearchService service;
	Loop loop;
	int status = 0;
	unsigned retry_after = 0;
	bool admitted_below_the_cap = false;
	loop.run([&]() -> task::task<void> {
		const http::Request request;
		admitted_below_the_cap = service.create_extension(request) != nullptr;
		for (int i = 0; i < 300; ++i) {
			loop.host.spawn([]() -> task::task<void> {
				co_await task::yield_awaiter{true};   // spends its slice: waits demoted
			}());
		}
		co_await task::yield();   // every spawned task has run and parked itself demoted
		CHECK_GE(loop.reactor.scheduler().demoted(), 256u);   // the host's queue is the reactor's
		try {
			(void)service.create_extension(request);
		} catch (const http::RequestRejected& rejected) {
			status = rejected.status();
			retry_after = rejected.retry_after();
		}
		while (loop.host.scheduler().demoted() != 0) {
			co_await task::yield();
		}
	});
	CHECK_TRUE(admitted_below_the_cap);
	CHECK_EQ(status, 503);
	CHECK_EQ(retry_after, 1u);
}

// Shutdown with requests suspended on a turn, a chore, a body read, and an event while holding a turn (task's
// cancellation D8): the loop stops, the chores are cancelled and drained, the host closes and destroys its tasks, and
// no task frame is left; both turns are free for the next one.
static void test_teardown_destroys_requests_suspended_on_a_turn_a_chore_and_a_body_read() {
	DatabasePool pool(control_loop(), 8);
	auto entry = pin_index(pool, fresh("teardown"), WRITE);
	{
		Shard shard = use_index(entry, WRITE);
		fill(shard, 10);
	}
	auto other = pin_index(pool, fresh("teardown_other"), WRITE);
	(void)engine::sync_wait(other->take_turn(WRITE));   // opened now: an open index's own tasks are in the baseline
	std::optional<Turn> held(engine::sync_wait(entry->take_turn(WRITE)));   // the turn request (a) waits for
	task::event never;   // what request (d) waits on, holding the other index's turn
	IndexChores chores(pool.work_pool());
	std::atomic<bool> chore_started{false};
	std::atomic<bool> chore_release{false};
	std::atomic<int> ended{0};
	const auto baseline = task::detail::live_frames();
	{
		Loop loop;
		auto stream = std::make_shared<http::BodyStream>(loop.reactor.scheduler(), 8);
		http::Request hreq;
		hreq.body_stream = stream;
		hreq.set_scheduler(&loop.reactor.scheduler());
		Request request;
		request.http_req = &hreq;
		request.mode = Request::Mode::STREAM_NDJSON;
		request.has_content_length = true;
		request.parse_chores = &chores;

		auto start = [&](std::function<task::task<void>()> body) {
			loop.host.spawn([](Loop& loop, std::atomic<int>& ended, std::function<task::task<void>()> body) -> task::task<void> {
				try {
					co_await engine::counted(loop.host, body());
				} catch (...) {
				}
				++ended;
			}(loop, ended, std::move(body)));
		};
		start([&]() -> task::task<void> {   // (a) a turn
			auto mine = co_await entry->take_turn(WRITE);
		});
		start([&]() -> task::task<void> {   // (b) a chore
			co_await chores.run(engine::ChoreClass::flush, [&] {
				chore_started = true;
				while (!chore_release) {
					std::this_thread::sleep_for(1ms);
				}
			});
		});
		start([&]() -> task::task<void> {   // (c) a body read
			std::vector<MsgPack> batch;
			while (co_await request.next_batch(batch)) {
			}
		});
		start([&]() -> task::task<void> {   // (d) a turn held across a wait: its Turn points into the request's counters
			auto mine = co_await other->take_turn(WRITE);
			co_await never;
		});
		asio::steady_timer stopper(loop.reactor.io(), 300ms);
		stopper.async_wait([&](const asio::error_code&) { loop.reactor.io().stop(); });
		loop.reactor.run();
		CHECK_TRUE(chore_started.load());
		CHECK_EQ(ended.load(), 0);

		// The loop has stopped. The running chore ends later; cancel_all() waits for it.
		std::thread releaser([&] {
			std::this_thread::sleep_for(50ms);
			chore_release = true;
		});
		CHECK_TRUE(IndexChores::cancel_all(std::chrono::steady_clock::now() + 5s));
		releaser.join();
		loop.reactor.scheduler().close();
		loop.reactor.scheduler().destroy_roots();
		CHECK_EQ(ended.load(), 0);   // destroyed, not completed
	}
	// The destroyed waiter's node stays queued until the holder's unlock sheds it; a deadline's state goes with
	// its timer, in the loop's io_context. Both are gone now.
	held.reset();
#if TASK_COUNT_FRAMES
	CHECK_EQ(task::detail::live_frames(), baseline);
#else
	(void)baseline;
#endif
	(void)engine::sync_wait(entry->take_turn(WRITE));   // the destroyed waiter does not keep the turn
	(void)engine::sync_wait(other->take_turn(WRITE));   // the destroyed holder gave its turn back
	CHECK_TRUE(chores.idle());
}


// A request's token ends its wait for a turn (F2): the turn's deadline is linked to it, so a cancelled request leaves
// the queue at once with task::cancelled, not `busy` at the turn timeout. A Shard passes its token to the wait.
static void test_a_cancelled_request_leaves_the_turn_queue() {
	DatabasePool pool(control_loop(), 8);
	pool.set_turn_timeout(60);
	auto entry = pin_index(pool, fresh("turn_cancel"), WRITE);
	{
		Shard shard = use_index(entry, WRITE);
		fill(shard, 1);
	}
	std::optional<Turn> held(engine::sync_wait(entry->take_turn(WRITE)));
	for (int through_shard = 0; through_shard < 2; ++through_shard) {
		Loop loop;
		task::cancel_source source;
		asio::steady_timer timer(loop.reactor.io(), 50ms);
		timer.async_wait([&](const asio::error_code& ec) {
			if (!ec) { source.request(); }
		});
		std::string outcome;
		const auto began = std::chrono::steady_clock::now();
		loop.run([&]() -> task::task<void> {
			try {
				if (through_shard) {
					Shard shard(co_await pool.pin(Endpoint(entry->path), WRITE), WRITE, source.token());
					(void)co_await shard.add_document(make("Kc"));
				} else {
					(void)co_await entry->take_turn(WRITE, source.token());
				}
				outcome = "turn";
			} catch (const task::cancelled&) {
				outcome = "cancelled";
			} catch (const engine::Error& error) {
				outcome = error.reason();
			}
		});
		const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
		CHECK_EQ(outcome, std::string("cancelled"));
		CHECK_LT(ms, 5000.0);
	}
	held.reset();
	(void)engine::sync_wait(entry->take_turn(WRITE));   // the cancelled waiters left the queue
}

// A request's token ends its wait for its index's pin too (CR2): a use's pin passes the request's token, so a request
// waiting on a cold open leaves at once with task::cancelled, and the open it started goes on.
static void test_a_cancelled_request_leaves_the_pin_queue() {
	DatabasePool pool(control_loop(), 8);
	std::promise<void> gate;
	std::shared_future<void> opens = gate.get_future().share();
	std::atomic<bool> opening{false};
	pool.set_index_io([&](const std::string& at) -> std::unique_ptr<kronuz::journal::IO> {
		opening = true;
		opens.wait();
		const auto dir = (std::filesystem::path(at) / "strata").string();
		std::filesystem::create_directories(dir);
		return std::make_unique<kronuz::journal::PosixIO>(dir, 0600, kronuz::journal::PosixIOOptions{true, true});
	});
	const auto path = fresh("pin_cancel");
	std::thread opener([&] { (void)pin_index(pool, path, WRITE); });
	while (!opening.load()) {
		std::this_thread::sleep_for(1ms);
	}
	Loop loop;
	task::cancel_source source;
	asio::steady_timer timer(loop.reactor.io(), 50ms);
	timer.async_wait([&](const asio::error_code& ec) {
		if (!ec) { source.request(); }
	});
	std::string outcome;
	const auto began = std::chrono::steady_clock::now();
	loop.run([&]() -> task::task<void> {
		try {
			Shard shard(pool, path, WRITE, source.token());   // as lock_shard makes one: its use pins with the token
			co_await shard.ready();
			outcome = "pinned";
		} catch (const task::cancelled&) {
			outcome = "cancelled";
		} catch (const engine::Error& error) {
			outcome = error.reason();
		}
	});
	const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
	CHECK_EQ(outcome, std::string("cancelled"));
	CHECK_LT(ms, 5000.0);
	gate.set_value();
	opener.join();
	CHECK_TRUE(static_cast<bool>(pin_index(pool, path, DB_OPEN)));
}

// The request's token ends a read that waits for the streamed body: Request::read_chunk passes it to
// BodyStream::read, and the read raises task::cancelled.
static void test_a_cancelled_request_ends_its_body_read() {
	Loop loop;
	auto stream = std::make_shared<http::BodyStream>(loop.reactor.scheduler(), 8);
	http::Request hreq;
	hreq.body_stream = stream;
	hreq.set_scheduler(&loop.reactor.scheduler());
	Request request;
	request.http_req = &hreq;
	asio::steady_timer cancel(loop.reactor.io(), 50ms);
	cancel.async_wait([&](const asio::error_code& ec) {
		if (!ec) { hreq.cancel(task::cancel_reason::shutdown); }
	});
	asio::steady_timer watchdog(loop.reactor.io(), 5s);   // ends the read cleanly when the token did not
	watchdog.async_wait([&](const asio::error_code& ec) {
		if (!ec) { stream->finish(); }
	});
	bool failed = false;
	loop.run([&]() -> task::task<void> {
		std::string chunk;
		try {
			(void)co_await request.read_chunk(chunk);
		} catch (const task::cancelled&) {
			failed = true;
		}
		watchdog.cancel();
	});
	CHECK_TRUE(failed);
}

// Teardown through HttpAsioService's phases (task's cancellation D8), as the manager runs it: a request waiting for a
// turn with its token ends at stop(); one waiting without a token (an admitted write) is left suspended by the bounded
// stop() and destroyed by destroy_roots(). No task frame is left and the turn is free.
static void test_service_phases_end_or_destroy_requests_waiting_for_a_turn() {
	DatabasePool pool(control_loop(), 8);
	pool.set_turn_timeout(60);
	auto entry = pin_index(pool, fresh("phases"), WRITE);
	{
		Shard shard = use_index(entry, WRITE);
		fill(shard, 1);
	}
	const auto baseline = task::detail::live_frames();
	std::optional<Turn> held(engine::sync_wait(entry->take_turn(WRITE)));

	struct Handler final : http::HttpHandler {
		ShardEndpoint* entry = nullptr;
		std::atomic<int> waiting{0};
		std::atomic<int> cancelled{0};
		std::atomic<int> ended{0};
		task::task<void> handle(http::Request& req, http::ResponseWriter& response) override {
			auto* host = dynamic_cast<engine::AsioHost*>(engine::Host::current());
			CHECK_TRUE(host != nullptr && &host->scheduler() == &req.scheduler());
			if (host == nullptr) {
				co_return;
			}
			const task::cancel_token token = req.path == "/read" ? req.cancel_token() : task::cancel_token{};
			++waiting;
			try {
				auto turn = co_await engine::counted(*host, entry->take_turn(WRITE, token));
			} catch (const task::cancelled&) {
				++cancelled;
			}
			++ended;
			response.send(200, "ok");
		}
	} handler;
	handler.entry = &entry.entry();

	unsigned short port;
	{
		asio::io_context io;
		asio::ip::tcp::acceptor reservation(io, {asio::ip::address_v4::loopback(), 0});
		port = reservation.local_endpoint().port();
	}
	{
		http::HttpAsioService service(handler, 1, 0);
		service.set_shutdown_grace(100ms);
		engine::AsioHost host(service.reactor(0).scheduler());   // destroyed before the service, as the manager's
		asio::post(service.reactor(0).io(), [&host] { host.bind_loop_thread(); });
		http::AsioBindOptions bind;
		bind.address = "127.0.0.1";
		service.set_bind_options(bind);
		service.start(port);

		asio::io_context client;
		std::vector<asio::ip::tcp::socket> sockets;
		for (const char* path : {"/read", "/write"}) {
			auto& socket = sockets.emplace_back(client);
			socket.connect({asio::ip::address_v4::loopback(), port});
			const std::string request = std::string("GET ") + path + " HTTP/1.1\r\nHost: t\r\n\r\n";
			asio::write(socket, asio::buffer(request));
		}
		for (int i = 0; i < 500 && handler.waiting.load() < 2; ++i) {
			std::this_thread::sleep_for(10ms);
		}
		CHECK_EQ(handler.waiting.load(), 2);

		const auto began = std::chrono::steady_clock::now();
		service.stop();
		const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
		CHECK_LT(ms, 2000.0);                  // bounded: grace, force, grace
		CHECK_EQ(handler.cancelled.load(), 1);  // the read's token ended its wait
		CHECK_EQ(handler.ended.load(), 1);      // the write is still suspended on the turn
		CHECK_TRUE(IndexChores::cancel_all(std::chrono::steady_clock::now() + 5s));
		service.close();
		service.destroy_roots();
		CHECK_EQ(handler.ended.load(), 1);      // destroyed, not completed
	}
	held.reset();   // sheds the destroyed waiter's node
#if TASK_COUNT_FRAMES
	CHECK_EQ(task::detail::live_frames(), baseline);
#else
	(void)baseline;
#endif
	(void)engine::sync_wait(entry->take_turn(WRITE));   // the destroyed waiter does not keep the turn
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
	DatabasePool pool(control_loop(), 8);
	auto entry = pin_index(pool, fresh("rules"), WRITE);
	Shard shard = use_index(entry, WRITE);
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
		// After an Asio completion too: it resumes the request through its counted scheduler, not inline.
		Loop loop;
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			asio::steady_timer timer(loop.reactor.io(), 1ms);
			(void)co_await task::await_asio<void(asio::error_code)>([&](auto handler) { timer.async_wait(std::move(handler)); });
			auto view = strata->view();
			co_await shard.add_document(make("Kw"));
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
		// A pin may open or wake its index: a request pins every index it reads before it takes any view (D4).
		Loop loop;
		const Endpoint other(fresh("rules_pin"));
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			auto view = strata->view();
			auto pin = co_await pool.pin(other, WRITE);
		}), std::string("a pin"));
	}
	{
		// Closing an index of its own (the hostless path; a catalog's is the catalog's to close) waits for its
		// readers, and this task is one.
		auto opened = engine::sync_wait(engine::strata::open_local(fresh("rules_drain"), true, true));
		auto* own = dynamic_cast<engine::strata::Shard*>(opened.shard.get());
		REQUIRE(own != nullptr);
		Loop loop;
		CHECK_EQ(broken_by(loop, [&]() -> task::task<void> {
			auto view = own->view();
			co_await own->close();
		}), std::string("a drain"));
	}
}

// The same waits are fine once the view is let go, and a search that yields while it holds views is fine.
static void test_the_rule_allows_a_yield_and_a_wait_after_the_view() {
	DatabasePool pool(control_loop(), 8);
	auto entry = pin_index(pool, fresh("rules_ok"), WRITE);
	Shard shard = use_index(entry, WRITE);
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
		auto pin = co_await pool.pin(Endpoint(fresh("rules_ok_pin")), WRITE);
		co_await shard.add_document(make("Ky"));
	}), std::string());
}
#endif

#ifndef NDEBUG
TEST_MAIN(
	test_a_search_with_a_budget_shares_its_loop,
	test_a_cancelled_search_stops_and_lets_go,
	test_the_cap_is_an_admission_that_counts,
	test_a_bulk_body_is_parsed_off_the_loop,
	test_teardown_destroys_requests_suspended_on_a_turn_a_chore_and_a_body_read,
	test_a_loop_past_its_demoted_cap_refuses_new_requests,
	test_a_cancelled_request_leaves_the_turn_queue,
	test_a_cancelled_request_leaves_the_pin_queue,
	test_a_cancelled_request_ends_its_body_read,
	test_service_phases_end_or_destroy_requests_waiting_for_a_turn,
	test_a_task_that_holds_a_view_waits_for_nothing,
	test_the_rule_allows_a_yield_and_a_wait_after_the_view
)
#else
TEST_MAIN(
	test_a_search_with_a_budget_shares_its_loop,
	test_a_cancelled_search_stops_and_lets_go,
	test_the_cap_is_an_admission_that_counts,
	test_a_bulk_body_is_parsed_off_the_loop,
	test_teardown_destroys_requests_suspended_on_a_turn_a_chore_and_a_body_read,
	test_a_loop_past_its_demoted_cap_refuses_new_requests,
	test_a_cancelled_request_leaves_the_turn_queue,
	test_a_cancelled_request_leaves_the_pin_queue,
	test_a_cancelled_request_ends_its_body_read,
	test_service_phases_end_or_destroy_requests_waiting_for_a_turn
)
#endif
