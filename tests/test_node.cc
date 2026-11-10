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

// The control plane and the caches on a node running in this process (docs/design/phase7c.md D11, D13): concurrent
// first uses of a schema or of an index's settings load it once while the others await, and the control loop keeps
// answering its lag probe while a commit is slow and while a schema load waits. The node serves on a port of its
// own; every test uses indexes of its own.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "database/data.h"
#include "database/flags.h"
#include "database/handler.h"
#include "database/lock.h"
#include "engine/query.h"
#include "database/pool.h"
#include "database/schemas_lru.h"
#include "database/shard.h"
#include "pool_support.h"
#include "endpoint.h"
#include "logger.h"
#include "engine/chores.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/sync.h"
#include "http_asio.h"
#include "index_resolver_lru.h"
#include "manager.h"
#include "opts.h"
#include "reactor_lag.h"
#include "task.hh"
#include "traceback.h"
#include "xapiand_exception.h"

using namespace std::chrono_literals;

struct PoolTestAccess {
	static void set_chores(ShardEndpoint& entry, std::unique_ptr<engine::Chores> chores) {
		entry.chores = std::move(chores);
		entry.engine_->use_chores(entry.chores.get());
	}
	static std::unique_ptr<engine::Chores> take_chores(ShardEndpoint& entry) { return std::move(entry.chores); }
};

namespace {

using A = PoolTestAccess;

const std::shared_ptr<XapiandManager>& manager();

constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN;   // with the WAL and the autocommit, as a node writes
constexpr int WRITE_QUIET = WRITE | DB_DISABLE_AUTOCOMMIT;

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit = 5s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	return done();
}

// Kills the process when a test hangs.
class Watchdog {
public:
	explicit Watchdog(const char* name, std::chrono::seconds limit = 40s) : name_(name) {
		thread_ = std::thread([this, limit] {
			const auto end = std::chrono::steady_clock::now() + limit;
			while (!done_ && std::chrono::steady_clock::now() < end) {
				std::this_thread::sleep_for(20ms);
			}
			if (!done_) {
				std::fprintf(stderr, "  FAIL %s: hung (schemas: %llu loads, %llu joined, %llu waiting; settings: %llu loads, %llu joined, %llu waiting)\n", name_,
					static_cast<unsigned long long>(manager()->schemas->loads()), static_cast<unsigned long long>(manager()->schemas->joined()), static_cast<unsigned long long>(manager()->schemas->waiting()),
					static_cast<unsigned long long>(manager()->index_settings_resolver->loads()), static_cast<unsigned long long>(manager()->index_settings_resolver->joined()), static_cast<unsigned long long>(manager()->index_settings_resolver->waiting()));
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

// Holds an index's turn from a thread of its own until released.
class HoldTurn {
public:
	HoldTurn(ShardEndpoint& entry, int flags) {
		thread_ = std::thread([this, &entry, flags] { engine::sync_wait(hold(entry, flags)); });
		eventually([this] { return held_.load(); });
	}
	~HoldTurn() { release(); }
	void release() {
		release_.set();
		if (thread_.joinable()) {
			thread_.join();
		}
	}

private:
	task::task<void> hold(ShardEndpoint& entry, int flags) {
		auto turn = co_await entry.take_turn(flags);
		held_ = true;
		co_await release_;
	}

	std::atomic<bool> held_{false};
	task::event release_;
	std::thread thread_;
};

// Holds the next flush chore of an index until released.
class GateChores final : public engine::Chores {
public:
	explicit GateChores(std::unique_ptr<engine::Chores> inner) : inner_(std::move(inner)) { }

	void arm() {
		std::lock_guard<std::mutex> lk(m_);
		armed_ = true;
		gate_ = std::make_shared<task::event>();
	}
	bool entered() const { return entered_.load(); }
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
			{
				std::lock_guard<std::mutex> lk(m_);
				if (armed_) {
					armed_ = false;
					gate = gate_;
				}
			}
			if (gate) {
				entered_ = true;
				co_await *gate;
			}
		}
		co_await inner_->run(cls, std::move(fn));
	}

private:
	std::unique_ptr<engine::Chores> inner_;
	std::mutex m_;
	bool armed_ = false;
	std::shared_ptr<task::event> gate_;
	std::atomic<bool> entered_{false};
};

engine::Document make(const std::string& key) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.set_data(key);
	return doc;
}

// The node of the test process: a manager whose control loop runs on its own thread, serving on a port of its own,
// in a data directory of its own.
class RunningNode {
public:
	explicit RunningNode(const std::string& name = "node_test_data", const std::string& port = "18781", bool keep = false) {
		// What main() installs: a node takes SIGUSR2 to collect the threads' call stacks.
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);

		const auto root = std::filesystem::current_path() / name;
		if (!keep) {
			std::filesystem::remove_all(root);
		}
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";

		std::vector<std::string> args = {"xapiand", "--port", port, "--name", "nodetest", "-D", root.string(), "--shards", "1", "--processors", "1"};
		std::vector<char*> argv;
		for (auto& arg : args) {
			argv.push_back(arg.data());
		}
		opts = parseOptions(static_cast<int>(argv.size()), argv.data());
		// Short debounce times: the autocommit of a test write fires 200 ms after it.
		opts.committer_debounce_timeout = 200;
		opts.committer_debounce_busy_timeout = 300;
		opts.committer_debounce_min_force_timeout = 400;
		opts.committer_debounce_max_force_timeout = 400;

		XapiandManager::make();
		manager()->housekeeping_period = 100ms;
		thread_ = std::thread([this] {
			try {
				XapiandManager::manager()->run();
			} catch (const SystemExit& exit) {
				exit_code_ = exit.code;   // what main() ends the process with
			}
			exited_ = true;
		});
		ready_ = eventually([this] { return exited_.load() || XapiandManager::get_state() == XapiandManager::State::READY; }, 30s) && !exited_.load();
	}

	~RunningNode() {
		if (auto manager = XapiandManager::manager()) {
			const auto now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			manager->shutdown(now, now);
		}
		if (thread_.joinable()) {
			thread_.join();
		}
		XapiandManager::reset();
	}

	bool ready() const { return ready_; }

	// Whether run() ended (the node stopped itself).
	bool exited() const { return exited_.load(); }

	// Joins the control loop's thread once run() ended, and gives the status the process ends with.
	int join() {
		if (thread_.joinable()) {
			thread_.join();
		}
		return exit_code_.load();
	}

private:
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
	std::atomic<int> exit_code_{0};
};

const std::shared_ptr<XapiandManager>& manager() {
	return XapiandManager::manager();
}

// The endpoints of an index, as a request resolves them.
Endpoints endpoints_of(const std::string& path) {
	return engine::sync_wait(XapiandManager::resolve_index_endpoints(Endpoint(path), true));
}

}  // namespace


// Concurrent first uses of a schema read its metadata once: the first call loads, the others await it. The index's turn
// is held, so every call has started by the time the load can end.
static void test_concurrent_first_uses_of_a_schema_load_it_once() {
	Watchdog watchdog("one schema load");
	constexpr int N = 6;
	const auto endpoints = endpoints_of("schema_once");
	auto entry = pin_index(*manager()->database_pool, endpoints[0].path, WRITE);
	// A write applied and not yet refreshed: a read of the index takes its turn to refresh first.
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));
	const auto loads_before = manager()->schemas->loads();
	std::atomic<int> done{0};
	std::vector<std::shared_ptr<const MsgPack>> got(N);
	std::vector<std::thread> threads;
	{
		HoldTurn hold(entry.entry(), WRITE);
		for (int i = 0; i < N; ++i) {
			threads.emplace_back([&, i] {
				DatabaseHandler handler(endpoints, DB_OPEN);
				auto result = engine::sync_wait(manager()->schemas->get(&handler, nullptr));
				got[i] = std::get<0>(result);
				++done;
			});
		}
		CHECK(eventually([&] { return manager()->schemas->waiting() == N - 1; }));
		CHECK_EQ(manager()->schemas->loads() - loads_before, 1u);   // one load for six calls
		CHECK_EQ(done.load(), 0);
	}
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_EQ(done.load(), N);
	CHECK_EQ(manager()->schemas->loads() - loads_before, 2u);   // the metadata read, and the foreign schema it links to: once each
	if (manager()->schemas->loads() - loads_before != 2u) {
		std::fprintf(stderr, "  schema loads: %llu\n", static_cast<unsigned long long>(manager()->schemas->loads() - loads_before));
	}
	CHECK_GE(manager()->schemas->joined(), static_cast<std::uint64_t>(N - 1));
	for (int i = 1; i < N; ++i) {
		CHECK(got[i] == got[0]);   // the one schema object of the cache
	}
}

// The same for the settings of an index: the resolver reads them once.
static void test_concurrent_first_uses_of_index_settings_load_them_once() {
	Watchdog watchdog("one settings load");
	constexpr int N = 6;
	const auto reserved = engine::sync_wait(XapiandManager::resolve_index_endpoints(Endpoint(".xapiand/indexes"), true));
	auto entry = pin_index(*manager()->database_pool, reserved[0].path, WRITE);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("settings_once_marker")));   // so that a read of the index takes its turn
	const auto loads_before = manager()->index_settings_resolver->loads();
	std::atomic<int> done{0};
	std::vector<std::thread> threads;
	{
		HoldTurn hold(entry.entry(), WRITE);
		for (int i = 0; i < N; ++i) {
			threads.emplace_back([&] {
				auto settings = engine::sync_wait(XapiandManager::resolve_index_settings("settings_once"));
				(void)settings;
				++done;
			});
		}
		CHECK(eventually([&] { return manager()->index_settings_resolver->waiting() == N - 1; }));
		CHECK_EQ(manager()->index_settings_resolver->loads() - loads_before, 1u);
		CHECK_EQ(done.load(), 0);
	}
	for (auto& thread : threads) {
		thread.join();
	}
	CHECK_EQ(done.load(), N);
	CHECK_EQ(manager()->index_settings_resolver->loads() - loads_before, 1u);
}

// Late uses are served from the cache and load nothing.
static void test_a_cached_schema_is_not_loaded_again() {
	Watchdog watchdog("cached schema");
	const auto endpoints = endpoints_of("schema_cached");
	DatabaseHandler handler(endpoints, WRITE);
	engine::sync_wait(manager()->schemas->get(&handler, nullptr));
	const auto loads = manager()->schemas->loads();
	for (int i = 0; i < 5; ++i) {
		DatabaseHandler again(endpoints, DB_OPEN);
		engine::sync_wait(manager()->schemas->get(&again, nullptr));
	}
	CHECK_EQ(manager()->schemas->loads(), loads);
}


// The idle housekeeping is a task of the control loop that ticks on its period (100 ms here, a minute in a node).
static void test_the_housekeeping_ticks_on_the_control_loop() {
	Watchdog watchdog("housekeeping");
	const auto first = manager()->housekeeping_ticks();
	CHECK(eventually([&] { return manager()->housekeeping_ticks() >= first + 3; }, 5s));
}


// How late the control loop ran its probe between two snapshots.
struct Lag {
	std::uint64_t wakes;
	std::uint64_t p99_us;
	std::uint64_t over_50ms;
};

static Lag lag_between(const reactor::LagSnapshot& before, const reactor::LagSnapshot& after) {
	const auto d = after.since(before);
	Lag lag{};
	std::uint64_t total = 0;
	for (std::size_t i = 0; i < reactor::LagSnapshot::buckets; ++i) {
		total += d.count[i];
		if (reactor::LagSnapshot::lower_us(i) >= 50000) {
			lag.over_50ms += d.count[i];
		}
	}
	lag.wakes = total;
	lag.p99_us = d.quantile_upper_us(0.99);
	return lag;
}

// Keeps the control loop's probe at its 1 ms interval, as a loop that serves requests does.
class Pulse {
public:
	Pulse() : thread_([this] {
		while (!stop_) {
			manager()->control_lag().note_activity();
			std::this_thread::sleep_for(20ms);
		}
	}) { }
	~Pulse() {
		stop_ = true;
		thread_.join();
	}

private:
	std::atomic<bool> stop_{false};
	std::thread thread_;
};

// A commit that takes 400 ms runs as a task of the control loop (the autocommit), and the loop keeps answering its
// probe the whole time: nothing on it waits for the commit.
static void test_the_control_loop_is_free_during_a_slow_commit() {
	Watchdog watchdog("slow commit");
	Pulse pulse;
	const auto endpoints = endpoints_of("slow_commit");
	auto entry = pin_index(*manager()->database_pool, endpoints[0].path, WRITE);
	auto gate = std::make_unique<GateChores>(A::take_chores(entry.entry()));
	auto* raw = gate.get();
	A::set_chores(entry.entry(), std::move(gate));
	Shard shard = use_index(entry, WRITE);
	engine::sync_wait(shard.ready());
	engine::sync_wait(shard.add_document(make("K1")));   // schedules the autocommit
	raw->arm();                                           // and the commit it starts waits for release()
	CHECK(entry->is_modified());

	const auto before = manager()->control_lag().snapshot();
	CHECK(eventually([&] { return raw->entered(); }));   // the autocommit's commit chore is held
	std::this_thread::sleep_for(400ms);
	CHECK(entry->is_modified());                          // still not committed
	const auto during = manager()->control_lag().snapshot();
	raw->release();
	CHECK(eventually([&] { return !entry->is_modified(); }));

	const auto lag = lag_between(before, during);
	CHECK_GT(lag.wakes, 150u);              // the probe kept waking, about once a millisecond
	CHECK_EQ(lag.over_50ms, 0u);            // never 50 ms late
	CHECK_LE(lag.p99_us, 16384u);           // and p99 under 16 ms
}

// A schema load that waits 400 ms for its index runs as a task of the control loop, and the loop serves its other
// tasks and its probe meanwhile.
static void test_the_control_loop_is_free_during_a_schema_load() {
	Watchdog watchdog("slow schema load");
	Pulse pulse;
	const auto endpoints = endpoints_of("slow_schema");
	auto entry = pin_index(*manager()->database_pool, endpoints[0].path, WRITE);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));   // applied and not refreshed: the schema read takes the turn
	std::atomic<bool> loaded{false};
	std::atomic<int> sentinels{0};
	const auto before = manager()->control_lag().snapshot();
	{
		HoldTurn hold(entry.entry(), WRITE);
		manager()->spawn_control([](const Endpoints& endpoints, std::atomic<bool>& loaded) -> task::task<void> {
			DatabaseHandler handler(endpoints, DB_OPEN);
			co_await manager()->schemas->get(&handler, nullptr);
			loaded = true;
		}(endpoints, loaded));
		CHECK(eventually([&] { return manager()->schemas->waiting() + manager()->schemas->loads() >= 1; }));
		// Other work on the loop is served while the load waits.
		for (int i = 0; i < 20; ++i) {
			manager()->spawn_control([](std::atomic<int>& sentinels) -> task::task<void> {
				++sentinels;
				co_return;
			}(sentinels));
			std::this_thread::sleep_for(20ms);
		}
		CHECK_EQ(sentinels.load(), 20);
		CHECK_FALSE(loaded.load());
	}
	const auto during = manager()->control_lag().snapshot();
	CHECK(eventually([&] { return loaded.load(); }));

	const auto lag = lag_between(before, during);
	CHECK_GT(lag.wakes, 150u);
	CHECK_EQ(lag.over_50ms, 0u);
	CHECK_LE(lag.p99_us, 16384u);
}


// ---- a shutdown that cannot finish a write in its bound ----

// The node reports ready a moment before its listener accepts: connect again for up to 5 s.
static int connect_to(int port) {
	for (int attempt = 0; attempt < 100; ++attempt) {
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
			return fd;
		}
		::close(fd);
		std::this_thread::sleep_for(50ms);
	}
	return -1;
}

// One request on a connection of its own; the whole response (the server closes after it).
static std::string http_call(int port, const std::string& method, const std::string& path, const std::string& body = "") {
	const int fd = connect_to(port);
	if (fd < 0) {
		return "";
	}
	timeval timeout{10, 0};
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	const auto request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
	::send(fd, request.data(), request.size(), 0);
	std::string response;
	char buf[4096];
	ssize_t n;
	while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
		response.append(buf, static_cast<std::size_t>(n));
	}
	::close(fd);
	return response;
}

static constexpr int abandon_port = 18782;
static constexpr int acked_writes = 5;

struct AbandonSpec {
	std::chrono::milliseconds bound = 2000ms;
	bool request = true;          // a PUT waits for the held turn (otherwise only the registry has work left)
	bool acked = false;           // acknowledged writes to another index before the shutdown
	bool second_signal = false;   // a second SIGTERM 1 s into the shutdown
	std::chrono::milliseconds release_after{0};   // the held turn is released this long into the shutdown (0: never)
};

// The child: a node whose index turn is held by a thread of its own, a PUT waiting for that turn, and a shutdown
// with a bound. The shutdown reports what it abandons and ends the process with status 75.
[[noreturn]] static void abandon_child(const char* log_path, AbandonSpec spec) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);   // freopen to a file leaves stderr buffered, and abort() does not flush it
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	RunningNode node("node_abandon_data", std::to_string(abandon_port));
	if (!node.ready()) {
		std::_Exit(2);
	}
	manager()->shutdown_write_bound = spec.bound;
	auto entry = pin_index(*manager()->database_pool, endpoints_of("abandon")[0].path, WRITE);
	HoldTurn hold(entry.entry(), WRITE);

	if (spec.request) {
		const int fd = connect_to(abandon_port);
		if (fd < 0) {
			std::_Exit(3);
		}
		const char* request = "PUT /abandon/1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 9\r\n\r\n{\"a\":\"b\"}";
		::send(fd, request, std::strlen(request), 0);
		if (!eventually([] { return manager()->http_service->reactor(0).gate().inflight.load() == 1; })) {
			std::_Exit(4);
		}
		std::this_thread::sleep_for(300ms);   // the write is waiting for its turn
	}

	// Acknowledged: the answer was a 2xx. The shutdown follows at once, before the autocommit (200 ms) fires.
	for (int i = 1; spec.acked && i <= acked_writes; ++i) {
		const auto response = http_call(abandon_port, "PUT", "/acked/" + std::to_string(i), "{\"marker\":\"ack-" + std::to_string(i) + "\"}");
		if (response.rfind("HTTP/1.1 2", 0) != 0) {
			std::_Exit(6);
		}
	}

	const auto now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	L(-LOG_NOTICE, NOTICE_COL, "SHUTDOWN_AT {}", now);
	manager()->shutdown(now, now);
	if (spec.second_signal) {
		std::this_thread::sleep_for(1s);
		manager()->signal_sig(SIGTERM);
	}
	if (spec.release_after.count() != 0) {
		std::this_thread::sleep_for(spec.release_after);
		hold.release();
		entry.release();   // the catalog closes the index only once nobody pins it
		if (!eventually([&] { return node.exited(); }, 60s)) {
			std::_Exit(7);
		}
		const int code = node.join();
		Logging::finish();
		Logging::join();
		std::_Exit(code);
	}
	std::this_thread::sleep_for(60s);   // the shutdown ends the process
	std::_Exit(5);
}

// The restart: the data directory of the abandoned node, and a read of what it acknowledged.
[[noreturn]] static void restart_child(const char* log_path, bool expect_put = false) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);   // freopen to a file leaves stderr buffered, and abort() does not flush it
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	RunningNode node("node_abandon_data", std::to_string(abandon_port), true);
	if (!node.ready()) {
		std::_Exit(2);
	}
	int missing = 0;
	for (int i = 1; !expect_put && i <= acked_writes; ++i) {
		const auto response = http_call(abandon_port, "GET", "/acked/" + std::to_string(i));
		if (response.rfind("HTTP/1.1 200", 0) != 0 || response.find("ack-" + std::to_string(i)) == std::string::npos) {
			std::fprintf(stderr, "MISSING acknowledged write %d: %s\n", i, response.substr(0, 60).c_str());
			++missing;
		}
	}
	if (expect_put) {
		const auto response = http_call(abandon_port, "GET", "/abandon/1");
		if (response.rfind("HTTP/1.1 200", 0) != 0) {
			std::fprintf(stderr, "MISSING the write that was waiting at the shutdown: %s\n", response.substr(0, 60).c_str());
			++missing;
		}
	}
	std::fflush(nullptr);
	std::_Exit(missing == 0 ? 0 : 10 + missing);
}

static std::string read_file(const char* path) {
	std::string out;
	if (FILE* f = std::fopen(path, "r")) {
		char buf[4096];
		std::size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
			out.append(buf, n);
		}
		std::fclose(f);
	}
	return out;
}

struct ChildResult {
	bool exited = false;
	int code = -1;
	std::chrono::steady_clock::duration took{};
};

// Forks a child that runs `body` and waits for it up to `limit`. Run before this process has threads.
template <typename F>
static ChildResult run_child(F&& body, std::chrono::seconds limit) {
	std::fflush(nullptr);
	const pid_t pid = ::fork();
	if (pid == 0) {
		body();
		std::_Exit(99);
	}
	CHECK(pid > 0);
	int status = 0;
	const auto began = std::chrono::steady_clock::now();
	pid_t done = 0;
	while ((done = ::waitpid(pid, &status, WNOHANG)) == 0 && std::chrono::steady_clock::now() - began < limit) {
		std::this_thread::sleep_for(50ms);
	}
	if (done == 0) {
		::kill(pid, SIGKILL);
		::waitpid(pid, &status, 0);
	}
	ChildResult result;
	result.took = std::chrono::steady_clock::now() - began;
	result.exited = done == pid && WIFEXITED(status);
	result.code = result.exited ? WEXITSTATUS(status) : -1;
	if (WIFSIGNALED(status)) {
		std::fprintf(stderr, "  the child ended with signal %d\n", WTERMSIG(status));
	}
	return result;
}

// How long the stop took: from the child's SHUTDOWN_AT line to its "Exiting" line (the logs' own clock, so the
// process teardown after it, slow under a sanitizer, is not counted).
static long long stop_ms(const std::string& log) {
	const auto at = log.find("SHUTDOWN_AT ");
	const auto exiting = log.find("Exiting with unfinished writes abandoned");
	if (at == std::string::npos || exiting == std::string::npos) {
		return -1;
	}
	const auto line_start = log.rfind('\n', exiting);
	const auto stamp = log.substr(line_start == std::string::npos ? 0 : line_start + 1);
	const auto date = stamp.find("20");   // "▍2026-11-02 07:54:36.095 ..."
	std::tm tm{};
	if (date == std::string::npos || strptime(stamp.c_str() + date, "%Y-%m-%d %H:%M:%S", &tm) == nullptr) {
		return -1;
	}
	tm.tm_isdst = -1;
	const auto millis = std::stoll(stamp.substr(date + 20, 3));
	const long long exiting_ms = static_cast<long long>(std::mktime(&tm)) * 1000 + millis;
	return exiting_ms - std::stoll(log.substr(at + 12));
}

// A line of the report that names the abandoned index's key and its one pin held ("'<path>/abandon' (state, 1 pin, pinned)").
static bool names_the_abandoned_index(const std::string& log) {
	std::size_t at = 0;
	while (at < log.size()) {
		auto end = log.find('\n', at);
		if (end == std::string::npos) {
			end = log.size();
		}
		const auto line = log.substr(at, end - at);
		if (line.find("abandon' (") != std::string::npos && line.find(", 1 pin, pinned)") != std::string::npos) {
			return true;
		}
		at = end + 1;
	}
	return false;
}

// The shutdown abandons what it cannot finish in its bound and exits 75 (EX_TEMPFAIL). Every write acknowledged
// before it is there after a restart of the data directory. The stop is one bound long, not one for the service and
// another for the registry. The bound is under the reserve, so the HTTP stop gets the shortest grace and ends the
// write that waits for its turn.
static void test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound() {
	const char* log_path = "node_abandon.log";
	AbandonSpec spec;
	spec.acked = true;
	const auto result = run_child([&] { abandon_child(log_path, spec); }, 90s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 75);   // EX_TEMPFAIL
	const auto log = read_file(log_path);
	const auto stopped_in = stop_ms(log);
	CHECK_GE(stopped_in, 0);
	CHECK_LT(stopped_in, spec.bound.count() + 800);
	if (stopped_in >= spec.bound.count() + 800) {
		std::fprintf(stderr, "%s\n", log.c_str());   // the timeline of the stop
	}   // one bound (and the log flush), where two bounds took more than twice the bound
	CHECK(log.find("Shutdown abandoned the indexes that are still in use") != std::string::npos);
	CHECK(names_the_abandoned_index(log));   // the turn is held under a pin, which the catalog names
	CHECK(log.find("unfinished item") == std::string::npos);   // the HTTP stop ended in its grace: the write was destroyed, not abandoned
	CHECK(log.find("Exiting with unfinished writes abandoned") != std::string::npos);

	// The restart reads back what the abandoned shutdown had acknowledged.
	const auto restart = run_child([&] { restart_child("node_restart.log"); }, 90s);
	CHECK(restart.exited);
	CHECK_EQ(restart.code, 0);
	if (restart.code != 0) {
		std::fprintf(stderr, "%s\n", read_file("node_restart.log").c_str());
	}
}

// D16's arithmetic: the HTTP service's grace is half of what the budget has left after the reserve for the
// registry, so its stop (a grace, the force, a grace) ends before the budget does; with little left, the shortest.
static void test_the_http_grace_follows_the_time_left_in_the_budget() {
	const auto reserve = 5s;
	CHECK_EQ(XapiandManager::http_shutdown_grace(XapiandManager::default_shutdown_write_bound()).count(), 15000);
	for (auto left : {35000ms, 8000ms, 6000ms, 5100ms}) {
		const auto grace = XapiandManager::http_shutdown_grace(left);
		CHECK_LE(2 * grace, std::max(left - reserve, 2 * 250ms));
		CHECK_GE(grace, 250ms);
	}
	CHECK_EQ(XapiandManager::http_shutdown_grace(0ms).count(), 250);
	CHECK_EQ(XapiandManager::http_shutdown_grace(20000ms).count(), 7500);
}

// A write admitted before the stop that waits for its turn beyond the old 5 s grace (and the 10 s the stop took) is
// let finish while the budget has time: its turn comes 14 s into a 25 s budget, past the first grace (10 s, where the
// connections of what is left are cut) and inside the second. The write applies, the node exits 0 with nothing
// abandoned, and the restart reads the write.
static void test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time() {
	const char* log_path = "node_late_write.log";
	AbandonSpec spec;
	spec.bound = 25s;
	spec.release_after = 14s;
	const auto result = run_child([&] { abandon_child(log_path, spec); }, 90s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 0);
	const auto log = read_file(log_path);
	CHECK(log.find("Shutdown abandoned") == std::string::npos);
	CHECK(log.find("Exiting with unfinished writes abandoned") == std::string::npos);
	if (result.code != 0) {
		std::fprintf(stderr, "%s\n", log.c_str());
	}

	const auto restart = run_child([&] { restart_child("node_late_restart.log", true); }, 90s);
	CHECK(restart.exited);
	CHECK_EQ(restart.code, 0);
	if (restart.code != 0) {
		std::fprintf(stderr, "%s\n", read_file("node_late_restart.log").c_str());
	}
}

// The service stops at once; the registry cannot close an index whose turn a thread holds, and its close ends at the
// bound like the rest of the stop (it used to wait without a limit).
static void test_a_registry_that_cannot_close_ends_the_stop_at_the_bound() {
	const char* log_path = "node_registry.log";
	AbandonSpec spec;
	spec.request = false;
	const auto result = run_child([&] { abandon_child(log_path, spec); }, 90s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 75);
	const auto log = read_file(log_path);
	const auto stopped_in = stop_ms(log);
	CHECK_GE(stopped_in, spec.bound.count() - 100);   // it waited for the indexes
	CHECK_LT(stopped_in, spec.bound.count() + 800);
	if (stopped_in >= spec.bound.count() + 800) {
		std::fprintf(stderr, "%s\n", log.c_str());   // the timeline of the stop
	}
	CHECK(log.find("Shutdown abandoned the indexes that are still in use") != std::string::npos);
	CHECK(names_the_abandoned_index(log));   // the turn is held under a pin, which the catalog names
	CHECK(log.find("unfinished item") == std::string::npos);   // the service itself stopped
	CHECK(log.find("Exiting with unfinished writes abandoned") != std::string::npos);
}

// A node that serves a write and is told to stop: the stop's steps run in the order the note states, and the status is 0.
[[noreturn]] static void ordered_stop_child(const char* log_path) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);   // freopen to a file leaves stderr buffered, and abort() does not flush it
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	{
		RunningNode node("node_order_data", std::to_string(abandon_port));
		if (!node.ready()) {
			std::_Exit(2);
		}
		const auto response = http_call(abandon_port, "PUT", "/ordered/1", "{\"a\":\"b\"}");
		if (response.rfind("HTTP/1.1 2", 0) != 0) {
			std::_Exit(3);
		}
	}   // the node's destructor stops it: the shutdown, then join()
	Logging::finish();
	Logging::join();
	std::_Exit(0);
}

// The stop: the HTTP service, then the autocommits, the indexes, the write-ahead logs and the fsync threads (the
// logs after the indexes, since closing an index appends to its log).
static void test_the_stop_runs_in_its_stated_order() {
	const char* log_path = "node_order.log";
	const auto result = run_child([&] { ordered_stop_child(log_path); }, 60s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 0);
	const auto log = read_file(log_path);
	std::size_t at = 0;
	for (const char* step : {"Stop 1: the HTTP service", "Stop 2: the autocommits", "Stop 3: the indexes", "Stop 4: the write-ahead logs", "Stop 5: the fsync threads"}) {
		const auto found = log.find(step, at);
		CHECK(found != std::string::npos);
		if (found == std::string::npos) {
			std::fprintf(stderr, "  step \"%s\" is missing or out of order in:\n%s\n", step, log.c_str());
			break;
		}
		CHECK_EQ(log.rfind(step), found);   // each step is logged once
		at = found;
	}
}

// A second SIGTERM during the bound ends the wait and the registry's, and the process exits 75 at once.
static void test_a_second_signal_ends_the_shutdowns_wait() {
	const char* log_path = "node_signal.log";
	AbandonSpec spec;
	spec.bound = 60s;
	spec.second_signal = true;
	const auto result = run_child([&] { abandon_child(log_path, spec); }, 90s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 75);
	CHECK_LT(result.took, 20s);   // not the 30 s the write waits for its turn, nor the 60 s bound
	const auto log = read_file(log_path);
	CHECK(log.find("stopped by a second signal") != std::string::npos);
	CHECK(log.find("Exiting with unfinished writes abandoned") != std::string::npos);
}

// A node whose port is taken cannot set itself up: it ends the process with a status, and says why.
[[noreturn]] static void taken_port_child(const char* log_path) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);   // freopen to a file leaves stderr buffered, and abort() does not flush it
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	const int taken = ::socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(18783);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (::bind(taken, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(taken, 8) != 0) {
		std::_Exit(2);
	}
	RunningNode node("node_taken_data", "18783");
	if (!eventually([&] { return node.exited(); }, 20s)) {
		std::_Exit(5);   // still up, serving nothing
	}
	const int code = node.join();
	Logging::finish();
	Logging::join();
	std::_Exit(code);
}

static void test_a_node_on_a_taken_port_ends_the_process() {
	const char* log_path = "node_taken.log";
	const auto result = run_child([&] { taken_port_child(log_path); }, 60s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 70);   // EX_SOFTWARE
	CHECK_LT(result.took, 15s);
	const auto log = read_file(log_path);
	CHECK(log.find("Address already in use") != std::string::npos);
	CHECK(log.find("The node setup failed") != std::string::npos);
}


// The resolver's lookup of an index's settings, run as a task of the control loop, waits 400 ms for the reserved index,
// and the loop keeps serving meanwhile.
static void test_the_control_loop_is_free_during_a_settings_load() {
	Watchdog watchdog("slow settings load");
	Pulse pulse;
	const auto reserved = engine::sync_wait(XapiandManager::resolve_index_endpoints(Endpoint(".xapiand/indexes"), true));
	auto entry = pin_index(*manager()->database_pool, reserved[0].path, WRITE);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("slow_settings_marker")));   // so that a read of the index takes its turn
	std::atomic<bool> resolved{false};
	std::atomic<int> sentinels{0};
	const auto before = manager()->control_lag().snapshot();
	{
		HoldTurn hold(entry.entry(), WRITE);
		manager()->spawn_control([](std::atomic<bool>& resolved) -> task::task<void> {
			co_await XapiandManager::resolve_index_settings("slow_settings");
			resolved = true;
		}(resolved));
		CHECK(eventually([&] { return manager()->index_settings_resolver->loads() >= 1; }));
		for (int i = 0; i < 20; ++i) {
			manager()->spawn_control([](std::atomic<int>& sentinels) -> task::task<void> {
				++sentinels;
				co_return;
			}(sentinels));
			std::this_thread::sleep_for(20ms);
		}
		CHECK_EQ(sentinels.load(), 20);
		CHECK_FALSE(resolved.load());
	}
	const auto during = manager()->control_lag().snapshot();
	CHECK(eventually([&] { return resolved.load(); }));

	const auto lag = lag_between(before, during);
	CHECK_GT(lag.wakes, 150u);
	CHECK_EQ(lag.over_50ms, 0u);
	CHECK_LE(lag.p99_us, 16384u);
}


// A search over several indexes skips one that does not exist (a list or a pattern may name it): its pin's not_found
// comes at the use, inside the read, where the batch skips it, not at lock_shard's pin.
static void test_a_search_batch_skips_a_missing_index() {
	Watchdog watchdog("a search batch skips a missing index");
	auto endpoints = endpoints_of("batch_present");
	{
		auto entry = pin_index(*manager()->database_pool, endpoints[0].path, WRITE_QUIET);
		Shard shard = use_index(entry, WRITE_QUIET);
		engine::sync_wait(shard.add_document(make("K1")));
		engine::sync_wait(shard.commit());
	}
	const auto missing = endpoints_of("batch_missing");
	endpoints.add(missing[0]);
	DatabaseHandler handler(endpoints, DB_OPEN);
	const auto result = engine::sync_wait(handler.get_mset(engine::Query::term("f", "body")));   // its batches
	CHECK_EQ(result.hits.size(), 1u);
	CHECK_FALSE(std::filesystem::exists(missing[0].path));
}


// A write that may not create its index, to an index that does not exist, is not_found (a 404), as a read of it
// is: its use has no pin, and that use names the missing index rather than a closed one.
static void test_a_write_to_a_missing_index_is_not_found() {
	Watchdog watchdog("a write to a missing index is not_found");
	const auto endpoints = endpoints_of("write_missing");
	DatabaseHandler handler(endpoints, DB_OPEN | DB_WRITABLE);
	engine::errc kind = engine::errc::internal;
	try {
		engine::sync_wait(handler.set_metadata(std::string("foo"), std::string("bar")));
	} catch (const engine::Error& error) {
		kind = error.kind;
	}
	CHECK(kind == engine::errc::not_found);
	CHECK_FALSE(std::filesystem::exists(endpoints[0].path));
}


// A search that names one index twice pins it once and searches it at both positions: a second pin of a key in one
// request would wait for itself while its index is replaced. The read waits for the held turn with every pin taken.
static void test_a_search_batch_pins_a_repeated_index_once() {
	Watchdog watchdog("a search batch pins a repeated index once");
	auto& pool = *manager()->database_pool;
	auto endpoints = endpoints_of("batch_repeated");
	const std::string key = endpoints[0].path;
	auto entry = pin_index(pool, key, WRITE_QUIET);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));
	engine::sync_wait(shard.commit());
	const auto once = engine::sync_wait(DatabaseHandler(endpoints, DB_OPEN).get_mset(engine::Query::term("f", "body"))).hits.size();
	Endpoint again = endpoints[0];
	again.node_name += "_again";
	endpoints.add(again);
	REQUIRE(endpoints.size() == 2u);
	const auto pins = [&] {
		for (const auto& slot : pool.catalog().snapshot()) {
			if (slot.key == key) {
				return slot.pins;
			}
		}
		return std::uint64_t{0};
	};
	const auto before = pins();
	engine::sync_wait(shard.add_document(make("K2")));   // applied, not refreshed: the search's read takes the turn
	std::size_t twice = 0;
	{
		HoldTurn hold(entry.entry(), WRITE_QUIET);
		std::thread search([&] {
			DatabaseHandler handler(endpoints, DB_OPEN);
			twice = engine::sync_wait(handler.get_mset(engine::Query::term("f", "body"))).hits.size();
		});
		CHECK(eventually([&] { return pins() > before; }, 3s));
		std::this_thread::sleep_for(200ms);   // a second pin would come before the read, which waits for the turn
		CHECK_EQ(pins(), before + 1);
		hold.release();
		search.join();
	}
	const auto after = engine::sync_wait(DatabaseHandler(endpoints_of("batch_repeated"), DB_OPEN).get_mset(engine::Query::term("f", "body"))).hits.size();
	CHECK_EQ(once, 1u);
	CHECK_EQ(twice, 2 * after);
}


// A search's batch waits for its indexes with the request's token (its budget's): a cancelled search stops waiting.
// Here its read waits for a turn another task holds.
static void test_a_cancelled_search_stops_waiting_for_its_index() {
	Watchdog watchdog("a cancelled search stops waiting");
	auto& pool = *manager()->database_pool;
	const auto endpoints = endpoints_of("cancelled_search");
	auto entry = pin_index(pool, endpoints[0].path, WRITE_QUIET);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));   // applied, not refreshed: the search's read takes the turn
	HoldTurn hold(entry.entry(), WRITE_QUIET);
	task::cancel_source cancel;
	task::budget budget(std::chrono::milliseconds(10), cancel.token());
	std::atomic<bool> ended{false}, cancelled{false};
	std::thread search([&] {
		DatabaseHandler handler(endpoints, DB_OPEN);
		handler.budget = &budget;
		try {
			engine::sync_wait(handler.get_mset(engine::Query::term("f", "body")));
		} catch (const task::cancelled&) {
			cancelled = true;
		}
		ended = true;
	});
	std::this_thread::sleep_for(100ms);
	CHECK_FALSE(ended.load());
	cancel.request();
	CHECK(eventually([&] { return ended.load(); }, 3s));
	hold.release();
	search.join();
	CHECK_TRUE(cancelled.load());
}


// A request's lock_shard waits for its index with the request's token: a cancelled request stops waiting with
// task::cancelled. Here the index is being replaced, which waits for the pin still held.
static void test_a_cancelled_request_stops_waiting_for_its_index() {
	Watchdog watchdog("a cancelled request stops waiting");
	auto& pool = *manager()->database_pool;
	const auto endpoints = endpoints_of("cancelled_request");
	std::optional<IndexPin> holder(pin_index(pool, endpoints[0].path, WRITE_QUIET));
	pin_index(pool, endpoints[0].path, WRITE_QUIET).reopen();   // the replace waits for the holder
	task::cancel_source cancel;
	std::atomic<bool> ended{false}, cancelled{false};
	std::thread request([&] {
		lock_shard lk_shard(endpoints[0], WRITE_QUIET, cancel.token());
		try {
			engine::sync_wait(lk_shard.pin());
		} catch (const task::cancelled&) {
			cancelled = true;
		}
		ended = true;
	});
	std::this_thread::sleep_for(100ms);
	CHECK_FALSE(ended.load());
	cancel.request();
	request.join();
	CHECK_TRUE(cancelled.load());
	holder.reset();
	CHECK(static_cast<bool>(pin_index(pool, endpoints[0].path, WRITE_QUIET)));
}


// An autocommit whose turn does not come is armed again (x-pool D8): the write it could not commit is committed once
// the turn is let go, with no other write to schedule it.
static void test_a_busy_autocommit_comes_back_for_its_write() {
	Watchdog watchdog("busy autocommit");
	auto& pool = *manager()->database_pool;
	const auto endpoints = endpoints_of("busy_autocommit");
	auto entry = pin_index(pool, endpoints[0].path, WRITE);
	Shard shard = use_index(entry, WRITE);
	engine::sync_wait(shard.ready());
	pool.set_turn_timeout(0.1);
	engine::sync_wait(shard.add_document(make("K1")));   // schedules the autocommit (200 ms)
	{
		HoldTurn hold(entry.entry(), WRITE);
		std::this_thread::sleep_for(700ms);   // its commit times out with busy, at least twice
		CHECK(entry->is_modified());
	}
	CHECK(eventually([&] { return !entry->is_modified(); }, 3s));
	pool.set_turn_timeout(DB_TIMEOUT);
}

int main(int argc, char** argv) {
	const std::string filter = argc > 1 ? argv[1] : "";
	auto wanted = [&](const char* name) { return filter.empty() || std::string(name).find(filter) != std::string::npos; };

	// First, while this process has no threads: each child forks from a clean process.
	struct Forked {
		const char* name;
		void (*fn)();
	};
	const Forked forked[] = {
		{"test_a_node_on_a_taken_port_ends_the_process", test_a_node_on_a_taken_port_ends_the_process},
		{"test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound", test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound},
		{"test_the_stop_runs_in_its_stated_order", test_the_stop_runs_in_its_stated_order},
		{"test_a_registry_that_cannot_close_ends_the_stop_at_the_bound", test_a_registry_that_cannot_close_ends_the_stop_at_the_bound},
		{"test_the_http_grace_follows_the_time_left_in_the_budget", test_the_http_grace_follows_the_time_left_in_the_budget},
		{"test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time", test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time},
		{"test_a_second_signal_ends_the_shutdowns_wait", test_a_second_signal_ends_the_shutdowns_wait},
	};
	for (const auto& f : forked) {
		if (std::getenv("SKIP_FORKED") != nullptr || !wanted(f.name)) {
			continue;
		}
		const int before = testh::failures;
		f.fn();
		std::fprintf(stderr, "%s %s\n", testh::failures == before ? "ok  " : "FAIL", f.name);
	}

	if (std::getenv("NODE_LOG") != nullptr) {
		Logging::handlers.push_back(std::make_unique<StderrLogger>());
	}
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	struct Case {
		const char* name;
		void (*fn)();
	};
	const Case cases[] = {
		{"test_concurrent_first_uses_of_a_schema_load_it_once", test_concurrent_first_uses_of_a_schema_load_it_once},
		{"test_concurrent_first_uses_of_index_settings_load_them_once", test_concurrent_first_uses_of_index_settings_load_them_once},
		{"test_a_cached_schema_is_not_loaded_again", test_a_cached_schema_is_not_loaded_again},
		{"test_the_housekeeping_ticks_on_the_control_loop", test_the_housekeeping_ticks_on_the_control_loop},
		{"test_the_control_loop_is_free_during_a_slow_commit", test_the_control_loop_is_free_during_a_slow_commit},
		{"test_the_control_loop_is_free_during_a_schema_load", test_the_control_loop_is_free_during_a_schema_load},
		{"test_the_control_loop_is_free_during_a_settings_load", test_the_control_loop_is_free_during_a_settings_load},
		{"test_a_search_batch_skips_a_missing_index", test_a_search_batch_skips_a_missing_index},
		{"test_a_write_to_a_missing_index_is_not_found", test_a_write_to_a_missing_index_is_not_found},
		{"test_a_search_batch_pins_a_repeated_index_once", test_a_search_batch_pins_a_repeated_index_once},
		{"test_a_cancelled_request_stops_waiting_for_its_index", test_a_cancelled_request_stops_waiting_for_its_index},
		{"test_a_cancelled_search_stops_waiting_for_its_index", test_a_cancelled_search_stops_waiting_for_its_index},
		{"test_a_busy_autocommit_comes_back_for_its_write", test_a_busy_autocommit_comes_back_for_its_write},
	};
	for (const auto& c : cases) {
		if (!wanted(c.name)) {
			continue;
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
