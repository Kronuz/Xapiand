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

// A whole-database RESTORE of one MessagePack array streams as its elements (docs/perf/bulk-load-rss.md):
// Request::append takes the array's header and holds it to its declared count, and every body a dump could send
// answers as it did when the array was decoded whole. The node runs in this process with one HTTP loop.
//
//   xapiand_test_restore_stream [FILTER]   runs the tests whose name holds FILTER
//   RESTORE_STREAM_CAPTURE=1 ...           prints each body's answers instead of checking them

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/utils.h"
#include "endpoint.h"
#include "manager.h"
#include "msgpack.h"
#include "opts.h"
#include "rapidjson/document.h"
#include "server/search_views.h"
#include "traceback.h"
#include "xapiand_exception.h"

using namespace std::chrono_literals;

namespace {

// ---- Bodies.

std::string packed(const std::function<void(msgpack::packer<msgpack::sbuffer>&)>& pack) {
	msgpack::sbuffer buffer;
	msgpack::packer<msgpack::sbuffer> pk(buffer);
	pack(pk);
	return std::string(buffer.data(), buffer.size());
}

// A document {"_id": id, "n": n}.
std::string doc(const std::string& id, int n) {
	return packed([&](auto& pk) {
		pk.pack_map(2);
		pk.pack(std::string("_id"));
		pk.pack(id);
		pk.pack(std::string("n"));
		pk.pack(n);
	});
}

// An array header of `size` bytes (1, 3 or 5) declaring `count` elements.
std::string header(std::size_t size, std::uint32_t count) {
	std::string h;
	if (size == 1) {
		h += static_cast<char>(0x90 | (count & 0x0f));
	} else if (size == 3) {
		h += static_cast<char>(0xdc);
		h += static_cast<char>(count >> 8 & 0xff);
		h += static_cast<char>(count & 0xff);
	} else {
		h += static_cast<char>(0xdd);
		for (int shift = 24; shift >= 0; shift -= 8) {
			h += static_cast<char>(count >> shift & 0xff);
		}
	}
	return h;
}

// `depth` nested one-element arrays around the integer 1.
std::string nested(std::size_t depth) {
	return std::string(depth, static_cast<char>(0x91)) + '\x01';
}

std::string join(const std::vector<std::string>& parts) {
	std::string s;
	for (const auto& part : parts) {
		s += part;
	}
	return s;
}

std::vector<std::string> three_docs() {
	return {doc("a", 1), doc("b", 2), doc("c", 3)};
}

// ---- The feed, without a node: Request::append over pieces of a body.

struct Fed {
	std::vector<std::string> objects;    // each object append yielded, packed
	std::vector<bool> arrays;            // whether each was an array
	std::vector<std::string> elements;   // the arrays' items, in order, packed
	std::string error;                   // the ClientError's message, or empty
};

Fed feed(const std::vector<std::string>& pieces) {
	Request request;
	request.mode = Request::Mode::STREAM_MSGPACK;
	request.has_content_length = true;
	Fed fed;
	try {
		for (const auto& piece : pieces) {
			request.append(piece.data(), piece.size());
		}
		request.append(nullptr, 0);
	} catch (const ClientError& exc) {
		fed.error = exc.what();
		if (fed.error.empty()) {
			fed.error = "ClientError";
		}
	}
	for (const auto& obj : request.objects) {
		fed.objects.push_back(obj.serialize());
		fed.arrays.push_back(obj.is_array());
		if (obj.is_array()) {
			for (const auto& item : obj) {
				fed.elements.push_back(item.serialize());
			}
		}
	}
	return fed;
}

// Every way to cut `body` in two, and in three with both cuts at most `head` bytes in, and in single bytes.
std::vector<std::vector<std::string>> splits(const std::string& body, std::size_t head) {
	std::vector<std::vector<std::string>> all = {{body}};
	for (std::size_t p = 1; p < body.size(); ++p) {
		all.push_back({body.substr(0, p), body.substr(p)});
	}
	for (std::size_t p = 1; p <= head && p < body.size(); ++p) {
		for (std::size_t q = p + 1; q <= head + 1 && q < body.size(); ++q) {
			all.push_back({body.substr(0, p), body.substr(p, q - p), body.substr(q)});
		}
	}
	std::vector<std::string> bytes;
	for (char c : body) {
		bytes.emplace_back(1, c);
	}
	all.push_back(bytes);
	return all;
}

// A non-empty array's elements arrive as arrays of them, however the body is cut, its header too.
void test_an_array_streams_as_its_elements_at_every_split() {
	for (std::size_t size : {1, 3, 5}) {
		const auto elements = three_docs();
		const auto body = header(size, 3) + join(elements);
		for (const auto& pieces : splits(body, size)) {
			const auto fed = feed(pieces);
			CHECK_EQ(fed.error, std::string());
			CHECK_TRUE(fed.elements == elements);
			bool all_arrays = !fed.arrays.empty();
			for (bool is_array : fed.arrays) {
				all_arrays = all_arrays && is_array;
			}
			CHECK_TRUE(all_arrays);   // the first object is an array, as the whole body was: no settings are read from it
		}
	}
}

// An element that is an array stays one element: the indexer takes it whole, and refuses it, as before.
void test_a_nested_array_element_stays_one_element() {
	const std::vector<std::string> elements = {doc("a", 1), header(1, 2) + doc("b", 2) + doc("c", 3), doc("d", 4)};
	const auto body = header(1, 3) + join(elements);
	for (const auto& pieces : splits(body, 1)) {
		const auto fed = feed(pieces);
		CHECK_EQ(fed.error, std::string());
		CHECK_TRUE(fed.elements == elements);
	}
}

// An empty array is the body's one object, as before: the unpacker reads its header whole, however it is cut.
void test_an_empty_array_is_one_object() {
	for (std::size_t size : {1, 3, 5}) {
		const auto body = header(size, 0);
		for (const auto& pieces : splits(body, size)) {
			const auto fed = feed(pieces);
			CHECK_EQ(fed.error, std::string());
			REQUIRE_EQ(fed.objects.size(), std::size_t{1});
			CHECK_EQ(fed.objects[0], std::string(1, static_cast<char>(0x90)));
		}
	}
}

// A body that is not one array is a stream of objects, each its own, as before.
void test_a_stream_of_objects_is_unchanged() {
	const auto elements = three_docs();
	for (const auto& pieces : splits(join(elements), 1)) {
		const auto fed = feed(pieces);
		CHECK_EQ(fed.error, std::string());
		CHECK_TRUE(fed.objects == elements);
	}
}

// A4: an array whose header declares more elements than the body holds ends as an incomplete body.
void test_fewer_elements_than_declared_fail() {
	for (std::size_t size : {1, 3, 5}) {
		const auto docs = three_docs();
		for (const auto& body : {header(size, 3) + docs[0] + docs[1], header(size, 3) + docs[0] + docs[1] + docs[2].substr(0, 4), header(size, 3).substr(0, size - 1)}) {
			if (body.empty()) {
				continue;
			}
			for (const auto& pieces : splits(body, size)) {
				CHECK_EQ(feed(pieces).error, std::string("The request body ended inside its top-level array"));
			}
		}
	}
}

// A4: an array followed by more than it declares fails as soon as the surplus arrives.
void test_more_elements_than_declared_fail() {
	for (std::size_t size : {1, 3, 5}) {
		const auto docs = three_docs();
		for (const auto& body : {header(size, 2) + join(docs), header(size, 2) + docs[0] + docs[1] + "\xc0", header(size, 1) + docs[0] + header(1, 1) + docs[1]}) {
			for (const auto& pieces : splits(body, size)) {
				CHECK_EQ(feed(pieces).error, std::string("The request body holds more than its top-level array declares"));
			}
		}
	}
}

// An element is nested at most as deep as it could be inside the array decoded whole: MAX_JSON_DEPTH levels, the
// array one of them.
void test_an_element_nests_as_deep_as_inside_the_whole_array() {
	{
		const auto fed = feed({header(1, 2) + nested(MAX_JSON_DEPTH - 1) + doc("after", 1)});
		CHECK_EQ(fed.error, std::string());
		CHECK_EQ(fed.elements.size(), std::size_t{2});
	}
	{
		const auto fed = feed({header(1, 2) + nested(MAX_JSON_DEPTH) + doc("after", 1)});
		CHECK_EQ(fed.error.rfind("The request body's top-level array holds an invalid element", 0), std::size_t{0});
	}
	// The whole array, decoded at once, draws the same line.
	auto whole = [](const std::string& body) {
		auto unpacker = make_msgpack_unpacker();
		unpacker.reserve_buffer(body.size());
		std::memcpy(unpacker.buffer(), body.data(), body.size());
		unpacker.buffer_consumed(body.size());
		msgpack::object_handle result;
		try {
			return unpacker.next(result);
		} catch (const std::exception&) {
			return false;
		}
	};
	CHECK_TRUE(whole(header(1, 1) + nested(MAX_JSON_DEPTH - 1)));
	CHECK_FALSE(whole(header(1, 1) + nested(MAX_JSON_DEPTH)));
}

// An element the unpacker refuses fails the body: the elements before it may have been restored.
void test_an_invalid_element_fails() {
	const auto docs = three_docs();
	const auto fed = feed({header(1, 3) + docs[0] + "\xc1" + docs[2]});
	CHECK_EQ(fed.error.rfind("The request body's top-level array holds an invalid element", 0), std::size_t{0});
}

// ---- A node in this process.

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

int node_port = 0;

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(5ms);
	}
	return done();
}

class RunningNode {
public:
	RunningNode() {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		node_port = free_port();
		const auto root = std::filesystem::current_path() / "restore_stream_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(node_port), "--name", "restorestream", "-D", root.string(), "--shards", "1", "--processors", "1"};
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

// One request on a connection of its own, its body sent in `pieces` with a pause between them: the status (0 when
// there is none), and the body into `reply`.
int http_call(const std::string& method, const std::string& path, const std::vector<std::string>& pieces, const std::string& content_type, std::string* reply) {
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
	std::string head = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAccept: application/json\r\n";
	if (!content_type.empty()) {
		head += "Content-Type: " + content_type + "\r\nContent-Length: " + std::to_string(length) + "\r\n";
	}
	send_all(head + "\r\n");
	for (std::size_t i = 0; i < pieces.size(); ++i) {
		if (i != 0) {
			std::this_thread::sleep_for(100ms);
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
	const auto end = response.find("\r\n\r\n");
	*reply = end == std::string::npos ? std::string() : response.substr(end + 4);
	return response.rfind("HTTP/1.1 ", 0) == 0 && response.size() > 12 ? std::atoi(response.c_str() + 9) : 0;
}

// A JSON reply without its timing, as one line; the raw reply when it is not JSON.
std::string normalized(const std::string& reply) {
	MsgPack obj;
	try {
		rapidjson::Document rdoc;
		json_load(rdoc, reply);
		obj = MsgPack(rdoc);
	} catch (const std::exception&) {
		return reply;
	}
	if (!obj.is_map()) {
		return obj.to_string();
	}
	MsgPack out = MsgPack::MAP();
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		const auto key = it->str_view();
		if (key != "took" && key != "#took" && key != "_took") {
			out[key] = it.value();
		}
	}
	return out.to_string();
}

std::string answer(const std::string& method, const std::string& path, const std::vector<std::string>& pieces = {}, const std::string& content_type = "") {
	std::string reply;
	const int status = http_call(method, path, pieces, content_type, &reply);
	return std::to_string(status) + " " + normalized(reply);
}

// What a body answers: the RESTORE's, then its index's COUNT and DUMP.
struct Answers {
	std::string restore, count, dump;
};

bool capture() {
	const char* env = std::getenv("RESTORE_STREAM_CAPTURE");
	return env != nullptr && *env != '\0' && *env != '0';
}

// Restores `pieces` into the index `name` and checks its answers against `expected` (or prints them, capturing).
void check_answers(const std::string& name, const std::string& path, const std::vector<std::string>& pieces, const std::string& content_type, const Answers& expected) {
	Answers got;
	got.restore = answer("RESTORE", path, pieces, content_type);
	got.count = answer("COUNT", "/" + name + "/");
	got.dump = answer("DUMP", "/" + name + "/");
	if (capture()) {
		std::printf("\t{\"%s\", {R\"~(%s)~\", R\"~(%s)~\", R\"~(%s)~\"}},\n", name.c_str(), got.restore.c_str(), got.count.c_str(), got.dump.c_str());
		return;
	}
	CHECK_EQ(got.restore, expected.restore);
	CHECK_EQ(got.count, expected.count);
	CHECK_EQ(got.dump, expected.dump);
	if (got.restore != expected.restore || got.count != expected.count || got.dump != expected.dump) {
		std::fprintf(stderr, "  %s:\n    restore %s\n    count   %s\n    dump    %s\n", name.c_str(), got.restore.c_str(), got.count.c_str(), got.dump.c_str());
	}
}

// Each body's answers, captured at master (Xapiand:e1be368395a1) unless marked.
const std::map<std::string, Answers>& expected_answers() {
	static const std::map<std::string, Answers> answers = {
	};
	return answers;
}

const Answers& expected(const std::string& name) {
	static const Answers none;
	const auto& all = expected_answers();
	const auto it = all.find(name);
	if (it == all.end()) {
		if (!capture()) {
			std::fprintf(stderr, "  FAIL: no expected answers for %s\n", name.c_str());
			++testh::failures;
		}
		return none;
	}
	return it->second;
}

void restore_msgpack(const std::string& name, const std::vector<std::string>& pieces) {
	check_answers(name, "/" + name + "/?commit", pieces, "application/x-msgpack", expected(name));
}

// A5: the bodies a dump could send answer as they did with the array decoded whole.
void test_arrays_answer_as_before() {
	for (std::size_t size : {1, 3, 5}) {
		restore_msgpack("array" + std::to_string(size), {header(size, 3) + join(three_docs())});
		restore_msgpack("empty" + std::to_string(size), {header(size, 0)});
	}
}

void test_a_split_header_answers_as_before() {
	for (std::size_t size : {1, 3, 5}) {
		const auto body = header(size, 3) + join(three_docs());
		for (std::size_t p = 1; p <= size; ++p) {
			restore_msgpack("split" + std::to_string(size) + "at" + std::to_string(p), {body.substr(0, p), body.substr(p)});
		}
	}
}

void test_odd_elements_answer_as_before() {
	restore_msgpack("nested", {header(1, 3) + doc("a", 1) + header(1, 2) + doc("b", 2) + doc("c", 3) + doc("d", 4)});
	restore_msgpack("nonmap", {header(1, 5) + doc("a", 1) + "\x05" + "\xa1x" + "\xc0" + doc("e", 5)});
	restore_msgpack("deep", {header(1, 2) + nested(MAX_JSON_DEPTH - 1) + doc("after", 1)});
	// The first element's settings are not the index's: the whole array was the first object.
	const auto settings = packed([](auto& pk) {
		pk.pack_map(3);
		pk.pack(std::string("_settings"));
		pk.pack_map(1);
		pk.pack(std::string("number_of_shards"));
		pk.pack(3);
		pk.pack(std::string("_id"));
		pk.pack(std::string("s"));
		pk.pack(std::string("n"));
		pk.pack(1);
	});
	restore_msgpack("settings", {header(1, 2) + settings + doc("t", 2)});
}

void test_other_bodies_answer_as_before() {
	restore_msgpack("stream", {join(three_docs())});
	check_answers("ndjson", "/ndjson/?commit", {"{\"_id\":\"a\",\"n\":1}\n{\"_id\":\"b\",\"n\":2}\n{\"_id\":\"c\",\"n\":3}\n"}, "application/x-ndjson", expected("ndjson"));
	check_answers("single", "/single/one?commit", {doc("one", 1)}, "application/x-msgpack", expected("single"));
}

// A4: the declared count holds end to end (these answers are the change's: master restored nothing for the first,
// and the declared elements and the surplus for the second).
void test_a_short_array_fails() {
	const auto docs = three_docs();
	restore_msgpack("fewer", {header(3, 3) + docs[0] + docs[1]});
}

void test_a_long_array_fails() {
	const auto docs = three_docs();
	restore_msgpack("more", {header(3, 2) + join(docs)});
}

// The answers of bodies the change refuses (they differ from master's by design: see the journal's rulings).
void test_refused_bodies_fail() {
	const auto docs = three_docs();
	restore_msgpack("toodeep", {header(1, 2) + nested(MAX_JSON_DEPTH) + doc("after", 1)});
	restore_msgpack("invalid", {header(1, 3) + docs[0] + "\xc1" + docs[2]});
	restore_msgpack("trailing", {header(1, 2) + docs[0] + docs[1] + "\xc0"});
	restore_msgpack("twoarrays", {header(1, 1) + docs[0] + header(1, 1) + docs[1]});
}

struct Test {
	const char* name;
	void (*run)();
	bool node;
};

}  // namespace

int main(int argc, char** argv) {
	const Test tests[] = {
		{"feed_split", test_an_array_streams_as_its_elements_at_every_split, false},
		{"feed_nested", test_a_nested_array_element_stays_one_element, false},
		{"feed_empty", test_an_empty_array_is_one_object, false},
		{"feed_stream", test_a_stream_of_objects_is_unchanged, false},
		{"feed_fewer", test_fewer_elements_than_declared_fail, false},
		{"feed_more", test_more_elements_than_declared_fail, false},
		{"feed_depth", test_an_element_nests_as_deep_as_inside_the_whole_array, false},
		{"feed_invalid", test_an_invalid_element_fails, false},
		{"node_arrays", test_arrays_answer_as_before, true},
		{"node_split", test_a_split_header_answers_as_before, true},
		{"node_odd", test_odd_elements_answer_as_before, true},
		{"node_other", test_other_bodies_answer_as_before, true},
		{"node_fewer", test_a_short_array_fails, true},
		{"node_more", test_a_long_array_fails, true},
		{"node_refused", test_refused_bodies_fail, true},
	};
	const std::string filter = argc > 1 ? argv[1] : "";
	const auto selected = [&](const Test& test) { return std::string(test.name).find(filter) != std::string::npos; };
	for (const auto& test : tests) {
		if (selected(test) && !test.node) {
			std::fprintf(stderr, "%s\n", test.name);
			test.run();
		}
	}
	bool any_node = false;
	for (const auto& test : tests) {
		any_node = any_node || (selected(test) && test.node);
	}
	if (any_node) {
		RunningNode node;
		if (!node.ready()) {
			std::fprintf(stderr, "FAIL: the node did not become ready\n");
			return 1;
		}
		for (const auto& test : tests) {
			if (selected(test) && test.node) {
				std::fprintf(stderr, "%s\n", test.name);
				test.run();
			}
		}
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
