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

// UUID-named indexes on a node (docs: options.md, Identifiers): every spelling of a UUID reaches one index, which INFO
// names by its normalized path, before and after a restart, with `--uuid partition` too. Each node runs in a child of
// its own, forked before this process has threads.

#include "test_harness.h"
#include "utils_node.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "database/utils.h"
#include "endpoint.h"
#include "engine/sync.h"
#include "manager.h"
#include "msgpack.h"
#include "opts.h"
#include "pool_support.h"
#include "rapidjson/document.h"
#include "serialize.h"
#include "traceback.h"

using namespace std::chrono_literals;

namespace {

const std::string V1 = "a2d6e3b4-0000-1000-8000-0100000000ab";   // version 1: the v6 codec renders it differently
const std::string V4 = "5759b016-10c3-4526-a981-47d6a8fd9c30";
const std::string V4_UPPER = "5759B016-10C3-4526-A981-47D6A8FD9C30";

template <typename F>
bool eventually(F&& done, std::chrono::milliseconds limit = 5s) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (!done() && std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(2ms);
	}
	return done();
}

bool same(const std::string& got, const std::string& want, const char* what) {
	if (got != want) {
		std::fprintf(stderr, "  %s: got %s, want %s\n", what, got.c_str(), want.c_str());
	}
	return got == want;
}

// A node in this process, in the data directory `name` (emptied unless `keep`), started with `extra` options.
class RunningNode {
public:
	RunningNode(const std::string& name, int port, const std::vector<std::string>& extra, bool keep) {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;   // as main() does: a stalled loop's watchdog collects the threads' stacks with SIGUSR2
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		root_ = std::filesystem::current_path() / name;
		if (!keep) {
			std::filesystem::remove_all(root_);
		}
		std::filesystem::create_directories(root_ / ".xapiand");
		std::filesystem::current_path(root_);
		Endpoint::cwd = root_.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(port), "--name", "uuidtest", "-D", root_.string(), "--shards", "1", "--processors", "1", "--admin-commands"};
		args.insert(args.end(), extra.begin(), extra.end());
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
	const std::filesystem::path& root() const { return root_; }

private:
	std::filesystem::path root_;
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
};

struct Reply {
	int status = 0;
	MsgPack body;
};

// One request on a connection of its own: its status and its JSON body (undefined when it has none).
Reply call(int port, const std::string& method, const std::string& path, const std::string& body = "") {
	Reply reply;
	const int fd = test_utils::connect_to(port);
	if (fd < 0) {
		return reply;
	}
	timeval timeout{10, 0};
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	const auto request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
	::send(fd, request.data(), request.size(), 0);
	std::string response;
	char buf[4096];
	ssize_t n;
	while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
		response.append(buf, static_cast<std::size_t>(n));
	}
	::close(fd);
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
	if (reply.status >= 300) {
		std::fprintf(stderr, "  %s %s: %d %s\n", method.c_str(), path.c_str(), reply.status, payload.substr(0, 200).c_str());
	}
	return reply;
}

std::string text_of(const Reply& reply, std::string_view name) {
	if (!reply.body.is_map()) {
		return {};
	}
	const auto it = reply.body.find(name);
	if (it == reply.body.end()) {
		return {};
	}
	const auto value = it.value();
	return value.is_string() ? value.str() : value.to_string();
}

// Forks a child that runs `body`, which ends it with its status; the status, or -1 when it did not end in `limit`.
template <typename F>
int run_child(F&& body, std::chrono::seconds limit = 120s) {
	std::fflush(nullptr);
	const pid_t pid = ::fork();
	if (pid == 0) {
		::alarm(static_cast<unsigned>(limit.count()));
		body();
		std::fflush(nullptr);
		std::_Exit(testh::failures == 0 ? 0 : 1);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	if (WIFSIGNALED(status)) {
		std::fprintf(stderr, "  the child ended with signal %d\n", WTERMSIG(status));
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// An index the test creates by `name`, and other spellings of it, each reaching the same index.
struct Named {
	std::string name;
	std::vector<std::string> spellings;
};

std::vector<Named> named() {
	return {
		{V1, {V1}},
		{V4_UPPER, {V4, "urn:uuid:" + V4, "{" + V4 + "}"}},
		{V4_UPPER + "/child", {V4 + "/child"}},   // nested in a UUID-named index
		{"plain/" + V4, {"plain/" + V4_UPPER}},   // a UUID nested in a plain name
	};
}

// The path a node normalizes `name` to: under `--uuid partition` a vanilla UUID is under its 3rd and 2nd groups.
std::string path_of(const std::string& name) {
	return Endpoint(name).path;
}

// What every spelling of every index answers on a node: its documents, its search, and INFO naming the index by its
// normalized path, which names it again. `docs` documents are in each index.
void check_reads(int port, int docs) {
	for (const auto& n : named()) {
		const auto path = path_of(n.name);
		CHECK(same(path_of(path), path, "a normalized path normalizes to itself"));
		auto spellings = n.spellings;
		spellings.push_back(n.name);
		spellings.push_back(path);
		for (const auto& s : spellings) {
			const auto got = call(port, "GET", "/" + s + "/1");
			CHECK_EQ(got.status, 200);
			CHECK(same(text_of(got, "n"), "1", ("GET /" + s + "/1").c_str()));
			const auto info = call(port, "INFO", "/" + s + "/");
			CHECK_EQ(info.status, 200);
			CHECK(same(text_of(info, "endpoint"), path, ("INFO /" + s + "/").c_str()));
			const auto found = call(port, "SEARCH", "/" + s + "/");
			CHECK_EQ(found.status, 200);
			CHECK(same(text_of(found, "total"), std::to_string(docs), ("SEARCH /" + s + "/").c_str()));
		}
		CHECK(std::filesystem::is_directory(Endpoint::cwd + path));
	}
}

void check_layout(bool partition) {
	// Each index is in one directory: under `--uuid partition`, its UUID's groups once, never again.
	CHECK_EQ(std::filesystem::exists(Endpoint::cwd + "4526/10c3/" + V4), partition);
	CHECK_FALSE(std::filesystem::exists(Endpoint::cwd + "4526/10c3/4526"));
	CHECK_FALSE(std::filesystem::exists(Endpoint::cwd + "plain/4526/10c3/4526"));
	CHECK_EQ(std::filesystem::exists(Endpoint::cwd + V4), !partition);
}

// A node in `--uuid` mode `uuid` creates, writes to, flushes and searches the UUID-named indexes; restarted, it finds
// them by every spelling, writes to them again, and after a second restart finds both documents.
void a_node_reaches_its_uuid_named_indexes(const char* dir, const std::vector<std::string>& uuid, bool partition) {
	const test_utils::ReservedPort reserved;
	const int port = reserved.port();
	auto write = [&](int id) {
		for (const auto& n : named()) {
			CHECK_EQ(call(port, "PUT", "/" + n.name + "/" + std::to_string(id), "{\"n\":" + std::to_string(id) + "}").status / 100, 2);
		}
		CHECK_EQ(call(port, "FLUSH", "/").status / 100, 2);
	};
	CHECK_EQ(run_child([&] {
		RunningNode node(dir, port, uuid, false);
		REQUIRE(node.ready());
		CHECK_EQ(opts.uuid_partition, partition);
		write(1);
		check_reads(port, 1);
		check_layout(partition);
	}), 0);
	CHECK_EQ(run_child([&] {
		RunningNode node(dir, port, uuid, true);
		REQUIRE(node.ready());
		check_reads(port, 1);
		write(2);
	}), 0);
	CHECK_EQ(run_child([&] {
		RunningNode node(dir, port, uuid, true);
		REQUIRE(node.ready());
		check_reads(port, 2);
		check_layout(partition);
	}), 0);
}

}  // namespace

// The default mode (encoded, compact): a UUID that cannot be condensed keeps its vanilla text.
static void test_uuid_named_indexes_default() {
	a_node_reaches_its_uuid_named_indexes("uuid_default_data", {}, false);
}

// Vanilla: the version 1 UUID is renamed to the v6 codec's text (upgrade.md, "Identifiers move to the v6 format").
static void test_uuid_named_indexes_vanilla() {
	a_node_reaches_its_uuid_named_indexes("uuid_vanilla_data", {"--uuid", "vanilla"}, false);
}

static void test_uuid_named_indexes_partition() {
	a_node_reaches_its_uuid_named_indexes("uuid_partition_data", {"--uuid", "partition"}, true);
}

static void test_uuid_named_indexes_vanilla_partition() {
	a_node_reaches_its_uuid_named_indexes("uuid_vanilla_partition_data", {"--uuid", "vanilla", "--uuid", "partition"}, true);
}

// The registry under `--uuid partition`: a pin's index has the key it was pinned by, so a reopen and a flush, which
// find the index by it, reach the index pinned (cheap-reopen review F1, F2).
static void test_a_partitioned_uuid_index_is_pinned_by_its_key() {
	constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT;
	const auto root = std::filesystem::current_path() / "uuid_pool_data";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	std::filesystem::current_path(root);   // the registry opens a key relative to the process's directory
	Endpoint::cwd = root.string() + "/";
	opts.uuid_repr = static_cast<std::uint32_t>(UUIDRepr::vanilla);
	opts.uuid_partition = true;
	DatabasePool pool(control_loop(), 200, 8);
	for (const auto& name : {V4, V4 + "/child", "plain/" + V4}) {
		const Endpoint e(name);
		IndexPin pin = engine::sync_wait(pool.pin(e, WRITE));
		REQUIRE(static_cast<bool>(pin));
		CHECK(same(pin->path, e.path, "the pinned index's key"));
		pin.reopen();
		IndexPin again = engine::sync_wait(pool.pin(e, DB_OPEN));
		CHECK(static_cast<bool>(again));
	}
	CHECK(std::filesystem::is_directory(Endpoint::cwd + "4526/10c3/" + V4 + "/child"));
	engine::sync_wait(pool.flush());
}

int main() {
	void (*const forked[])() = {test_uuid_named_indexes_default, test_uuid_named_indexes_vanilla, test_uuid_named_indexes_partition,
	                            test_uuid_named_indexes_vanilla_partition};
	for (auto* t : forked) {
		t();
	}
	try {
		test_a_partitioned_uuid_index_is_pinned_by_its_key();
	} catch (const std::exception& exc) {
		std::fprintf(stderr, "  FAIL the registry test threw %s\n", exc.what());
		++testh::failures;
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
