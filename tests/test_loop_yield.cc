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
// RESTORE prepares its objects in chores, in the order a sequential restore sees them. The node runs in this process
// with one HTTP loop.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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


// A RESTORE's plain objects are prepared in chores while its HTTP loop applies them: the loop's CPU for a text-heavy
// restore is well below its CPU when every object is prepared on the loop, as an update's is (D1: it reads the stored
// document, so its prepare waits for every earlier object). Both are measured here, so the check holds on any core count.
static void test_a_restore_prepares_its_objects_off_its_loop() {
	constexpr int documents = 2000;
	// An update of a new record indexes it, prepared on the loop.
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
	// The loop's CPU of restoring the same documents into an index.
	auto loop_cpu = [&](const std::string& index, bool on_loop) {
		CHECK_EQ(http_call("RESTORE", index + "?commit", lines(1, 10, on_loop), "application/x-ndjson") / 100, 2);   // the schema exists after it
		const auto body = lines(11, documents, on_loop);
		const auto loop_before = loop_cpu_ms();
		const auto process_before = process_cpu_ms();
		CHECK_EQ(http_call("RESTORE", index + "?commit", body, "application/x-ndjson") / 100, 2);
		const auto loop_ms = loop_cpu_ms() - loop_before;
		const auto process_ms = process_cpu_ms() - process_before;
		std::fprintf(stderr, "  restore of %d documents %s: loop %.1f ms of the process's %.1f ms CPU\n", documents,
			on_loop ? "prepared on the loop" : "prepared in chores", loop_ms, process_ms);
		CHECK_GT(process_ms, 200.0);   // long enough to tell
		return loop_ms;
	};
	const auto on_loop_ms = loop_cpu("/onloop/", true);
	const auto chores_ms = loop_cpu("/offloop/", false);
	std::fprintf(stderr, "  loop CPU: %.1f ms with the prepares in chores, %.1f ms on the loop\n", chores_ms, on_loop_ms);
	CHECK_LT(chores_ms, on_loop_ms * 0.6);
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


int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	test_a_search_of_many_hits_yields_its_loop_between_hits();
	test_a_restored_record_is_compressed_once_and_reads_back();
	test_a_restore_prepares_its_objects_off_its_loop();
	test_an_update_sees_the_earlier_objects_of_its_stream();
	test_schema_changes_commit_in_input_order();
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
