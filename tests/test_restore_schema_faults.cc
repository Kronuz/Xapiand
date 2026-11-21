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


// A schema read or write that fails in the middle of a RESTORE is the failing object's result, not the request's
// (review F2 of x7-loop-stalls): SchemasLRU's fault points fail the next get (the schema read of a round's fetch) and
// the next set (the offer of an object's schema change) once.

#include "test_harness.h"
#include "utils_node.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/data.h"
#include "database/handler.h"
#include "database/pool.h"
#include "database/utils.h"
#include "engine/chores.h"
#include "engine/sync.h"
#include "http_asio.h"
#include "database/schemas_lru.h"
#include "endpoint.h"
#include "manager.h"
#include "msgpack.h"
#include "opts.h"
#include "rapidjson/document.h"
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

const test_utils::ReservedPort node_port;

// A node in this process with one HTTP loop, in a data directory of its own.
class RunningNode {
public:
	RunningNode() {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		const auto root = std::filesystem::current_path() / "restore_schema_faults_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(node_port.port()), "--name", "rfault", "-D", root.string(), "--shards", "1", "--processors", "1"};
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

// One request on a connection of its own; the response's status (0 when there is none), its body into `reply`.
int http_call(const std::string& method, const std::string& path, const std::string& body, std::string* reply = nullptr) {
	const int fd = test_utils::connect_to(node_port.port());
	if (fd < 0) {
		return 0;
	}
	timeval timeout{60, 0};
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	const std::string request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
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
	if (reply != nullptr) {
		const auto end = response.find("\r\n\r\n");
		*reply = end == std::string::npos ? std::string() : response.substr(end + 4);
	}
	return response.rfind("HTTP/1.1 ", 0) == 0 && response.size() > 12 ? std::atoi(response.c_str() + 9) : 0;
}

MsgPack json_of(const std::string& reply) {
	rapidjson::Document rdoc;
	json_load(rdoc, reply);
	return MsgPack(rdoc);
}

// The value of a reply's field, as named or with the prefix the response may give it; undefined when there is none.
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

// A RESTORE of `objects` into `index`: the request's status and each object's result.
int restore(const std::string& index, const std::vector<std::string>& objects, MsgPack& items) {
	std::string body = "[";
	for (std::size_t i = 0; i < objects.size(); ++i) {
		body += (i != 0 ? "," : "") + objects[i];
	}
	body += "]";
	std::string reply;
	const auto status = http_call("RESTORE", "/" + index + "/?commit", body, &reply);
	items = field_of(json_of(reply), "items");
	std::fprintf(stderr, "  RESTORE %s: %d, %s\n", index.c_str(), status, items.is_undefined() ? reply.c_str() : items.to_string().c_str());
	return status;
}

// Each of the objects changes the schema, so a RESTORE of them runs one round per object.
const std::vector<std::string> objects = {
	R"({"_id":1,"f1":1})",
	R"({"_id":2,"f2":1})",
	R"({"_id":3,"f3":1})",
	R"({"_id":4,"f4":1})",
};

bool failed(const MsgPack& item) {
	return !field_of(item, "status").is_undefined();
}

}  // namespace


// A schema that cannot be read by a round's fetch sends the rest of the run to the loop: each object's own prepare
// reads it, so every object lands and the request succeeds.
static void test_a_schema_read_that_fails_mid_restore_fails_no_object() {
	auto& schemas = *XapiandManager::manager()->schemas;
	CHECK_EQ(http_call("RESTORE", "/rfget/?commit", R"([{"_id":0,"p":1}])") / 100, 2);
	std::atomic<bool> armed{false};
	DocIndexer::seams.after_placement = [&](DatabaseHandler&) -> task::task<void> {
		if (!armed.exchange(true)) {
			schemas.fail_next_get = true;   // the next round's fetch reads the schema
		}
		co_return;
	};
	MsgPack items;
	const auto status = restore("rfget", objects, items);
	DocIndexer::seams.after_placement = nullptr;
	CHECK_TRUE(armed.load());
	CHECK_FALSE(schemas.fail_next_get.load());   // a read took the fault
	schemas.fail_next_get = false;
	CHECK_EQ(status / 100, 2);
	CHECK_EQ(items.size(), objects.size());
	for (std::size_t i = 0; i < items.size(); ++i) {
		CHECK_FALSE(failed(items.at(i)));
	}
}


// A schema write that throws is the changing object's result: it is reported and not applied, and the objects after
// it are prepared again and applied.
static void test_a_schema_write_that_fails_mid_restore_fails_its_object_only() {
	auto& schemas = *XapiandManager::manager()->schemas;
	CHECK_EQ(http_call("RESTORE", "/rfset/?commit", R"([{"_id":0,"p":1}])") / 100, 2);
	schemas.fail_next_set = true;   // the first offer of a change
	MsgPack items;
	const auto status = restore("rfset", objects, items);
	CHECK_FALSE(schemas.fail_next_set.load());   // an offer took the fault
	schemas.fail_next_set = false;
	CHECK_EQ(status / 100, 2);
	CHECK_EQ(items.size(), objects.size());
	if (items.size() == objects.size()) {
		CHECK_TRUE(failed(items.at(0)));
		for (std::size_t i = 1; i < items.size(); ++i) {
			CHECK_FALSE(failed(items.at(i)));
		}
	}
}


int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	test_a_schema_read_that_fails_mid_restore_fails_no_object();
	test_a_schema_write_that_fails_mid_restore_fails_its_object_only();
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
