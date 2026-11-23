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

// Queries over one shard and several, positional queries, appends and the spy, against the model.

#include "support.h"

namespace detail {

void test_one_shard_impl() {
	Index strata;
	Model model;
	build("one", 1, 60, 3, strata, model);
	random_queries(strata, model, 1, 7, 300, true);
	random_queries(strata, model, 1, 8, 300, false);
	// A single term finds what the model says, heaviest first.
	const auto found = run(strata, 1, Query::term("f", "w1"), 0, 10);
	CHECK_GT(found.hits.size(), 0u);
	for (std::size_t i = 1; i < found.hits.size(); ++i) {
		CHECK_GE(found.hits[i - 1].weight, found.hits[i].weight);
	}
	CHECK_EQ(found.hits.front().percent, 100);
}

// Positional operators on their own and under AND match and weigh as the model says.
void test_positional_impl() {
	Index strata;
	Model model;
	build("positional", 2, 80, 2, strata, model);
	const auto t = [](const char* w) { return Query::term("f", w); };
	for (const std::size_t shards : {1u, 2u}) {
		for (int a = 0; a < vocabulary; ++a) {
			for (int b = 0; b < vocabulary; ++b) {
				if (a == b) { continue; }
				const auto wa = "w" + std::to_string(a), wb = "w" + std::to_string(b);
				const auto phrase = Query::combine(Op::PHRASE, {t(wa.c_str()), t(wb.c_str())});
				const auto near = Query::combine(Op::NEAR, {t(wa.c_str()), t(wb.c_str())}, 4);
				for (const auto& query : {phrase, near, Query::combine(Op::AND, phrase, t("common")), Query::combine(Op::AND, near, t("even"))}) {
					const int before = testh::failures;
					compare(strata, model, shards, query);
					if (testh::failures != before) { std::fprintf(stderr, "  %s\n", query.description().c_str()); }
				}
			}
		}
	}
}

// An append onto a query that matches nothing by its structure (a text that analysed to nothing) is the
// appended query; a term that no segment holds is a different matter and joins as nothing.
void test_append_impl() {
	Index strata;
	Model model;
	build("append", 2, 40, 2, strata, model);
	const auto t = [](const char* w) { return Query::term("f", w); };
	std::vector<Query> queries;
	queries.push_back(Query::append(Op::AND, Query(), t("w1")));
	queries.push_back(Query::append(Op::AND, Query::append(Op::AND, Query(), t("w1")), t("tag2")));
	queries.push_back(Query::append(Op::OR, Query::append(Op::AND, Query(), t("w1")), t("tag2")));
	queries.push_back(Query::append(Op::AND, Query::append(Op::AND, Query(), Query()), t("w3")));
	queries.push_back(Query::append(Op::AND, t("zzz"), t("w1")));
	queries.push_back(Query::append(Op::AND, t("w1"), t("zzz")));
	queries.push_back(Query::append(Op::AND_NOT, Query(), t("w1")));
	for (std::size_t shards = 1; shards <= 2; ++shards) {
		for (const auto& query : queries) {
			compare(strata, model, shards, query);
		}
	}
	CHECK_EQ(run(strata, 2, queries[0], 0, 1000).estimated, model_hits(model, t("w1"), 2).size());
	CHECK_EQ(run(strata, 2, queries[4], 0, 1000).estimated, 0u);
}

void test_two_shards_impl() {
	Index strata;
	Model model;
	build("two", 2, 50, 2, strata, model);
	random_queries(strata, model, 2, 11, 300, true);
	random_queries(strata, model, 2, 12, 300, false);
	random_queries(strata, model, 1, 13, 50, true);
}

// A spy is shown every match once, with its id in its shard, its data and its weight, whatever window or
// order the request asks for and whether or not any hit is wanted.
void test_spy_impl() {
	Index strata;
	Model model;
	build("spy", 3, 30, 2, strata, model);
	const auto query = Query::combine(Op::OR, Query::term("f", "w1"), Query::term("f", "tag2"));
	const auto expected = model_hits(model, query, 3);
	// What the spy must be shown: the (id in its shard, weight) of every match, sorted.
	std::vector<std::pair<engine::docid, double>> want;
	for (const auto& [id, weight] : expected) { want.emplace_back(static_cast<engine::docid>((id - 1) / 3 + 1), weight); }
	std::sort(want.begin(), want.end());
	struct Variant { engine::doccount first, maxitems; engine::DocidOrder order; };
	for (const auto& variant : {Variant{0, 3, engine::DocidOrder::ascending}, Variant{0, 0, engine::DocidOrder::ascending}, Variant{2, 4, engine::DocidOrder::descending}}) {
		CountingSpy spy;
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		request.first = variant.first;
		request.maxitems = variant.maxitems;
		request.order = variant.order;
		request.check_at_least = 100000;
		request.spy = &spy;
		const auto result = engine::sync_wait(engine::search(strata.shards(3), request));
		CHECK_EQ(spy.seen.size(), expected.size());
		CHECK_EQ(result.estimated, expected.size());
		std::vector<std::pair<engine::docid, double>> got;
		for (std::size_t i = 0; i < spy.seen.size(); ++i) {
			CHECK_EQ(spy.data[i], "d" + std::to_string(spy.seen[i]));
			got.emplace_back(spy.seen[i], spy.weights[i]);
		}
		std::sort(got.begin(), got.end());
		REQUIRE_EQ(got.size(), want.size());
		for (std::size_t i = 0; i < got.size(); ++i) {
			CHECK_EQ(got[i].first, want[i].first);
			CHECK_TRUE(close(got[i].second, want[i].second));
		}
	}
	// A deleted document is not shown: the common term matches every live document of the three shards.
	engine::sync_wait(strata.owned[0]->erase_by_key("Q0_5", true, std::nullopt));
	CountingSpy spy;
	auto prepared = engine::prepare(Query::term("f", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1;
	request.spy = &spy;
	const auto result = engine::sync_wait(engine::search(strata.shards(3), request));
	CHECK_EQ(spy.seen.size(), 89u);
	CHECK_EQ(result.estimated, 89u);
	CHECK_EQ(std::count(spy.seen.begin(), spy.seen.end(), 5u), 2);
}

void test_five_shards_impl() {
	Index strata;
	Model model;
	build("five", 5, 40, 2, strata, model);
	random_queries(strata, model, 5, 17, 200, true);
	random_queries(strata, model, 5, 18, 200, false);
	// Ties go to the lower id: match_all weighs every document alike.
	const auto all = run(strata, 5, Query::match_all(), 0, 1000);
	REQUIRE_EQ(all.hits.size(), 200u);
	for (std::size_t i = 0; i < all.hits.size(); ++i) {
		CHECK_EQ(all.hits[i].did, i + 1);
	}
	const auto descending = run(strata, 5, Query::match_all(), 0, 1000, engine::DocidOrder::descending);
	REQUIRE_EQ(descending.hits.size(), 200u);
	for (std::size_t i = 0; i < descending.hits.size(); ++i) {
		CHECK_EQ(descending.hits[i].did, 200 - i);
	}
}

void test_no_shards_and_empty_impl() {
	Index strata;
	Model model;
	build("empty", 1, 4, 1, strata, model);
	auto none = engine::prepare(Query());
	engine::SearchRequest request;
	request.query = none.get();
	CHECK_EQ(engine::sync_wait(engine::search(strata.shards(1), request)).hits.size(), 0u);
	auto some = engine::prepare(Query::match_all());
	request.query = some.get();
	CHECK_EQ(engine::sync_wait(engine::search(std::span<engine::Shard* const>{}, request)).hits.size(), 0u);
	CHECK_EQ(engine::sync_wait(engine::search(strata.shards(1), request)).hits.size(), 4u);
}

}  // namespace detail
