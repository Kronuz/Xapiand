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

// What a request costs its HTTP loop (docs/perf/x7-loop-stalls.md): a search that returns thousands of hits keeps
// its loop's 1 ms probe answered while it reads them, a restored record is compressed once, in its write, and a
// RESTORE prepares its objects in chores, in the order a sequential restore sees them, and a light request runs ahead
// of heavy turns (docs/design/loop-scheduling.md). The node runs in this process with one HTTP loop.

#include "test_harness.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <time.h>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/data.h"
#include "database/handler.h"
#include "database/pool.h"
#include "database/utils.h"
#include "endpoint.h"
#include "http_asio.h"
#include "manager.h"
#include "msgpack.h"
#include "opts.h"
#include "rapidjson/document.h"
#include "reactor_lag.h"
#include "traceback.h"
#include "xapiand_exception.h"

using namespace std::chrono_literals;

namespace {

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(5ms);
	}
	return done();
}

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

const int node_port = free_port();

// A node in this process with one HTTP loop, in a data directory of its own.
class RunningNode {
public:
	RunningNode() {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;   // as main() does: a stalled loop's watchdog collects the threads' stacks with SIGUSR2
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		const auto root = std::filesystem::current_path() / "loop_yield_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(node_port), "--name", "loopyield", "-D", root.string(), "--shards", "1", "--processors", "1"};
		std::vector<char*> argv;
		for (auto& arg : args) {
			argv.push_back(arg.data());
		}
		opts = parseOptions(static_cast<int>(argv.size()), argv.data());
		XapiandManager::make();
		thread_ = std::thread([this] {
			try {
				XapiandManager::manager()->run();
			} catch (const SystemExit&) { }
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

private:
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
};

// One request on a connection of its own, its body sent in `pieces` with `pause` between them; the response's status
// (0 when there is none), its body into `reply`.
int http_call_pieces(const std::string& method, const std::string& path, const std::vector<std::string>& pieces, const std::string& content_type, std::chrono::milliseconds pause, std::string* reply = nullptr) {
	int fd = -1;
	for (int attempt = 0; attempt < 100 && fd < 0; ++attempt) {
		fd = ::socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(node_port));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
			::close(fd);
			fd = -1;
			std::this_thread::sleep_for(50ms);
		}
	}
	if (fd < 0) {
		return 0;
	}
	timeval timeout{60, 0};
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	std::size_t length = 0;
	for (const auto& piece : pieces) {
		length += piece.size();
	}
	const auto send_all = [fd](const std::string& data) {
		for (std::size_t sent = 0; sent < data.size();) {
			const auto n = ::send(fd, data.data() + sent, data.size() - sent, 0);
			if (n <= 0) {
				return;
			}
			sent += static_cast<std::size_t>(n);
		}
	};
	send_all(method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(length) + "\r\n\r\n");
	for (std::size_t i = 0; i < pieces.size(); ++i) {
		if (i != 0) {
			std::this_thread::sleep_for(pause);
		}
		send_all(pieces[i]);
	}
	std::string response;
	char buf[65536];
	ssize_t n;
	while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
		response.append(buf, static_cast<std::size_t>(n));
	}
	::close(fd);
	if (reply != nullptr) {
		const auto end = response.find("\r\n\r\n");
		*reply = end == std::string::npos ? std::string() : response.substr(end + 4);
	}
	return response.rfind("HTTP/1.1 ", 0) == 0 && response.size() > 12 ? std::atoi(response.c_str() + 9) : 0;
}

// One request on a connection of its own; the response's status (0 when there is none), its body into `reply`.
int http_call(const std::string& method, const std::string& path, const std::string& body, const std::string& content_type = "application/json", std::string* reply = nullptr) {
	return http_call_pieces(method, path, {body}, content_type, 0ms, reply);
}

// A JSON reply as a MsgPack.
MsgPack json_of(const std::string& reply) {
	rapidjson::Document rdoc;
	json_load(rdoc, reply);
	return MsgPack(rdoc);
}

// The value of a reply's field, as named or with the prefix the response may give it ("#items", "_schema");
// undefined when there is none.
MsgPack field_of(const MsgPack& obj, std::string_view name) {
	if (obj.is_map()) {
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			const auto key = it->str_view();
			if (key == name || (key.size() == name.size() + 1 && (key[0] == '#' || key[0] == '_') && key.substr(1) == name)) {
				return it.value();
			}
		}
	}
	return MsgPack();
}

// The first value named `name` anywhere in `obj`, depth first; undefined when there is none.
MsgPack find_field(const MsgPack& obj, std::string_view name) {
	if (obj.is_map()) {
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			if (it->str_view() == name) {
				return it.value();
			}
			auto found = find_field(it.value(), name);
			if (!found.is_undefined()) {
				return found;
			}
		}
	}
	return MsgPack();
}

double thread_cpu_ms() {
	timespec ts;
	::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
}

double process_cpu_ms() {
	rusage usage;
	::getrusage(RUSAGE_SELF, &usage);
	const auto ms = [](const timeval& tv) { return static_cast<double>(tv.tv_sec) * 1e3 + static_cast<double>(tv.tv_usec) / 1e3; };
	return ms(usage.ru_utime) + ms(usage.ru_stime);
}

// The CPU time the HTTP loop's thread has used, read on that thread.
double loop_cpu_ms() {
	std::atomic<bool> done{false};
	double ms = 0;
	asio::post(XapiandManager::manager()->http_service->reactor(0).io(), [&] {
		ms = thread_cpu_ms();
		done = true;
	});
	eventually([&] { return done.load(); }, 10s);
	return ms;
}

// Keeps the HTTP loop's probe at its 1 ms interval while a request runs.
class Pulse {
public:
	explicit Pulse(reactor::LagProbe& probe) : thread_([this, &probe] {
		while (!stop_) {
			probe.note_activity();
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

// The upper edge, in microseconds, of the latest wake between two snapshots of one probe.
std::uint64_t worst_us_between(const reactor::LagSnapshot& before, const reactor::LagSnapshot& after) {
	const auto d = after.since(before);
	std::uint64_t worst = 0;
	for (std::size_t i = 0; i < reactor::LagSnapshot::buckets; ++i) {
		if (d.count[i] != 0) {
			worst = reactor::LagSnapshot::upper_us(i);
		}
	}
	return worst;
}

// Creates the index `name` with `shards` shards and no replicas.
void create_index(const std::string& name, int shards) {
	const auto settings = R"({"_settings":{"number_of_shards":)" + std::to_string(shards) + R"(,"number_of_replicas":0}})";
	CHECK_EQ(http_call("PUT", "/" + name + "/", settings) / 100, 2);
}

// A RESTORE's items, one per object, as strings.
std::vector<std::string> items_of(const std::string& reply) {
	std::vector<std::string> items;
	const auto all = field_of(json_of(reply), "items");
	for (std::size_t i = 0; i < all.size(); ++i) {
		items.push_back(all.at(i).to_string());
	}
	return items;
}

// The items of `objects` restored one object per request: the serial model of a RESTORE.
std::vector<std::string> restored_one_by_one(const std::string& index, const std::vector<std::string>& objects) {
	std::vector<std::string> items;
	for (std::size_t i = 0; i < objects.size(); ++i) {
		std::string reply;
		const auto commit = i + 1 == objects.size() ? "?commit" : "";
		CHECK_EQ(http_call("RESTORE", "/" + index + "/" + commit, "[" + objects[i] + "]", "application/json", &reply) / 100, 2);
		const auto one = items_of(reply);
		items.push_back(one.size() == 1 ? one[0] : std::string("none"));
	}
	return items;
}

// The first difference of two runs' items, or none.
bool same_items(const std::vector<std::string>& parallel, const std::vector<std::string>& serial) {
	CHECK_EQ(parallel.size(), serial.size());
	bool same = parallel.size() == serial.size();
	for (std::size_t i = 0; i < parallel.size() && i < serial.size(); ++i) {
		if (parallel[i] != serial[i]) {
			std::fprintf(stderr, "  object %zu: %s / %s\n", i, parallel[i].c_str(), serial[i].c_str());
			same = false;
		}
	}
	return same;
}

// Fixes the objects each prepare chore takes, so a range holds that many objects on any build; clears the seams
// and the run's report.
class FixedRanges {
public:
	explicit FixedRanges(std::size_t objects) {
		DocIndexer::stats.reset();
		DocIndexer::seams.objects_per_prepare = objects;
	}
	~FixedRanges() {
		DocIndexer::seams.objects_per_prepare = 0;
		DocIndexer::seams.writes_per_slice = 0;
		DocIndexer::seams.after_placement = nullptr;
		DocIndexer::seams.after_slice = nullptr;
		DocIndexer::seams.throw_in_slice = false;
		DocIndexer::seams.stale_on_term.clear();
		DocIndexer::seams.serial = false;
		DocIndexer::seams.busy_placement_shard = 0;
	}
};

// The lanes a request's slices run in, seen from DocIndexer's after_placement seam on the loop: the seam spends slices
// of the request's budget and reads task::current().heavy() after each (docs/design/loop-scheduling.md D1, D2).
namespace lanes {

constexpr unsigned light_slices = 8;     // REQUEST_LIGHT_SLICES
constexpr unsigned stream_slices = 4;    // REQUEST_STREAM_SLICES

// Runs on the loop until the budget's slice is spent, then yields: one spent slice.
task::task<void> spend(task::budget& b) {
	while (!b.exhausted()) { }
	co_await b.checkpoint();
}

// The seam's job, set by each test before its requests.
enum class Mode { none, heavy_then_light, spend_heavy, spend_heavy_and_throw, record_entry, record_chunks };
std::atomic<Mode> mode{Mode::none};

// heavy_then_light: the RESTOREs that enter first spin slices until `stop`; once `light_armed`, the next one spends
// three slices and records how many heavy turns ran while each waited.
std::atomic<bool> stop{false};
std::atomic<bool> light_armed{false};
std::atomic<bool> light_taken{false};
std::atomic<int> heavy_ready{0};          // heavy RESTOREs past light_slices spent slices
std::atomic<long> heavy_turns{0};
std::atomic<int> lane_mismatches{0};      // a resume whose lane is not spent() >= light_slices
std::atomic<long> light_waited_most{-1};  // the most heavy turns between two turns of the light request
std::atomic<bool> light_went_heavy{false};

// record_entry: the lane the seam was entered in. record_chunks: per call, the spent count at entry and the lane
// after each of its spent slices.
std::atomic<int> entered_heavy{-1};
std::mutex chunks_mtx;
std::vector<std::pair<unsigned, std::vector<bool>>> chunks;

task::task<void> seam(DatabaseHandler& db) {
	auto& b = *db.budget;
	switch (mode.load()) {
		case Mode::heavy_then_light: {
			if (light_armed.load() && !light_taken.exchange(true)) {
				for (int i = 0; i < 3; ++i) {
					while (!b.exhausted()) { }
					const auto before = heavy_turns.load();
					co_await b.checkpoint();
					const auto waited = heavy_turns.load() - before;
					if (waited > light_waited_most.load()) {
						light_waited_most = waited;
					}
					light_went_heavy = light_went_heavy.load() || task::current().heavy();
				}
				stop = true;
				co_return;
			}
			bool ready = false;
			while (!stop.load()) {
				co_await spend(b);
				++heavy_turns;
				if (task::current().heavy() != (b.spent() >= light_slices)) {
					++lane_mismatches;
				}
				if (!ready && b.spent() >= light_slices) {
					ready = true;
					++heavy_ready;
				}
			}
			co_return;
		}
		case Mode::spend_heavy:
		case Mode::spend_heavy_and_throw:
			for (unsigned i = 0; i <= light_slices; ++i) {
				co_await spend(b);
			}
			if (mode.load() == Mode::spend_heavy_and_throw) {
				THROW(ClientError, "The seam fails this request");
			}
			co_return;
		case Mode::record_entry:
			entered_heavy = task::current().heavy() ? 1 : 0;
			co_return;
		case Mode::record_chunks: {
			std::pair<unsigned, std::vector<bool>> chunk{b.spent(), {}};
			for (unsigned i = 0; i < light_slices; ++i) {
				co_await spend(b);
				chunk.second.push_back(task::current().heavy());
			}
			std::lock_guard<std::mutex> lk(chunks_mtx);
			chunks.push_back(std::move(chunk));
			co_return;
		}
		case Mode::none:
			co_return;
	}
}

// Sets the seam for a test and clears it after.
class Seam {
public:
	explicit Seam(Mode m) {
		stop = false;
		light_armed = false;
		light_taken = false;
		heavy_ready = 0;
		heavy_turns = 0;
		lane_mismatches = 0;
		light_waited_most = -1;
		light_went_heavy = false;
		entered_heavy = -1;
		chunks.clear();
		mode = m;
		DocIndexer::seams.after_placement = seam;
	}
	~Seam() {
		DocIndexer::seams.after_placement = nullptr;
		mode = Mode::none;
	}
};

}  // namespace lanes

// Requests on one keep-alive connection, each response read to its Content-Length.
class KeepAlive {
public:
	KeepAlive() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(node_port));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (fd_ >= 0 && ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
			::close(fd_);
			fd_ = -1;
		}
		timeval timeout{60, 0};
		if (fd_ >= 0) {
			::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		}
	}
	~KeepAlive() {
		if (fd_ >= 0) {
			::close(fd_);
		}
	}
	KeepAlive(const KeepAlive&) = delete;
	KeepAlive& operator=(const KeepAlive&) = delete;

	// The response's status, or 0 when the connection gave none.
	int call(const std::string& method, const std::string& path, const std::string& body, const std::string& content_type = "application/json") {
		if (fd_ < 0) {
			return 0;
		}
		const auto request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
		for (std::size_t sent = 0; sent < request.size();) {
			const auto n = ::send(fd_, request.data() + sent, request.size() - sent, 0);
			if (n <= 0) {
				return 0;
			}
			sent += static_cast<std::size_t>(n);
		}
		std::size_t end;
		while ((end = buffer_.find("\r\n\r\n")) == std::string::npos) {
			if (!fill()) {
				return 0;
			}
		}
		std::string head = buffer_.substr(0, end);
		for (auto& c : head) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		const auto at = head.find("\r\ncontent-length:");
		const std::size_t length = at == std::string::npos ? 0 : std::strtoull(head.c_str() + at + 17, nullptr, 10);
		while (buffer_.size() < end + 4 + length) {
			if (!fill()) {
				return 0;
			}
		}
		const int status = head.rfind("http/1.1 ", 0) == 0 && head.size() > 12 ? std::atoi(head.c_str() + 9) : 0;
		buffer_.erase(0, end + 4 + length);
		return status;
	}

private:
	bool fill() {
		char buf[65536];
		const auto n = ::recv(fd_, buf, sizeof(buf), 0);
		if (n <= 0) {
			return false;
		}
		buffer_.append(buf, static_cast<std::size_t>(n));
		return true;
	}

	int fd_;
	std::string buffer_;
};

}  // namespace


// A search of 5,000 hits reads each hit's record after the search (the hit loop): the loop answers its probe
// within a fraction of the request's time, since the hits yield between slices. Without the yield the probe
// waits for the whole hit loop, which is most of the request.
static void test_a_search_of_many_hits_yields_its_loop_between_hits() {
	constexpr int documents = 5000;
	const std::string text = "a record long enough that a stored frame holds a few hundred of them and a hit costs a read of its frame; ";
	std::string body;
	for (int i = 1; i <= documents; ++i) {
		body += R"({"_id":)" + std::to_string(i) + R"(,"n":")" + std::to_string(i) + R"(","text":")" + text + text + text + "\"}\n";
	}
	CHECK_EQ(http_call("RESTORE", "/hits/?commit", body, "application/x-ndjson") / 100, 2);
	const std::string search = R"({"_query":"*","_limit":)" + std::to_string(documents) + "}";
	CHECK_EQ(http_call("SEARCH", "/hits/", search), 200);   // warms the schema and the index

	auto& probe = XapiandManager::manager()->http_service->reactor(0).lag();
	Pulse pulse(probe);
	std::this_thread::sleep_for(1100ms);   // the probe is at its 1 ms interval
	const auto before = probe.snapshot();
	const auto began = std::chrono::steady_clock::now();
	CHECK_EQ(http_call("SEARCH", "/hits/", search), 200);
	const auto took_us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - began).count());
	const auto worst_us = worst_us_between(before, probe.snapshot());
	std::fprintf(stderr, "  search of %d hits took %llu us; the loop's worst probe wake %llu us\n", documents,
		static_cast<unsigned long long>(took_us), static_cast<unsigned long long>(worst_us));
	CHECK_GT(took_us, 20000u);           // long enough to tell a yielding hit loop from one that is not
	CHECK_LT(worst_us * 3, took_us);     // the loop was answered within a third of the request
}


// A RESTORE compresses each record once, in its write (finalise_write), not also in its prepare, and the record reads
// back as it was restored, with its id and version.
static void test_a_restored_record_is_compressed_once_and_reads_back() {
	constexpr int documents = 200;
	const std::string text = "a record long enough to be compressed, and repeated so that it compresses; ";
	auto lines = [&](int first, int count) {
		std::string body;
		for (int i = first; i < first + count; ++i) {
			body += R"({"_id":)" + std::to_string(i) + R"(,"n":")" + std::to_string(i) + R"(","text":")" + text + text + "\"}\n";
		}
		return body;
	};
	CHECK_EQ(http_call("RESTORE", "/once/?commit", lines(1, 10), "application/x-ndjson") / 100, 2);   // the schema exists after it
	const auto before = Locator::compressions();
	CHECK_EQ(http_call("RESTORE", "/once/?commit", lines(11, documents), "application/x-ndjson") / 100, 2);
	const auto compressed = Locator::compressions() - before;
	std::fprintf(stderr, "  %d restored records, %llu compressions\n", documents, static_cast<unsigned long long>(compressed));
	CHECK_EQ(compressed, static_cast<std::uint64_t>(documents));

	for (const int id : {11, 100, 10 + documents}) {
		std::string reply;
		const auto status = http_call("GET", "/once/" + std::to_string(id), "", "application/json", &reply);
		CHECK_EQ(status, 200);
		if (status != 200) {
			continue;
		}
		const auto obj = json_of(reply);
		CHECK_EQ(field_of(obj, "n").to_string(), "\"" + std::to_string(id) + "\"");
		CHECK_EQ(field_of(obj, "text").to_string(), "\"" + text + text + "\"");
		CHECK_EQ(field_of(obj, "_id").to_string(), std::to_string(id));
		CHECK_EQ(field_of(obj, "_version").to_string(), std::string("1"));
	}
}


// A RESTORE's plain objects are prepared and written in chores, the writes one child per shard: the HTTP loop's CPU
// for a text-heavy restore into 5 shards is well below its CPU when every object is prepared and written on the loop,
// as an update's is (D1: it reads the stored document, so it waits for every earlier object). Both are measured here,
// so the check holds on any core count.
static void test_a_restore_prepares_and_writes_its_objects_off_its_loop() {
	constexpr int documents = 2000;
	// An update of a new record indexes it, prepared and written on the loop.
	auto lines = [](int first, int count, bool on_loop) {
		std::string body;
		for (int i = first; i < first + count; ++i) {
			std::string text;
			for (int w = 0; w < 1000; ++w) {
				text += "word" + std::to_string((i * 131 + w * 7) % 20011) + " ";
			}
			body += R"({"_id":)" + std::to_string(i) + (on_loop ? R"(,"_op_type":"update")" : "") + R"(,"n":)" + std::to_string(i) + R"(,"text":")" + text + "\"}\n";
		}
		return body;
	};
	// The loop's CPU of restoring the same documents into a 5-shard index.
	auto loop_cpu = [&](const std::string& index, bool on_loop) {
		create_index(index, 5);
		CHECK_EQ(http_call("RESTORE", "/" + index + "/?commit", lines(1, 10, on_loop), "application/x-ndjson") / 100, 2);   // the schema exists after it
		const auto body = lines(11, documents, on_loop);
		DocIndexer::stats.reset();
		const auto loop_before = loop_cpu_ms();
		const auto process_before = process_cpu_ms();
		CHECK_EQ(http_call("RESTORE", "/" + index + "/?commit", body, "application/x-ndjson") / 100, 2);
		const auto loop_ms = loop_cpu_ms() - loop_before;
		const auto process_ms = process_cpu_ms() - process_before;
		const auto children = DocIndexer::stats.children.load();
		const auto peak = DocIndexer::stats.peak_children.load();
		std::fprintf(stderr, "  restore of %d documents %s: loop %.1f ms of the process's %.1f ms CPU; %zu children at most, %zu at once\n", documents,
			on_loop ? "on the loop" : "in chores", loop_ms, process_ms, children, peak);
		CHECK_GT(process_ms, 200.0);   // long enough to tell
		if (!on_loop && children > 1) {   // a pool of 5 threads or more
			CHECK_GT(peak, std::size_t{1});
		}
		return loop_ms;
	};
	const auto on_loop_ms = loop_cpu("onloop", true);
	const auto chores_ms = loop_cpu("offloop", false);
	std::fprintf(stderr, "  loop CPU: %.1f ms with the objects in chores, %.1f ms on the loop\n", chores_ms, on_loop_ms);
	CHECK_LT(chores_ms, on_loop_ms * 0.2);   // 0.06-0.09 measured; 0.34 with the restore fan-out off
}


// One RESTORE of a stream into 5 shards ends as the stream restored one object per request: ids repeated across it
// (a later object replacing an earlier one, within one shard's sub-run too), on every shard, and already there.
static void test_a_sharded_restore_matches_its_serial_model() {
	create_index("model_par", 5);
	create_index("model_seq", 5);
	std::string seed;
	for (int k = 0; k < 20; ++k) {
		seed += R"({"_id":"k)" + std::to_string(k) + R"(","v":-1})" "\n";
	}
	for (const auto* index : {"/model_par/?commit", "/model_seq/?commit"}) {
		CHECK_EQ(http_call("RESTORE", index, seed, "application/x-ndjson") / 100, 2);
	}
	std::vector<std::string> objects;
	std::string body;
	for (int i = 0; i < 300; ++i) {
		objects.push_back(R"({"_id":"k)" + std::to_string(i * 7 % 60) + R"(","v":)" + std::to_string(i) + "}");
		body += objects.back() + "\n";
	}
	std::string reply;
	{
		FixedRanges ranges(32);
		CHECK_EQ(http_call("RESTORE", "/model_par/?commit", body, "application/x-ndjson", &reply) / 100, 2);
		std::fprintf(stderr, "  largest range %zu, %zu ids repeated within a sub-run\n", DocIndexer::stats.largest_range.load(), DocIndexer::stats.repeated_in_subrun.load());
		CHECK_GE(DocIndexer::stats.largest_range.load(), std::size_t{16});
		CHECK_GE(DocIndexer::stats.repeated_in_subrun.load(), std::size_t{1});
	}
	CHECK_TRUE(same_items(items_of(reply), restored_one_by_one("model_seq", objects)));
	for (int k = 0; k < 60; ++k) {
		std::string par, seq;
		const auto path = "/k" + std::to_string(k);
		CHECK_EQ(http_call("GET", "/model_par" + path, "", "application/json", &par), 200);
		CHECK_EQ(http_call("GET", "/model_seq" + path, "", "application/json", &seq), 200);
		CHECK_EQ(field_of(json_of(par), "v").to_string(), field_of(json_of(seq), "v").to_string());
		CHECK_EQ(field_of(json_of(par), "_version").to_string(), field_of(json_of(seq), "_version").to_string());
	}
}


// Objects with no id in a numeric-id index of 5 shards each take the least used shard, which the writes before them
// change: they take the serial path after the writes before them, and get the ids the same RESTORE gets with every
// write on the serial path. One object per request is no model here: the least used shard counts a document replaced
// while still buffered twice, so those ids depend on when the buffer flushes.
static void test_new_numeric_ids_match_their_serial_model() {
	create_index("num_par", 5);
	create_index("num_seq", 5);
	for (const auto* index : {"/num_par/?commit", "/num_seq/?commit"}) {
		CHECK_EQ(http_call("RESTORE", index, R"({"_id":1,"n":0})" "\n", "application/x-ndjson") / 100, 2);   // integer ids
	}
	std::string body;
	for (int i = 1; i <= 40; ++i) {
		body += (i % 4 == 0 ? R"({"n":)" + std::to_string(i) + "}" : R"({"_id":)" + std::to_string(100 + i) + R"(,"n":)" + std::to_string(i) + "}") + "\n";
	}
	std::string reply, serial;
	{
		FixedRanges ranges(32);
		CHECK_EQ(http_call("RESTORE", "/num_par/?commit", body, "application/x-ndjson", &reply) / 100, 2);
		std::fprintf(stderr, "  largest range %zu, %zu new numeric ids after a join\n", DocIndexer::stats.largest_range.load(), DocIndexer::stats.serial_after_join.load());
		CHECK_GE(DocIndexer::stats.largest_range.load(), std::size_t{16});
		CHECK_GE(DocIndexer::stats.serial_after_join.load(), std::size_t{2});
		DocIndexer::seams.serial = true;
		CHECK_EQ(http_call("RESTORE", "/num_seq/?commit", body, "application/x-ndjson", &serial) / 100, 2);
	}
	CHECK_TRUE(same_items(items_of(reply), items_of(serial)));
	std::string par_found, seq_found;
	CHECK_EQ(http_call("SEARCH", "/num_par/?volatile", R"({"_limit":100})", "application/json", &par_found), 200);
	CHECK_EQ(http_call("SEARCH", "/num_seq/?volatile", R"({"_limit":100})", "application/json", &seq_found), 200);
	CHECK_EQ(field_of(json_of(par_found), "total").to_string(), field_of(json_of(seq_found), "total").to_string());
}


// A write whose document left between placement and apply (another request erased it and wrote it again) is
// placed afresh, and so is every later write of its id in the range, in input order: the document ends as the
// stream's last object of that id.
static void test_a_write_missed_since_placement_is_placed_afresh() {
	create_index("miss", 5);
	std::string seed;
	for (int k = 1; k <= 12; ++k) {
		seed += R"({"_id":"k)" + std::to_string(k) + R"(","v":"seed"})" "\n";
	}
	CHECK_EQ(http_call("RESTORE", "/miss/?commit", seed, "application/x-ndjson") / 100, 2);
	std::string body;
	for (int k = 1; k <= 12; ++k) {
		body += R"({"_id":"k)" + std::to_string(k) + R"(","v":"new"})" "\n";
		if (k == 3 || k == 9) {
			body += R"({"_id":"k7","v":")" + std::string(k == 3 ? "first" : "last") + "\"}\n";
		}
	}
	std::string reply;
	std::atomic<int> erased{0};
	{
		FixedRanges ranges(32);
		DocIndexer::seams.after_placement = [&erased](DatabaseHandler& handler) -> task::task<void> {
			if (erased.fetch_add(1) == 0) {
				co_await handler.delete_document("k7", UNKNOWN_REVISION);
				auto obj = MsgPack::MAP();
				obj["v"] = "erased and written again";
				co_await handler.index(MsgPack("k7"), UNKNOWN_REVISION, true, obj, false, json_type);
			}
		};
		const auto status = http_call("RESTORE", "/miss/?commit", body, "application/x-ndjson", &reply);
		CHECK_EQ(status / 100, 2);
		if (status / 100 != 2) {
			std::fprintf(stderr, "  RESTORE: %d %s\n", status, reply.c_str());
		}
		std::fprintf(stderr, "  %zu writes deferred\n", DocIndexer::stats.deferred.load());
		CHECK_GE(erased.load(), 1);
		CHECK_EQ(DocIndexer::stats.deferred.load(), std::size_t{3});   // k7 three times, each missed
	}
	for (const auto& item : items_of(reply)) {
		CHECK_TRUE(item.find("status") == std::string::npos);
	}
	std::string got;
	CHECK_EQ(http_call("GET", "/miss/k7", "", "application/json", &got), 200);
	CHECK_EQ(field_of(json_of(got), "v").to_string(), std::string("\"last\""));
}


// The review's interleaving (B1): an id is erased before its first write of the range applies (that write misses),
// and written again, taking its docid, before its next write: that write must not replace the new document in
// place, where the deferred first write would then land over it. Every later write of the id waits for the serial
// pass, and the document ends as the stream's last object.
static void test_a_miss_defers_every_later_write_of_its_id() {
	create_index("reuse", 5);
	std::string seed;
	for (int k = 1; k <= 12; ++k) {
		seed += R"({"_id":)" + std::to_string(k) + R"(,"v":"seed"})" "\n";
	}
	CHECK_EQ(http_call("RESTORE", "/reuse/?commit", seed, "application/x-ndjson") / 100, 2);
	std::string body;
	for (int k = 1; k <= 12; ++k) {
		body += R"({"_id":)" + std::to_string(k) + R"(,"v":"new"})" "\n";
		if (k == 3 || k == 9) {
			body += R"({"_id":7,"v":")" + std::string(k == 3 ? "first" : "last") + "\"}\n";
		}
	}
	std::string reply;
	std::atomic<int> erased{0};
	std::atomic<int> reborn{0};
	{
		FixedRanges ranges(32);
		DocIndexer::seams.writes_per_slice = 1;
		DocIndexer::seams.after_placement = [&erased](DatabaseHandler& handler) -> task::task<void> {
			if (erased.fetch_add(1) == 0) {
				co_await handler.delete_document("7", UNKNOWN_REVISION);
			}
		};
		DocIndexer::seams.after_slice = [&reborn](DatabaseHandler& handler, const std::string&, bool missed) -> task::task<void> {
			if (missed && reborn.fetch_add(1) == 0) {
				auto obj = MsgPack::MAP();
				obj["v"] = "written again";
				co_await handler.index(MsgPack(7), UNKNOWN_REVISION, true, obj, false, json_type);
			}
		};
		const auto status = http_call("RESTORE", "/reuse/?commit", body, "application/x-ndjson", &reply);
		CHECK_EQ(status / 100, 2);
		std::fprintf(stderr, "  %zu writes deferred\n", DocIndexer::stats.deferred.load());
		CHECK_GE(erased.load(), 1);
		CHECK_GE(reborn.load(), 1);
		CHECK_EQ(DocIndexer::stats.deferred.load(), std::size_t{3});   // id 7 three times, the later two without a try
	}
	for (const auto& item : items_of(reply)) {
		CHECK_TRUE(item.find("status") == std::string::npos);
	}
	std::string reused;
	CHECK_EQ(http_call("GET", "/reuse/7", "", "application/json", &reused), 200);
	CHECK_EQ(field_of(json_of(reused), "v").to_string(), std::string("\"last\""));
}


// An apply slice that fails before its writes (a pin or a turn that cannot be had, here a thrown slice) fails its
// shard's next write alone, and the rest retry, each its own result as one request each would have; the engine's chores
// are restored: no engine runs its chores inline afterwards, and a later RESTORE writes.
static void test_a_throwing_apply_slice_fails_one_write_and_restores_the_engines_chores() {
	create_index("throws", 5);
	CHECK_EQ(http_call("RESTORE", "/throws/?commit", R"({"_id":"seed","v":1})" "\n", "application/x-ndjson") / 100, 2);   // the schema exists: no range of one
	std::string body;
	for (int k = 1; k <= 40; ++k) {
		body += R"({"_id":"k)" + std::to_string(k) + R"(","v":1})" "\n";
	}
	std::string reply;
	{
		FixedRanges ranges(32);
		DocIndexer::seams.throw_in_slice = true;
		CHECK_EQ(http_call("RESTORE", "/throws/?commit", body, "application/x-ndjson", &reply) / 100, 2);
		CHECK_FALSE(DocIndexer::seams.throw_in_slice.load());   // it threw
	}
	std::size_t failed = 0;
	for (const auto& item : items_of(reply)) {
		failed += item.find("status") != std::string::npos;
	}
	std::fprintf(stderr, "  %zu of 40 writes failed with the slice\n", failed);
	CHECK_EQ(failed, std::size_t{1});
	std::string found;
	CHECK_EQ(http_call("SEARCH", "/throws/?volatile", R"({"_limit":100})", "application/json", &found), 200);
	CHECK_EQ(field_of(json_of(found), "total").to_string(), std::string("40"));   // the seed and 39 of 40
	CHECK_TRUE(eventually([] { return ShardEndpoint::engines_inline() == 0; }, 2s));
	CHECK_EQ(http_call("RESTORE", "/throws/?commit", R"({"_id":"k1","v":2})" "\n", "application/x-ndjson") / 100, 2);
	std::string got;
	CHECK_EQ(http_call("GET", "/throws/k1", "", "application/json", &got), 200);
	CHECK_EQ(field_of(json_of(got), "v").to_string(), std::string("2"));
}


// A write that fails and marks the index stale ends its slice, so the later writes of the shard run after the index is
// replaced, in a slice of their own, as one request each would: with the failing write, the run has one more slice
// than the same run with no failure (a slice takes a shard's whole sub-run unless it ends), the failing write alone
// fails, and every other object is stored.
static void test_a_stale_marking_failure_ends_its_slice() {
	create_index("stale_a", 5);
	create_index("stale_b", 5);
	for (const auto* index : {"/stale_a/?commit", "/stale_b/?commit"}) {
		CHECK_EQ(http_call("RESTORE", index, R"({"_id":"seed","v":1})" "\n", "application/x-ndjson") / 100, 2);   // the schema exists: no range of one
	}
	std::string body;
	for (int k = 1; k <= 60; ++k) {
		body += R"({"_id":"k)" + std::to_string(k) + R"(","v":1})" "\n";
	}
	auto run = [&](const std::string& index, const std::string& failing, std::size_t& slices, std::size_t& failed, std::string& total) {
		std::atomic<std::size_t> counted{0};
		std::string reply;
		{
			FixedRanges ranges(1000);
			DocIndexer::seams.writes_per_slice = 1000;
			DocIndexer::seams.stale_on_term = failing;
			DocIndexer::seams.after_slice = [&counted](DatabaseHandler&, const std::string&, bool) -> task::task<void> {
				counted.fetch_add(1);
				co_return;
			};
			CHECK_EQ(http_call("RESTORE", "/" + index + "/?commit", body, "application/x-ndjson", &reply) / 100, 2);
		}
		slices = counted.load();
		failed = 0;
		for (const auto& item : items_of(reply)) {
			failed += item.find("status") != std::string::npos;
		}
		std::string found;
		CHECK_EQ(http_call("SEARCH", "/" + index + "/?volatile", R"({"_limit":100})", "application/json", &found), 200);
		total = field_of(json_of(found), "total").to_string();
	};
	std::size_t without_slices = 0, without_failed = 0, with_slices = 0, with_failed = 0;
	std::string without_total, with_total;
	run("stale_a", "", without_slices, without_failed, without_total);
	run("stale_b", "k1", with_slices, with_failed, with_total);
	std::fprintf(stderr, "  slices %zu without a failure, %zu with one; failed %zu; stored %s\n", without_slices, with_slices, with_failed, with_total.c_str());
	CHECK_EQ(without_failed, std::size_t{0});
	CHECK_EQ(with_failed, std::size_t{1});
	CHECK_EQ(with_total, std::string("60"));   // the seed and 59 of 60
	CHECK_EQ(with_slices, without_slices + 1);
	CHECK_TRUE(eventually([] { return ShardEndpoint::engines_inline() == 0; }, 2s));
}


// A placement read that fails (here shard 2's turn, busy) sends the terms it asked to the serial path after the join,
// in input order, each object with its own turn, as one request each would: no object fails, every object is
// stored, and the deferred objects are those of the ids neither shard 0 nor shard 1 holds (D68).
static void test_a_failed_placement_read_defers_its_terms_to_the_serial_path() {
	create_index("placebusy", 5);
	std::string seed;
	for (int k = 1; k <= 40; ++k) {
		seed += R"({"_id":)" + std::to_string(k) + R"(,"v":"seed"})" "\n";
	}
	CHECK_EQ(http_call("RESTORE", "/placebusy/?commit", seed, "application/x-ndjson") / 100, 2);
	std::string body;
	std::size_t affected = 0;
	for (int k = 1; k <= 40; ++k) {
		body += R"({"_id":)" + std::to_string(k) + R"(,"v":"new"})" "\n";
		affected += (k - 1) % 5 >= 2;   // id k is on shard (k - 1) % 5: shards 2-4 are asked of shard 2
		if (k == 20) {
			body += R"({"_id":3,"v":"last"})" "\n";   // a second object of a deferred id, later in the stream
			++affected;
		}
	}
	std::string reply;
	{
		FixedRanges ranges(64);
		DocIndexer::seams.busy_placement_shard = 3;
		const auto status = http_call("RESTORE", "/placebusy/?commit", body, "application/x-ndjson", &reply);
		CHECK_EQ(status / 100, 2);
		std::fprintf(stderr, "  %zu writes deferred of %zu affected\n", DocIndexer::stats.deferred.load(), affected);
		CHECK_EQ(DocIndexer::seams.busy_placement_shard.load(), std::size_t{0});   // it threw
		CHECK_GE(DocIndexer::stats.largest_range.load(), std::size_t{16});
		CHECK_EQ(DocIndexer::stats.deferred.load(), affected);
	}
	const auto items = items_of(reply);
	CHECK_EQ(items.size(), std::size_t{41});
	for (const auto& item : items) {
		CHECK_TRUE(item.find("status") == std::string::npos);
	}
	for (int k = 1; k <= 40; ++k) {
		std::string got;
		CHECK_EQ(http_call("GET", "/placebusy/" + std::to_string(k), "", "application/json", &got), 200);
		CHECK_EQ(field_of(json_of(got), "v").to_string(), std::string(k == 3 ? "\"last\"" : "\"new\""));
	}
}


// An object that reads the stored document (an update) sees every earlier object of its stream applied, whether it
// follows a run of plain objects in its batch or starts the next batch.
static void test_an_update_sees_the_earlier_objects_of_its_stream() {
	std::string body = "[";
	for (int i = 1; i <= 40; ++i) {
		body += R"({"_id":"p)" + std::to_string(i) + R"(","n":)" + std::to_string(i) + "},";
	}
	body += R"({"_id":"a","first":1},{"_id":"a","_op_type":"update","second":2}])";
	CHECK_EQ(http_call("RESTORE", "/barrier/?commit", body) / 100, 2);

	// Two pieces with a pause: the update is the first object of the second batch.
	std::string first_piece;
	for (int i = 1; i <= 40; ++i) {
		first_piece += R"({"_id":"p)" + std::to_string(i) + R"(","n":)" + std::to_string(i) + "}\n";
	}
	first_piece += R"({"_id":"c","first":1})" "\n";
	std::string second_piece = R"({"_id":"c","_op_type":"update","second":2})" "\n";
	for (int i = 41; i <= 45; ++i) {
		second_piece += R"({"_id":"p)" + std::to_string(i) + R"(","n":)" + std::to_string(i) + "}\n";
	}
	CHECK_EQ(http_call_pieces("RESTORE", "/boundary/?commit", {first_piece, second_piece}, "application/x-ndjson", 500ms) / 100, 2);

	for (const auto* path : {"/barrier/a", "/boundary/c"}) {
		std::string reply;
		CHECK_EQ(http_call("GET", path, "", "application/json", &reply), 200);
		const auto obj = json_of(reply);
		std::fprintf(stderr, "  GET %s: %s\n", path, obj.to_string().c_str());
		CHECK_EQ(field_of(obj, "first").to_string(), std::string("1"));
		CHECK_EQ(field_of(obj, "second").to_string(), std::string("2"));
	}
}


// Schema changes commit in input order: a new index restored with `x` first an integer (in an object slow to
// prepare) and then a string (fast, so its chore ends first) ends as a sequential restore does, object by object.
static void test_schema_changes_commit_in_input_order() {
	std::string heavy;
	for (int w = 0; w < 8000; ++w) {
		heavy += "heavy" + std::to_string(w) + " ";
	}
	const std::vector<std::string> objects = {
		R"({"_id":1,"x":1,"text":")" + heavy + "\"}",
		R"({"_id":2,"x":"s"})",
		R"({"_id":3,"x":2})",
	};
	std::string body = "[";
	for (std::size_t i = 0; i < objects.size(); ++i) {
		body += (i != 0 ? "," : "") + objects[i];
	}
	body += "]";
	std::string reply;
	CHECK_EQ(http_call("RESTORE", "/order_par/?commit", body, "application/json", &reply) / 100, 2);
	const auto parallel = field_of(json_of(reply), "items");
	CHECK_EQ(parallel.size(), objects.size());

	for (std::size_t i = 0; i < objects.size(); ++i) {
		CHECK_EQ(http_call("RESTORE", "/order_seq/?commit", "[" + objects[i] + "]", "application/json", &reply) / 100, 2);
		const auto sequential = field_of(json_of(reply), "items");
		if (i < parallel.size() && sequential.size() == 1) {
			std::fprintf(stderr, "  object %zu: %s / %s\n", i, parallel.at(i).to_string().c_str(), sequential.at(0).to_string().c_str());
			CHECK_EQ(parallel.at(i).to_string(), sequential.at(0).to_string());
		}
	}
	if (parallel.size() > 1) {
		CHECK_TRUE(parallel.at(1).to_string().find("status") != std::string::npos);   // the string fails, as sequentially
	}

	std::string par_info, seq_info;
	CHECK_EQ(http_call("GET", "/order_par/", "", "application/json", &par_info), 200);
	CHECK_EQ(http_call("GET", "/order_seq/", "", "application/json", &seq_info), 200);
	const auto par_x = find_field(json_of(par_info), "x").to_string();
	std::fprintf(stderr, "  x in the schema: %s\n", par_x.c_str());
	CHECK_TRUE(par_x.find("integer") != std::string::npos);
	CHECK_EQ(par_x, find_field(json_of(seq_info), "x").to_string());
}


// A light request runs ahead of heavy turns (docs/design/loop-scheduling.md D1, D2): four RESTOREs past 8 spent slices
// are heavy and spin their slices in the heavy lane; a fifth, light, spends three slices, and at most one heavy turn
// runs between two of its turns. With every spent slice demoted (today's policy) it waits behind all four.
static void test_a_light_request_runs_ahead_of_heavy_turns() {
	constexpr int heavy = 4;
	for (int i = 0; i <= heavy; ++i) {
		CHECK_EQ(http_call("RESTORE", "/lane" + std::to_string(i) + "/?commit", R"([{"_id":"w","n":1}])") / 100, 2);   // the schema exists after it
	}
	lanes::Seam seam(lanes::Mode::heavy_then_light);
	std::vector<std::thread> heavies;
	std::atomic<int> heavy_ok{0};
	for (int i = 1; i <= heavy; ++i) {
		heavies.emplace_back([i, &heavy_ok] {
			if (http_call("RESTORE", "/lane" + std::to_string(i) + "/", R"([{"_id":"h","n":2}])") / 100 == 2) {
				++heavy_ok;
			}
		});
	}
	const bool ready = eventually([] { return lanes::heavy_ready.load() == heavy; }, 30s);
	CHECK_TRUE(ready);
	lanes::light_armed = true;
	const auto light = http_call("RESTORE", "/lane0/", R"([{"_id":"l","n":3}])");
	lanes::stop = true;
	for (auto& t : heavies) {
		t.join();
	}
	std::fprintf(stderr, "  %ld heavy turns; the light request waited at most %ld of them between its turns\n",
		lanes::heavy_turns.load(), lanes::light_waited_most.load());
	CHECK_EQ(light / 100, 2);
	CHECK_EQ(heavy_ok.load(), heavy);
	CHECK_GE(lanes::light_waited_most.load(), 0L);   // it spent its slices
	CHECK_LE(lanes::light_waited_most.load(), 1L);   // one heavy turn per pass of the loop
	CHECK_FALSE(lanes::light_went_heavy.load());
	CHECK_EQ(lanes::lane_mismatches.load(), 0);     // heavy exactly from the 8th spent slice, debug pumps included
}


// A request after a heavy one on the same connection starts light, also when the heavy one throws: the connection goes
// on in the light lane after each request (D2 f), so the next request's read and work are not heavy turns.
static void test_a_request_after_a_heavy_one_starts_light() {
	CHECK_EQ(http_call("RESTORE", "/lanenext/?commit", R"([{"_id":"w","n":1}])") / 100, 2);   // the schema exists after it
	for (const auto first : {lanes::Mode::spend_heavy, lanes::Mode::spend_heavy_and_throw}) {
		lanes::Seam seam(first);
		KeepAlive connection;
		const auto status = connection.call("RESTORE", "/lanenext/", R"([{"_id":"a","n":2}])");
		CHECK_EQ(status / 100, first == lanes::Mode::spend_heavy ? 2 : 4);
		lanes::mode = lanes::Mode::record_entry;
		CHECK_EQ(connection.call("RESTORE", "/lanenext/", R"([{"_id":"b","n":3}])") / 100, 2);
		std::fprintf(stderr, "  after a heavy request%s, the next one entered %s\n", first == lanes::Mode::spend_heavy ? "" : " that threw",
			lanes::entered_heavy.load() == 1 ? "heavy" : lanes::entered_heavy.load() == 0 ? "light" : "nothing");
		CHECK_EQ(lanes::entered_heavy.load(), 0);
	}
}


// Each body chunk of a streamed RESTORE starts light (D81) for REQUEST_STREAM_SLICES of its own slices (D82): the
// rearm runs before the chunk's children start. Without it the RESTORE stays heavy from its 8th slice on, so every
// later chunk's first slice is a heavy turn.
static void test_each_chunk_of_a_streamed_restore_starts_light() {
	CHECK_EQ(http_call("RESTORE", "/lanestream/?commit", R"([{"_id":"w","n":1}])") / 100, 2);   // the schema exists after it
	std::vector<std::string> pieces;
	for (int p = 0; p < 4; ++p) {
		std::string piece;
		for (int i = 0; i < 5; ++i) {
			piece += R"({"_id":"s)" + std::to_string(p * 5 + i) + R"(","n":)" + std::to_string(i) + "}\n";
		}
		pieces.push_back(piece);
	}
	lanes::Seam seam(lanes::Mode::record_chunks);
	CHECK_EQ(http_call_pieces("RESTORE", "/lanestream/?commit", pieces, "application/x-ndjson", 150ms) / 100, 2);
	std::lock_guard<std::mutex> lk(lanes::chunks_mtx);
	CHECK_GE(lanes::chunks.size(), std::size_t{2});
	bool first_light = true;
	bool lanes_by_count = true;
	for (const auto& [spent, heavy] : lanes::chunks) {
		std::string seen;
		for (std::size_t j = 0; j < heavy.size(); ++j) {
			seen += heavy[j] ? 'H' : 'l';
			lanes_by_count = lanes_by_count && heavy[j] == (spent + j + 1 >= lanes::light_slices);
		}
		std::fprintf(stderr, "  a chunk entered with %u slices spent; lanes of its slices %s\n", spent, seen.c_str());
		first_light = first_light && !heavy.empty() && !heavy[0];
		CHECK_LE(spent, lanes::light_slices - lanes::stream_slices);
	}
	CHECK_TRUE(first_light);       // every chunk's first spent slice runs light
	CHECK_TRUE(lanes_by_count);    // and turns heavy at the rearmed count
}


int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	test_a_search_of_many_hits_yields_its_loop_between_hits();
	test_a_restored_record_is_compressed_once_and_reads_back();
	test_a_restore_prepares_and_writes_its_objects_off_its_loop();
	test_a_sharded_restore_matches_its_serial_model();
	test_new_numeric_ids_match_their_serial_model();
	test_a_write_missed_since_placement_is_placed_afresh();
	test_a_miss_defers_every_later_write_of_its_id();
	test_a_throwing_apply_slice_fails_one_write_and_restores_the_engines_chores();
	test_a_stale_marking_failure_ends_its_slice();
	test_a_failed_placement_read_defers_its_terms_to_the_serial_path();
	test_an_update_sees_the_earlier_objects_of_its_stream();
	test_schema_changes_commit_in_input_order();
	test_a_light_request_runs_ahead_of_heavy_turns();
	test_a_request_after_a_heavy_one_starts_light();
	test_each_chunk_of_a_streamed_restore_starts_light();
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
