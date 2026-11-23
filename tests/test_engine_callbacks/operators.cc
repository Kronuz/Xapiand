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

// Terms without frequencies, single documents, appends, operators and the expansions of wildcards.

#include "support.h"

namespace detail {

// Terms that carry no frequency (a schema's keyword and number fields) are searched like any other.
void test_boolean_terms() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("boolean", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const std::vector<std::pair<const char*, Query>> queries = {
			{"boolean", t("Bg1")},
			{"boolean absent", t("Bg9")},
			{"and", Query::combine(Op::AND, t("Bg1"), t("common"))},
			{"or", Query::combine(Op::OR, t("Bg1"), t("Bg2"))},
			{"filter", Query::combine(Op::FILTER, t("apple"), t("Bg0"))},
			{"and_not", Query::combine(Op::AND_NOT, t("common"), t("Bg3"))},
			{"id", Query::term(std::string(engine::KEY_FIELD), "Q0_5")},
			{"wildcard", Query::wildcard("f", "Bg", 0, engine::Expansion::error, 0, Op::OR)},
		};
		for (const auto& [name, query] : queries) {
			const auto s = both(corpus, n, query, name);
			if (std::string(name) == "boolean") {
				CHECK_GT(s.hits.size(), 0u);
			}
		}
		CHECK_EQ(run(corpus.strata, n, Query::term(std::string(engine::KEY_FIELD), "Q0_5")).hits.size(), 1u);
		CHECK_EQ(run(corpus.strata, n, t("Q0_5")).hits.size(), 0u);  // the key is not a term of the field
	}
}

// Shards of one document each, with no commit between the writes and the search: every term has one posting.
void test_single_documents() {
	Corpus corpus;
	build("single", 5, 1, 1, corpus);
	for (const std::size_t n : {std::size_t{1}, std::size_t{5}}) {
		for (const auto& [name, query] : std::vector<std::pair<const char*, Query>>{{"common", t("common")}, {"boolean", t("Bg1")}, {"or", Query::combine(Op::OR, t("common"), t("Bg1"))}, {"all", Query::match_all()}}) {
			const auto s = both(corpus, n, query, name);
			if (std::string(name) == "common") {
				CHECK_EQ(s.hits.size(), n);
			}
		}
	}
}

// An append onto a query that is empty by its structure (a text that analyses to nothing) is the next query.
void test_appends() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("appends", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const Query nothing = Query::combine(Op::AND, t("apple"), Query());
		CHECK_TRUE(nothing.empty());
		const std::vector<std::pair<const char*, Query>> queries = {
			{"append to nothing", Query::append(Op::AND, nothing, t("Bg1"))},
			{"twice", Query::append(Op::AND, Query::append(Op::AND, nothing, t("Bg1")), t("common"))},
			{"or", Query::append(Op::OR, Query::append(Op::AND, nothing, t("band")), t("apple"))},
			{"after a missing term", Query::append(Op::OR, t("absent"), t("apple"))},
			{"and with a missing term", Query::append(Op::AND, t("absent"), t("apple"))},
			{"nothing on nothing", Query::append(Op::AND, nothing, nothing)},
		};
		for (const auto& [name, query] : queries) {
			both(corpus, n, query, name);
		}
		CHECK_GT(run(corpus.strata, n, queries[0].second).hits.size(), 0u);
	}
}

void test_operators() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("operators", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const auto syn = [](std::vector<Query> terms) { return Query::combine(Op::SYNONYM, std::move(terms)); };
		const std::vector<std::pair<const char*, Query>> queries = {
			{"synonym", syn({t("apple"), t("apricot")})},
			{"synonym three", syn({t("band"), t("bandana"), t("bond")})},
			{"synonym one", syn({t("band")})},
			{"synonym absent", syn({t("band"), t("absent")})},
			{"synonym all absent", syn({t("absent"), t("nothing")})},
			{"synonym repeated", syn({t("band"), t("band")})},
			{"synonym and", Query::combine(Op::AND, syn({t("apple"), t("cat")}), t("common"))},
			{"synonym or", Query::combine(Op::OR, syn({t("apple"), t("cat")}), t("even"))},
			{"xor", Query::combine(Op::XOR, {t("apple"), t("band"), t("cat")})},
			{"xor two", Query::combine(Op::XOR, t("apple"), t("band"))},
			{"xor nested", Query::combine(Op::XOR, Query::combine(Op::XOR, t("apple"), t("band")), t("cat"))},
			{"xor of and", Query::combine(Op::XOR, Query::combine(Op::AND, t("apple"), t("even")), t("band"))},
			{"xor absent", Query::combine(Op::XOR, t("apple"), t("absent"))},
			{"xor appended", Query::append(Op::XOR, t("apple"), t("band"))},
			{"max", Query::combine(Op::MAX, {t("apple"), t("band"), t("cat")})},
			{"max two", Query::combine(Op::MAX, t("apple"), t("band"))},
			{"max wqf", Query::combine(Op::MAX, t("apple", 3), t("band"))},
			{"max of or", Query::combine(Op::MAX, Query::combine(Op::OR, t("apple"), t("cat")), t("even"))},
			{"xor of or", Query::combine(Op::XOR, Query::combine(Op::OR, t("apple"), t("cat")), t("even"))},
			{"max of nested or", Query::combine(Op::MAX, Query::combine(Op::OR, Query::combine(Op::OR, t("apple"), t("cat")), t("coat")), t("even"))},
			{"max of or of and", Query::combine(Op::MAX, Query::combine(Op::OR, Query::combine(Op::AND, t("apple"), t("common")), t("cat")), t("even"))},
			{"max of max", Query::combine(Op::MAX, Query::combine(Op::MAX, t("apple"), t("cat")), t("even"))},
			{"and of or in max", Query::combine(Op::AND, Query::combine(Op::MAX, Query::combine(Op::OR, t("apple"), t("cat")), t("even")), t("common"))},
			{"max of and", Query::combine(Op::MAX, Query::combine(Op::AND, t("common"), t("even")), t("band"))},
			{"max of scale", Query::combine(Op::MAX, Query::scale(t("apple"), 2.0), t("band"))},
			{"max of synonym", Query::combine(Op::MAX, syn({t("apple"), t("cat")}), t("band"))},
			{"max appended", Query::append(Op::MAX, t("apple"), t("band"))},
			{"scale", Query::scale(Query::combine(Op::OR, t("apple"), t("band")), 2.5)},
			{"scale zero", Query::scale(t("apple"), 0.0)},
			{"scale synonym", Query::scale(syn({t("apple"), t("cat")}), 3.0)},
			{"and_maybe", Query::combine(Op::AND_MAYBE, t("common"), syn({t("apple"), t("cat")}))},
			{"filter", Query::combine(Op::FILTER, syn({t("apple"), t("cat")}), t("even"))},
		};
		for (const auto& [name, query] : queries) {
			both(corpus, n, query, name);
			both(corpus, n, query, name, [](engine::SearchRequest& r) { r.first = 2; r.maxitems = 6; });
		}
	}
}

// What a wildcard or edit-distance node stands for is the terms the model says, whatever the pattern.
void test_expansions() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("expansions", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const auto wild = [](const char* pattern, engine::termcount max = 0, engine::Expansion limit = engine::Expansion::error, unsigned flags = 0, Op combiner = Op::SYNONYM) {
			return Query::wildcard("f", pattern, max, limit, flags, combiner);
		};
		const auto fuzzy = [](const char* pattern, unsigned distance, std::size_t prefix = 0, engine::termcount max = 0, engine::Expansion limit = engine::Expansion::error, Op combiner = Op::SYNONYM) {
			return Query::edit_distance("f", pattern, max, limit, combiner, distance, prefix);
		};
		const unsigned S = engine::PATTERN_SINGLE, M = engine::PATTERN_MULTI;
		std::vector<std::pair<std::string, Query>> queries;
		for (const Op combiner : {Op::SYNONYM, Op::OR, Op::MAX}) {
			const std::string c = std::string(" ") + engine::to_string(combiner);
			queries.emplace_back("prefix ban" + c, wild("ban", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("prefix ca" + c, wild("ca", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("prefix caf" + c, wild("caf", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("single b?nd" + c, wild("b?nd", 0, engine::Expansion::error, S, combiner));
			queries.emplace_back("multi *an*" + c, wild("*an*", 0, engine::Expansion::error, M, combiner));
			queries.emplace_back("multi ca*t" + c, wild("ca*t", 0, engine::Expansion::error, M, combiner));
			queries.emplace_back("both ?a*" + c, wild("?a*", 0, engine::Expansion::error, S | M, combiner));
			queries.emplace_back("fuzzy bnad 1" + c, fuzzy("bnad", 1, 0, 0, engine::Expansion::error, combiner));
			queries.emplace_back("fuzzy aple 2" + c, fuzzy("aple", 2, 0, 0, engine::Expansion::error, combiner));
			queries.emplace_back("fuzzy cat 1 prefix 1" + c, fuzzy("cat", 1, 1, 0, engine::Expansion::error, combiner));
		}
		queries.emplace_back("none", wild("zzz"));
		queries.emplace_back("single unicode caf?", wild("caf?", 0, engine::Expansion::error, S));
		queries.emplace_back("single unicode na?ve", wild("na?ve", 0, engine::Expansion::error, S));
		queries.emplace_back("fuzzy cafe 1", fuzzy("cafe", 1));
		queries.emplace_back("fuzzy naive 1", fuzzy("naive", 1));
		queries.emplace_back("fuzzy band 3", fuzzy("band", 3));
		queries.emplace_back("limit first", wild("ban", 2, engine::Expansion::first));
		queries.emplace_back("limit most frequent", wild("mf_", 2, engine::Expansion::most_frequent));
		queries.emplace_back("limit most frequent, 3", wild("mf_", 3, engine::Expansion::most_frequent, 0, Op::OR));
		queries.emplace_back("limit first, mf", wild("mf_", 2, engine::Expansion::first));
		queries.emplace_back("limit unmet", wild("ban", 10, engine::Expansion::error));
		queries.emplace_back("fuzzy limit first", fuzzy("bnad", 2, 0, 2, engine::Expansion::first));
		queries.emplace_back("wildcard and", Query::combine(Op::AND, wild("ban"), t("common")));
		queries.emplace_back("wildcard or fuzzy", Query::combine(Op::OR, wild("ap"), fuzzy("cart", 1)));
		std::size_t found = 0;
		for (const auto& [name, query] : queries) {
			same_documents(corpus, n, query, name.c_str());
			found += !expected_of(corpus, n, query).datas.empty();
		}
		CHECK_GT(found, queries.size() / 2);
		// The limits pick the terms the corpus gives them: the first two prefixed "mf_" by byte order, the two with the most documents.
		CHECK_EQ(expected_of(corpus, n, wild("mf_", 2, engine::Expansion::first)).datas.size(), 5u);   // mf_a (docs 1-5) and mf_b (1-3)
		CHECK_EQ(expected_of(corpus, n, wild("mf_", 2, engine::Expansion::most_frequent)).datas.size(), 8u);   // mf_c (docs 1-8) and mf_a (1-5)
		// Past the limit with `error` fails the search.
		CHECK_TRUE(expected_of(corpus, n, wild("ban", 2)).error);
		CHECK_TRUE(error_kind([&] { run(corpus.strata, n, wild("ban", 2)); }).has_value());
		CHECK_TRUE(expected_of(corpus, n, fuzzy("bnad", 2, 0, 1)).error);
		CHECK_TRUE(error_kind([&] { run(corpus.strata, n, fuzzy("bnad", 2, 0, 1)); }).has_value());
	}
}

}  // namespace detail
