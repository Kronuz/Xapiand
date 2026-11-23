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

// Reads over a changing index: long chains, concurrent searches and snapshot consistency.

#include "support.h"

namespace detail {

namespace {

// A query's whole answer against the model: the documents, their weights and their order (heavier first,
// ties by id in the requested order), and the estimate.
void check_against_model(const Index& index, const Model& model, std::size_t shards, const Query& query, engine::DocidOrder order = engine::DocidOrder::ascending) {
	const auto expected = model_hits(model, query, shards);
	const auto found = run(index, shards, query, 0, 100000, order);
	CHECK_EQ(found.estimated, expected.size());
	REQUIRE_EQ(found.hits.size(), expected.size());
	for (std::size_t i = 0; i < found.hits.size(); ++i) {
		const auto& hit = found.hits[i];
		const auto it = expected.find(hit.did);
		REQUIRE(it != expected.end());
		CHECK_TRUE(close(hit.weight, it->second));
		if (i > 0) {
			const auto& before = found.hits[i - 1];
			CHECK_TRUE(before.weight >= hit.weight);
			if (before.weight == hit.weight) {
				CHECK_TRUE(order == engine::DocidOrder::descending ? before.did > hit.did : before.did < hit.did);
			}
		}
	}
}

}  // namespace

// Long chains of appends are built, run and freed without recursion that grows with the chain: the
// scorers of a run of one operator are one flat scorer, as the query tree frees iteratively.
void test_long_chains_impl() {
	Index strata;
	Model model;
	build("chains", 2, 40, 2, strata, model);
	const auto t = [](const char* w) { return Query::term("f", w); };
	const int kChain = static_cast<int>(engine::MAX_QUERY_CLAUSES) - 100;   // the longest chain a query may hold
	Query ors = t("w1");
	for (int i = 0; i < kChain; ++i) {
		ors = Query::append(Op::OR, ors, i % 2 ? t("w1") : t("zzz"));
	}
	check_against_model(strata, model, 2, ors);
	Query ands = t("common");
	for (int i = 0; i < kChain; ++i) {
		ands = Query::append(Op::AND, ands, t("common"));
	}
	check_against_model(strata, model, 2, ands);
	Query mixed = t("w1");
	for (int i = 0; i < kChain; ++i) {
		mixed = Query::append(i % 3 == 0 ? Op::AND : Op::OR, mixed, i % 2 ? t("common") : t("w1"));
	}
	check_against_model(strata, model, 2, mixed);   // ends on OR common
	// A chain of exclusions nests one scorer per link; thousands of links run.
	Query excluded = t("common");
	for (int i = 0; i < kChain; ++i) {
		excluded = Query::append(Op::AND_NOT, excluded, t("zzz"));
	}
	check_against_model(strata, model, 2, excluded);
	// A wide OR and a wide AND in one node.
	std::vector<Query> wide;
	for (int i = 0; i < kChain; ++i) { wide.push_back(t("w1")); }
	check_against_model(strata, model, 2, Query::combine(Op::OR, wide));
	check_against_model(strata, model, 2, Query::combine(Op::AND, wide));
}

// Searches that name the same shards in opposite orders, with writers, neither deadlock nor lose a document.
void test_concurrent_searches_impl() {
	Index strata;
	Model model;
	build("threads", 2, 40, 2, strata, model, true);
	std::atomic<bool> stop{false};
	std::atomic<int> failures{0};
	const auto searcher = [&](bool reversed) {
		std::vector<engine::Shard*> shards{strata.owned[0].get(), strata.owned[1].get()};
		if (reversed) { std::swap(shards[0], shards[1]); }
		auto prepared = engine::prepare(Query::term("f", "common"));
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 5;
		for (int i = 0; i < 300; ++i) {
			const auto result = engine::sync_wait(engine::search(shards, request));
			if (result.estimated < 80 || result.hits.size() != 5) { ++failures; }
		}
	};
	std::thread writer([&] {
		std::mt19937 rng(5);
		for (int i = 0; i < 100 && !stop; ++i) {
			auto doc = random_doc(rng, 1000 + i);
			auto written = make(1, 1000 + i, doc);
			engine::sync_wait(strata.owned[i % 2]->add(written, {}));
			engine::sync_wait(strata.owned[i % 2]->refresh());
		}
	});
	std::thread a(searcher, false), b(searcher, true), c(searcher, false);
	a.join();
	b.join();
	c.join();
	stop = true;
	writer.join();
	CHECK_EQ(failures.load(), 0);
	// The same shard named twice is searched once and counted twice.
	std::vector<engine::Shard*> twice{strata.owned[0].get(), strata.owned[0].get()};
	auto prepared = engine::prepare(Query::term("f", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1000;
	const auto result = engine::sync_wait(engine::search(twice, request));
	const auto once = engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[0].get()}, request));
	CHECK_EQ(result.estimated, 2 * once.estimated);
}

namespace {

// The BM25 weight of "common" in an index of n identical documents: the weight every hit has when the
// searched snapshot holds exactly n documents.
double weight_in(std::size_t n, const Model::Doc& doc) {
	Model sized;
	sized.shards.assign(3, {});
	sized.shards[0].assign(n, doc);
	return weigh(statistics_of(sized, 3), doc, Query::term("f", "common")).second;
}

}  // namespace

// A search that runs while another thread writes and commits sees, in each shard, one snapshot of it: the
// documents 1..count, whole. The writer adds document g to shard 0, then 1, then 2, and the search holds a view
// of each shard (acquired one after the other, so the shards may be at different moments), with the
// statistics (and so the weights) of exactly the documents the views hold. Searches name the shards in
// opposite orders.
void test_snapshot_consistency_impl() {
	Index strata;
	Model model;
	build("snapshot", 3, 0, 1, strata, model, true);
	Model::Doc doc;
	doc.live = true;
	doc.terms = {"common", "pad"};
	doc.wdf = {{"common", 1}, {"pad", 2}};
	doc.length = 3;
	constexpr int generations = 150;
	std::atomic<bool> done{false};
	std::atomic<int> bad{0}, searches{0};
	std::mutex guard;
	std::map<std::size_t, double> weights;   // n documents: their weight
	std::set<std::size_t> sizes;
	const auto weight_for = [&](std::size_t n) {
		std::lock_guard lock(guard);
		sizes.insert(n);
		const auto it = weights.find(n);
		return it != weights.end() ? it->second : weights[n] = weight_in(n, doc);
	};
	const auto fail = [&](const char* what, std::size_t a, std::size_t b, std::size_t c) {
		if (bad++ == 0) { std::fprintf(stderr, "snapshot: %s (%zu %zu %zu)\n", what, a, b, c); }
	};
	// kind 0: shards in order; 1: reversed, descending ids; 2: in order with a spy that sees every match.
	const auto searcher = [&](int kind) {
		std::vector<engine::Shard*> shards = strata.shards(3);
		std::vector<std::size_t> shard_at{0, 1, 2};   // the writer's shard at each position of the request
		if (kind == 1) { std::reverse(shards.begin(), shards.end()); std::reverse(shard_at.begin(), shard_at.end()); }
		auto prepared = engine::prepare(Query::term("f", "common"));
		while (!done || searches < 40) {
			CountingSpy spy;
			engine::SearchRequest request;
			request.query = prepared.get();
			request.maxitems = 100000;
			request.order = kind == 1 ? engine::DocidOrder::descending : engine::DocidOrder::ascending;
			if (kind == 2) { request.spy = &spy; }
			const auto result = engine::sync_wait(engine::search(shards, request));
			++searches;
			std::size_t count[3] = {0, 0, 0}, top[3] = {0, 0, 0};
			for (const auto& hit : result.hits) {
				const auto position = (hit.did - 1) % 3;
				++count[shard_at[position]];
				top[shard_at[position]] = std::max<std::size_t>(top[shard_at[position]], (hit.did - 1) / 3 + 1);
			}
			const std::size_t n = result.hits.size();
			if (result.estimated != n) { fail("estimate differs from the hits", result.estimated, n, 0); }
			for (std::size_t s = 0; s < 3; ++s) {
				if (top[s] != count[s]) { fail("a shard's documents are not 1..count", s, top[s], count[s]); }
			}
			for (std::size_t s = 0; s < 3; ++s) {
				if (count[s] > generations) { fail("a shard holds more than was written", s, count[s], generations); }
			}
			if (n > 0) {
				const double want = weight_for(n);
				for (const auto& hit : result.hits) {
					if (!close(hit.weight, want)) { fail("weights of another snapshot", n, 0, 0); break; }
				}
			}
			if (kind == 2) {
				if (spy.seen.size() != n) { fail("the spy saw other documents than the hits", spy.seen.size(), n, 0); }
				for (const auto weight : spy.weights) {
					if (n > 0 && !close(weight, weight_for(n))) { fail("spy weights of another snapshot", n, 0, 0); break; }
				}
			}
		}
	};
	std::thread a(searcher, 0), b(searcher, 1), c(searcher, 2), d(searcher, 0);
	std::thread writer([&] {
		for (int g = 1; g <= generations; ++g) {
			for (std::size_t s = 0; s < 3; ++s) {
				auto written = make(static_cast<int>(s), g, doc);
				engine::sync_wait(strata.owned[s]->add(written, {}));
				if (g % 7 == 0) {
					engine::sync_wait(strata.owned[s]->commit(engine::Transaction::none));
				} else {
					engine::sync_wait(strata.owned[s]->refresh());
				}
			}
		}
		done = true;
	});
	writer.join();
	a.join();
	b.join();
	c.join();
	d.join();
	CHECK_EQ(bad.load(), 0);
	CHECK_GT(sizes.size(), 3u);   // the searches overlapped the writes
	const auto last = run(strata, 3, Query::term("f", "common"), 0, 100000);
	CHECK_EQ(last.estimated, 3u * generations);
	for (const auto& hit : last.hits) { CHECK_TRUE(close(hit.weight, weight_for(3u * generations))); }
}

}  // namespace detail
