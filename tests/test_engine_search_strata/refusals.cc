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

// What the engine refuses: bad requests, invalid query shapes and options.

#include "support.h"

namespace detail {

namespace {

engine::SearchResult search_with(const Index& index, const Query& query, const std::function<void(engine::SearchRequest&)>& tweak) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 10;
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(1), request));
}

}  // namespace

void test_refusals_impl() {
	Index strata;
	Model model;
	build("refusals", 1, 10, 1, strata, model);
	const auto t = [](const char* w) { return Query::term("f", w); };
	const auto none = [](engine::SearchRequest&) { };
	const auto syntax = engine::errc::syntax;
	CHECK_EQ(search_with(strata, t("w1"), none).hits.size() > 0, true);
	// A PHRASE with a window answers rather than refusing (strata reads its terms' positions, not the window).
	search_with(strata, Query::combine(Op::PHRASE, {t("w1"), t("w2")}, 5), none);
	// A tree nested past the cap is refused when it is built, before any engine sees it.
	CHECK_EQ(error_kind([&] {
		Query deep = t("w1");
		for (int i = 0; i < 1200; ++i) {
			deep = Query::combine(Op::AND, deep, t("common"));
		}
	}), syntax);
}

// What is not a valid query is refused when the query is built, whatever the engine and the index: a
// PHRASE or NEAR over anything but words and their alternatives, a SYNONYM of anything but words, an
// operator that cannot take a list or join a query, and an expansion combined by anything but SYNONYM,
// OR or MAX.
void test_invalid_shapes_impl() {
	const auto t = [](const char* w) { return Query::term("f", w); };
	const auto syntax = engine::errc::syntax;
	const auto refused = [&](const std::function<void()>& build) { return error_kind(build) == syntax; };
	const auto both = Query::combine(Op::AND, t("w1"), t("w2"));
	const auto either = Query::combine(Op::OR, t("w2"), t("w3"));
	const auto wildcard = Query::wildcard("f", "w");
	for (const auto op : {Op::PHRASE, Op::NEAR}) {
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), both}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {both, t("w1")}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), Query::combine(Op::AND_NOT, t("w2"), t("w3"))}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), Query::combine(Op::PHRASE, t("w2"), t("w3"))}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), Query::combine(Op::OR, either, both)}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), Query::scale(t("w2"), 2)}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), Query::match_all()}); }));
		// Words, expansions and ORs and SYNONYMs of those are positions.
		Query::combine(op, {t("w1"), either});
		Query::combine(op, {t("w1"), wildcard});
		Query::combine(op, {t("w1"), Query::edit_distance("f", "w1", 0, engine::Expansion::error, Op::SYNONYM, 1, 0)});
		Query::combine(op, {t("w1"), Query::combine(Op::OR, wildcard, t("w3"))});
		Query::combine(op, {t("w1"), Query::combine(Op::SYNONYM, t("w2"), t("w3"))});
		Query::combine(op, {t("w1"), Query::append(Op::OR, Query::append(Op::OR, Query(), t("w2")), t("w3"))});
		CHECK_FALSE(Query::combine(op, {t("w1"), t("w2")}).alternatives());
		CHECK_TRUE(Query::combine(op, {t("w1"), either}).alternatives());
		CHECK_TRUE(Query::combine(Op::AND, Query::combine(op, {t("w1"), wildcard}), t("w3")).alternatives());
		CHECK_FALSE(Query::combine(Op::AND, either, t("w3")).alternatives());
	}
	CHECK_TRUE(refused([&] { Query::combine(Op::SYNONYM, {t("w1"), either}); }));
	CHECK_TRUE(refused([&] { Query::combine(Op::SYNONYM, {t("w1"), wildcard}); }));
	CHECK_TRUE(refused([&] { Query::combine(Op::SYNONYM, {t("w1"), Query::match_all()}); }));
	CHECK_TRUE(refused([&] { Query::combine(Op::SYNONYM, t("w1"), both); }));
	Query::combine(Op::SYNONYM, {t("w1"), t("w2"), t("w3")});
	for (const auto op : {Op::SCALE_WEIGHT, Op::WILDCARD, Op::EDIT_DISTANCE, Op::INVALID}) {
		CHECK_TRUE(refused([&] { Query::combine(op, {t("w1"), t("w2")}); }));
		CHECK_TRUE(refused([&] { Query::combine(op, t("w1"), t("w2")); }));
	}
	// An append of an operator that does not join onto a query; onto nothing it is the next query.
	for (const auto op : {Op::PHRASE, Op::NEAR, Op::SYNONYM, Op::SCALE_WEIGHT}) {
		CHECK_TRUE(refused([&] { Query::append(op, t("w1"), t("w2")); }));
		Query::append(op, Query(), t("w2"));
	}
	for (const auto combiner : {Op::AND, Op::AND_NOT, Op::XOR, Op::FILTER, Op::AND_MAYBE, Op::PHRASE, Op::NEAR}) {
		CHECK_TRUE(refused([&] { Query::wildcard("f", "w", 0, engine::Expansion::error, 0, combiner); }));
		CHECK_TRUE(refused([&] { Query::edit_distance("f", "w1", 0, engine::Expansion::error, combiner, 1, 0); }));
	}
	for (const auto combiner : {Op::SYNONYM, Op::OR, Op::MAX}) {
		Query::wildcard("f", "w", 0, engine::Expansion::error, 0, combiner);
		Query::edit_distance("f", "w1", 0, engine::Expansion::error, combiner, 1, 0);
	}
}

}  // namespace detail
