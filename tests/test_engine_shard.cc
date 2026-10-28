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

// Unit tests for the Xapian implementation of the engine shard: versioned writes, errors by kind,
// transactions and reopen, each with the blocking calls on the calling thread and on a scheduler the
// caller chooses.

#include "test_harness.h"

#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "engine/sync.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/errors.h"
#include "engine/xapian/shard.h"
#include "task.hh"

namespace {

// A one-thread scheduler for the hop: resumes each posted handle on its own thread.
class Worker {
public:
	Worker() : thread_([this] { run(); }) { }
	~Worker() {
		{
			std::lock_guard lock(mutex_);
			stopping_ = true;
		}
		wake_.notify_all();
		thread_.join();
	}

	void post(std::coroutine_handle<> handle) {
		{
			std::lock_guard lock(mutex_);
			queue_.push_back(handle);
		}
		wake_.notify_one();
	}
	std::uint64_t now() const noexcept {
		return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
	}

private:
	void run() {
		while (true) {
			std::coroutine_handle<> handle;
			{
				std::unique_lock lock(mutex_);
				wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
				if (queue_.empty()) {
					return;
				}
				handle = queue_.front();
				queue_.pop_front();
			}
			task::running_on scope(task::scheduler_ref::of(*this));
			handle.resume();
		}
	}

	std::mutex mutex_;
	std::condition_variable wake_;
	std::deque<std::coroutine_handle<>> queue_;
	bool stopping_ = false;
	std::thread thread_;
};

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.add_boolean_term(key);
	doc.add_term("body", 2);
	doc.set_data(data);
	return doc;
}

template <typename F>
engine::errc kind_of(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

void run(bool offload) {
	const auto path = (std::filesystem::current_path() / (offload ? "engine_shard_test_offload.db" : "engine_shard_test.db")).string();
	std::filesystem::remove_all(path);

	Worker worker;
	const auto on = offload ? task::scheduler_ref::of(worker) : task::scheduler_ref{};

	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true, on));
	CHECK_TRUE(opened.created);
	auto& shard = *opened.shard;

	// A new document gets id 1 and version 1; the hook sees both before the write.
	engine::WriteArgs args;
	engine::docid hooked_did = 0;
	engine::rev hooked_version = 0;
	std::thread::id writer;
	args.finalise = [&](engine::Document& doc, engine::WriteResult& result) {
		writer = std::this_thread::get_id();
		hooked_did = result.did;
		hooked_version = result.version;
		doc.add_term("stamped");
	};
	auto doc = make("Qa", "one");
	auto added = engine::sync_wait(shard.add(doc, args));
	CHECK_EQ(added.did, 1u);
	CHECK_EQ(added.version, 1u);
	CHECK_EQ(hooked_did, 1u);
	CHECK_EQ(hooked_version, 1u);
	// The blocking call runs on the chosen scheduler when there is one, and on the caller's thread when not.
	CHECK_TRUE((writer != std::this_thread::get_id()) == offload);
	bool stamped = false;
	const auto& written = engine::xapian::xapian_document(doc);
	for (auto term = written.termlist_begin(); term != written.termlist_end(); ++term) {
		stamped = stamped || *term == "stamped";
	}
	CHECK_TRUE(stamped);

	// The document the write leaves is the stored one, as the write-ahead log keeps it.
	CHECK_EQ(doc.serialise(), shard.database()->get_document(1).serialise());

	// The version is recorded: the document carries it as a value and a term.
	auto view = engine::xapian::make_view(shard.database()->get_document(1));
	CHECK_EQ(view->version(), 1u);
	CHECK_EQ(view->data(), "one");

	// Replacing by id bumps the version; a stale expectation conflicts.
	engine::WriteArgs plain;
	auto doc2 = make("Qa", "two");
	auto replaced = engine::sync_wait(shard.replace(1, doc2, plain));
	CHECK_EQ(replaced.version, 2u);
	engine::WriteArgs stale;
	stale.expected_version = 1;
	auto doc3 = make("Qa", "three");
	CHECK_TRUE(kind_of([&] { engine::sync_wait(shard.replace(1, doc3, stale)); }) == engine::errc::version_conflict);
	engine::WriteArgs current;
	current.expected_version = 2;
	auto replaced3 = engine::sync_wait(shard.replace(1, doc3, current));
	CHECK_EQ(replaced3.version, 3u);

	// The conflict is also the Xapian exception it stands for.
	bool as_xapian = false;
	try {
		engine::sync_wait(shard.replace(1, doc3, stale));
	} catch (const Xapian::DocVersionConflictError&) {
		as_xapian = true;
	}
	CHECK_TRUE(as_xapian);

	// By key: a new key adds, an existing key replaces and bumps; a new document refuses a version.
	auto doc4 = make("Qb", "b1");
	auto by_key = engine::sync_wait(shard.replace_by_key("Qb", doc4, plain));
	CHECK_EQ(by_key.did, 2u);
	CHECK_EQ(by_key.version, 1u);
	CHECK_EQ(by_key.term, "Qb");
	auto doc5 = make("Qb", "b2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qb", doc5, plain)).version, 2u);
	engine::WriteArgs must_be_new;
	must_be_new.expected_version = 0;
	auto doc6 = make("Qc", "c1");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qc", doc6, must_be_new)).did, 3u);
	auto doc7 = make("Qc", "c2");
	CHECK_TRUE(kind_of([&] { engine::sync_wait(shard.replace_by_key("Qc", doc7, must_be_new)); }) == engine::errc::version_conflict);

	// Erasing: by id, and by key (which reports the id, and fails for a missing key).
	engine::sync_wait(shard.erase(1, true, std::nullopt));
	CHECK_EQ(engine::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)), 2u);
	CHECK_TRUE(kind_of([&] { engine::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)); }) == engine::errc::doc_not_found);

	// Commit, transactions, metadata and reopen.
	const auto before = shard.revision();
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);
	engine::sync_wait(shard.begin_transaction(true));
	engine::sync_wait(shard.set_metadata("k", "v"));
	engine::sync_wait(shard.commit(engine::Transaction::flushed));
	engine::sync_wait(shard.commit_transaction());
	CHECK_EQ(shard.database()->get_metadata("k"), "v");
	CHECK_FALSE(engine::sync_wait(shard.reopen()));
	CHECK_FALSE(shard.uuid().empty());

	// Opening what is missing fails with the not-found kind.
	CHECK_TRUE(kind_of([&] { engine::sync_wait(engine::xapian::open_local(path + ".missing", true, false)); }) == engine::errc::not_found);

	opened.shard.reset();
	std::filesystem::remove_all(path);
}

}  // namespace

static void test_inline() { run(false); }
static void test_offload() { run(true); }

TEST_MAIN(test_inline, test_offload)
