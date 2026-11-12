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

// A request's work after its search yields the HTTP loop between slices (docs/perf/x7-loop-stalls.md): a search
// that returns thousands of hits keeps its loop's 1 ms probe answered while it reads them. The node runs in this
// process with one HTTP loop.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "endpoint.h"
#include "http_asio.h"
#include "manager.h"
#include "opts.h"
#include "reactor_lag.h"
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

// One request on a connection of its own; the response's status (0 when there is none).
int http_call(const std::string& method, const std::string& path, const std::string& body, const std::string& content_type = "application/json") {
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
	const auto request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
	for (std::size_t sent = 0; sent < request.size();) {
		const auto n = ::send(fd, request.data() + sent, request.size() - sent, 0);
		if (n <= 0) {
			break;
		}
		sent += static_cast<std::size_t>(n);
	}
	std::string response;
	char buf[65536];
	ssize_t n;
	while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
		response.append(buf, static_cast<std::size_t>(n));
	}
	::close(fd);
	return response.rfind("HTTP/1.1 ", 0) == 0 && response.size() > 12 ? std::atoi(response.c_str() + 9) : 0;
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


int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	test_a_search_of_many_hits_yields_its_loop_between_hits();
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
