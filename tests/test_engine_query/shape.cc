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

// The shape a query keeps and what it matches: leaves, operators, emptiness, expansions, texts and patterns.

#include "support.h"

namespace detail {

constexpr Op binary_ops[] = { Op::AND, Op::OR, Op::AND_NOT, Op::XOR, Op::AND_MAYBE, Op::FILTER };

void test_leaves_and_ops() {
	CHECK_TRUE(Query().empty());
	CHECK_FALSE(Query::match_all().empty());
	CHECK_FALSE(Query::term("", "a").empty());
	const auto term = Query::term("", "a", 2, 3);
	CHECK_TRUE(term.node() != nullptr && term.node()->kind == engine::QueryKind::term);
	CHECK_EQ(term.node()->str, "a");
	CHECK_EQ(term.node()->wqf, 2u);
	CHECK_EQ(term.node()->pos, 3u);

	// The binary and list forms of an operator keep the operands in order.
	auto a = Query::term("", "a"), b = Query::term("", "b"), c = Query::term("", "c");
	const auto both = Query::combine(Op::AND, a, b);
	CHECK_TRUE(both.node()->kind == engine::QueryKind::combine && both.node()->op == Op::AND);
	CHECK_EQ(both.node()->subqueries.size(), 2u);
	const auto list = Query::combine(Op::PHRASE, { a, b, c }, 5);
	CHECK_TRUE(list.node()->op == Op::PHRASE);
	CHECK_EQ(list.node()->subqueries.size(), 3u);
	CHECK_EQ(list.node()->parameter, 5u);
	CHECK_EQ(list.node()->subqueries[2].node()->str, "c");
	const auto scaled = Query::scale(a, 2.5);
	CHECK_TRUE(scaled.node()->kind == engine::QueryKind::scale);
	CHECK_TRUE(scaled.node()->factor == 2.5);
}

// Whether a query matches nothing by its structure, operator by operator, for operands that do and do not.
void test_emptiness() {
	const auto term = Query::term("", "t");
	const auto none = Query();
	// Binary operators: matches nothing when ...
	struct Row { Op op; bool left_empty_is_nothing, right_empty_is_nothing, both_empty_is_nothing; };
	const Row rows[] = {
		{ Op::AND, true, true, true },
		{ Op::OR, false, false, true },
		{ Op::AND_NOT, true, false, true },
		{ Op::XOR, false, false, true },
		{ Op::AND_MAYBE, true, false, true },
		{ Op::FILTER, true, true, true },
	};
	for (const auto& row : rows) {
		CHECK_FALSE(Query::combine(row.op, term, term).empty());
		CHECK_EQ(Query::combine(row.op, none, term).empty(), row.left_empty_is_nothing);
		CHECK_EQ(Query::combine(row.op, term, none).empty(), row.right_empty_is_nothing);
		CHECK_EQ(Query::combine(row.op, none, none).empty(), row.both_empty_is_nothing);
		// The list form agrees.
		CHECK_EQ(Query::combine(row.op, std::vector<Query>{ none, term }).empty(), row.left_empty_is_nothing);
		CHECK_EQ(Query::combine(row.op, std::vector<Query>{ term, none }).empty(), row.right_empty_is_nothing);
	}
	// A list operator over nothing, and a scale or a synonym of it.
	CHECK_TRUE(Query::combine(Op::OR, std::vector<Query>{}).empty());
	CHECK_TRUE(Query::combine(Op::MAX, std::vector<Query>{ none, none }).empty());
	CHECK_FALSE(Query::combine(Op::MAX, std::vector<Query>{ none, term }).empty());
	CHECK_TRUE(Query::scale(none, 2.0).empty());
	CHECK_FALSE(Query::scale(term, 2.0).empty());
	CHECK_FALSE(Query::combine(Op::SYNONYM, std::vector<Query>{ term, Query::term("", "u") }).empty());
	// A phrase or a near of words is a query; with a missing operand it matches nothing.
	CHECK_FALSE(Query::combine(Op::PHRASE, std::vector<Query>{ term, Query::term("", "u") }, 2).empty());
	CHECK_TRUE(Query::combine(Op::NEAR, std::vector<Query>{ term, none }, 2).empty());
	// An append of nothing onto nothing is nothing; otherwise the append has something to find.
	CHECK_TRUE(Query::append(Op::AND, none, none).empty());
	CHECK_FALSE(Query::append(Op::AND, none, term).empty());
	CHECK_FALSE(Query::append(Op::AND, term, term).empty());
	// Nesting keeps the rule: an AND with a branch that matches nothing is nothing, an OR is not.
	const auto dead = Query::combine(Op::AND, term, none);
	CHECK_TRUE(dead.empty());
	CHECK_TRUE(Query::combine(Op::AND, dead, term).empty());
	CHECK_FALSE(Query::combine(Op::OR, dead, term).empty());
	CHECK_TRUE(Query::combine(Op::OR, dead, dead).empty());
	CHECK_TRUE(Query::combine(Op::AND_NOT, dead, term).empty());
	CHECK_FALSE(Query::combine(Op::AND_NOT, term, dead).empty());
}

void test_expansions() {
	const auto plain = Query::wildcard("", "ab");
	CHECK_TRUE(plain.node()->kind == engine::QueryKind::wildcard);
	CHECK_EQ(plain.node()->str, "ab");
	CHECK_EQ(plain.node()->parameter, 0u);
	CHECK_TRUE(plain.node()->limit == engine::Expansion::error);
	CHECK_TRUE(plain.node()->op == Op::SYNONYM);
	CHECK_EQ(plain.node()->pattern_flags, 0u);

	const auto limited = Query::wildcard("", "ab", 50, engine::Expansion::most_frequent, 0, Op::SYNONYM);
	CHECK_EQ(limited.node()->parameter, 50u);
	CHECK_TRUE(limited.node()->limit == engine::Expansion::most_frequent);

	const auto glob = Query::wildcard("", "a?c*e", 7, engine::Expansion::first, engine::PATTERN_SINGLE | engine::PATTERN_MULTI, Op::OR);
	CHECK_EQ(glob.node()->str, "a?c*e");
	CHECK_EQ(glob.node()->parameter, 7u);
	CHECK_TRUE(glob.node()->limit == engine::Expansion::first);
	CHECK_EQ(glob.node()->pattern_flags, engine::PATTERN_SINGLE | engine::PATTERN_MULTI);
	CHECK_TRUE(glob.node()->op == Op::OR);

	const auto fuzzy = Query::edit_distance("", "Xabc", 0, engine::Expansion::error, Op::SYNONYM, 2, 3);
	CHECK_TRUE(fuzzy.node()->kind == engine::QueryKind::edit_distance);
	CHECK_EQ(fuzzy.node()->str, "Xabc");
	CHECK_EQ(fuzzy.node()->distance, 2u);
	CHECK_EQ(fuzzy.node()->prefix_length, 3u);
}

// What a wildcard does past its limit is the one thing a description does not show: run it.

// What a wildcard does past its limit is the one thing a description does not show: run it.
void test_expansion_limits() {
	const std::vector<std::vector<int>> docs_of_term = { {}, { 1, 2, 3, 4, 5 }, { 6, 7 }, { 8 }, { 9, 10, 11, 12 } };
	auto shard = testsupport::scratch_shard("engine_query_limits");
	for (int did = 1; did <= 12; ++did) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "zz");
		for (std::size_t t = 1; t < docs_of_term.size(); ++t) {
			for (int member : docs_of_term[t]) {
				if (member == did) {
					doc.add_term(doc.field("f"), "ab" + std::to_string(t));
				}
			}
		}
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(did));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	auto matched = [&](engine::Expansion limit) {
		return run(*shard, Query::wildcard("f", "ab", 2, limit));
	};
	CHECK_TRUE((matched(engine::Expansion::first) == std::set<engine::docid>{ 1, 2, 3, 4, 5, 6, 7 }));
	CHECK_TRUE((matched(engine::Expansion::most_frequent) == std::set<engine::docid>{ 1, 2, 3, 4, 5, 9, 10, 11, 12 }));
	CHECK_THROW(matched(engine::Expansion::error), engine::Error);
}

// A text without words reads as a query that matches nothing; append skips it.
void test_append() {
	engine::TextConfig config;
	config.field = "f";
	config.flags = engine::text::PHRASE;
	auto stops = engine::parse_text("", config);
	CHECK_TRUE(stops.empty());
	auto shard = terms_db("engine_query_append", { "w", "w", "x" });
	const auto word = Query::term("f", "w");
	CHECK_TRUE((run(*shard, Query::append(Op::AND, Query(), word)) == std::set<engine::docid>{ 1, 2 }));
	CHECK_TRUE((run(*shard, Query::append(Op::AND, stops, word)) == std::set<engine::docid>{ 1, 2 }));
	CHECK_TRUE(run(*shard, Query::append(Op::AND, word, Query())).empty());
	CHECK_TRUE((run(*shard, Query::append(Op::OR, word, Query::term("f", "x"))) == std::set<engine::docid>{ 1, 2, 3 }));
}

// A text query finds the documents its words name, with the operator and the stems the config says.
void test_text() {
	auto shard = terms_db("engine_query_text", { "hello", "world", "hello", "run", "ran" });
	engine::TextConfig config;
	config.field = "f";
	config.flags = engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::BOOLEAN;
	config.partial_max_expansion = 50;
	config.fuzzy_max_expansion = 0;

	config.default_op = Op::OR;
	CHECK_TRUE((run(*shard, engine::parse_text("hello world", config)) == std::set<engine::docid>{ 1, 2, 3 }));
	config.default_op = Op::AND;
	CHECK_TRUE(run(*shard, engine::parse_text("hello world", config)).empty());
	config.default_op = Op::OR;
	CHECK_TRUE((run(*shard, engine::parse_text("hello -world", config)) == std::set<engine::docid>{ 1, 3 }));
	CHECK_TRUE((run(*shard, engine::parse_text("+hello world", config)) == std::set<engine::docid>{ 1, 3 }));
	CHECK_TRUE((run(*shard, engine::parse_text("hello AND NOT world", config)) == std::set<engine::docid>{ 1, 3 }));

	// A wildcard and a fuzzy word expand over the terms of the shard.
	config.flags |= engine::text::WILDCARD;
	CHECK_TRUE((run(*shard, engine::parse_text("hel*", config)) == std::set<engine::docid>{ 1, 3 }));
	CHECK_TRUE((run(*shard, engine::parse_text("helo~", config)) == std::set<engine::docid>{ 1, 3 }));

	// Text the parser rejects raises QueryParserError, an engine syntax error.
	engine::TextConfig bad;
	bad.field = "XT";
	bad.flags = engine::text::BOOLEAN | engine::text::PHRASE;
	bool raised = false;
	try {
		engine::parse_text("AND AND", bad);
	} catch (const QueryParserError& exc) {
		raised = exc.kind == engine::errc::syntax;
	}
	CHECK_TRUE(raised);
}

// A pattern's '?' and '*' count only when the pattern flags say so.
void test_wildcard_patterns() {
	auto db = terms_db("engine_query_patterns", { "alice", "alyce", "alce", "alicce", "bob" });
	auto matched = [&](const std::string& pattern, unsigned flags) {
		return run(*db, Query::wildcard("f", pattern, 0, engine::Expansion::error, flags));
	};
	CHECK_TRUE((matched("al?ce", engine::PATTERN_SINGLE) == std::set<engine::docid>{ 1, 2 }));
	CHECK_TRUE(matched("al?ce", 0).empty());
	CHECK_TRUE((matched("al*e", engine::PATTERN_MULTI) == std::set<engine::docid>{ 1, 2, 3, 4 }));
	CHECK_TRUE(matched("al*e", 0).empty());
	CHECK_TRUE(matched("al*e", engine::PATTERN_SINGLE).empty());
	CHECK_TRUE((matched("a?*e", engine::PATTERN_SINGLE | engine::PATTERN_MULTI) == std::set<engine::docid>{ 1, 2, 3, 4 }));
}

// A fuzzy word that expands past its limit fails the search, where a partial word keeps the most
// frequent terms.
void test_fuzzy_limit() {
	auto db = terms_db("engine_query_fuzzy", { "alica", "alicb", "alicc", "alicd", "alice" });
	engine::TextConfig config;
	config.field = "f";
	config.flags = engine::text::FUZZY;
	config.fuzzy_max_expansion = 2;
	CHECK_THROW(run(*db, engine::parse_text("alice~", config)), engine::Error);

	config.fuzzy_max_expansion = 0;
	CHECK_EQ(run(*db, engine::parse_text("alice~", config)).size(), 5U);
}

// A binary combine and the same operands as a list match the same documents with the same weights.
void test_binary_equals_list() {
	auto db = testsupport::scratch_shard("engine_query_binary");
	for (int i = 0; i < 12; ++i) {
		engine::Document doc;
		if (i % 2 == 0) { doc.add_term(doc.field("f"), "a", 1); }
		if (i % 3 == 0) { doc.add_term(doc.field("f"), "b", 1 + i % 4); }
		if (i % 4 == 0) { doc.add_term(doc.field("f"), "c", 1); }
		doc.add_term(doc.field("f"), "z");
		testsupport::add_keyed(*db, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(db->commit(engine::Transaction::none));
	const std::vector<Query> operands = { Query::term("f", "a"), Query::term("f", "b"), Query::term("f", "nothing"), Query(), Query::match_all(), Query::term("f", "c", 2) };
	auto results = [&](const Query& query) {
		std::vector<std::pair<engine::docid, double>> hits;
		for (const auto& hit : testsupport::search({ db.get() }, query).hits) {
			hits.emplace_back(hit.did, hit.weight);
		}
		return hits;
	};
	std::size_t found = 0;
	for (auto op : binary_ops) {
		for (const auto& left : operands) {
			for (const auto& right : operands) {
				const auto binary = results(Query::combine(op, left, right));
				CHECK_TRUE(binary == results(Query::combine(op, std::vector<Query>{ left, right })));
				found += binary.size();
			}
		}
	}
	CHECK_GT(found, 100u);
}

}  // namespace detail
