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

// A script reads the method of its write in `_method`.

#include "support.h"
#include "tests.h"

namespace detail {

// --- a script reads the method of its write in `_method` ------------------------------------------------------

namespace {

// A node in this process with one HTTP loop, on a port of its own, in a data directory of its own.
class MethodNode {
public:
	MethodNode() {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		const auto root = std::filesystem::current_path() / "lua_method_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(port_.port()), "--name", "luamethod", "-D", root.string(), "--shards", "1", "--processors", "1"};
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
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (!exited_.load() && XapiandManager::get_state() != XapiandManager::State::READY && std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		ready_ = !exited_.load() && XapiandManager::get_state() == XapiandManager::State::READY;
	}

	~MethodNode() {
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

	// One request on a connection of its own: the response's status (0 when there is none), its body into `reply`.
	int call(const std::string& method, const std::string& path, const std::string& body, std::string* reply = nullptr) const {
		const int fd = test_utils::connect_to(port_.port());
		if (fd < 0) {
			return 0;
		}
		timeval timeout{60, 0};
		::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		const std::string request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAccept: application/json\r\n"
			"Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
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

private:
	test_utils::ReservedPort port_;   // held while the node lives
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
};

// The value of the field `seen` in a JSON reply, "<none>" when it has none.
std::string seen_of(const std::string& reply) {
	std::string compact;
	for (char c : reply) {
		if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
			compact.push_back(c);
		}
	}
	const std::string key = "\"seen\":\"";
	const auto at = compact.find(key);
	if (at == std::string::npos) {
		return "<none>";
	}
	const auto begin = at + key.size();
	return compact.substr(begin, compact.find('"', begin) - begin);
}

// The method a write's script saw, as its document stored it.
std::string stored_seen(const MethodNode& node, const std::string& path) {
	std::string reply;
	if (node.call("GET", path, "", &reply) != 200) {
		return "<status>";
	}
	return seen_of(reply);
}

#define CHECK_SEEN(got, want) \
	do { \
		const std::string got_ = (got); \
		if (got_ != (want)) { std::fprintf(stderr, "  `_method` was %s, %s expected\n", got_.c_str(), (want)); } \
		CHECK_EQ(got_, std::string(want)); \
	} while (0)

}  // namespace

// Every write that runs a script hands it the method that wrote: the request's ("PUT", "POST", "PATCH", "UPDATE",
// "RESTORE"), a restore line's `_op_type` ("UPDATE"), and "RESTORE" for a restore from the command line.
void test_a_script_reads_the_method_of_its_write() {
	const auto saved_cwd = std::filesystem::current_path();
	{
		MethodNode node;
		REQUIRE(node.ready());
		const std::string script = R"("_script":"_doc.seen = _method")";
		std::string reply;

		CHECK_EQ(node.call("PUT", "/lua_method/put?commit&echo", "{\"n\":1," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "PUT");
		CHECK_SEEN(stored_seen(node, "/lua_method/put"), "PUT");

		CHECK_EQ(node.call("POST", "/lua_method/?commit&echo", "{\"n\":2," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "POST");

		CHECK_EQ(node.call("PATCH", "/lua_method/put?commit&echo", R"([{"op":"replace","path":"/n","value":3},{"op":"add","path":"/_script","value":"_doc.seen = _method"}])", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "PATCH");
		CHECK_SEEN(stored_seen(node, "/lua_method/put"), "PATCH");

		CHECK_EQ(node.call("UPDATE", "/lua_method/put?commit&echo", "{\"n\":4," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "UPDATE");

		CHECK_EQ(node.call("RESTORE", "/lua_method/?commit", "[{\"_id\":\"restored\",\"n\":5," + script + "}]") / 100, 2);
		CHECK_SEEN(stored_seen(node, "/lua_method/restored"), "RESTORE");

		// The command line's restore has no request: a plain line sees "RESTORE", an `update` line "UPDATE".
		const auto dump = std::filesystem::current_path() / "lua_method.dump";
		{
			std::string bytes = MsgPack({{"_id", "line"}, {"n", 6}, {"_script", "_doc.seen = _method"}}).serialize();
			bytes += MsgPack({{"_id", "op"}, {"_op_type", "update"}, {"n", 7}, {"_script", "_doc.seen = _method"}}).serialize();
			const int fd = ::open(dump.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
			REQUIRE(fd >= 0);
			CHECK_EQ(::write(fd, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
			::close(fd);
		}
		const int fd = ::open(dump.c_str(), O_RDONLY);
		REQUIRE(fd >= 0);
		engine::sync_wait(DatabaseHandler::restore_documents(Endpoint("lua_method_cli"), fd));
		::close(fd);
		CHECK_SEEN(stored_seen(node, "/lua_method_cli/line"), "RESTORE");
		CHECK_SEEN(stored_seen(node, "/lua_method_cli/op"), "UPDATE");
	}
	std::filesystem::current_path(saved_cwd);
}

}  // namespace detail
