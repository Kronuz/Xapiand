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

// What is the strata engine's own: writes and deletes, phrase alternatives and fields.

#include "support.h"

namespace detail {

// --- what is the strata engine's own ---

// Writes become visible to a search with no commit, deletes and replacements are honoured, and a
// replaced document is found once, under its old id.
void test_writes_and_deletes_impl() {
	Index strata;
	Model model;
	build("writes", 1, 40, 2, strata, model);
	std::mt19937 rng(99);
	auto& shard = *strata.owned[0];
	// A write with no commit: searched at once.
	auto added = random_doc(rng, 41);
	added.terms.insert("fresh");
	added.wdf["fresh"] = 1;
	added.length += 1;
	auto doc = make(0, 41, added);
	engine::sync_wait(shard.add(doc, {}));
	CHECK_EQ(run(strata, 1, Query::term("f", "fresh"), 0, 10).estimated, 1u);
	CHECK_EQ(run(strata, 1, Query::term("f", "common"), 0, 10).estimated, 41u);
	// A delete.
	engine::sync_wait(shard.erase_by_key("Q0_5", true, std::nullopt));
	CHECK_EQ(run(strata, 1, Query::term("f", "common"), 0, 100).estimated, 40u);
	CHECK_EQ(run(strata, 1, Query::term("f", "common"), 0, 100).hits.size(), 40u);
	const auto gone = engine::sync_wait(shard.find_key("Q0_5"));
	CHECK_EQ(gone, 0u);
	for (const auto& hit : run(strata, 1, Query::term("f", "common"), 0, 100).hits) {
		CHECK_NE(hit.did, 5u);
	}
	// A replacement: new content under the key, found once.
	engine::Document replacement;
	replacement.set_key("Q0_7");
	replacement.add_posting(replacement.field("f"), "renewed", 1);
	replacement.add_posting(replacement.field("f"), "common", 2);
	engine::sync_wait(shard.replace_by_key("Q0_7", replacement, {}));
	const auto renewed = run(strata, 1, Query::term("f", "renewed"), 0, 10);
	CHECK_EQ(renewed.estimated, 1u);
	REQUIRE_EQ(renewed.hits.size(), 1u);
	CHECK_EQ(renewed.hits[0].did, engine::sync_wait(shard.find_key("Q0_7")));
	CHECK_EQ(run(strata, 1, Query::term("f", "common"), 0, 100).estimated, 40u);
	// The old content of the replaced document is gone from the terms it no longer has.
	std::size_t had = 0;
	for (const auto& term : model.shards[0][6].terms) {
		if (term.starts_with("tag")) {
			++had;
			const auto found = run(strata, 1, Query::term("f", term), 0, 100);
			for (const auto& hit : found.hits) {
				CHECK_NE(hit.did, engine::sync_wait(shard.find_key("Q0_7")));
			}
		}
	}
	CHECK_GT(had, 0u);
}

// A PHRASE or NEAR whose positions hold alternatives (a wildcard or edit-distance expansion, an OR or a
// SYNONYM of words, an expansion beside a word as a partial word reads): the documents, their weights (the
// positions' weights summed) and their order are the model's, over one to five shards and several segments.
void test_phrase_alternatives_impl() {
	Index strata;
	Model model;
	build("alternatives", 5, 150, 3, strata, model);
	const auto t = [](const char* w) { return Query::term("f", w); };
	const auto word = [&](int k) { return t(("w" + std::to_string(k)).c_str()); };
	const auto any = [&](int a, int b) { return Query::combine(Op::OR, word(a), word(b)); };
	const auto together = [&](int a, int b) { return Query::combine(Op::SYNONYM, {word(a), word(b)}); };
	const auto prefix = [](const char* p) { return Query::wildcard("f", p); };
	const auto fuzzy = [](const char* p, unsigned distance) { return Query::edit_distance("f", p, 0, engine::Expansion::error, Op::SYNONYM, distance, 0); };
	// "w1 (w2|w3)", "(w4|w5) w0" as a SYNONYM, "w1 w*", "w2 w1~" and "w0 w*|w4" (a word with a partial word).
	const std::vector<std::vector<Query>> shapes = {
		{ word(1), any(2, 3) },
		{ together(4, 5), word(0) },
		{ any(0, 1), any(1, 2), together(2, 3) },
		{ word(1), prefix("w") },
		{ word(2), fuzzy("w1", 1) },
		{ fuzzy("w3x", 1), word(0) },
		{ word(0), Query::combine(Op::OR, prefix("w"), word(4)) },
		{ prefix("w"), prefix("w") },
		{ prefix("w1"), Query::combine(Op::OR, any(2, 3), word(5)), word(0) },
	};
	int shapes_found = 0, near_found = 0;
	for (const std::size_t shards : {1u, 2u, 5u}) {
		for (const auto& slots : shapes) {
			for (const auto op : {Op::PHRASE, Op::NEAR}) {
				for (const std::uint32_t window : {0u, 3u, 6u}) {
					if (op == Op::PHRASE && window != 0) { continue; }
					const auto query = Query::combine(op, std::vector<Query>(slots), op == Op::NEAR ? window : 0);
					CHECK_TRUE(query.alternatives());
					for (const auto& candidate : {query, Query::combine(Op::AND, query, t("common")), Query::combine(Op::OR, query, t("tag1"))}) {
						const int before = testh::failures;
						const auto want = model_hits(model, candidate, shards);
						const auto got = run(strata, shards, candidate, 0, 100000);
						if (got.hits.size() != want.size() && std::getenv("DIAG")) { std::fprintf(stderr, "got %zu want %zu\n", got.hits.size(), want.size()); }
						CHECK_EQ(got.hits.size(), want.size());
						CHECK_EQ(got.estimated, want.size());
						for (std::size_t i = 0; i < got.hits.size(); ++i) {
							const auto it = want.find(got.hits[i].did);
							CHECK_TRUE(it != want.end());
							if (it != want.end()) { CHECK_TRUE(close(got.hits[i].weight, it->second)); }
							if (i != 0) { CHECK_GE(got.hits[i - 1].weight + tolerance, got.hits[i].weight); }
						}
						shapes_found += !want.empty();
						near_found += op == Op::NEAR && !want.empty();
						if (testh::failures != before) { std::fprintf(stderr, "  %zu shards: %s\n", shards, candidate.description().c_str()); }
					}
				}
			}
		}
	}
	CHECK_GT(shapes_found, 100);
	CHECK_GT(near_found, 50);
	// The query is a valid one: the engine prepares it.
	{
		auto prepared = engine::prepare(Query::combine(Op::PHRASE, {word(1), any(2, 3)}));
		CHECK_FALSE(prepared->empty());
	}
}

// A term is looked up under its field: two fields holding the same word are found each under its own, a
// marker term (a field with an empty term) matches the documents that have the field, not every
// document, and a phrase reads its words under their field. Both engines agree.
void test_fields_impl() {
	Index strata;
	auto opened = engine::sync_wait(engine::open_local(path_of("fields", 0), true, true));
	strata.owned.clear();
	strata.owned.push_back(std::move(opened.shard));
	auto& shard = *strata.owned[0];
	const auto add = [&](const char* key, const std::function<void(engine::Document&)>& fill) {
		engine::Document doc;
		doc.set_key(key);
		fill(doc);
		doc.set_data(key);
		engine::sync_wait(shard.add(doc, {}));
	};
	add("Q1", [](engine::Document& d) {   // title: same, "first second"; the tags marker
		const auto title = d.field("title"), tags = d.field("tags");
		d.add_term(title, "same");
		d.add_posting(title, "first", 1);
		d.add_posting(title, "second", 2);
		d.add_term(tags, "");
	});
	add("Q2", [](engine::Document& d) {   // body: same, "first second" in the other order
		const auto body = d.field("body");
		d.add_term(body, "same");
		d.add_posting(body, "second", 1);
		d.add_posting(body, "first", 2);
	});
	add("Q3", [](engine::Document& d) {   // both fields, the words split across fields
		const auto title = d.field("title"), body = d.field("body");
		d.add_term(title, "same");
		d.add_term(body, "same");
		d.add_posting(title, "first", 1);
		d.add_posting(body, "second", 1);
	});
	add("Q4", [](engine::Document& d) {   // gap: "first" at 1 and "second" at 3, a stop word between
		const auto gap = d.field("gap");
		d.add_posting(gap, "first", 1);
		d.add_posting(gap, "second", 3);
	});
	engine::sync_wait(shard.commit(engine::Transaction::none));
	const auto ids = [](const Index& index, const Query& query) {
		std::set<engine::docid> found;
		for (const auto& hit : run(index, 1, query, 0, 100).hits) { found.insert(hit.did); }
		return found;
	};
	using Set = std::set<engine::docid>;
	const auto t = [](const char* f, const char* w) { return Query::term(f, w); };
	const std::pair<Query, Set> cases[] = {
		{t("title", "same"), {1, 3}},
		{t("body", "same"), {2, 3}},
		{t("", "same"), {}},
		{t("tags", ""), {1}},
		{t("title", ""), {}},
		{Query::match_all(), {1, 2, 3, 4}},
		{Query::combine(Op::AND, t("title", "same"), t("body", "same")), {3}},
		{Query::combine(Op::PHRASE, {t("title", "first"), t("title", "second")}), {1}},
		{Query::combine(Op::PHRASE, {t("body", "second"), t("body", "first")}), {2}},
		{Query::combine(Op::PHRASE, {t("title", "first"), t("body", "second")}), {}},
		// What the query DSL builds for a text under _phrase or _near: an append onto nothing.
		{Query::append(Op::PHRASE, Query(), Query::combine(Op::PHRASE, {t("title", "first"), t("title", "second")})), {1}},
		{Query::append(Op::PHRASE, Query(), Query::combine(Op::PHRASE, {t("body", "second"), t("body", "first")})), {2}},
		{Query::append(Op::NEAR, Query(), Query::combine(Op::NEAR, {t("title", "second"), t("title", "first")})), {1}},
		{Query::append(Op::PHRASE, Query(), t("title", "first")), {1, 3}},
		// The key field: a term on it is a key, and a key is never a term of another field.
		{t(std::string(engine::KEY_FIELD).c_str(), "Q1"), {1}},
		{t(std::string(engine::KEY_FIELD).c_str(), "Q3"), {3}},
		{t(std::string(engine::KEY_FIELD).c_str(), "Q5"), {}},
		{t("title", "Q1"), {}},
		{t("", "Q1"), {}},
		{Query::wildcard(std::string(engine::KEY_FIELD), "Q"), {1, 2, 3, 4}},
		{Query::wildcard(std::string(engine::KEY_FIELD), "Q2"), {2}},
		{Query::field(std::string(engine::KEY_FIELD)), {1, 2, 3, 4}},
		// A wildcard reads its own field only; Query::field takes every field that starts with the name.
		{Query::wildcard("title", "sec"), {1}},
		{Query::wildcard("body", "sec"), {2, 3}},
		{Query::wildcard("tit", "sec"), {}},
		{Query::field("title"), {1, 3}},
		{Query::field("ti"), {1, 3}},
		{Query::field("body"), {2, 3}},
		{Query::field("nothing"), {}},
	};
	for (const auto& [query, expected] : cases) {
		const int before = testh::failures;
		CHECK_TRUE(ids(strata, query) == expected);
		if (testh::failures != before) { std::fprintf(stderr, "  %s\n", query.description().c_str()); }
	}
	// A PHRASE's terms match at exactly their relative query positions.
	const auto at = [](const char* w, engine::termpos pos) { return Query::term("gap", w, 1, pos); };
	const auto phrase = [&](engine::termpos a, engine::termpos b) { return Query::combine(Op::PHRASE, {at("first", a), at("second", b)}); };
	CHECK_TRUE(ids(strata, phrase(1, 3)) == Set{4});
	CHECK_TRUE(ids(strata, phrase(5, 7)) == Set{4});
	CHECK_TRUE(ids(strata, phrase(1, 2)).empty());
	CHECK_TRUE(ids(strata, phrase(1, 4)).empty());
	CHECK_TRUE(ids(strata, Query::combine(Op::PHRASE, {at("first", 0), at("second", 0)})).empty());
}

}  // namespace detail
