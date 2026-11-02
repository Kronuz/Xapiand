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
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
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
#include "database/pool.h"
#include "database/schemas_lru.h"
#include "database/shard.h"
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

using namespace std::chrono_literals;

struct PoolTestAccess {
	static void set_chores(ShardEndpoint& entry, std::unique_ptr<engine::Chores> chores) { entry.chores = std::move(chores); }
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
	explicit RunningNode(const std::string& name = "node_test_data", const std::string& port = "18781") {
		// What main() installs: a node takes SIGUSR2 to collect the threads' call stacks.
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);

		const auto root = std::filesystem::current_path() / name;
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";

		std::vector<std::string> args = {"xapiand", "--port", port, "--name", "nodetest", "-D", root.string(), "--shards", "1", "--http-servers", "1"};
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
		thread_ = std::thread([] { XapiandManager::manager()->run(); });
		ready_ = eventually([] { return XapiandManager::get_state() == XapiandManager::State::READY; }, 30s);
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

private:
	std::thread thread_;
	bool ready_ = false;
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
	auto entry = manager()->database_pool->spawn(endpoints[0]);
	// A write applied and not yet refreshed: a read of the index takes its turn to refresh first.
	Shard shard(*entry.get(), WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));
	const auto loads_before = manager()->schemas->loads();
	std::atomic<int> done{0};
	std::vector<std::shared_ptr<const MsgPack>> got(N);
	std::vector<std::thread> threads;
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	auto entry = manager()->database_pool->spawn(reserved[0]);
	Shard shard(*entry.get(), WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("settings_once_marker")));   // so that a read of the index takes its turn
	const auto loads_before = manager()->index_settings_resolver->loads();
	std::atomic<int> done{0};
	std::vector<std::thread> threads;
	{
		HoldTurn hold(*entry.get(), WRITE);
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
	auto entry = manager()->database_pool->spawn(endpoints[0]);
	auto gate = std::make_unique<GateChores>(A::take_chores(*entry.get()));
	auto* raw = gate.get();
	A::set_chores(*entry.get(), std::move(gate));
	Shard shard(*entry.get(), WRITE);
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
	auto entry = manager()->database_pool->spawn(endpoints[0]);
	Shard shard(*entry.get(), WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));   // applied and not refreshed: the schema read takes the turn
	std::atomic<bool> loaded{false};
	std::atomic<int> sentinels{0};
	const auto before = manager()->control_lag().snapshot();
	{
		HoldTurn hold(*entry.get(), WRITE);
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

// The child: a node whose index turn is held by a thread of its own, a PUT waiting for that turn, and a shutdown
// with a bound of 1.5 s. The shutdown reports what it abandons and ends the process with status 75.
[[noreturn]] static void abandon_child(const char* log_path) {
	std::freopen(log_path, "w", stderr);
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	RunningNode node("node_abandon_data", "18782");
	if (!node.ready()) {
		std::_Exit(2);
	}
	manager()->shutdown_write_bound = 1500ms;
	auto entry = manager()->database_pool->spawn(endpoints_of("abandon")[0]);
	HoldTurn hold(*entry.get(), WRITE);

	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(18782);
	::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
		std::_Exit(3);
	}
	const char* request = "PUT /abandon/1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 9\r\n\r\n{\"a\":\"b\"}";
	::send(fd, request, std::strlen(request), 0);
	if (!eventually([] { return manager()->http_service->reactor(0).gate().inflight.load() == 1; })) {
		std::_Exit(4);
	}
	std::this_thread::sleep_for(300ms);   // the write is waiting for its turn

	const auto now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	manager()->shutdown(now, now);
	std::this_thread::sleep_for(60s);   // the shutdown ends the process
	std::_Exit(5);
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

// Run before this process has threads: the child is a node of its own.
static void test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound() {
	const char* log_path = "node_abandon.log";
	std::fflush(nullptr);
	const pid_t pid = ::fork();
	if (pid == 0) {
		abandon_child(log_path);
	}
	REQUIRE(pid > 0);
	int status = 0;
	const auto began = std::chrono::steady_clock::now();
	pid_t done = 0;
	while ((done = ::waitpid(pid, &status, WNOHANG)) == 0 && std::chrono::steady_clock::now() - began < 90s) {
		std::this_thread::sleep_for(50ms);
	}
	if (done == 0) {
		::kill(pid, SIGKILL);
		::waitpid(pid, &status, 0);
	}
	const auto took = std::chrono::steady_clock::now() - began;
	CHECK(done == pid);
	if (WIFSIGNALED(status)) {
		std::fprintf(stderr, "  the child ended with signal %d\n", WTERMSIG(status));
	}
	CHECK(WIFEXITED(status));
	CHECK_EQ(WEXITSTATUS(status), 75);   // EX_TEMPFAIL
	CHECK_LT(took, 45s);                 // the bound, not the 30 s turn wait twice over
	const auto log = read_file(log_path);
	CHECK(log.find("Shutdown abandoned 2 unfinished items") != std::string::npos);
	CHECK(log.find("1 request in flight on http0") != std::string::npos);
	CHECK(log.find("turn held") != std::string::npos);
	CHECK(log.find("Exiting with unfinished writes abandoned") != std::string::npos);
}


int main(int argc, char** argv) {
	const std::string filter = argc > 1 ? argv[1] : "";
	auto wanted = [&](const char* name) { return filter.empty() || std::string(name).find(filter) != std::string::npos; };

	// First, while this process has no threads: the child forks from a clean process.
	if (std::getenv("SKIP_ABANDON") == nullptr && wanted("test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound")) {
		const int before = testh::failures;
		test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound();
		std::fprintf(stderr, "%s test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound\n", testh::failures == before ? "ok  " : "FAIL");
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
		{"test_the_control_loop_is_free_during_a_slow_commit", test_the_control_loop_is_free_during_a_slow_commit},
		{"test_the_control_loop_is_free_during_a_schema_load", test_the_control_loop_is_free_during_a_schema_load},
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
