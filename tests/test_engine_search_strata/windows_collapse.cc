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

// Result windows, percent cutoffs, and collapse by the value of a slot.

#include "support.h"

namespace detail {

// A window larger than the index is clamped to it: it answers as a window of the index's size does, at
// once, and holds no memory for the number asked.
void test_huge_window_impl() {
	Index strata;
	Model model;
	build("huge_window", 2, 40, 2, strata, model);
	const auto query = Query::term("f", "common");
	const auto all = run(strata, 2, query, 0, 1000);
	CHECK_EQ(all.hits.size(), 80u);
	constexpr engine::doccount big = 0xFFFFFFFFu;
	const auto begin = std::chrono::steady_clock::now();
	for (const auto order : {engine::DocidOrder::ascending, engine::DocidOrder::descending}) {
		const auto reference = run(strata, 2, query, 0, 1000, order);
		for (const auto& [first, maxitems] : {std::pair{0u, big}, std::pair{big, 10u}, std::pair{10u, big}, std::pair{big, big}}) {
			const auto found = run(strata, 2, query, first, maxitems, order);
			CHECK_EQ(found.estimated, all.estimated);
			const std::size_t skip = std::min<std::size_t>(first, reference.hits.size());
			CHECK_EQ(found.hits.size(), reference.hits.size() - skip);
			for (std::size_t i = 0; i < found.hits.size(); ++i) {
				CHECK_EQ(found.hits[i].did, reference.hits[skip + i].did);
				CHECK_EQ(found.hits[i].rank, reference.hits[skip + i].rank);
			}
		}
	}
	CountingSpy spy;
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = big;
	request.spy = &spy;
	CHECK_EQ(engine::sync_wait(engine::search(strata.shards(2), request)).hits.size(), 80u);
	CHECK_EQ(spy.seen.size(), 80u);
	CHECK_TRUE(std::chrono::steady_clock::now() - begin < std::chrono::seconds(10));
}

// A hit's percent stays in [0, 100] when the top weight overflows to infinity: the infinite hits are 100, the
// rest 0 (a finite weight against an infinite top).
void test_percent_infinite_impl() {
	Index strata;
	Model model;
	build("percent", 2, 40, 2, strata, model);
	const auto query = Query::combine(Op::OR, Query::scale(Query::term("f", "w1"), HUGE_VAL), Query::term("f", "w2"));
	for (const bool spied : {false, true}) {
		CountingSpy spy;
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 100;
		request.spy = spied ? &spy : nullptr;
		const auto found = engine::sync_wait(engine::search(strata.shards(2), request));
		std::size_t infinite = 0;
		CHECK_TRUE(!found.hits.empty());
		for (const auto& hit : found.hits) {
			infinite += std::isinf(hit.weight);
			CHECK_EQ(hit.percent, std::isinf(hit.weight) ? 100 : 0);
		}
		CHECK_TRUE(infinite > 0);
	}
	// Finite weights keep their ratio to the top.
	const auto plain = run(strata, 2, Query::combine(Op::OR, Query::term("f", "w1"), Query::term("f", "w2")), 0, 100);
	CHECK_TRUE(!plain.hits.empty());
	CHECK_EQ(plain.hits.front().percent, 100);
	for (const auto& hit : plain.hits) {
		CHECK_EQ(hit.percent, static_cast<int>(hit.weight / plain.hits.front().weight * 100.0 + 0.5));
	}
}

namespace {

// Orders hits by the value of a slot.
class SlotKey final : public engine::KeyMaker {
public:
	explicit SlotKey(engine::valueno slot) : slot_(slot) { }
	std::string key(const engine::DocView& doc) const override { return doc.value(slot_); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(slot_); }
	std::string name() const override { return "SlotKey"; }
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view) const override { return std::make_unique<SlotKey>(slot_); }

private:
	engine::valueno slot_;
};

}  // namespace

// Collapse keeps the best `collapse_max` hits per value of slot 2 and never collapses a document
// without the value. The strata engine's answer is checked against an oracle (the uncollapsed ranking,
// cut to the first `collapse_max` of each group), over one shard and several, for each docid order, with
// and without a sorter, and the spy is shown every match. Windows are slices of the full answer.
void test_collapse_impl() {
	const SlotKey by_slot(3);
	const std::vector<Query> queries{
		Query::combine(Op::OR, Query::term("f", "w1"), Query::term("f", "tag2")),
		Query::term("f", "common"),
		Query::match_all(),
		Query::term("f", "even"),
	};
	struct Window { engine::doccount first, maxitems; };
	const auto search = [&](const Index& index, std::size_t shards, const Query& query, engine::doccount collapse_max, engine::DocidOrder order, bool sorted, Window window, CountingSpy* spy) {
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		request.first = window.first;
		request.maxitems = window.maxitems;
		request.check_at_least = 100000;
		request.order = order;
			request.collapse_key = 2;
		request.collapse_max = collapse_max;
		request.sorter = sorted ? &by_slot : nullptr;
		request.spy = spy;
		return engine::sync_wait(engine::search(index.shards(shards), request));
	};
	// The group of a hit, as `make` assigns it; -1 for a document without one.
	const auto group_of = [](engine::docid did, std::size_t shards) {
		const std::size_t shard = (did - 1) % shards, i = (did - 1) / shards + 1;
		return i % 7 == 0 ? -1 : static_cast<int>((shard * 3 + i) % 4);
	};
	std::size_t compared = 0, collapsed_away = 0;
	for (const std::size_t shards : {1u, 2u, 5u}) {
		Index strata;
		Model model;
		build("collapse", static_cast<int>(shards), 40, 2, strata, model);
		for (const auto& query : queries) {
			for (const auto order : {engine::DocidOrder::ascending, engine::DocidOrder::descending}) {
				for (const bool sorted : {false, true}) {
					const auto all = search(strata, shards, query, 0, order, sorted, {0, 1000}, nullptr);
					for (const engine::doccount collapse_max : {1u, 2u, 3u}) {
						CountingSpy spy_s;
						const auto s = search(strata, shards, query, collapse_max, order, sorted, {0, 1000}, &spy_s);
						// The oracle: walk the full ranking, keep the first collapse_max of each group.
						std::vector<engine::docid> want;
						std::map<int, engine::doccount> taken;
						for (const auto& hit : all.hits) {
							const int group = group_of(hit.did, shards);
							if (group < 0 || taken[group]++ < collapse_max) { want.push_back(hit.did); }
						}
						REQUIRE_EQ(s.hits.size(), want.size());
						CHECK_EQ(s.estimated, want.size());
						for (std::size_t i = 0; i < want.size(); ++i) {
							CHECK_EQ(s.hits[i].did, want[i]);
							CHECK_EQ(s.hits[i].rank, i);
						}
						// The spy is shown every match, collapsed away or not.
						auto seen_s = spy_s.seen;
						std::sort(seen_s.begin(), seen_s.end());
						CHECK_EQ(seen_s.size(), all.hits.size());
						++compared;
						if (s.hits.size() < seen_s.size()) { ++collapsed_away; }
						for (const auto window : {Window{0, 3}, Window{2, 4}, Window{5, 1000}, Window{1000, 5}, Window{0, 0}}) {
							const auto got = search(strata, shards, query, collapse_max, order, sorted, window, nullptr);
							CHECK_EQ(got.estimated, s.estimated);
							const std::size_t begin = std::min<std::size_t>(window.first, s.hits.size());
							const std::size_t end = std::min<std::size_t>(begin + window.maxitems, s.hits.size());
							REQUIRE_EQ(got.hits.size(), end - begin);
							for (std::size_t i = begin; i < end; ++i) {
								CHECK_EQ(got.hits[i - begin].did, s.hits[i].did);
								CHECK_EQ(got.hits[i - begin].rank, i);
							}
						}
					}
				}
			}
		}
		// One hit per value (collapse_max 1), and every document without the value stays.
		const auto result = search(strata, shards, Query::match_all(), 1, engine::DocidOrder::ascending, false, {0, 1000}, nullptr);
		std::size_t loose = 0;
		for (std::size_t i = 1; i <= 40; ++i) { loose += i % 7 == 0; }
		CHECK_EQ(result.hits.size(), loose * shards + 4);
	}
	CHECK_GT(compared, 100u);
	CHECK_GT(collapsed_away, 50u);
}

}  // namespace detail
