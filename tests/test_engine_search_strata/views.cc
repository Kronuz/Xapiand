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

// A search's view holds nothing: writers finish while a spy is held, and a failed reopen is pinned.

#include "support.h"

namespace detail {

namespace {

// A spy that stops inside its first see() until released, with a view of every shard held by the search.
class BlockingSpy final : public engine::MatchSpy {
public:
	std::promise<void> inside;
	std::shared_future<void> gate;
	std::atomic<bool> first{true};
	std::atomic<int> seen{0};

	void see(const engine::DocView&, double) override {
		++seen;
		if (first.exchange(false)) {
			inside.set_value();
			gate.wait();
		}
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountingSpy>(); }
	std::string name() const override { return "BlockingSpy"; }
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialize(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialize_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "BlockingSpy"; }
};

}  // namespace

// While a spy is held inside see(), every shard is read through its view: a write, a refresh, a commit and a
// delete on each shard all finish meanwhile, and the held search still sees the state it started with. The spy
// is released only after the writers return, so a write that waited for the search would never return.
// (Reads once took the shard's lock for the whole search, and these waited for it: phase 7c D6.)
void test_view_holds_nothing_impl() {
	Index strata;
	Model model;
	build("view_holds", 3, 20, 2, strata, model, true);
	std::promise<void> release;
	BlockingSpy spy;
	spy.gate = release.get_future().share();
	engine::SearchResult result;
	std::thread searcher([&] {
		std::vector<engine::Shard*> order{strata.owned[2].get(), strata.owned[0].get(), strata.owned[1].get()};
		auto prepared = engine::prepare(Query::term("f", "common"));
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 1000;
		request.spy = &spy;
		result = engine::sync_wait(engine::search(order, request));
	});
	spy.inside.get_future().wait();
	std::atomic<int> finished{0};
	std::vector<std::thread> writers;
	for (std::size_t s = 0; s < 3; ++s) {
		// One thread per shard is the shard's writer: the host's turn.
		writers.emplace_back([&, s] {
			auto doc = make(static_cast<int>(s), 777, model.shards[0][0]);
			engine::sync_wait(strata.owned[s]->add(doc, {}));
			++finished;
			engine::sync_wait(strata.owned[s]->refresh());
			++finished;
			engine::sync_wait(strata.owned[s]->commit(engine::Transaction::none));
			++finished;
			engine::sync_wait(strata.owned[s]->erase_by_key("Q" + std::to_string(s) + "_3", true, std::nullopt));
			engine::sync_wait(strata.owned[s]->refresh());
			++finished;
			engine::sync_wait(strata.owned[s]->commit(engine::Transaction::none));
			++finished;
		});
	}
	for (auto& writer : writers) { writer.join(); }
	const int during = finished.load();
	const int seen_during = spy.seen.load();
	release.set_value();
	searcher.join();
	CHECK_EQ(during, 15);
	CHECK_EQ(seen_during, 1);   // the search was still held inside its first see()
	CHECK_EQ(result.hits.size(), 60u);
	CHECK_EQ(result.estimated, 60u);
	CHECK_EQ(spy.seen.load(), 60);
	// The next search sees the writes: three documents added, three erased.
	const auto after = run(strata, 3, Query::term("f", "common"), 0, 1000);
	CHECK_EQ(after.hits.size(), 60u);
}

// A pin after a failed reopen raises a database error and releases the other shards' locks; it never reads the
// closed index. After a reopen the shard searches again.
void test_pin_after_failed_reopen_impl() {
	if (::geteuid() == 0) {
		return;
	}
	Index strata;
	Model model;
	build("pin_failed", 2, 20, 1, strata, model);
	auto& broken = *strata.owned[0];
	engine::sync_wait(broken.begin_transaction(true));
	auto pending = make(0, 556, model.shards[0][0]);
	engine::sync_wait(broken.add(pending, {}));
	// path_of() clears the path it names; this is the directory build() made for shard 0.
	const auto dir = (std::filesystem::current_path() / "engine_search_strata_pin_failed_0.db" / "strata").string();
	::chmod(dir.c_str(), 0000);
	CHECK_EQ(error_kind([&] { engine::sync_wait(broken.cancel_transaction()); }), engine::errc::database);
	::chmod(dir.c_str(), 0700);

	auto prepared = engine::prepare(Query::term("f", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1000;
	CHECK_EQ(error_kind([&] { engine::sync_wait(engine::search(strata.shards(2), request)); }), engine::errc::database);
	CHECK_EQ(error_kind([&] { engine::sync_wait(engine::search(strata.shards(1), request)); }), engine::errc::database);
	// The other shard's lock was released with the failed search: a write to it goes through.
	auto doc = make(1, 555, model.shards[1][0]);
	engine::sync_wait(strata.owned[1]->add(doc, {}));
	CHECK_EQ(engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[1].get()}, request)).hits.size(), 21u);

	CHECK_TRUE(engine::sync_wait(broken.reopen()));
	CHECK_EQ(engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[0].get()}, request)).hits.size(), 20u);
}

}  // namespace detail
