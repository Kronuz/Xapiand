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

// The strata engine shard in host mode (docs/design/phase7c.md D6): a write does no I/O of its own,
// the index's owed flush runs as a chore and the write is applied again, and a read sees a write only
// after refresh(). The chores here run on threads of their own, so a test can tell where the device
// work ran.

#include "test_harness.h"

#include <algorithm>
#include <atomic>
#include <coroutine>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "engine/chores.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/sync.h"

namespace {

// A chores runner that executes each step on a thread of its own and resumes the awaiter there.
class ThreadChores final : public engine::Chores {
public:
	task::task<void> run(engine::ChoreClass cls, std::function<void()> fn) override {
		struct Awaiter {
			ThreadChores& self;
			engine::ChoreClass cls;
			std::function<void()> fn;
			std::exception_ptr error;

			bool await_ready() const noexcept { return false; }
			void await_suspend(std::coroutine_handle<> handle) {
				std::thread([this, handle] {
					ThreadChores& owner = self;
					const auto id = std::this_thread::get_id();
					const auto kind = cls;
					try {
						fn();
					} catch (...) {
						error = std::current_exception();
					}
					{
						std::lock_guard<std::mutex> lk(owner.mtx_);
						owner.threads_.insert(id);
						owner.classes_.push_back(kind);
					}
					handle.resume();  // nothing touches *this after this
				}).detach();
			}
			void await_resume() const {
				if (error) {
					std::rethrow_exception(error);
				}
			}
		};
		co_await Awaiter{*this, cls, std::move(fn), nullptr};
	}

	std::size_t count() const {
		std::lock_guard<std::mutex> lk(mtx_);
		return classes_.size();
	}
	std::size_t count(engine::ChoreClass cls) const {
		std::lock_guard<std::mutex> lk(mtx_);
		return static_cast<std::size_t>(std::count(classes_.begin(), classes_.end(), cls));
	}
	bool ran_on(std::thread::id id) const {
		std::lock_guard<std::mutex> lk(mtx_);
		return threads_.count(id) != 0;
	}

private:
	mutable std::mutex mtx_;
	std::set<std::thread::id> threads_;
	std::vector<engine::ChoreClass> classes_;
};

std::string scratch(const char* name) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_host_test_") + name + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.set_data(data);
	return doc;
}

engine::OpenOptions host(ThreadChores& chores, std::uint64_t cap = 0) {
	engine::OpenOptions options;
	options.chores = &chores;
	options.host = true;
	options.buffered_docs_cap = cap;
	return options;
}

template <typename F>
engine::errc error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

// Opening is a chore of the query class, off the opener's thread.
void test_open_is_a_chore() {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch("open"), true, true, host(chores)));
	CHECK_TRUE(opened.created);
	CHECK_EQ(chores.count(), 1u);
	CHECK_EQ(chores.count(engine::ChoreClass::query), 1u);
	CHECK_FALSE(chores.ran_on(std::this_thread::get_id()));
}

// A write that fits the buffer owes nothing; the one past the bound owes a flush, which runs as a chore
// off the writer's thread, and the same write is then applied: every write lands once, in order.
void test_owed_flush_runs_as_a_chore_and_the_write_is_retried() {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch("owed"), true, true, host(chores, 3)));
	auto& shard = *opened.shard;
	CHECK_EQ(chores.count(engine::ChoreClass::flush), 0u);
	for (int i = 1; i <= 10; ++i) {
		auto doc = make("K" + std::to_string(i), "d" + std::to_string(i));
		const auto written = engine::sync_wait(shard.add(doc, {}));
		CHECK_EQ(written.did, static_cast<engine::docid>(i));
		CHECK_EQ(written.version, 1u);
		// Writes 4, 7 and 10 each found the buffer full.
		CHECK_EQ(chores.count(engine::ChoreClass::flush), static_cast<std::size_t>((i - 1) / 3));
	}
	CHECK_FALSE(chores.ran_on(std::this_thread::get_id()));
	// The flushed ones are in segments; the last is only in the buffer.
	CHECK_TRUE(shard.dirty());
	CHECK_EQ(engine::sync_wait(shard.get(9))->data(), "d9");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(10)); }) == engine::errc::doc_not_found);
	// A read sees one published view, which holds what the last refresh flushed and not the buffer.
	CHECK_EQ(engine::sync_wait(shard.find_key("K10")), 0u);
	CHECK_EQ(engine::sync_wait(shard.find_key("K9")), 9u);
	engine::sync_wait(shard.refresh());
	CHECK_EQ(engine::sync_wait(shard.find_key("K10")), 10u);
}

// A host's read sees what was applied only once refresh() flushed it, and refresh() does nothing when
// there is nothing to flush.
void test_reads_see_writes_after_refresh() {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch("refresh"), true, true, host(chores)));
	auto& shard = *opened.shard;
	CHECK_FALSE(shard.dirty());
	auto doc = make("Ka", "a");
	engine::sync_wait(shard.add(doc, {}));
	CHECK_TRUE(shard.dirty());
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::doc_not_found);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 0u);
	const auto before = chores.count();
	engine::sync_wait(shard.refresh());
	CHECK_EQ(chores.count(), before + 1);
	CHECK_FALSE(shard.dirty());
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 1u);
	engine::sync_wait(shard.refresh());
	CHECK_EQ(chores.count(), before + 1);  // nothing to flush: no chore
}

// Without a host the same index flushes inside the write and reads refresh for themselves.
void test_without_a_host_the_work_is_inline() {
	engine::OpenOptions options;
	options.buffered_docs_cap = 3;
	auto opened = engine::sync_wait(engine::open_local(scratch("inline"), true, true, options));
	auto& shard = *opened.shard;
	for (int i = 1; i <= 10; ++i) {
		auto doc = make("K" + std::to_string(i), "d" + std::to_string(i));
		CHECK_EQ(engine::sync_wait(shard.add(doc, {})).did, static_cast<engine::docid>(i));
	}
	CHECK_EQ(engine::sync_wait(shard.get(10))->data(), "d10");
}

// A commit and a close are flush chores; a closed shard answers `closed`, and the data is on disk.
void test_commit_and_close_are_chores() {
	const auto path = scratch("close");
	ThreadChores chores;
	{
		auto opened = engine::sync_wait(engine::open_local(path, true, true, host(chores)));
		auto& shard = *opened.shard;
		auto doc = make("Ka", "a");
		engine::sync_wait(shard.add(doc, {}));
		const auto revision = shard.revision();
		const auto before = chores.count(engine::ChoreClass::flush);
		engine::sync_wait(shard.commit(engine::Transaction::none));
		CHECK_EQ(chores.count(engine::ChoreClass::flush), before + 1);
		CHECK_EQ(shard.revision(), revision + 1);
		auto doc2 = make("Kb", "b");
		engine::sync_wait(shard.add(doc2, {}));
		engine::sync_wait(shard.close());  // commits Kb
		CHECK_EQ(chores.count(engine::ChoreClass::flush), before + 2);
		CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.commit(engine::Transaction::none)); }) == engine::errc::closed);
		CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::closed);
	}
	auto reopened = engine::sync_wait(engine::open_local(path, true, false, host(chores)));
	CHECK_FALSE(reopened.created);
	auto& shard = *reopened.shard;
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "a");
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b");
}

// What a chore throws reaches the task that awaited it, translated as any engine error is.
void test_a_chore_error_reaches_the_awaiter() {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch("error"), true, true, host(chores)));
	auto& shard = *opened.shard;
	engine::sync_wait(shard.close());
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.commit(engine::Transaction::none)); }) == engine::errc::closed);
}

// The idle index's housekeeping, reclaim(), is a merge-class chore: it publishes finished merges and frees the files the
// index queued for removal, off the thread that awaits it.
void test_reclaim_is_a_merge_chore() {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch("reclaim"), true, true, host(chores)));
	auto& shard = *opened.shard;
	auto doc = make("Ka", "a");
	engine::sync_wait(shard.add(doc, {}));
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.merges_pending(), 0u);
	CHECK_EQ(chores.count(engine::ChoreClass::merge), 0u);
	engine::sync_wait(shard.reclaim());
	CHECK_EQ(chores.count(engine::ChoreClass::merge), 1u);
	CHECK_FALSE(chores.ran_on(std::this_thread::get_id()));
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "a");
}

}  // namespace

TEST_MAIN(test_reclaim_is_a_merge_chore, test_open_is_a_chore, test_owed_flush_runs_as_a_chore_and_the_write_is_retried, test_reads_see_writes_after_refresh, test_without_a_host_the_work_is_inline, test_commit_and_close_are_chores, test_a_chore_error_reaches_the_awaiter)
