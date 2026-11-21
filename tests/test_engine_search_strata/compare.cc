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

// Running a query through the engine and checking it against the model, and random queries.

#include "support.h"

namespace detail {

// --- running a query ---

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, engine::doccount first, engine::doccount maxitems, engine::DocidOrder order) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.first = first;
	request.maxitems = maxitems;
	request.check_at_least = 100000;
	request.order = order;
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

namespace {

Ids ids_of(const engine::SearchResult& result) {
	Ids ids;
	for (const auto& hit : result.hits) {
		ids.insert(hit.did);
	}
	return ids;
}

}  // namespace

// One query against the model: the documents, the estimate, the weights, the order and the ranks.
void compare(const Index& strata, const Model& model, std::size_t shards, const Query& query) {
	const auto weights = model_hits(model, query, shards);
	Ids expected;
	for (const auto& [id, weight] : weights) { expected.insert(id); }
	const auto s = run(strata, shards, query, 0, 100000);
	CHECK_TRUE(ids_of(s) == expected);
	CHECK_EQ(s.estimated, expected.size());
	CHECK_EQ(s.hits.size(), expected.size());
	for (const auto& hit : s.hits) {
		const auto it = weights.find(hit.did);
		if (it == weights.end() || !close(hit.weight, it->second)) {
			CHECK_TRUE(false);
			break;
		}
	}
	for (std::size_t i = 1; i < s.hits.size(); ++i) {
		CHECK_GE(s.hits[i - 1].weight + 1e-12, s.hits[i].weight);
	}
	for (std::size_t i = 0; i < s.hits.size(); ++i) {
		CHECK_EQ(s.hits[i].rank, i);
	}
	// Windows: the same documents as the full ranking.
	for (const auto& [first, count] : {std::pair{0u, 3u}, {2u, 5u}, {7u, 4u}, {0u, 0u}, {40u, 10u}}) {
		const auto window = run(strata, shards, query, first, count);
		CHECK_EQ(window.estimated, expected.size());
		const std::size_t begin = std::min<std::size_t>(first, s.hits.size());
		const std::size_t end = std::min<std::size_t>(begin + count, s.hits.size());
		REQUIRE_EQ(window.hits.size(), end - begin);
		for (std::size_t i = begin; i < end; ++i) {
			CHECK_EQ(window.hits[i - begin].did, s.hits[i].did);
			CHECK_EQ(window.hits[i - begin].rank, i);
		}
	}
}

// --- random queries ---

namespace {

// The terms a query may use. With `distinct`, no term is used twice in a query and no boolean term is
// used; otherwise a query may repeat terms and use the boolean ones.
struct Terms {
	std::mt19937& rng;
	bool distinct;
	std::set<std::string> used;
	int missing = 0;

	std::string pick() {
		static const char* const pool[] = {"w0", "w1", "w2", "w3", "w4", "w5", "common", "even", "tag0", "tag1", "tag2", "tag3", "tag4", "bool0", "bool1", "bool2", "bool3", "bool4"};
		for (int attempt = 0; attempt < 40; ++attempt) {
			std::string term = pool[rng() % (distinct ? 13 : 18)];   // the first 13 have no boolean terms
			if (rng() % 12 == 0) {
				term = "zzz" + std::to_string(missing++);
			}
			if (!distinct || used.insert(term).second) {
				return term;
			}
		}
		return "zzz" + std::to_string(missing++);
	}
	std::string word() {
		for (int attempt = 0; attempt < 40; ++attempt) {
			std::string term = "w" + std::to_string(rng() % vocabulary);
			if (!distinct || used.insert(term).second) {
				return term;
			}
		}
		return "zzz" + std::to_string(missing++);
	}
};

Query random_query(Terms& terms, int depth) {
	auto& rng = terms.rng;
	if (depth == 0 || rng() % 4 == 0) {
		return Query::term("f", terms.pick(), 1 + static_cast<engine::termcount>(rng() % 3));
	}
	switch (rng() % 9) {
		case 0:
		case 1: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 2; n > 0; --n) { subs.push_back(random_query(terms, depth - 1)); }
			return Query::combine(Op::AND, std::move(subs));
		}
		case 2:
		case 3: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 3; n > 0; --n) { subs.push_back(random_query(terms, depth - 1)); }
			return Query::combine(Op::OR, std::move(subs));
		}
		case 4: return Query::combine(Op::AND_NOT, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 5: return Query::combine(Op::AND_MAYBE, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 6: return Query::combine(Op::FILTER, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 7: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 2; n > 0; --n) { subs.push_back(Query::term("f", terms.word())); }
			const auto n = static_cast<engine::termcount>(subs.size());
			if (rng() % 2) {
				return Query::combine(Op::PHRASE, std::move(subs), rng() % 2 ? 0 : n);
			}
			return Query::combine(Op::NEAR, std::move(subs), rng() % 2 ? 0 : n + static_cast<engine::termcount>(rng() % 3));
		}
		default: return Query::append(Op::OR, random_query(terms, depth - 1), random_query(terms, depth - 1));
	}
}

}  // namespace

void random_queries(const Index& strata, const Model& model, std::size_t shards, unsigned seed, int count, bool distinct) {
	std::mt19937 rng(seed);
	for (int i = 0; i < count; ++i) {
		Terms terms{rng, distinct, {}, 0};
		const auto query = random_query(terms, 3);
		const int before = testh::failures;
		compare(strata, model, shards, query);
		if (testh::failures != before) {
			std::fprintf(stderr, "  query %d: %s\n", i, query.description().c_str());
		}
	}
}

}  // namespace detail
