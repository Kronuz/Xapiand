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


// Coverage tests for src/database/: a node in this process, and the requests that reach the schema (types, properties,
// refusals), the handler (documents, updates, scripts, deletes), the shards and the WAL. Each check reads a reply's value.

#include "test_harness.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "database/handler.h"
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
	try {
		rapidjson::Document rdoc;
		json_load(rdoc, reply);
		return MsgPack(rdoc);
	} catch (const std::exception&) {
		return MsgPack();
	}
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

// Creates the index `name` with `shards` shards and no replicas.
void create_index(const std::string& name, int shards) {
	const auto settings = R"({"_settings":{"number_of_shards":)" + std::to_string(shards) + R"(,"number_of_replicas":0}})";
	CHECK_EQ(http_call("PUT", "/" + name + "/", settings) / 100, 2);
}


struct Reply {
	int status;
	std::string raw;
};

Reply call(const std::string& method, const std::string& path, const std::string& body = "") {
	Reply r;
	r.status = http_call(method, path, body, "application/json", &r.raw);
	return r;
}

void probe(const std::string& method, const std::string& path, const std::string& body = "") {
	const auto r = call(method, path, body);
	std::printf("P %s %s %s -> %d %s\n", method.c_str(), path.c_str(), body.c_str(), r.status, r.raw.substr(0, 700).c_str());
}

}  // namespace

// The message of a refusal, or the status when the reply is not one.
std::string message_of(const Reply& r) {
	const auto body = json_of(r.raw);
	const auto msg = field_of(body, "message");
	return msg.is_string() ? std::string(msg.str_view()) : std::to_string(r.status) + " " + r.raw.substr(0, 150);
}

int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	probe("PUT", "/t1/1", R"({"s":"hello world","i":5,"f":1.5,"b":true,"d":"2020-01-02","dt":"2020-01-02T03:04:05Z","tm":"10:30:00","td":"+01:30:00","u":"550e8400-e29b-41d4-a716-446655440000","n":null,"o":{"x":1,"y":[1,2,3]},"a":["x","y"]})");
	probe("GET", "/t1/");
	probe("GET", "/t1/1");
	const std::vector<std::string> props = {"_weight", "_position", "_spelling", "_positions", "_language", "_ngram", "_cjk_ngram", "_accuracy", "_accuracy_prefix", "_store", "_type", "_dynamic", "_strict", "_slot", "_index", "_prefix", "_recurse", "_ignore", "_namespace", "_partial_paths", "_index_uuid_field", "_stop_strategy", "_stem_strategy", "_stem_language", "_foreign", "_date_detection", "_datetime_detection", "_time_detection", "_timedelta_detection", "_numeric_detection", "_geo_detection", "_bool_detection", "_text_detection", "_term_detection", "_uuid_detection", "_bool_term", "_value", "_script", "_meta", "_bogus"};
	const std::vector<std::string> vals = {"true", "\"s\"", "7", "-1", "1.5", "[]", "[1]", "[\"a\"]", "{}", "null"};
	int n = 0;
	for (const auto& prop : props) {
		for (const auto& val : vals) {
			const auto r = call("PUT", "/pr/" + std::to_string(++n), "{\"f\":{\"_type\":\"text\",\"" + prop + "\":" + val + "}}");
			std::printf("R %s %s -> %d %s\n", prop.c_str(), val.c_str(), r.status, r.status / 100 == 2 ? "" : message_of(r).c_str());
		}
	}
	const std::vector<std::pair<std::string, std::string>> docs = {
		{"PUT /ty/1", R"({"v":"2020-01-02"})"}, {"PUT /ty/2", R"({"v":"10:30"})"}, {"PUT /ty/3", R"({"v":"+01:30"})"}, {"PUT /ty/4", R"({"v":"1.5e3"})"},
		{"PUT /ty/5", R"({"v":[1,"a"]})"}, {"PUT /ty/6", R"({"v":{"_type":"geo","_point":{"_latitude":1,"_longitude":2}}})"},
		{"PUT /ty/7", R"({"v":{"_point":{"_latitude":1,"_longitude":2}}})"}, {"PUT /ty/8", R"({"v":{"_value":5,"_type":"positive"}})"},
		{"PUT /ty/9", R"({"v":{"_value":"abc","_type":"keyword"}})"}, {"PUT /ty/10", R"({"v":{"_value":"abc","_type":"string"}})"},
		{"PUT /ty/11", R"({"v":{"_value":"2020-01-01","_type":"date"}})"}, {"PUT /ty/12", R"({"v":{"_value":"x","_type":"nope"}})"},
		{"PUT /ty/13", R"({"v":{"_type":"object","w":{"_type":"integer","_value":3}}})"}, {"PUT /ty/14", R"({"v":{"_type":"array","_value":[1,2]}})"},
		{"PUT /ty/15", R"({"v":{"_type":"foreign/object","_value":"x"}})"}, {"PUT /ty/16", R"({"v":{"_type":"uuid","_value":"550e8400-e29b-41d4-a716-446655440000"}})"},
		{"PUT /ty/17", R"({"v":{"_type":"timedelta","_value":"+01:00"}})"}, {"PUT /ty/18", R"({"v":{"_type":"time","_value":"01:00"}})"},
		{"PUT /ty/19", R"({"v":{"_type":"datetime","_value":"2020-01-01T00:00:00"}})"}, {"PUT /ty/20", R"({"v":{"_type":"boolean","_value":"true"}})"},
		{"PUT /ty/21", R"({"v":{"_type":"floating","_value":"1.5"}})"}, {"PUT /ty/22", R"({"v":{"_type":"integer","_value":"7"}})"},
		{"PUT /ty/23", R"({"v":{"_type":"text","_value":5}})"}, {"PUT /ty/24", R"({"v":{"_type":"term","_value":"Abc Def"}})"},
		{"PUT /ty/25", R"j({"v":{"_type":"geo","_value":"POINT(1 2)"}})j"}, {"PUT /ty/26", R"({"v":{"_type":"geo","_value":{"_type":"circle","_latitude":1,"_longitude":2,"_radius":10}}})"},
		{"PUT /ty/27", R"({"v":{"_type":"geo","_value":{"_type":"polygon","_points":[[1,1],[1,2],[2,2]]}}})"}, {"PUT /ty/28", R"({"v":{"_type":"geo","_value":"x"}})"},
		{"PUT /ty/29", R"({"v":{"_type":"geo","_value":{"_type":"nope"}}})"}, {"PUT /ty/30", R"({"v":{"_type":"integer","_value":"x"}})"},
		{"PUT /ty/31", R"({"v":{"_type":"integer","_value":1.5}})"}, {"PUT /ty/32", R"({"v":{"_type":"integer","_value":18446744073709551615}})"},
		{"PUT /ty/33", R"({"v":{"_type":"positive","_value":-1}})"}, {"PUT /ty/34", R"({"v":{"_type":"date","_value":"bad"}})"},
		{"PUT /ty/35", R"({"v":{"_type":"time","_value":"bad"}})"}, {"PUT /ty/36", R"({"v":{"_type":"uuid","_value":"bad"}})"},
		{"PUT /ty/37", R"({"v":{"_type":"boolean","_value":"maybe"}})"}, {"PUT /ty/38", R"({"v":{"_type":"floating","_value":"x"}})"},
		{"PUT /ty/39", R"({"_id":"abc","a":1})"}, {"PUT /ty/40", R"({"_id":5,"a":1})"}, {"PUT /ty/41", R"({"_id":1.5})"}, {"PUT /ty/42", R"({"_id":[1]})"},
		{"PUT /ty/43", R"({"_version":5})"}, {"PUT /ty/44", R"({"_data":1})"}, {"PUT /ty/45", R"({"_meta":{"a":1}})"}, {"PUT /ty/46", R"({"_endpoint":"x"})"},
		{"PUT /ty/47", R"({"a":{"_script":"_doc.b = 1"}})"}, {"PUT /ty/48", R"({"_script":"_doc.c = 2"})"}, {"PUT /ty/49", R"({"_script":"bad("})"},
		{"PUT /ty/50", R"({"_script":{"_name":"s1","_body":"_doc.c = 2"}})"}, {"PUT /ty/51", R"({"_script":{"_body":"return 1"}})"},
		{"PUT /ty/52", R"({"x":{"_type":"object","_dynamic":false,"y":1}})"}, {"PUT /ty/53", R"({"x":{"y":1}})"}, {"PUT /ty/54", R"({"x.y":1})"}, {"PUT /ty/55", R"({"x.y.z":{"q":1}})"},
		{"PUT /ty/56", R"({"":1})"}, {"PUT /ty/57", R"({"a b":1})"}, {"PUT /ty/58", R"({"_a":1})"}, {"PUT /ty/59", R"({"a.":1})"}, {"PUT /ty/60", R"({"é":1})"},
	};
	for (const auto& d : docs) {
		const auto sp = d.first.find(' ');
		probe(d.first.substr(0, sp), d.first.substr(sp + 1), d.second);
	}
	probe("GET", "/ty/");
	probe("GET", "/ty/?pretty");
	probe("GET", "/ty/_search?q=v:abc");
	probe("GET", "/ty/_search", R"({"_query":{"v":{"_in":[1,2]}}})");
	probe("GET", "/ty/_count");
	probe("GET", "/ty/1?volatile");
	probe("PATCH", "/ty/1", R"([{"op":"add","path":"/z","value":1}])");
	probe("PATCH", "/ty/1", R"([{"op":"replace","path":"/nope","value":1}])");
	probe("PATCH", "/ty/1", R"([{"op":"bogus"}])");
	probe("UPDATE", "/ty/1", R"({"z":2})");
	probe("UPDATE", "/ty/1", R"({"_script":"_doc.z = _doc.z + 1"})");
	probe("UPSERT", "/ty/99", R"({"z":2})");
	probe("UPDATE", "/ty/1", R"({"v":{"_op":"add","_value":1}})");
	probe("UPDATE", "/ty/4", R"({"v":{"_op":"incr","_value":1}})");
	probe("DELETE", "/ty/1");
	probe("DELETE", "/ty/1");
	probe("GET", "/ty/1");
	probe("HEAD", "/ty/2");
	probe("DELETE", "/ty/");
	probe("GET", "/ty/");
	probe("PUT", "/sc/", R"({"_schema":{"a":{"_type":"integer"},"b":{"_type":"text","_language":"en"}}})");
	probe("GET", "/sc/");
	probe("PUT", "/sc/1", R"({"a":"x"})");
	probe("PUT", "/sc/2", R"({"a":1,"b":"The running dogs","c":{"d":[1,2,3]}})");
	probe("GET", "/sc/");
	probe("PUT", "/sc/", R"({"_schema":{"a":{"_type":"text"}}})");
	probe("PUT", "/sc/", R"({"_schema":"x"})");
	probe("PUT", "/sc/", R"({"_schema":{"_type":"foreign/object"}})");
	probe("PUT", "/sc/", R"({"_schema":{"_strict":true,"_dynamic":false}})");
	probe("PUT", "/sc/3", R"({"zz":1})");
	if (testh::failures != 0) {
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
