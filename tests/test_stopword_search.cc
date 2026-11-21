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

// A search over several indexes checks every index against the stop words list it was built with
// (docs/mechanisms/stopword-lists.md). The lists are fixed for a process, so two processes run on one data
// directory: `build` (the shipped lists) creates `zz`; `check` (an `en.txt` that replaces the English list)
// creates `aa`, then searches them.

#include "test_harness.h"
#include "utils_node.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/data.h"
#include "database/flags.h"
#include "database/handler.h"
#include "database/utils.h"
#include "endpoint.h"
#include "engine/sync.h"
#include "manager.h"
#include "msgpack.h"
#include "opts.h"
#include "stopper.h"
#include "traceback.h"
#include "xapiand_exception.h"

using namespace std::chrono_literals;

namespace fs = std::filesystem;

namespace {

// The node of this process: a manager whose control loop runs on a thread of its own, in `name` under the
// working directory (kept when `keep`).
class RunningNode {
public:
	RunningNode(const std::string& name, bool keep, const std::vector<std::string>& extra = {}) {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);

		const auto root = fs::current_path() / name;
		if (!keep) {
			fs::remove_all(root);
		}
		fs::create_directories(root / ".xapiand");
		fs::current_path(root);
		Endpoint::cwd = root.string() + "/";

		std::vector<std::string> args = {"xapiand", "--port", std::to_string(port_.port()), "--name", "nodetest", "-D", root.string(), "--shards", "1", "--processors", "1"};
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
			} catch (const SystemExit&) {
			}
			exited_ = true;
		});
		const auto end = std::chrono::steady_clock::now() + 30s;
		while (!exited_.load() && XapiandManager::get_state() != XapiandManager::State::READY && std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(5ms);
		}
		ready_ = !exited_.load() && XapiandManager::get_state() == XapiandManager::State::READY;
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
	test_utils::ReservedPort port_;   // held while the node lives
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
};

Endpoints
endpoints_of(const std::vector<std::string>& indexes)
{
	Endpoints endpoints;
	for (const auto& index : indexes) {
		const auto one = engine::sync_wait(XapiandManager::resolve_index_endpoints(Endpoint(index), true));
		for (const auto& endpoint : one) {
			endpoints.add(endpoint);
		}
	}
	return endpoints;
}

void
put(const std::string& index, const std::string& text)
{
	DatabaseHandler handler(endpoints_of({index}), DB_WRITABLE | DB_CREATE_OR_OPEN);
	const MsgPack body({{"t", MsgPack({{"_type", "text"}, {"_language", "en"}, {"_value", text}})}});
	engine::sync_wait(handler.index(MsgPack("1"), UNKNOWN_REVISION, true, body, true, json_type));
}

// What a search over `indexes` fails with ("" when it answers).
std::string
search(const std::vector<std::string>& indexes)
{
	DatabaseHandler handler(endpoints_of(indexes), DB_OPEN);
	query_field_t query_field;
	const MsgPack qdsl({{"_query", MsgPack({{"t", "fox"}})}});
	try {
		engine::sync_wait(handler.get_mset(query_field, &qdsl, nullptr));
	} catch (const StopWordsMismatchError& exc) {
		return exc.what();
	} catch (const std::exception& exc) {
		return std::string("<other> ") + exc.what();
	}
	return "";
}

bool
contains(const std::string& text, std::string_view part)
{
	return text.find(part) != std::string::npos;
}

int
phase_build()
{
	RunningNode node("stopword_search_node", false);
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	put("zz", "the quick zebra fox");
	CHECK_EQ(search({"zz"}), "");
	return testh::failures == 0 ? 0 : 1;
}

int
phase_check()
{
	RunningNode node("stopword_search_node", true);
	if (!node.ready()) {
		std::fprintf(stderr, "FAIL: the node did not become ready\n");
		return 1;
	}
	const auto changed = stopWordsFingerprint("en");
	CHECK(changed != "d1834faea3c14e55");   // the replaced list, not the shipped one
	put("aa", "the quick red fox");   // built under the changed list

	CHECK_EQ(search({"aa"}), "");
	for (const auto& indexes : {std::vector<std::string>{"zz"}, {"aa", "zz"}, {"zz", "aa"}}) {
		const auto refusal = search(indexes);
		CHECK(contains(refusal, "Index zz was built with stop words list en"));
		CHECK(contains(refusal, "d1834faea3c14e55"));
		CHECK(contains(refusal, changed));
		CHECK(!contains(refusal, ".__"));   // the index, not its shards
	}
	CHECK_EQ(search({"aa", "gone"}), "");   // an index that does not exist is skipped, as before

	// An index found to agree is remembered for the process, so nothing may rewrite what it recorded: a client
	// that sends a fingerprint of its own is refused (Schema validation), and the index keeps agreeing.
	put("bb", "the quick red fox");
	CHECK_EQ(search({"bb", "aa"}), "");   // aa agrees, as the second index of the search
	std::string swap_refusal;
	try {
		const MsgPack body({{"u", MsgPack({{"_type", "text"}, {"_language", "en"}, {"_stop_fingerprint", "0000000000000000"}, {"_value", "the quick fox"}})}});
		DatabaseHandler handler(endpoints_of({"aa"}), DB_WRITABLE | DB_CREATE_OR_OPEN);
		engine::sync_wait(handler.index(MsgPack("2"), UNKNOWN_REVISION, true, body, true, json_type));
	} catch (const std::exception& exc) {
		swap_refusal = exc.what();
	}
	CHECK(contains(swap_refusal, "'_stop_fingerprint'"));
	CHECK_EQ(search({"bb", "aa"}), "");

	// Over the bound the set starts over and every index is checked again: zz is refused whatever was forgotten.
	put("cc", "the quick red fox");
	put("dd", "the quick red fox");
	reset_agreeing_indexes(2);
	CHECK_EQ(search({"aa", "bb", "cc", "dd"}), "");
	CHECK(agreeing_indexes_size() <= 2);
	for (int round = 0; round < 3; ++round) {
		CHECK(contains(search({"aa", "bb", "zz", "cc", "dd"}), "Index zz was built with stop words list en"));
		CHECK_EQ(search({"dd", "cc", "bb", "aa"}), "");
		CHECK(agreeing_indexes_size() <= 2);
	}
	reset_agreeing_indexes();
	return testh::failures == 0 ? 0 : 1;
}

// `--probe wide N POOL`: milliseconds a search over N one-document indexes takes, five times over, on a node whose schema
// cache holds POOL schemas (the default is 100: most of a wide search's schemas are not in it).
int
probe_wide(int indexes, const std::string& pool)
{
	RunningNode node("stopword_wide_node", false, {"--schema-pool-size", pool});
	if (!node.ready()) {
		return 1;
	}
	std::vector<std::string> names;
	for (int i = 0; i < indexes; ++i) {
		char name[16];
		std::snprintf(name, sizeof(name), "w%05d", i);
		names.emplace_back(name);
		put(name, "the quick fox");
	}
	const auto endpoints = endpoints_of(names);
	for (int rep = 0; rep < 5; ++rep) {
		DatabaseHandler handler(endpoints, DB_OPEN);
		query_field_t query_field;
		const MsgPack qdsl({{"_query", MsgPack({{"t", "fox"}})}});
		const auto t0 = std::chrono::steady_clock::now();
		const auto result = engine::sync_wait(handler.get_mset(query_field, &qdsl, nullptr));
		const auto ms = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count() / 1000.0;
		std::printf("probe wide indexes=%d pool=%s rep=%d ms=%.1f hits=%zu\n", indexes, pool.c_str(), rep, ms, std::get<0>(result).hits.size());
	}
	return 0;
}

}  // namespace

int
main(int argc, char** argv)
{
	if (argc > 2 && std::string(argv[1]) == "--phase") {
		return std::string(argv[2]) == "build" ? phase_build() : phase_check();
	}
	if (argc > 4 && std::string(argv[1]) == "--probe" && std::string(argv[2]) == "wide") {
		::setenv("XAPIAND_DATA_PATH", (fs::current_path() / "stopword_search_lists").c_str(), 1);
		return probe_wide(std::atoi(argv[3]), argv[4]);
	}
	const auto lists = fs::current_path() / "stopword_search_lists";
	fs::remove_all(lists);
	fs::create_directories(lists / "stopwords");
	::setenv("XAPIAND_DATA_PATH", lists.c_str(), 1);
	const std::string self = std::string("\"") + argv[0] + "\"";
	int status = std::system((self + " --phase build").c_str());
	std::fprintf(stderr, "%s build (the shipped lists)\n", status == 0 ? "ok  " : "FAIL");
	if (status == 0) {
		std::ofstream(lists / "stopwords" / "en.txt") << "the\nquick\n";
		status = std::system((self + " --phase check").c_str());
		std::fprintf(stderr, "%s check (a changed English list)\n", status == 0 ? "ok  " : "FAIL");
	}
	return status == 0 ? 0 : 1;
}
