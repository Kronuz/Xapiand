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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>
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
#include "database/utils.h"
#include "pool_support.h"
#include "endpoint.h"
#include "logger.h"
#include "sortable_serialise.h"
#include "engine/chores.h"
#include "engine/host.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/sync.h"
#include "http_asio.h"
#include "index_resolver_lru.h"
#include "manager.h"
#include "opts.h"
#include "rapidjson/document.h"
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
	// With wait false it only queues for the turn; held() says when it has it.
	HoldTurn(ShardEndpoint& entry, int flags, bool wait = true) {
		thread_ = std::thread([this, &entry, flags] { engine::sync_wait(hold(entry, flags)); });
		if (wait) {
			eventually([this] { return held_.load(); });
		}
	}
	bool held() const { return held_.load(); }
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

// A host of one thread that tells when it has run out of work. A task started on it that stops with the host idle
// is waiting on a held turn, whose lock it queued in that same step; tasks started one after another, each once the
// host is idle, queue on a turn in that order.
class ParkHost final : public engine::Host {
public:
	template <typename T>
	class Job {
	public:
		bool done() const { return state_->done.load(); }
		// Waits for the task to end; returns its value or rethrows its exception.
		T get() {
			CHECK(eventually([this] { return done(); }, 30s));
			if (state_->error) {
				std::rethrow_exception(state_->error);
			}
			if constexpr (!std::is_void_v<T>) {
				return std::move(*state_->value);
			}
		}

	private:
		friend class ParkHost;
		struct State {
			std::atomic<bool> done{false};
			std::exception_ptr error;
			std::optional<std::conditional_t<std::is_void_v<T>, std::monostate, T>> value;
		};
		std::shared_ptr<State> state_ = std::make_shared<State>();
	};

	ParkHost() : thread_([this] { loop(); }) { }
	~ParkHost() override {
		{
			std::lock_guard<std::mutex> lk(m_);
			stop_ = true;
		}
		cv_.notify_one();
		thread_.join();
	}

	void post(std::coroutine_handle<> h) override { push({h, nullptr, nullptr}); }
	void post_call(void (*fn)(void*), void* arg) override { push({{}, fn, arg}); }

	// True when nothing is left to run here (timers apart).
	bool idle() const {
		std::lock_guard<std::mutex> lk(m_);
		return idle_;
	}

	template <typename T>
	Job<T> start(task::task<T> t) {
		Job<T> job;
		auto state = job.state_;
		if constexpr (std::is_void_v<T>) {
			task::spawn(task::scheduler_ref::of(*this), std::move(t), [state](std::exception_ptr e) {
				state->error = e;
				state->done = true;
			});
		} else {
			task::spawn(task::scheduler_ref::of(*this), std::move(t), [state](std::exception_ptr e, std::optional<T> v) {
				state->error = e;
				state->value = std::move(v);
				state->done = true;
			});
		}
		return job;
	}

protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) override {
		std::lock_guard<std::mutex> lk(m_);
		const auto id = ++last_id_;
		timers_.push_back({at, id, std::move(node)});
		cv_.notify_one();
		return std::make_unique<Armed>(*this, id);
	}

private:
	struct Ready {
		std::coroutine_handle<> h;
		void (*fn)(void*);
		void* arg;
	};
	struct Entry {
		std::chrono::steady_clock::time_point at;
		std::uint64_t id;
		std::unique_ptr<TimerNode> node;
	};
	class Armed final : public Timer {
	public:
		Armed(ParkHost& owner, std::uint64_t id) : owner_(owner), id_(id) { }
		~Armed() override {
			std::unique_ptr<TimerNode> dropped;
			std::lock_guard<std::mutex> lk(owner_.m_);
			const auto it = std::find_if(owner_.timers_.begin(), owner_.timers_.end(), [this](const Entry& e) { return e.id == id_; });
			if (it != owner_.timers_.end()) {
				dropped = std::move(it->node);
				owner_.timers_.erase(it);
			}
		}

	private:
		ParkHost& owner_;
		std::uint64_t id_;
	};

	void push(Ready ready) {
		std::lock_guard<std::mutex> lk(m_);
		ready_.push_back(ready);
		idle_ = false;
		cv_.notify_one();
	}

	void loop() {
		Host::bind_thread(this);
		std::unique_lock<std::mutex> lk(m_);
		for (;;) {
			if (!ready_.empty()) {
				const auto next = ready_.front();
				ready_.pop_front();
				lk.unlock();
				{
					task::running_on scope(task::scheduler_ref::of(*this));
					if (next.fn != nullptr) {
						next.fn(next.arg);
					} else {
						next.h.resume();
					}
				}
				lk.lock();
				continue;
			}
			if (stop_) {
				return;
			}
			idle_ = true;
			if (!timers_.empty()) {
				const auto next = std::min_element(timers_.begin(), timers_.end(), [](const Entry& a, const Entry& b) { return a.at < b.at; });
				if (next->at <= std::chrono::steady_clock::now()) {
					auto node = std::move(next->node);
					timers_.erase(next);
					lk.unlock();
					node->fire();
					node.reset();
					lk.lock();
					continue;
				}
				cv_.wait_until(lk, next->at);
				continue;
			}
			cv_.wait(lk);
		}
	}

	mutable std::mutex m_;
	std::condition_variable cv_;
	std::deque<Ready> ready_;
	std::vector<Entry> timers_;
	std::uint64_t last_id_ = 0;
	bool idle_ = false;
	bool stop_ = false;
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

// A TCP port the OS has just handed out, so test runs at the same time on one host never share one.
int free_port() {
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	socklen_t len = sizeof(addr);
	if (fd < 0 || ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
		std::fprintf(stderr, "FAIL: no free port\n");
		std::exit(1);
	}
	::close(fd);
	return ntohs(addr.sin_port);
}

// The port of the test process's node.
const int node_port = free_port();

// The node of the test process: a manager whose control loop runs on its own thread, serving on a port of its own,
// in a data directory of its own.
class RunningNode {
public:
	explicit RunningNode(const std::string& name = "node_test_data", const std::string& port = std::to_string(node_port), bool keep = false) {
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
static std::string http_call(int port, const std::string& method, const std::string& path, const std::string& body = "", const std::string& content_type = "application/json") {
	const int fd = connect_to(port);
	if (fd < 0) {
		return "";
	}
	timeval timeout{10, 0};
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	const auto request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
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

static const int abandon_port = free_port();
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

// ---- a write whose client half-closes after its body ----

static const int half_close_port = free_port();

// The child: a PUT that waits for a held turn while its client half-closes (the server reads its EOF, which requests
// the request's token), then the turn is let go. The write is admitted, so it completes and is answered.
[[noreturn]] static void half_closed_put_child(const char* log_path) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	int code = 0;
	{
		RunningNode node("node_half_close_data", std::to_string(half_close_port));
		if (!node.ready()) {
			std::_Exit(2);
		}
		std::optional<IndexPin> entry(pin_index(*manager()->database_pool, endpoints_of("half_closed")[0].path, WRITE));
		std::optional<HoldTurn> hold(std::in_place, entry->entry(), WRITE);
		const int fd = connect_to(half_close_port);
		if (fd < 0) {
			std::_Exit(3);
		}
		const char* request = "PUT /half_closed/1?commit HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 21\r\n\r\n{\"marker\":\"half-way\"}";
		::send(fd, request, std::strlen(request), 0);
		::shutdown(fd, SHUT_WR);   // the body is whole; the client sends nothing more
		if (!eventually([] { return manager()->http_service->reactor(0).gate().inflight.load() == 1; })) {
			std::_Exit(4);
		}
		std::this_thread::sleep_for(300ms);   // the server has read the EOF while the write waits for its turn
		hold.reset();
		entry.reset();
		timeval timeout{10, 0};
		::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		std::string response;
		char buf[4096];
		ssize_t n;
		while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
			response.append(buf, static_cast<std::size_t>(n));
		}
		::close(fd);
		if (response.rfind("HTTP/1.1 204", 0) != 0) {
			std::fprintf(stderr, "HALF-CLOSED PUT answered: %s\n", response.substr(0, 80).c_str());
			code = 5;
		}
	}   // the node stops
	Logging::finish();
	Logging::join();
	std::_Exit(code);
}

// The restart: the half-closed write is there.
[[noreturn]] static void half_closed_restart_child(const char* log_path) {
	std::freopen(log_path, "w", stderr);
	std::setvbuf(stderr, nullptr, _IONBF, 0);
	Logging::handlers.push_back(std::make_unique<StderrLogger>());
	RunningNode node("node_half_close_data", std::to_string(half_close_port), true);
	if (!node.ready()) {
		std::_Exit(2);
	}
	const auto response = http_call(half_close_port, "GET", "/half_closed/1");
	if (response.rfind("HTTP/1.1 200", 0) != 0 || response.find("half-way") == std::string::npos) {
		std::fprintf(stderr, "MISSING the half-closed write: %s\n", response.substr(0, 80).c_str());
		std::_Exit(10);
	}
	std::_Exit(0);
}

// A PUT whose client half-closes after its body completes durably (T9): the EOF cancels reads, not an admitted write.
static void test_a_put_whose_client_half_closes_completes_durably() {
	const auto result = run_child([&] { half_closed_put_child("node_half_close.log"); }, 90s);
	CHECK(result.exited);
	CHECK_EQ(result.code, 0);
	if (result.code != 0) {
		std::fprintf(stderr, "%s\n", read_file("node_half_close.log").c_str());
	}
	const auto restart = run_child([&] { half_closed_restart_child("node_half_close_restart.log"); }, 90s);
	CHECK(restart.exited);
	CHECK_EQ(restart.code, 0);
	if (restart.code != 0) {
		std::fprintf(stderr, "%s\n", read_file("node_half_close_restart.log").c_str());
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
	const int port = free_port();
	const int taken = ::socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<std::uint16_t>(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (::bind(taken, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(taken, 8) != 0) {
		std::_Exit(2);
	}
	RunningNode node("node_taken_data", std::to_string(port));
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

// ---- writes over HTTP: document ids and expected versions ----

// A response of the test's node: its status and its JSON body (undefined when it has none).
struct Reply {
	int status = 0;
	MsgPack body;
};

static Reply call(const std::string& method, const std::string& path, const std::string& body = "", const std::string& content_type = "application/json") {
	const auto response = http_call(node_port, method, path, body, content_type);
	Reply reply;
	if (response.size() < 12 || response.rfind("HTTP/1.1 ", 0) != 0) {
		return reply;
	}
	reply.status = std::atoi(response.c_str() + 9);
	const auto end = response.find("\r\n\r\n");
	if (end == std::string::npos) {
		return reply;
	}
	auto payload = response.substr(end + 4);
	if (response.substr(0, end).find("chunked") != std::string::npos) {
		std::string joined;
		for (std::size_t pos = 0;;) {
			const auto eol = payload.find("\r\n", pos);
			const auto size = eol == std::string::npos ? 0 : std::stoul(payload.substr(pos, eol - pos), nullptr, 16);
			if (size == 0) {
				break;
			}
			joined.append(payload, eol + 2, size);
			pos = eol + 2 + size + 2;
		}
		payload = std::move(joined);
	}
	if (!payload.empty()) {
		try {
			rapidjson::Document rdoc;
			json_load(rdoc, payload);
			reply.body = MsgPack(rdoc);
		} catch (...) { }
	}
	return reply;
}

static bool success(const Reply& reply) {
	return reply.status >= 200 && reply.status < 300;
}

// A field of a reply's body; undefined when there is none.
static MsgPack field_of(const Reply& reply, std::string_view name) {
	if (!reply.body.is_map()) {
		return MsgPack();
	}
	const auto it = reply.body.find(name);
	return it == reply.body.end() ? MsgPack() : it.value();
}

// A field of a reply's body as text: a number as its digits, a string as it is; "" when there is none.
static std::string text_of(const Reply& reply, std::string_view name) {
	const auto value = field_of(reply, name);
	return value.is_string() ? value.str() : value.is_undefined() ? std::string() : value.to_string();
}

// The `_version` of a reply's body; -1 when it has none.
static long long version_of(const Reply& reply) {
	const auto value = field_of(reply, "_version");
	return value.is_number() ? value.as_i64() : -1;
}

// The `n` of each hit of a search, in order.
static std::vector<std::string> hits_of(const Reply& reply) {
	std::vector<std::string> ns;
	const auto hits = field_of(reply, "hits");
	if (hits.is_array()) {
		for (const auto& hit : hits) {
			const auto it = hit.find("n");
			ns.push_back(it == hit.end() ? std::string() : it.value().str());
		}
	}
	return ns;
}

// The shard counts the id and version cases run at: one shard, and more than one, where a key's shard is chosen
// before its turn.
constexpr int shard_counts[] = {1, 2};

// Creates the index `name` with `shards` shards and no replicas, and `schema` (a JSON object) when given.
static void create_index(const std::string& name, int shards, const std::string& schema = "") {
	const auto settings = R"({"_settings":{"number_of_shards":)" + std::to_string(shards) + R"(,"number_of_replicas":0})";
	CHECK(success(call("PUT", "/" + name + "/", settings + (schema.empty() ? "" : R"(,"_schema":)" + schema) + "}")));
	CHECK_EQ(static_cast<int>(endpoints_of(name).size()), shards);
}

// Holds the turn of every shard of an index from threads of their own until released.
class HoldEveryShard {
public:
	explicit HoldEveryShard(const std::string& name) {
		for (const auto& endpoint : endpoints_of(name)) {
			pins_.push_back(pin_index(*manager()->database_pool, endpoint.path, WRITE));
		}
		for (auto& pin : pins_) {
			holds_.push_back(std::make_unique<HoldTurn>(pin.entry(), WRITE));
		}
	}
	void release() { holds_.clear(); }

private:
	std::vector<IndexPin> pins_;
	std::vector<std::unique_ptr<HoldTurn>> holds_;
};

static std::size_t requests_inflight() {
	return manager()->http_service->reactor(0).gate().inflight.load();
}

// Ids round trip through PUT, GET, SEARCH and DELETE, each touching only its own document: `ids` in the index at
// `path`, whose first id sets the type of its ids, each written once with its `n` the id's text; `literal` is an id
// as a JSON value. With `numbered`, a POST then adds a document whose id the shard numbers, which takes none of them.
static void ids_round_trip(const std::string& path, const std::vector<std::string>& ids, std::string (*literal)(const std::string&), bool numbered) {
	for (const auto& id : ids) {
		CHECK_EQ(call("PUT", path + id + "?commit", R"({"n":")" + id + R"("})").status, 204);
	}
	auto all = ids;
	if (numbered) {
		const auto posted = call("POST", path + "?commit&echo", R"({"n":"new"})");
		CHECK(success(posted));
		const auto id = text_of(posted, "_id");
		CHECK(field_of(posted, "_id").is_integer());
		CHECK(std::find(ids.begin(), ids.end(), id) == ids.end());
		CHECK_EQ(version_of(posted), 1);
		all.push_back(id);
	}
	const auto n_of = [&](std::size_t i) { return i < ids.size() ? ids[i] : std::string("new"); };
	for (std::size_t i = 0; i < all.size(); ++i) {
		const auto got = call("GET", path + all[i]);
		CHECK_EQ(got.status, 200);
		CHECK_EQ(text_of(got, "n"), n_of(i));
		CHECK_EQ(text_of(got, "_id"), all[i]);
		CHECK_EQ(version_of(got), 1);   // written once: no other write landed on it
		const auto found = call("SEARCH", path + "?volatile", R"({"_query":{"_id":)" + literal(all[i]) + "}}");
		CHECK_EQ(found.status, 200);
		CHECK(hits_of(found) == std::vector<std::string>{n_of(i)});
	}
	CHECK_EQ(field_of(call("SEARCH", path + "?volatile", R"({"_limit":20})"), "total").as_u64(), all.size());
	for (std::size_t i = 0; i < all.size(); ++i) {
		CHECK_EQ(call("DELETE", path + all[i] + "?commit").status, 204);
		CHECK_EQ(call("GET", path + all[i]).status, 404);
		for (std::size_t j = i + 1; j < all.size(); ++j) {
			CHECK_EQ(text_of(call("GET", path + all[j]), "n"), n_of(j));
		}
	}
	CHECK_EQ(field_of(call("SEARCH", path + "?volatile", R"({"_limit":20})"), "total").as_u64(), 0u);
}

static std::string number(const std::string& id) { return id; }
static std::string quoted(const std::string& id) { return "\"" + id + "\""; }

// A document id 0 is the document 0: it was the id a new numeric id is numbered from, so a PUT of id 0 landed on
// the next free document id, the id of another document. Negative, the largest, and those beside them are stored
// as themselves too, and a keyword id "0" or "00" is its own. The largest docid as an id leaves the shard ids to
// number after it.
static void test_document_ids_are_themselves() {
	Watchdog watchdog("document ids are themselves", 240s);
	for (const int shards : shard_counts) {
		const auto at = "_s" + std::to_string(shards);
		create_index("ids_int" + at, shards);
		ids_round_trip("/ids_int" + at + "/", {"-1", "0", "1", "2", "4294967295", "9223372036854775807"}, number, true);   // -1 first: the ids are integers
		create_index("ids_pos" + at, shards, R"({"_type":"object","_id":{"_type":"positive"}})");
		ids_round_trip("/ids_pos" + at + "/", {"0", "1", "18446744073709551615"}, number, true);
		create_index("ids_kw" + at, shards);
		ids_round_trip("/ids_kw" + at + "/", {"a", "0", "00", "-1"}, quoted, false);   // a first: the ids are keywords

		// An id names the document id it would take when that is free: a new id the shard numbers is not it. One
		// shard numbers it after the id; more shards put it in the least used one, which numbers it there.
		const auto seq = "/ids_seq" + at + "/";
		create_index("ids_seq" + at, shards);
		CHECK_EQ(call("PUT", seq + "2?commit", R"({"n":"2"})").status, 204);
		const auto posted = call("POST", seq + "?commit&echo", R"({"n":"new"})");
		CHECK(success(posted));
		if (shards == 1) {
			CHECK_EQ(text_of(posted, "_id"), "3");
		} else {
			CHECK(field_of(posted, "_id").is_integer());
			CHECK_NE(text_of(posted, "_id"), "2");
		}
		CHECK_EQ(text_of(call("GET", seq + "2"), "n"), "2");
		CHECK_EQ(version_of(call("GET", seq + "2")), 1);

		// A body's id 0 is an id too (a RESTORE line), not a new one; an id false is none, so each such document gets
		// its own new id, the one an index without an id type gives (a UUID), not false.
		const auto restore = "/ids_restore" + at + "/";
		create_index("ids_restore" + at, shards);
		CHECK(success(call("RESTORE", restore + "?commit", "{\"_id\":1,\"n\":\"1\"}\n{\"_id\":0,\"n\":\"0\"}\n", "application/x-ndjson")));
		for (const std::string id : {"0", "1"}) {
			const auto got = call("GET", restore + id);
			CHECK_EQ(text_of(got, "n"), id);
			CHECK_EQ(version_of(got), 1);
		}
		const auto unnamed = "/ids_false" + at + "/";
		create_index("ids_false" + at, shards);
		CHECK(success(call("RESTORE", unnamed + "?commit", "{\"_id\":false,\"n\":\"f\"}\n{\"_id\":false,\"n\":\"g\"}\n", "application/x-ndjson")));
		const auto found = call("SEARCH", unnamed + "?volatile", R"({"_limit":5})");
		CHECK_EQ(field_of(found, "total").as_u64(), 2u);   // two new ids: false names no document for the second to replace
		std::map<std::string, std::string> generated;   // id -> n
		const auto hits = field_of(found, "hits");
		if (hits.is_array()) {
			for (const auto& hit : hits) {
				const auto id = hit.find("_id");
				const auto n = hit.find("n");
				if (id != hit.end() && id.value().is_string() && n != hit.end() && n.value().is_string()) {
					generated[id.value().str()] = n.value().str();
				}
			}
		}
		CHECK_EQ(generated.size(), 2u);
		for (const auto& [id, n] : generated) {
			CHECK_TRUE(id.size() > 5);   // a UUID, not false's text ("false", "f", "0") or empty
			CHECK_TRUE(n == "f" || n == "g");
			CHECK_EQ(text_of(call("GET", unnamed + id), "n"), n);
		}
		CHECK_FALSE(success(call("GET", unnamed + "false")));
	}
}

// The body of a write of `v` by each method: a JSON patch for PATCH, the document for the others.
static std::string write_body(const std::string& method, int v) {
	if (method == "PATCH") {
		return R"([{"op":"replace","path":"/v","value":)" + std::to_string(v) + "}]";
	}
	return method == "DELETE" ? std::string() : R"({"v":)" + std::to_string(v) + "}";
}

// `?version=N` is a precondition of every write: applied only while the document's version is N (0: no document),
// else a 409 that says the current version in `_version` and changes nothing. A missing document has version 0 for
// every method (Elasticsearch's if_seq_no); with the precondition held the method does what it does without one.
static void a_version_is_a_precondition_of_every_write_at(int shards) {
	const auto index = "versions_s" + std::to_string(shards);
	create_index(index, shards);
	for (const std::string method : {"PUT", "PATCH", "UPDATE", "UPSERT", "DELETE"}) {
		const auto path = "/" + index + "/" + method;
		CHECK_EQ(call("PUT", path + "?commit", R"({"v":1})").status, 204);
		CHECK_EQ(call("PUT", path + "?commit", R"({"v":2})").status, 204);
		for (const auto stale : {1, 0, 3}) {
			const auto refused = call(method, path + "?commit&version=" + std::to_string(stale), write_body(method, 9));
			CHECK_EQ(refused.status, 409);
			CHECK_EQ(version_of(refused), 2);
			const auto kept = call("GET", path);
			CHECK_EQ(text_of(kept, "v"), "2");
			CHECK_EQ(version_of(kept), 2);
		}
		CHECK(success(call(method, path + "?commit&version=2", write_body(method, 3))));
		const auto applied = call("GET", path);
		if (method == "DELETE") {
			CHECK_EQ(applied.status, 404);
		} else {
			CHECK_EQ(text_of(applied, "v"), "3");
			CHECK_EQ(version_of(applied), 3);
		}

		const auto missing = "/" + index + "/missing_" + method;
		const auto refused = call(method, missing + "?commit&version=1", write_body(method, 4));
		CHECK_EQ(refused.status, 409);
		CHECK_EQ(version_of(refused), 0);
		CHECK_EQ(call("GET", missing).status, 404);
		const auto absent = call(method, missing + "?commit&version=0", write_body(method, 5));
		if (method == "PUT" || method == "UPSERT") {
			CHECK(success(absent));
			const auto created = call("GET", missing);
			CHECK_EQ(text_of(created, "v"), "5");
			CHECK_EQ(version_of(created), 1);
		} else {
			CHECK_EQ(absent.status, 404);
			CHECK_EQ(call("GET", missing).status, 404);
		}
	}
}

static void test_a_version_is_a_precondition_of_every_write() {
	Watchdog watchdog("a version is a precondition of every write", 240s);
	for (const int shards : shard_counts) {
		a_version_is_a_precondition_of_every_write_at(shards);
	}
}

// Writers that expect one version: exactly one is applied and the others are 409 with the version that one left.
// The turn of every shard of the index is held until every writer waits on one (placement's look takes each shard's,
// the write its own), so all are in flight before any applies (the interleaving the check in the turn is for).
static void writers_expecting_one_version_apply_once_at(int shards) {
	constexpr int writers = 6;
	const auto index = "race_s" + std::to_string(shards);
	create_index(index, shards);
	for (const std::string method : {"PUT", "PATCH", "UPDATE", "UPSERT", "DELETE"}) {
		const auto path = "/" + index + "/" + method;
		CHECK_EQ(call("PUT", path + "?commit", R"({"v":-1})").status, 204);
		std::vector<Reply> replies(writers);
		std::vector<std::thread> threads;
		{
			HoldEveryShard hold(index);
			for (int i = 0; i < writers; ++i) {
				threads.emplace_back([&, i] { replies[i] = call(method, path + "?commit&version=1", write_body(method == "UPSERT" ? "UPDATE" : method, i)); });
			}
			CHECK(eventually([] { return requests_inflight() == writers; }));
			std::this_thread::sleep_for(200ms);   // each waits on a turn: its placement look's or its write's
			hold.release();
		}
		for (auto& thread : threads) {
			thread.join();
		}
		int won = -1, wins = 0, conflicts = 0;
		for (int i = 0; i < writers; ++i) {
			if (success(replies[i])) {
				won = i;
				++wins;
			} else if (replies[i].status == 409) {
				++conflicts;
				CHECK_EQ(version_of(replies[i]), method == "DELETE" ? 0 : 2);
			}
		}
		CHECK_EQ(wins, 1);
		CHECK_EQ(conflicts, writers - 1);
		const auto got = call("GET", path);
		if (method == "DELETE") {
			CHECK_EQ(got.status, 404);
		} else {
			CHECK_EQ(text_of(got, "v"), std::to_string(won));
			CHECK_EQ(version_of(got), 2);
		}
	}
}

static void test_writers_expecting_one_version_apply_once() {
	Watchdog watchdog("writers expecting one version apply once", 240s);
	for (const int shards : shard_counts) {
		writers_expecting_one_version_apply_once_at(shards);
	}
}

// A versioned DELETE checks the document its key names, in the turn: between its lookup and its turn the document is
// deleted and its docid taken by a new id, and the DELETE is refused (the keyed document is gone) and leaves that one.
static void test_a_versioned_delete_checks_the_document_its_key_names() {
	Watchdog watchdog("a versioned delete checks the document its key names", 120s);
	for (const int shards : shard_counts) {
		const auto index = "reuse_s" + std::to_string(shards);
		const auto path = "/" + index + "/";
		create_index(index, shards);
		CHECK_EQ(call("PUT", path + "-1?commit", R"({"v":"minus"})").status, 204);
		const auto docid = field_of(call("INFO", path + "-1"), "docid");
		CHECK(docid.is_number());
		if (!docid.is_number()) {
			continue;
		}
		const auto other = std::to_string(docid.as_i64());   // a new id that names the docid -1 frees
		CHECK_EQ(call("GET", path + other).status, 404);
		Reply deleted, put, versioned;
		std::thread t1, t2, t3;
		{
			HoldEveryShard hold(index);
			t1 = std::thread([&] { deleted = call("DELETE", path + "-1?commit"); });
			CHECK(eventually([] { return requests_inflight() == 1; }));
			std::this_thread::sleep_for(200ms);
			t2 = std::thread([&] { put = call("PUT", path + other + "?commit", R"({"v":"other"})"); });
			CHECK(eventually([] { return requests_inflight() == 2; }));
			std::this_thread::sleep_for(200ms);
			t3 = std::thread([&] { versioned = call("DELETE", path + "-1?commit&version=1"); });
			CHECK(eventually([] { return requests_inflight() == 3; }));
			std::this_thread::sleep_for(200ms);   // the versioned DELETE waits on a turn: its placement look's or its erase's
			hold.release();
		}
		t1.join();
		t2.join();
		t3.join();
		CHECK_EQ(deleted.status, 204);
		CHECK_EQ(put.status, 204);
		CHECK_EQ(versioned.status, 409);
		CHECK_EQ(version_of(versioned), 0);
		const auto kept = call("GET", path + other);
		CHECK_EQ(kept.status, 200);
		CHECK_EQ(text_of(kept, "v"), "other");
		CHECK_EQ(version_of(kept), 1);
	}
}

// ---- placement: where a write puts a document, read without flushing the shards ----

// The segments a shard's index holds now (its seg-<id>.sx files) and the highest id among them, which counts the
// segments it wrote: a merge's output takes a new id too.
struct SegmentFiles {
	int count = 0;
	std::uint64_t highest = 0;
};

static SegmentFiles segment_files(const Endpoint& endpoint) {
	auto dir = std::filesystem::path(endpoint.path) / "strata";
	if (dir.is_relative()) {
		dir = std::filesystem::path(Endpoint::cwd) / dir;
	}
	CHECK(std::filesystem::is_directory(dir));
	SegmentFiles files;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		const auto name = entry.path().filename().string();
		if (name.rfind("seg-", 0) != 0 || !name.ends_with(".sx")) {
			continue;
		}
		++files.count;
		files.highest = std::max<std::uint64_t>(files.highest, std::stoull(name.substr(4)));
	}
	return files;
}

// A RESTORE of 1,000 documents into 5 shards writes a few segments per shard: placing each document reads the
// shards as their writes left them, so it flushes none of them (one segment per document when it did). With ids,
// and without (each document gets a new id, placed by its hash).
static void test_a_restore_into_shards_writes_a_few_segments() {
	Watchdog watchdog("a restore into shards writes a few segments", 240s);
	for (const bool ids : {true, false}) {
		const std::string index = ids ? "restore_ids_s5" : "restore_new_s5";
		create_index(index, 5);
		std::string body;
		for (int i = 1; i <= 1000; ++i) {
			body += (ids ? R"({"_id":)" + std::to_string(i) + "," : std::string("{")) + R"("n":")" + std::to_string(i) + "\"}\n";
		}
		CHECK(success(call("RESTORE", "/" + index + "/?commit", body, "application/x-ndjson")));
		for (const auto& endpoint : endpoints_of(index)) {
			const auto files = segment_files(endpoint);
			std::fprintf(stderr, "  %s %s: %d segments, highest id %llu\n", index.c_str(), endpoint.path.c_str(), files.count, static_cast<unsigned long long>(files.highest));
			CHECK_GE(files.count, 1);
			CHECK_LE(files.highest, 3u);
		}
		CHECK_EQ(field_of(call("SEARCH", "/" + index + "/?volatile", R"({"_limit":1})"), "total").as_u64(), 1000u);
	}
}

// A document on another shard than its id's hash names (where an earlier rule placed it) is written again on the
// shard that holds it, while its write there is still buffered and after a commit, by a PUT and by a RESTORE line:
// placement's look in every shard sees what each shard's writes left, flushed or not.
static void test_an_existing_id_written_again_lands_on_its_shard() {
	Watchdog watchdog("an existing id written again lands on its shard", 120s);
	constexpr std::size_t shards = 5;
	const std::string index = "placed_s5";
	const auto path = "/" + index + "/";
	create_index(index, shards);
	CHECK_EQ(call("PUT", path + "seed?commit", R"({"n":"seed"})").status, 204);   // the ids are keywords
	const auto endpoints = endpoints_of(index);
	DatabaseHandler db(endpoints, WRITE);
	const auto holds = [&](std::size_t shard, const std::string& term) {
		try {
			return engine::sync_wait(DatabaseHandler(Endpoints(endpoints[shard]), DB_OPEN).get_docid_term(term)) != 0;
		} catch (const engine::Error& error) {
			CHECK(engine::is_missing(error.kind));
			return false;
		}
	};
	for (int i = 0; i < 8; ++i) {
		const bool flushed = i % 2 == 1;
		const bool restored = i / 2 % 2 == 1;
		const auto id = "k" + std::to_string(i);
		auto obj = MsgPack{{"_id", id}, {"n", "first"}};
		auto prepared = engine::sync_wait(db.prepare_document(obj, 0));
		const auto term = std::get<0>(prepared);
		const std::size_t hashed = key_placement_hash(term) % shards;
		const std::size_t held = (hashed + 1 + static_cast<std::size_t>(i) % (shards - 1)) % shards;   // any shard but the hash's
		{
			auto pin = pin_index(*manager()->database_pool, endpoints[held].path, WRITE_QUIET);
			auto shard = use_index(pin, WRITE_QUIET);
			engine::sync_wait(shard.replace_document_term(term, std::move(std::get<1>(prepared)), flushed, false));
			CHECK_EQ(pin.entry().engine().dirty(), !flushed);   // buffered until a commit or a read refreshes it
		}
		if (restored) {
			CHECK(success(call("RESTORE", path, R"({"_id":")" + id + R"(","n":"again"})" "\n", "application/x-ndjson")));
		} else {
			CHECK_EQ(call("PUT", path + id, R"({"n":"again"})").status, 204);
		}
		for (std::size_t s = 0; s < shards; ++s) {
			CHECK_EQ(holds(s, term), s == held);
		}
		const auto got = call("GET", path + id);
		CHECK_EQ(text_of(got, "n"), "again");
		CHECK_EQ(version_of(got), 2);
	}
	CHECK_EQ(field_of(call("SEARCH", path + "?volatile", R"({"_limit":1})"), "total").as_u64(), 9u);
}

// An id seated off its hash shard (where an earlier placement rule put it) in a new index of 5 shards, and direct
// writes of it on one shard, for the races of a write whose lookup found it there.
struct OffHashId {
	static constexpr std::size_t shards = 5;
	std::string path;
	Endpoints endpoints;
	std::string id;
	std::string term;
	std::size_t hashed = 0;
	std::size_t off = 0;

	OffHashId(const std::string& index, std::string id_) : path("/" + index + "/"), id(std::move(id_)) {
		create_index(index, shards);
		CHECK_EQ(call("PUT", path + "seed?commit", R"({"n":"seed"})").status, 204);   // the ids are keywords
		endpoints = endpoints_of(index);
		term = std::get<0>(prepare("first"));
		hashed = key_placement_hash(term) % shards;
		off = hashed == 1 ? 2 : 1;   // not the hash's, and not shard 0, which a request's schema reads
		seat(off, "first");
		CHECK(holds(off));
		CHECK_FALSE(holds(hashed));
	}

	// The id's document with `n`, prepared for a write.
	std::tuple<std::string, engine::Document, MsgPack> prepare(const std::string& n) const {
		auto obj = MsgPack{{"_id", id}, {"n", n}};
		return engine::sync_wait(DatabaseHandler(endpoints, WRITE).prepare_document(obj, 0));
	}

	// Writes the id's document with `n` on `shard` by its key, committed: its shard docid.
	engine::docid seat(std::size_t shard, const std::string& n) const {
		auto pin = pin_index(*manager()->database_pool, endpoints[shard].path, WRITE_QUIET);
		auto use = use_index(pin, WRITE_QUIET);
		return engine::sync_wait(use.replace_document_term(term, std::move(std::get<1>(prepare(n))), true, false)).did;
	}

	// Erases the id on `shard` by its key, in one turn of that shard.
	void erase(std::size_t shard) const {
		auto pin = pin_index(*manager()->database_pool, endpoints[shard].path, WRITE_QUIET);
		auto use = use_index(pin, WRITE_QUIET);
		engine::sync_wait(use.delete_document_term(term, false, false));
	}

	bool holds(std::size_t shard) const {
		try {
			return engine::sync_wait(DatabaseHandler(Endpoints(endpoints[shard]), DB_OPEN).get_docid_term(term)) != 0;
		} catch (const engine::Error& error) {
			CHECK(engine::is_missing(error.kind));
			return false;
		}
	}
};

// A replace with must_exist writes the keyed document only while it is at the docid: a docid that holds another
// document, or none since the key was erased, answers doc_not_found and creates nothing; with an expected version
// the write is checked as one to no document first (version_conflict); at its docid it is replaced.
static void test_a_replace_that_must_exist_writes_only_where_the_key_is() {
	Watchdog watchdog("a replace that must exist writes only where the key is");
	const OffHashId x("must_exist_s5", "x");
	auto pin = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE_QUIET);
	auto use = use_index(pin, WRITE_QUIET);
	// The write's failure, or none.
	const auto replace = [&](engine::docid did, bool must_exist, std::optional<engine::rev> expected = std::nullopt) -> std::optional<engine::errc> {
		auto doc = std::get<1>(x.prepare("again"));
		if (expected) {
			doc.add_value(DB_SLOT_VERSION, sortable_serialise(static_cast<double>(*expected)));
		}
		try {
			engine::sync_wait(use.replace_document(did, std::move(doc), false, false, true, must_exist));
		} catch (const engine::Error& error) {
			return error.kind;
		}
		return std::nullopt;
	};
	const auto did = engine::sync_wait(use.written_key(x.term));
	CHECK(did != 0);
	CHECK(replace(did + 1, true) == engine::errc::doc_not_found);
	CHECK(replace(did + 1, false) == engine::errc::invalid);   // without it: the key belongs to another document
	CHECK(replace(did, true) == std::nullopt);
	CHECK_EQ(engine::sync_wait(use.written_key(x.term)), did);
	x.erase(x.off);
	CHECK(replace(did, true, 2) == engine::errc::version_conflict);
	CHECK(replace(did, true) == engine::errc::doc_not_found);
	CHECK_EQ(engine::sync_wait(use.written_key(x.term)), engine::docid(0));
	CHECK(replace(did, false) == std::nullopt);   // without it: created at the docid
}

// An off-hash id written by a PUT whose lookup found it while a DELETE and a PUT of another request come between its
// lookup and its write must not end on two shards. The shard's turn is a FIFO queue, so held turns order the three:
// DELETE's lookup, a hold, the PUT's lookup, and a second hold queued while the first is held; releasing the first
// runs the PUT's lookup (it finds the id) and then the DELETE's erase, and the second hold stops the PUT's write
// behind them. The new PUT (its lookup missed, so it is placed on the hash shard) is the write the test makes there,
// and the stale write is let go last.
static void test_a_stale_write_does_not_put_an_id_on_two_shards() {
	Watchdog watchdog("a stale write does not put an id on two shards", 120s);
	const OffHashId x("twice_s5", "twice");
	auto entry = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE);
	Reply deleted, put;
	std::atomic<bool> delete_replied{false};
	auto first = std::make_unique<HoldTurn>(entry.entry(), WRITE);
	std::thread t_delete([&] {
		deleted = call("DELETE", x.path + x.id + "?commit");
		delete_replied = true;
	});
	CHECK(eventually([] { return requests_inflight() == 1; }));
	std::this_thread::sleep_for(200ms);   // the DELETE's lookup waits on the turn
	auto second = std::make_unique<HoldTurn>(entry.entry(), WRITE, false);
	std::this_thread::sleep_for(100ms);
	std::thread t_put([&] { put = call("PUT", x.path + x.id, R"({"n":"stale"})"); });
	CHECK(eventually([] { return requests_inflight() == 2; }));
	std::this_thread::sleep_for(200ms);   // the PUT's lookup waits behind the second hold
	first->release();   // the DELETE's lookup runs and queues its erase; the second hold has the turn
	CHECK(eventually([&] { return second->held(); }));
	auto third = std::make_unique<HoldTurn>(entry.entry(), WRITE, false);
	std::this_thread::sleep_for(100ms);   // queued behind the PUT's lookup and the DELETE's erase
	second->release();   // the PUT's lookup finds the id; the DELETE's erase runs; the third hold has the turn
	CHECK(eventually([&] { return third->held(); }));
	// The DELETE's ?commit waits for a turn of its own behind the third hold, so it replies only once that is
	// released: joined before, the client's 10 s read timeout ended it with no reply (status 0).
	std::this_thread::sleep_for(200ms);
	CHECK_FALSE(delete_replied.load());
	x.seat(x.hashed, "new");   // the new PUT's write: its lookup missed, so it goes to the hash shard
	third->release();   // the stale write and the DELETE's commit run
	t_delete.join();
	t_put.join();
	CHECK_EQ(deleted.status, 204);
	CHECK_EQ(put.status, 204);
	CHECK(x.holds(x.hashed));
	CHECK_FALSE(x.holds(x.off));
	CHECK_EQ(text_of(call("GET", x.path + x.id), "n"), "stale");
	CHECK_EQ(field_of(call("SEARCH", x.path + "?volatile", R"({"_limit":5})"), "total").as_u64(), 2u);
}

// A replace by term on an index of shards finds an off-hash id and writes it by docid in a later turn: erased between
// the two, the write places the document afresh on its hash shard instead of putting it back where it was found.
// Both run on a host that says when it is idle: the lookup parks on a held turn, the erase queues behind it, and
// releasing the turn runs the lookup (it finds the id), then the erase, and the write queues behind that.
static void test_a_replace_by_term_of_an_id_erased_since_its_lookup_places_it_afresh() {
	Watchdog watchdog("a replace by term of an id erased since its lookup places it afresh", 120s);
	const OffHashId x("afresh_s5", "afresh");
	auto entry = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE);
	auto erasing = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE_QUIET);
	auto use = use_index(erasing, WRITE_QUIET);
	auto hold = std::make_unique<HoldTurn>(entry.entry(), WRITE);
	ParkHost host;
	DatabaseHandler db(x.endpoints, WRITE_QUIET);
	auto prepared = x.prepare("stale");
	auto replace = host.start(db.replace_document_term(x.term, std::move(std::get<1>(prepared)), true));
	CHECK(eventually([&] { return host.idle(); }));
	CHECK_FALSE(replace.done());   // the lookup waits on the turn
	auto erase = host.start(use.delete_document_term(x.term, false, false));
	CHECK(eventually([&] { return host.idle(); }));
	CHECK_FALSE(erase.done());   // the erase waits behind the lookup
	hold->release();
	erase.get();
	const auto info = replace.get();
	CHECK_EQ((info.did - 1) % OffHashId::shards, x.hashed);
	CHECK(x.holds(x.hashed));
	CHECK_FALSE(x.holds(x.off));
	CHECK_EQ(text_of(call("GET", x.path + x.id), "n"), "stale");
}

// A PATCH reads an off-hash id and writes it by docid expecting the version it read: erased since, and another
// document of the id written on its hash shard at that same version, the write is a version conflict, so the PATCH
// reads again and patches the new document, never the old content over it. The off shard is refreshed, so the
// PATCH's read takes no turn; on a host that says when it is idle, the erase parks on the held turn and the PATCH,
// which reads and then parks on its write's turn, queues behind it.
static void test_a_patch_of_an_id_erased_since_its_read_patches_the_new_document() {
	Watchdog watchdog("a patch of an id erased since its read patches the new document", 120s);
	const OffHashId x("repatch_s5", "repatch");
	auto entry = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE);
	CHECK_FALSE(entry.entry().engine().dirty());
	auto erasing = pin_index(*manager()->database_pool, x.endpoints[x.off].path, WRITE_QUIET);
	auto use = use_index(erasing, WRITE_QUIET);
	auto hold = std::make_unique<HoldTurn>(entry.entry(), WRITE);
	ParkHost host;
	auto erase = host.start(use.delete_document_term(x.term, false, false));
	CHECK(eventually([&] { return host.idle(); }));
	CHECK_FALSE(erase.done());   // the erase waits on the turn
	DatabaseHandler db(x.endpoints, WRITE);
	const MsgPack id{x.id};
	rapidjson::Document rdoc;
	json_load(rdoc, R"([{"op":"add","path":"/p","value":"patched"}])");
	const MsgPack patches(rdoc);
	auto patch = host.start(db.patch(id, UNKNOWN_REVISION, false, patches, true));
	CHECK(eventually([&] { return host.idle(); }));
	CHECK_FALSE(patch.done());   // the PATCH read the old document; its write waits behind the erase
	x.seat(x.hashed, "new");   // version 1, as the old one
	hold->release();
	erase.get();
	patch.get();
	CHECK(x.holds(x.hashed));
	CHECK_FALSE(x.holds(x.off));
	const auto got = call("GET", x.path + x.id);
	CHECK_EQ(text_of(got, "n"), "new");
	CHECK_EQ(text_of(got, "p"), "patched");
	CHECK_EQ(version_of(got), 2);
}

// New numeric ids in a burst, none committed, spread over the 5 shards: the least used shard counts the documents
// each shard holds buffered, not only those it flushed (with the flushed ones alone, every one went to shard 0).
static void test_new_numeric_ids_spread_over_the_shards_while_buffered() {
	Watchdog watchdog("new numeric ids spread over the shards while buffered", 120s);
	constexpr int shards = 5;
	const std::string index = "spread_s5";
	const auto path = "/" + index + "/";
	create_index(index, shards, R"({"_type":"object","_id":{"_type":"positive"}})");
	std::vector<std::string> ids;
	for (int i = 0; i < 4 * shards; ++i) {
		const auto posted = call("POST", path + "?echo", R"({"n":"p"})");
		CHECK(success(posted));
		ids.push_back(text_of(posted, "_id"));
	}
	std::vector<int> per(shards, 0);
	for (const auto& id : ids) {
		const auto docid = field_of(call("INFO", path + id), "docid");
		CHECK(docid.is_number());
		if (docid.is_number()) {
			++per[static_cast<std::size_t>((docid.as_i64() - 1) % shards)];
		}
	}
	std::fprintf(stderr, "  %s per shard: %d %d %d %d %d\n", index.c_str(), per[0], per[1], per[2], per[3], per[4]);
	const auto [fewest, most] = std::minmax_element(per.begin(), per.end());
	CHECK_LE(*most - *fewest, 1);
	CHECK_EQ(std::accumulate(per.begin(), per.end(), 0), 4 * shards);
}

// Writers on few documents, each writing the version it read (PUT and PATCH, with ?version), against the sequential
// model: a write that is applied lands at the version after the one it expected, per document every version from 2
// to its last is won by exactly one write, a refused write saw an older version than the one it is told, and the
// document holds the write that won its last version.
static void versioned_writers_match_a_sequential_model_at(int shards) {
	const auto index = "model_s" + std::to_string(shards);
	create_index(index, shards);
	constexpr int keys = 2;
	constexpr int threads_n = 4;
	constexpr int wins_wanted = 12;   // per thread
	for (int k = 0; k < keys; ++k) {
		CHECK_EQ(call("PUT", "/" + index + "/" + std::to_string(k) + "?commit", R"({"v":-1})").status, 204);
	}
	struct Win {
		int key;
		long long seen;   // the version the write expected
		long long version;
		int value;
	};
	std::vector<std::vector<Win>> wins(threads_n);
	std::vector<int> refused(threads_n, 0), unexpected(threads_n, 0);
	std::vector<std::thread> threads;
	for (int t = 0; t < threads_n; ++t) {
		threads.emplace_back([&, t] {
			std::mt19937 random(static_cast<unsigned>(t) * 7919u + 1u);
			for (int attempt = 0; static_cast<int>(wins[t].size()) < wins_wanted && attempt < 50 * wins_wanted; ++attempt) {
				const int k = static_cast<int>(random() % keys);
				const auto path = "/" + index + "/" + std::to_string(k);
				const auto seen = version_of(call("GET", path + "?volatile"));
				const int value = t * 1000 + attempt;
				const std::string method = attempt % 2 == 0 ? "PUT" : "PATCH";
				const auto reply = call(method, path + "?echo&version=" + std::to_string(seen), write_body(method, value));
				if (success(reply)) {
					wins[t].push_back({k, seen, version_of(reply), value});
				} else if (reply.status == 409 && version_of(reply) > seen) {
					++refused[t];
				} else {
					++unexpected[t];
				}
			}
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
	for (int t = 0; t < threads_n; ++t) {
		CHECK_EQ(static_cast<int>(wins[t].size()), wins_wanted);
		CHECK_EQ(unexpected[t], 0);
	}
	for (int k = 0; k < keys; ++k) {
		std::map<long long, int> by_version;   // version -> value, each won once
		std::size_t won = 0;
		for (const auto& thread_wins : wins) {
			for (const auto& win : thread_wins) {
				if (win.key == k) {
					++won;
					by_version.emplace(win.version, win.value);
					CHECK_EQ(win.version, win.seen + 1);   // applied at the version after the one it expected
				}
			}
		}
		CHECK_EQ(by_version.size(), won);
		const auto got = call("GET", "/" + index + "/" + std::to_string(k) + "?volatile");
		CHECK_EQ(version_of(got), static_cast<long long>(won) + 1);
		if (!by_version.empty()) {
			CHECK_EQ(by_version.begin()->first, 2);
			CHECK_EQ(by_version.rbegin()->first, static_cast<long long>(won) + 1);
			CHECK_EQ(text_of(got, "v"), std::to_string(by_version.rbegin()->second));
		}
	}
	int total_refused = 0;
	for (const auto r : refused) {
		total_refused += r;
	}
	std::fprintf(stderr, "  model at %d shards: %d writes won, %d refused\n", shards, threads_n * wins_wanted, total_refused);   // what the contention reached
}

static void test_versioned_writers_match_a_sequential_model() {
	Watchdog watchdog("versioned writers match a sequential model", 600s);
	for (const int shards : shard_counts) {
		versioned_writers_match_a_sequential_model_at(shards);
	}
}


// ---- cancellation and overload in /:metrics ----

// A series of the node's /:metrics; -1 when it is not there.
static double metric(const std::string& name) {
	const auto text = http_call(node_port, "GET", "/:metrics");
	std::size_t at = 0;
	while (at < text.size()) {
		auto end = text.find('\n', at);
		if (end == std::string::npos) {
			end = text.size();
		}
		const auto line = text.substr(at, end - at);
		if (line.rfind(name, 0) == 0 && line.size() > name.size() && (line[name.size()] == '{' || line[name.size()] == ' ')) {
			return std::strtod(line.c_str() + line.rfind(' ') + 1, nullptr);
		}
		at = end + 1;
	}
	return -1;
}

// A search on `index` whose client leaves while it waits for the index's turn frees its reactor at once (T9): the
// client's EOF requests its token, its turn wait ends, and its admission is let go while the turn is still held. The
// cancel is counted in /:metrics. With `cached`, a search first loads the schema, so the wait is the search's own;
// without, it is the schema's first load.
static void client_leaves_a_search(const std::string& index, bool cached) {
	create_index(index, 1);
	if (cached) {
		CHECK(success(call("SEARCH", "/" + index + "/?volatile", "{}")));
	}
	auto& pool = *manager()->database_pool;
	const auto endpoints = endpoints_of(index);
	auto entry = pin_index(pool, endpoints[0].path, WRITE_QUIET);
	Shard shard = use_index(entry, WRITE_QUIET);
	engine::sync_wait(shard.add_document(make("K1")));   // applied, not refreshed: a read takes the turn
	const auto before = metric("xapiand_http_requests_cancelled");
	CHECK_GE(before, 0.0);
	HoldTurn hold(entry.entry(), WRITE_QUIET);
	const int fd = connect_to(node_port);
	CHECK_GE(fd, 0);
	const std::string request = "SEARCH /" + index + "/?volatile HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}";
	::send(fd, request.data(), request.size(), 0);
	CHECK(eventually([] { return requests_inflight() == 1; }));
	std::this_thread::sleep_for(200ms);
	CHECK_EQ(requests_inflight(), 1u);   // the search waits for the held turn
	const auto left = std::chrono::steady_clock::now();
	::close(fd);
	CHECK(eventually([] { return requests_inflight() == 0; }, 2s));
	const auto freed = std::chrono::steady_clock::now() - left;
	std::fprintf(stderr, "  the reactor let the search go %lld us after its client left\n", static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(freed).count()));
	CHECK_EQ(metric("xapiand_http_requests_cancelled"), before + 1);
	hold.release();
}

static void test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted() {
	Watchdog watchdog("a search whose client leaves", 60s);
	client_leaves_a_search("left_search", true);
}

// The schema's first load reads the metadata with the search's budget, so its turn wait ends with the token too.
static void test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled() {
	Watchdog watchdog("a search whose client leaves in its first schema load", 60s);
	client_leaves_a_search("left_cold_search", false);
}

// Both admission peaks rise under load and /:metrics shows them (T8): writers that wait for a held turn fill the
// reactor's gate, and a script's slices wait demoted while other requests arrive.
static void test_the_admission_peaks_rise_under_load() {
	Watchdog watchdog("the admission peaks rise under load", 120s);
	create_index("peaks", 1);
	constexpr std::size_t writers = 8;
	std::vector<std::thread> threads;
	{
		HoldEveryShard hold("peaks");
		for (std::size_t i = 0; i < writers; ++i) {
			threads.emplace_back([i] { call("PUT", "/peaks/" + std::to_string(i), R"({"v":1})"); });
		}
		CHECK(eventually([] { return requests_inflight() == writers; }));
		hold.release();
	}
	for (auto& t : threads) {
		t.join();
	}
	CHECK_GE(metric("xapiand_admission_inflight_peak"), static_cast<double>(writers));

	// A script's slices wait demoted between its yields; scrapes admitted meanwhile find them.
	std::atomic<bool> scripted{false};
	std::thread scraper([&] {
		while (!scripted.load() && metric("xapiand_scheduler_demoted_peak") < 1) {
			std::this_thread::sleep_for(2ms);
		}
	});
	for (int i = 0; i < 5 && metric("xapiand_scheduler_demoted_peak") < 1; ++i) {
		const auto put = call("PUT", "/peaks/100", R"({"_script":"local n = 0; for i = 1, 10000000 do n = n + i end; _doc.n = n"})");
		CHECK(success(put));
		if (!success(put)) {
			std::fprintf(stderr, "  the script's PUT answered %d: %s\n", put.status, put.body.to_string().c_str());
		}
	}
	scripted = true;
	scraper.join();
	CHECK_GE(metric("xapiand_scheduler_demoted_peak"), 1.0);
}

// HEAD answers with the status and headers a GET would, and no body (RFC 9110 9.3.2), a miss included; OPTIONS lists
// the verbs the node routes, and none it does not (STORE).
static void test_head_has_no_body_and_options_lists_the_routed_verbs() {
	Watchdog watchdog("head has no body and options lists the routed verbs");
	CHECK(http_call(node_port, "PUT", "/head_idx/1", "{\"a\":\"b\"}").rfind("HTTP/1.1 2", 0) == 0);
	const auto body_of = [](const std::string& response) {
		const auto end = response.find("\r\n\r\n");
		return end == std::string::npos ? std::string("<no head>") : response.substr(end + 4);
	};
	const auto miss = http_call(node_port, "HEAD", "/head_idx/2");
	CHECK(miss.rfind("HTTP/1.1 404", 0) == 0);
	CHECK_EQ(body_of(miss), std::string());
	const auto hit = http_call(node_port, "HEAD", "/head_idx/1");
	CHECK(hit.rfind("HTTP/1.1 200", 0) == 0);
	CHECK_EQ(body_of(hit), std::string());

	const auto options = http_call(node_port, "OPTIONS", "/");
	const auto at = options.find("Allow: ");
	CHECK(at != std::string::npos);
	const auto allow = options.substr(at, options.find("\r\n", at) - at);
	CHECK(allow.find(" STORE") == std::string::npos);   // RESTORE is a verb of its own
	for (const char* verb : {"GET", "POST", "PUT", "PATCH", "UPDATE", "UPSERT", "DELETE", "HEAD", "OPTIONS", "SEARCH", "COUNT", "INFO", "COMMIT", "DUMP", "RESTORE", "CHECK", "FLUSH", "QUIT"}) {
		CHECK(allow.find(std::string(" ") + verb) != std::string::npos);
	}
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
		{"test_a_put_whose_client_half_closes_completes_durably", test_a_put_whose_client_half_closes_completes_durably},
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
		{"test_document_ids_are_themselves", test_document_ids_are_themselves},
		{"test_a_version_is_a_precondition_of_every_write", test_a_version_is_a_precondition_of_every_write},
		{"test_writers_expecting_one_version_apply_once", test_writers_expecting_one_version_apply_once},
		{"test_a_versioned_delete_checks_the_document_its_key_names", test_a_versioned_delete_checks_the_document_its_key_names},
		{"test_versioned_writers_match_a_sequential_model", test_versioned_writers_match_a_sequential_model},
		{"test_a_restore_into_shards_writes_a_few_segments", test_a_restore_into_shards_writes_a_few_segments},
		{"test_an_existing_id_written_again_lands_on_its_shard", test_an_existing_id_written_again_lands_on_its_shard},
		{"test_a_replace_that_must_exist_writes_only_where_the_key_is", test_a_replace_that_must_exist_writes_only_where_the_key_is},
		{"test_a_stale_write_does_not_put_an_id_on_two_shards", test_a_stale_write_does_not_put_an_id_on_two_shards},
		{"test_a_replace_by_term_of_an_id_erased_since_its_lookup_places_it_afresh", test_a_replace_by_term_of_an_id_erased_since_its_lookup_places_it_afresh},
		{"test_a_patch_of_an_id_erased_since_its_read_patches_the_new_document", test_a_patch_of_an_id_erased_since_its_read_patches_the_new_document},
		{"test_new_numeric_ids_spread_over_the_shards_while_buffered", test_new_numeric_ids_spread_over_the_shards_while_buffered},
		{"test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted", test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted},
		{"test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled", test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled},
		{"test_the_admission_peaks_rise_under_load", test_the_admission_peaks_rise_under_load},
		{"test_head_has_no_body_and_options_lists_the_routed_verbs", test_head_has_no_body_and_options_lists_the_routed_verbs},
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
