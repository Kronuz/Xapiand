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
#include "utils_node.h"

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

const test_utils::ReservedPort node_port;

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
		const auto root = std::filesystem::current_path() / "coverage_database_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(node_port.port()), "--name", "loopyield", "-D", root.string(), "--shards", "1", "--processors", "1"};
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
	const int fd = test_utils::connect_to(node_port.port());
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

struct Req {
	std::string method;
	std::string path;
	std::string body;
};

// The reply a request is pinned to: its status and the leading bytes of its body.
struct Pin {
	int status;
	const char* prefix;
};

std::string quoted(const std::string& text) {
	std::string out = "\"";
	for (const unsigned char c : text) {
		if (c == '"' || c == '\\') {
			out += '\\';
			out += static_cast<char>(c);
		} else if (c >= 0x20 && c < 0x7f) {
			out += static_cast<char>(c);
		} else {
			char hex[8];
			std::snprintf(hex, sizeof(hex), "\\x%02x\"\"", c);
			out += hex;
		}
	}
	return out + "\"";
}

// Every request of the run in order; the first of a state-dependent sequence comes before the ones that read it.
std::vector<Req> requests() {
	std::vector<Req> reqs;
	const auto add = [&reqs](std::string method, std::string path, std::string body = "") {
		reqs.push_back({std::move(method), std::move(path), std::move(body)});
	};

	// Every reserved property against a text field, with every JSON kind of value; each in an index of its own.
	const std::vector<std::string> props = {"_weight", "_position", "_spelling", "_positions", "_language", "_ngram", "_cjk_ngram", "_accuracy", "_accuracy_prefix", "_store", "_type", "_dynamic", "_strict", "_slot", "_index", "_prefix", "_recurse", "_ignore", "_namespace", "_partial_paths", "_index_uuid_field", "_stop_strategy", "_stem_strategy", "_stem_language", "_foreign", "_date_detection", "_datetime_detection", "_time_detection", "_timedelta_detection", "_numeric_detection", "_geo_detection", "_bool_detection", "_text_detection", "_term_detection", "_uuid_detection", "_bool_term", "_value", "_script", "_meta", "_bogus"};
	const std::vector<std::string> vals = {"true", "\"s\"", "7", "-1", "1.5", "[]", "[1]", "[\"a\"]", "{}", "null"};
	int n = 0;
	for (const auto& prop : props) {
		for (const auto& val : vals) {
			add("PUT", "/pr/" + std::to_string(++n), "{\"f\":{\"_type\":\"text\",\"" + prop + "\":" + val + "}}");
		}
	}

	// Each type with each kind of value: as the schema's type (`s`), as an explicit `_type` with `_value` (`w`), and
	// as the type inferred from the value alone.
	const std::vector<std::string> types = {"integer", "positive", "floating", "boolean", "keyword", "text", "string", "term", "date", "datetime", "time", "timedelta", "uuid", "geo", "object", "array", "foreign/object", "script", "nope"};
	const std::vector<std::string> values = {"5", "-1", "1.5", "\"7\"", "\"abc\"", "\"2020-01-02\"", "\"10:30:00\"", "\"+01:00:00\"", "\"550e8400-e29b-41d4-a716-446655440000\"", "\"true\"", "true", "null", "[1,2]", "[\"a\",\"b\"]", "{\"_latitude\":1,\"_longitude\":2}", "\"POINT(1 2)\"", "{}", "[]", "18446744073709551615", "\"Abc Def\"", "\"2020-01-01T00:00:00Z\""};
	int t = 0;
	for (const auto& type : types) {
		const auto idx = "/ty" + std::to_string(++t) + "/";
		add("PUT", idx, R"({"_schema":{"s":{"_type":")" + type + R"("}}})");
		int k = 0;
		for (const auto& val : values) {
			++k;
			// An object concrete type with a scalar value aborts a Debug server (hunt.md, "schema.cc:5339 assertion" row).
			if (type == "object") {
				continue;
			}
			add("PUT", idx + "s" + std::to_string(k), "{\"s\":" + val + "}");
			add("PUT", idx + "w" + std::to_string(k), "{\"w" + std::to_string(k) + "\":{\"_type\":\"" + type + "\",\"_value\":" + val + "}}");
		}
	}
	int k = 0;
	for (const auto& val : values) {
		add("PUT", "/inf/" + std::to_string(++k), "{\"v" + std::to_string(k) + "\":" + val + "}");
	}

	// Documents: ids, versions, bodies, updates, scripts, deletes.
	add("PUT", "/t1/1", R"({"s":"hello world","i":5,"f":1.5,"b":true,"d":"2020-01-02","dt":"2020-01-02T03:04:05Z","tm":"10:30:00","td":"+01:30:00","u":"550e8400-e29b-41d4-a716-446655440000","n":null,"o":{"x":1,"y":[1,2,3]},"a":["x","y"]})");
	add("GET", "/t1/");
	add("GET", "/t1/1");
	add("GET", "/t1/1?volatile");
	add("GET", "/t1/?pretty");
	add("HEAD", "/t1/1");
	add("INFO", "/t1/1");
	add("INFO", "/t1/");
	add("COMMIT", "/t1/");
	add("CHECK", "/t1/");
	add("FLUSH", "/t1/");
	add("DUMP", "/t1/1");
	const std::vector<std::pair<std::string, std::string>> docs = {
		{"/id/1", R"({"_id":"abc","a":1})"}, {"/id/2", R"({"_id":5,"a":1})"}, {"/id/3", R"({"_id":1.5})"}, {"/id/4", R"({"_id":[1]})"},
		{"/id/5", R"({"_version":5})"}, {"/id/6", R"({"_data":1})"}, {"/id/7", R"({"_meta":{"a":1}})"}, {"/id/8", R"({"_endpoint":"x"})"},
		{"/id/9", R"({"a":{"_script":"_doc.b = 1"}})"}, {"/id/10", R"({"_script":"_doc.c = 2"})"}, {"/id/11", R"({"_script":"bad("})"},
		{"/id/12", R"({"_script":{"_name":"s1","_body":"_doc.c = 2"}})"}, {"/id/13", R"({"_script":{"_body":"return 1"}})"},
		{"/id/14", R"({"x":{"_type":"object","_dynamic":false,"y":1}})"}, {"/id/15", R"({"x":{"y":1}})"}, {"/id/16", R"({"x.y":1})"}, {"/id/17", R"({"x.y.z":{"q":1}})"},
		{"/id/18", R"({"":1})"}, {"/id/19", R"({"a b":1})"}, {"/id/20", R"({"_a":1})"}, {"/id/21", R"({"a.":1})"}, {"/id/22", R"({"é":1})"},
		{"/id/23", R"({"_point":{"_latitude":1,"_longitude":2}})"}, {"/id/24", R"({"v":{"_point":{"_latitude":1,"_longitude":2}}})"},
		{"/id/25", R"({"a":[{"b":1},{"b":2}]})"}, {"/id/26", R"({"a":{"b":{"c":{"d":{"e":1}}}}})"}, {"/id/27", "[1,2]"}, {"/id/28", "5"}, {"/id/29", "null"}, {"/id/30", "{"},
	};
	for (const auto& d : docs) {
		add("PUT", d.first, d.second);
	}
	add("PUT", "/id/1?version=1", R"({"a":2})");
	add("PUT", "/id/1?version=7", R"({"a":2})");
	add("PUT", "/id/1?version=0", R"({"a":2})");
	add("POST", "/id/", R"({"p":1})");
	add("POST", "/id/", R"({"p":2})");
	add("GET", "/id/1");
	add("PATCH", "/id/1", R"([{"op":"add","path":"/z","value":1}])");
	add("PATCH", "/id/1", R"([{"op":"replace","path":"/z","value":3},{"op":"test","path":"/z","value":3}])");
	add("PATCH", "/id/1", R"([{"op":"remove","path":"/z"}])");
	add("PATCH", "/id/1", R"([{"op":"replace","path":"/nope","value":1}])");
	add("PATCH", "/id/1", R"([{"op":"bogus"}])");
	add("PATCH", "/id/1", R"([{"op":"copy","from":"/a","path":"/c"},{"op":"move","from":"/c","path":"/d"}])");
	add("PATCH", "/id/1", R"({"a":1})");
	add("UPDATE", "/id/1", R"({"z":2})");
	add("UPDATE", "/id/1", R"({"_script":"_doc.z = _doc.z + 1"})");
	add("UPDATE", "/id/1", R"({"_script":{"_body":"_doc.z = _doc.z + params.n","_params":{"n":5}}})");
	add("UPDATE", "/id/1", R"({"v":{"_op":"add","_value":1}})");
	add("UPDATE", "/id/1", R"({"z":{"_op":"incr","_value":1}})");
	add("UPDATE", "/id/1", R"({"z":{"_op":"decr","_value":1}})");
	add("UPDATE", "/id/1", R"({"z":null})");
	add("UPDATE", "/id/99", R"({"z":2})");
	add("UPSERT", "/id/98", R"({"z":2})");
	add("UPSERT", "/id/98", R"({"z":3})");
	add("GET", "/id/98");
	add("DELETE", "/id/1");
	add("DELETE", "/id/1");
	add("GET", "/id/1");
	add("HEAD", "/id/2");
	add("DELETE", "/id/2?version=9");
	add("DELETE", "/id/");

	// Schemas: stored, replaced, strict, dynamic.
	add("PUT", "/sc/", R"({"_schema":{"a":{"_type":"integer"},"b":{"_type":"text","_language":"en"}}})");
	add("GET", "/sc/");
	add("PUT", "/sc/1", R"({"a":"x"})");
	add("PUT", "/sc/2", R"({"a":1,"b":"The running dogs","c":{"d":[1,2,3]}})");
	add("GET", "/sc/");
	add("PUT", "/sc/", R"({"_schema":{"a":{"_type":"text"}}})");
	add("PUT", "/sc/", R"({"_schema":"x"})");
	add("PUT", "/sc/", R"({"_schema":{"_type":"foreign/object"}})");
	add("PUT", "/sc/", R"({"_schema":{"_strict":true,"_dynamic":false}})");
	add("PUT", "/sc/3", R"({"zz":1})");
	add("PUT", "/sc2/", R"({"_schema":{"_strict":true,"a":{"_type":"integer"}}})");
	add("PUT", "/sc2/1", R"({"a":1,"b":2})");
	add("PUT", "/sc2/2", R"({"a":2})");
	add("PUT", "/sc3/", R"({"_schema":{"_dynamic":false,"a":{"_type":"integer"}}})");
	add("PUT", "/sc3/1", R"({"a":1,"b":2})");
	add("GET", "/sc3/1");
	add("PUT", "/sc4/", R"({"_schema":{"_namespace":true,"_partial_paths":true}})");
	add("PUT", "/sc4/1", R"({"a":{"b":{"c":1}}})");
	add("PUT", "/sc5/", R"({"_schema":{"_ignore":["skip"],"_meta":{"k":"v"}}})");
	add("PUT", "/sc5/1", R"({"skip":1,"keep":2})");
	add("GET", "/sc5/1");
	add("PUT", "/sc6/", R"({"_schema":{"_type":"object","_date_detection":false,"_numeric_detection":false,"_bool_detection":false,"_text_detection":false,"_uuid_detection":false,"_geo_detection":false,"_time_detection":false,"_timedelta_detection":false,"_datetime_detection":false}})");
	add("PUT", "/sc6/1", R"({"a":"2020-01-01","b":"1","c":"true","d":"550e8400-e29b-41d4-a716-446655440000","e":"10:00","f":"+01:00","g":"x"})");
	add("PUT", "/sc7/1", R"j({"a":"2020-01-01","b":"1","c":"true","d":"550e8400-e29b-41d4-a716-446655440000","e":"10:00:00","f":"+01:00:00","g":"x y z","h":"POINT(1 2)","i":"2020-01-01T10:00:00"})j");
	add("GET", "/sc7/");
	add("GET", "/sc7/1");

	// DELETE of an index: it removes the index and leaves a nested one (`dl/sub`) intact and openable.
	add("PUT", "/dl/1", R"({"a":1})");
	add("PUT", "/dl/sub/1", R"({"b":2})");
	add("DELETE", "/dl/");
	add("GET", "/dl/1");
	add("GET", "/dl/sub/1");
	add("DELETE", "/dl/");
	add("PUT", "/dl/sub/2", R"({"b":3})");
	add("DELETE", "/dl/sub/");
	add("GET", "/dl/sub/1");
	add("PUT", "/dl/1", R"({"a":3})");
	add("GET", "/dl/1");
	return reqs;
}

#include "test_coverage_database_pins.inc"

}  // namespace

int main() {
	RunningNode node;
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	const auto reqs = requests();
	const char* capture = std::getenv("COVERAGE_DATABASE_CAPTURE");
	std::string out;
	if (capture == nullptr) {
		CHECK_EQ(reqs.size(), pins.size());
	}
	for (std::size_t i = 0; i < reqs.size(); ++i) {
		const auto& req = reqs[i];
		if (std::getenv("COVERAGE_DATABASE_TRACE") != nullptr) {
			std::fprintf(stderr, "%zu %s %s %s\n", i, req.method.c_str(), req.path.c_str(), req.body.c_str());
		}
		const auto r = call(req.method, req.path, req.body);
		if (capture != nullptr) {
			out += "\t{" + std::to_string(r.status) + ", " + quoted(r.raw.substr(0, 200)) + "},  // " + req.method + " " + req.path + "\n";
		} else if (i < pins.size()) {
			CHECK_EQ(r.status, pins[i].status);
			const auto got = r.raw.substr(0, std::strlen(pins[i].prefix));
			if (got != pins[i].prefix || r.status != pins[i].status) {
				std::fprintf(stderr, "pin %zu %s %s: status %d (pinned %d)\n  got:    %s\n  pinned: %s\n", i, req.method.c_str(), req.path.c_str(), static_cast<int>(r.status), static_cast<int>(pins[i].status), r.raw.substr(0, 400).c_str(), pins[i].prefix);
			}
			CHECK_EQ(got, std::string(pins[i].prefix));
		}
	}
	if (capture != nullptr) {
		std::ofstream(capture) << "// The pinned replies of requests(), in order: status and leading bytes of the body.\nconst std::vector<Pin> pins = {\n" << out << "};\n";
		std::printf("captured %zu replies\n", reqs.size());
		return 0;
	}
	if (testh::failures != 0) {
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
