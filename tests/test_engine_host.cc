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
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "engine/callbacks.h"
#include "engine/chores.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/strata/shard.h"
#include "engine/strata/viewed.h"
#include "strata/core/owner.hh"
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


// A spy that stops inside its first see() until released: the search that runs it holds its view of the index.
class BlockingSpy final : public engine::MatchSpy {
public:
	std::atomic<bool> inside{false};
	std::shared_future<void> gate;

	void see(const engine::DocView&, double) override {
		if (!inside.exchange(true)) {
			gate.wait();
		}
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<BlockingSpy>(); }
	std::string name() const override { return "BlockingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<BlockingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "BlockingSpy"; }
};

// Closing the index, or replacing it, ends only when the searches that hold a view of it have let go: an index is
// never destroyed under a view. Meanwhile no new view is given, and the search that holds one reads on.
void closing_waits_for_the_readers(const char* name, bool reopen) {
	ThreadChores chores;
	auto opened = engine::sync_wait(engine::open_local(scratch(name), true, true, host(chores)));
	auto& shard = *opened.shard;
	for (int i = 1; i <= 3; ++i) {
		auto doc = make("K" + std::to_string(i), "d");
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));
	if (reopen) {
		engine::sync_wait(shard.begin_transaction(true));
		auto doc = make("Kt", "t");
		engine::sync_wait(shard.add(doc, {}));
	}

	std::promise<void> release;
	BlockingSpy spy;
	spy.gate = release.get_future().share();
	std::size_t hits = 0;
	std::thread searcher([&] {
		auto prepared = engine::prepare(engine::Query::term("f", "body"));
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 10;
		request.spy = &spy;
		hits = engine::sync_wait(engine::search(std::vector<engine::Shard*>{&shard}, request)).hits.size();
	});
	for (int i = 0; i < 5000 && !spy.inside; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	CHECK_TRUE(spy.inside);

	std::atomic<bool> replaced{false};
	std::thread closer([&] {
		if (reopen) {
			engine::sync_wait(shard.cancel_transaction());
		} else {
			engine::sync_wait(shard.close());
		}
		replaced = true;
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	CHECK_FALSE(replaced.load());  // the view is still held
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.find_key("K1")); }) != engine::errc::internal);  // no new view
	release.set_value();
	searcher.join();
	closer.join();
	CHECK_TRUE(replaced.load());
	CHECK_EQ(hits, 3u);  // the search read the view it held to the end
	if (reopen) {
		CHECK_EQ(engine::sync_wait(shard.find_key("K1")), 1u);  // the index is open again, from its last commit
		CHECK_EQ(engine::sync_wait(shard.find_key("Kt")), 0u);
	} else {
		CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.find_key("K1")); }) == engine::errc::closed);
	}
}

void test_close_waits_for_the_readers() { closing_waits_for_the_readers("close_readers", false); }
void test_a_reopen_waits_for_the_readers() { closing_waits_for_the_readers("reopen_readers", true); }

// Every reader record of a registry is held by a search: the next read is refused with busy, and a record freed
// serves the next (C9).
void test_a_full_registry_answers_busy() {
	ThreadChores chores;
	auto options = host(chores);
	options.readers_per_index = 2;
	auto opened = engine::sync_wait(engine::open_local(scratch("registry"), true, true, options));
	auto& shard = *opened.shard;
	auto doc = make("Ka", "a");
	engine::sync_wait(shard.add(doc, {}));
	engine::sync_wait(shard.refresh());

	std::promise<void> release;
	auto gate = release.get_future().share();
	std::vector<std::unique_ptr<BlockingSpy>> spies;
	for (int i = 0; i < 2; ++i) {
		spies.push_back(std::make_unique<BlockingSpy>());
		spies.back()->gate = gate;
	}
	std::vector<std::thread> searchers;
	for (int i = 0; i < 2; ++i) {
		searchers.emplace_back([&, i] {
			auto prepared = engine::prepare(engine::Query::term("f", "body"));
			engine::SearchRequest request;
			request.query = prepared.get();
			request.spy = spies[static_cast<std::size_t>(i)].get();
			engine::sync_wait(engine::search(std::vector<engine::Shard*>{&shard}, request));
		});
	}
	for (int i = 0; i < 5000 && !(spies[0]->inside && spies[1]->inside); ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	CHECK_TRUE(spies[0]->inside && spies[1]->inside);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.find_key("Ka")); }) == engine::errc::busy);
	CHECK_TRUE(error_kind([&] { (void)shard.doc_count(); }) == engine::errc::busy);
	// A write, a refresh and a commit need no record.
	auto second = make("Kb", "b");
	engine::sync_wait(shard.add(second, {}));
	engine::sync_wait(shard.refresh());
	engine::sync_wait(shard.commit(engine::Transaction::none));
	release.set_value();
	for (auto& searcher : searchers) { searcher.join(); }
	CHECK_EQ(engine::sync_wait(shard.find_key("Kb")), 2u);
}

// A reader that outlives its index (a close whose drain timed out, then the shard dropped) lets go of its record
// afterwards, into the registry the index's Core owns: the registry must outlive that guard (R2). Under ASan the
// release is a heap-use-after-free when the Core frees the registry with a record still claimed.
void test_a_view_outlives_a_drain_that_timed_out() {
	ThreadChores chores;
	auto options = host(chores);
	options.drain_timeout = std::chrono::milliseconds(100);
	auto opened = engine::sync_wait(engine::open_local(scratch("outlive"), true, true, options));
	auto doc = make("Ka", "a");
	engine::sync_wait(opened.shard->add(doc, {}));
	engine::sync_wait(opened.shard->refresh());
	auto view = static_cast<engine::strata::Shard&>(*opened.shard).view();
	CHECK_TRUE(static_cast<bool>(view));
	CHECK_TRUE(error_kind([&] { engine::sync_wait(opened.shard->close()); }) == engine::errc::busy);
	// Destroying an index under a held view is reported as a break of the owner's duty (an abort when asserts are on).
	::strata::detail::abort_on_owner_break().store(false);
	opened.shard.reset();
	view.release();
	::strata::detail::abort_on_owner_break().store(true);
}

// An index's teardown asks the registry whether a record is claimed without allocating one: the registry grows
// with the readers an index has, not with its capacity (C3). With a capacity of 4096, walking the records would
// allocate every chunk of it, for every index closed.
void test_a_teardown_allocates_no_reader_records() {
	ThreadChores chores;
	auto options = host(chores);
	options.readers_per_index = 4096;
	auto opened = engine::sync_wait(engine::open_local(scratch("teardown"), true, true, options));
	auto doc = make("Ka", "a");
	engine::sync_wait(opened.shard->add(doc, {}));
	engine::sync_wait(opened.shard->refresh());
	auto& strata = static_cast<engine::strata::Shard&>(*opened.shard);
	{
		auto view = strata.view();  // one reader claims a record, so one chunk exists
		CHECK_TRUE(static_cast<bool>(view));
	}
	auto probe = strata.reader_records_probe();
	const auto before = probe();
	CHECK_GT(before, 0u);
	CHECK_LT(before, 4096u);
	opened.shard.reset();  // ~Core asks claimed()
	CHECK_EQ(probe(), before);
}

TEST_MAIN(test_reclaim_is_a_merge_chore, test_open_is_a_chore, test_owed_flush_runs_as_a_chore_and_the_write_is_retried, test_reads_see_writes_after_refresh, test_without_a_host_the_work_is_inline, test_commit_and_close_are_chores, test_a_chore_error_reaches_the_awaiter, test_close_waits_for_the_readers, test_a_reopen_waits_for_the_readers, test_a_full_registry_answers_busy, test_a_view_outlives_a_drain_that_timed_out, test_a_teardown_allocates_no_reader_records)
