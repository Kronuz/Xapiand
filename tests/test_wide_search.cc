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


// A wide search (docs/mechanisms/wide-search.md): the shards are searched in batches of at most K held at once
// and merged, a batch holding whole indexes. A relevance weight reads the statistics of its own index, so the same
// hits come back in the same order with the same weights whatever K is: over shard counts that straddle K
// (1, K-1, K, K+1, 3K+2) and over K itself (1, K-1, K, K+1, 3K+2, all at once), for the sort by key, the collapse,
// both docid orders, a window and the queries that weigh (a term, an OR, a wildcard, a synonym).

#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "database/flags.h"
#include "database/handler.h"
#include "database/pool.h"
#include "database/shard.h"
#include "pool_support.h"
#include "engine/callbacks.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace {

constexpr std::size_t K = 4;
constexpr std::size_t SHARDS = 3 * K + 2;
// The index each shard belongs to: nine indexes of one to three shards, so batches of K meet an index at their edge.
constexpr std::uint32_t INDEX_OF[SHARDS] = {0, 1, 1, 2, 3, 3, 3, 4, 5, 5, 6, 7, 8, 8};
constexpr int DOCS = 6;   // the fewest documents of a shard
constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT;
constexpr engine::valueno GROUP_SLOT = 21;  // the collapse value; absent on every seventh document
constexpr engine::valueno TIED_SLOT = 22;   // a sort key many documents share
constexpr engine::valueno UNIQUE_SLOT = 23; // a sort key no two documents share

std::string two(int n) {
	char buffer[16];
	std::snprintf(buffer, sizeof buffer, "%04d", n);
	return buffer;
}

// Documents in shard i: the shards differ in size, and so do the indexes in vocabulary and length.
int docs_in(std::size_t i) {
	return DOCS + static_cast<int>((i * 5) % 7);
}

// 3K+2 shards, built once; a case searches the first n.
struct Corpus {
	DatabasePool pool{control_loop(), 64};
	std::vector<std::unique_ptr<Shard>> shards;
	std::vector<engine::Shard*> engines;

	Corpus() {
		int n = 0;
		for (std::size_t i = 0; i < SHARDS; ++i) {
			const std::string path = "wide_test_" + std::to_string(i) + ".db";
			std::filesystem::remove_all(path);
			shards.push_back(std::make_unique<Shard>(pin_index(pool, path, WRITE), WRITE));
			for (int j = 0; j < docs_in(i); ++j, ++n) {
				engine::Document doc;
				doc.set_key("k" + std::to_string(n));
				doc.add_term(doc.field("f"), "body", 2);
				doc.add_term(doc.field("f"), (n % 2) != 0 ? "odd" : "even");
				doc.add_term(doc.field("f"), "text", static_cast<engine::termcount>(1 + n % 3));
				if (INDEX_OF[i] % 3 == 0 && j % 2 == 0) {
					doc.add_term(doc.field("f"), "rare", 2);
				}
				if (INDEX_OF[i] % 2 == 1 && j % 3 == 0) {
					doc.add_term(doc.field("f"), "tail", 1);
				}
				if (INDEX_OF[i] % 4 == 2) {
					doc.add_term(doc.field("f"), "pad", static_cast<engine::termcount>(3 * j));
				}
				if (n % 7 != 6) {
					doc.add_value(GROUP_SLOT, "g" + std::to_string((n * 7 + static_cast<int>(i)) % 4));
				}
				doc.add_value(TIED_SLOT, two((n * 3 + j) % 5));
				doc.add_value(UNIQUE_SLOT, two((n * 37) % 1000));
				doc.set_data(std::to_string(i) + "/" + std::to_string(j));
				engine::sync_wait(shards.back()->add_document(std::move(doc)));
			}
			engine::sync_wait(shards.back()->commit());
		}
		for (auto& shard : shards) {
			engines.push_back(engine::sync_wait(shard->engine_for_read()));
		}
	}
	~Corpus() {
		shards.clear();
		for (std::size_t i = 0; i < SHARDS; ++i) {
			std::filesystem::remove_all("wide_test_" + std::to_string(i) + ".db");
		}
	}
};

class SlotKey final : public engine::KeyMaker {
public:
	explicit SlotKey(engine::valueno slot) : slot_(slot) { }
	std::string key(const engine::DocView& doc) const override { return doc.value(slot_); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(slot_); }
	std::string name() const override { return "SlotKey"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view) const override { return std::make_unique<SlotKey>(slot_); }

private:
	engine::valueno slot_;
};

// Records the data and the weight of every document it is shown.
class RecordingSpy final : public engine::MatchSpy {
public:
	std::vector<std::string> data;
	std::vector<double> weights;
	void see(const engine::DocView& doc, double weight) override {
		data.push_back(doc.data());
		weights.push_back(weight);
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<RecordingSpy>(); }
	std::string name() const override { return "RecordingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<RecordingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "RecordingSpy"; }
};

// Searches the first n shards in batches of `batch` (0: one search of all of them).
engine::SearchResult run(Corpus& corpus, std::size_t n, std::size_t batch, engine::SearchRequest request, std::size_t* largest = nullptr) {
	std::size_t widest = 0;
	const engine::BatchRunner runner = [&](std::size_t begin, std::size_t end, const engine::SearchRequest& inner) -> task::task<engine::BatchResult> {
		widest = std::max(widest, end - begin);
		engine::BatchResult out;
		out.result = co_await engine::search(std::span<engine::Shard* const>(corpus.engines.data() + begin, end - begin), inner);
		for (auto i = begin; i < end; ++i) {
			out.positions.push_back(i);
		}
		co_return out;
	};
	auto result = engine::sync_wait(engine::search_batched(n, batch, std::span<const std::uint32_t>(INDEX_OF, n), runner, request));
	if (largest != nullptr) { *largest = widest; }
	return result;
}

struct Case {
	const char* name;
	engine::Query query;
	const engine::KeyMaker* sorter = nullptr;
	engine::valueno collapse_key = engine::BAD_VALUENO;
	engine::doccount collapse_max = 0;
	engine::DocidOrder order = engine::DocidOrder::ascending;
	engine::doccount first = 0;
	engine::doccount maxitems = 10;
};

engine::SearchRequest make(const Case& c, const engine::PreparedQuery& prepared, RecordingSpy& spy) {
	engine::SearchRequest request;
	request.query = &prepared;
	request.first = c.first;
	request.maxitems = c.maxitems;
	request.check_at_least = engine::CHECK_ALL;
	request.sorter = c.sorter;
	request.collapse_key = c.collapse_key;
	request.collapse_max = c.collapse_max;
	request.order = c.order;
	request.spy = &spy;
	return request;
}

// Two results are the same search: hits, ids, ranks, weights, percents and totals.
void same_hits(const engine::SearchResult& a, const engine::SearchResult& b) {
	CHECK_EQ(a.estimated, b.estimated);
	REQUIRE_EQ(a.hits.size(), b.hits.size());
	for (std::size_t i = 0; i < a.hits.size(); ++i) {
		CHECK_EQ(a.hits[i].did, b.hits[i].did);
		CHECK_EQ(a.hits[i].rank, b.hits[i].rank);
		CHECK_EQ(a.hits[i].weight, b.hits[i].weight);
		CHECK_EQ(a.hits[i].percent, b.hits[i].percent);
	}
}

void compare(Corpus& corpus, const Case& c, std::size_t n) {
	auto prepared = engine::prepare(c.query);
	RecordingSpy whole_spy, batched_spy, direct_spy;
	std::size_t widest = 0;
	const auto whole = run(corpus, n, 0, make(c, *prepared, whole_spy));
	const auto batched = run(corpus, n, K, make(c, *prepared, batched_spy), &widest);
	// The reference is also the engine's own search over every shard, with no merge in the way.
	auto direct_request = make(c, *prepared, direct_spy);
	direct_request.index_of = std::span<const std::uint32_t>(INDEX_OF, n);
	const auto direct = engine::sync_wait(engine::search(std::span<engine::Shard* const>(corpus.engines.data(), n), direct_request));

	std::fprintf(stderr, "    %s n=%zu: %zu hits of %u, widest batch %zu\n", c.name, n, batched.hits.size(), batched.estimated, widest);
	CHECK_LE(widest, K);
	if (n == SHARDS && std::string(c.name) != "no match") {
		CHECK_GT(whole.estimated, 0u);   // never a comparison of two empty results
	}
	same_hits(whole, direct);
	same_hits(batched, whole);
	// The spy sees every match once, in the same order, with the same weight.
	CHECK_TRUE(batched_spy.data == whole_spy.data);
	CHECK_TRUE(batched_spy.weights == whole_spy.weights);
	CHECK_TRUE(whole_spy.data == direct_spy.data);
	CHECK_TRUE(whole_spy.weights == direct_spy.weights);
}

std::vector<Case> all_cases(const engine::KeyMaker& tied, const engine::KeyMaker& unique) {
	using engine::Query;
	std::vector<Case> cases;
	const auto asc = engine::DocidOrder::ascending;
	const auto desc = engine::DocidOrder::descending;
	const auto none = engine::BAD_VALUENO;
	cases.push_back({"match_all", Query::match_all(), nullptr, none, 0, asc, 0, 10});
	cases.push_back({"match_all window", Query::match_all(), nullptr, none, 0, asc, 7, 9});
	cases.push_back({"match_all descending", Query::match_all(), nullptr, none, 0, desc, 3, 12});
	cases.push_back({"match_all tied key", Query::match_all(), &tied, none, 0, asc, 0, 15});
	cases.push_back({"match_all tied key descending", Query::match_all(), &tied, none, 0, desc, 2, 15});
	cases.push_back({"match_all collapse", Query::match_all(), nullptr, GROUP_SLOT, 2, asc, 0, 10});
	cases.push_back({"match_all collapse window", Query::match_all(), nullptr, GROUP_SLOT, 1, asc, 1, 3});
	cases.push_back({"match_all collapse tied key", Query::match_all(), &tied, GROUP_SLOT, 2, asc, 0, 10});
	cases.push_back({"term unique key", Query::term("f", "even"), &unique, none, 0, asc, 0, 20});
	cases.push_back({"term unique key window", Query::term("f", "text"), &unique, none, 0, asc, 5, 11});
	cases.push_back({"term unique key collapse", Query::term("f", "body"), &unique, GROUP_SLOT, 2, asc, 0, 8});
	cases.push_back({"no match", Query::term("f", "absent"), nullptr, none, 0, asc, 0, 10});
	cases.push_back({"zero window", Query::match_all(), nullptr, none, 0, asc, 0, 0});
	// The queries that rank by relevance, which the indexes' own statistics decide.
	cases.push_back({"term", Query::term("f", "text"), nullptr, none, 0, asc, 0, 30});
	cases.push_back({"term window", Query::term("f", "text"), nullptr, none, 0, asc, 4, 9});
	cases.push_back({"rare term", Query::term("f", "rare"), nullptr, none, 0, asc, 0, 12});
	cases.push_back({"term descending", Query::term("f", "text"), nullptr, none, 0, desc, 1, 25});
	cases.push_back({"or", Query::combine(engine::Op::OR, Query::term("f", "rare"), Query::term("f", "tail")), nullptr, none, 0, asc, 0, 40});
	cases.push_back({"and", Query::combine(engine::Op::AND, Query::term("f", "text"), Query::term("f", "odd")), nullptr, none, 0, asc, 0, 40});
	cases.push_back({"wildcard", Query::wildcard("f", "ta"), nullptr, none, 0, asc, 0, 30});
	cases.push_back({"wildcard or", Query::wildcard("f", "t", 0, engine::Expansion::error, 0, engine::Op::OR), nullptr, none, 0, asc, 0, 40});
	cases.push_back({"synonym", Query::combine(engine::Op::SYNONYM, Query::term("f", "rare"), Query::term("f", "tail")), nullptr, none, 0, asc, 0, 30});
	cases.push_back({"term collapse", Query::term("f", "text"), nullptr, GROUP_SLOT, 2, asc, 0, 10});
	cases.push_back({"term tied key", Query::term("f", "text"), &tied, none, 0, asc, 0, 30});
	return cases;
}

void test_a_batched_search_is_the_search_of_all_the_indexes() {
	Corpus corpus;
	const SlotKey tied(TIED_SLOT);
	const SlotKey unique(UNIQUE_SLOT);
	const auto cases = all_cases(tied, unique);
	// The values are stored and read back: a key reorders the hits and a collapse drops some.
	{
		const auto plain = [&](const engine::KeyMaker* sorter, engine::valueno collapse_key, engine::doccount collapse_max) {
			auto prepared = engine::prepare(engine::Query::match_all());
			engine::SearchRequest request;
			request.query = prepared.get();
			request.maxitems = 1000;
			request.sorter = sorter;
			request.collapse_key = collapse_key;
			request.collapse_max = collapse_max;
			return run(corpus, SHARDS, K, request);
		};
		const auto base = plain(nullptr, engine::BAD_VALUENO, 0);
		const auto keyed = plain(&tied, engine::BAD_VALUENO, 0);
		const auto collapsed = plain(nullptr, GROUP_SLOT, 2);
		std::size_t total = 0;
		for (std::size_t i = 0; i < SHARDS; ++i) { total += static_cast<std::size_t>(docs_in(i)); }
		REQUIRE_EQ(base.hits.size(), total);
		bool reordered = false;
		for (std::size_t i = 0; i < base.hits.size(); ++i) {
			reordered = reordered || base.hits[i].did != keyed.hits[i].did;
		}
		CHECK_TRUE(reordered);
		CHECK_LT(collapsed.hits.size(), base.hits.size());
		CHECK_GE(collapsed.hits.size(), std::size_t{8});
	}
	for (const std::size_t n : {std::size_t{1}, K - 1, K, K + 1, SHARDS}) {
		for (const auto& c : cases) {
			compare(corpus, c, n);
		}
	}
}

// The batch size changes nothing: K = 1, K-1, K, K+1, 3K+2 and one batch of all give the same hits, order, weights
// and totals. An index wider than K (K = 1 against a three-shard index) is a batch of its own.
void test_the_hits_do_not_depend_on_the_batch_size() {
	Corpus corpus;
	const SlotKey tied(TIED_SLOT);
	const SlotKey unique(UNIQUE_SLOT);
	std::size_t differing = 0;
	for (const auto& c : all_cases(tied, unique)) {
		auto prepared = engine::prepare(c.query);
		RecordingSpy base_spy;
		const auto base = run(corpus, SHARDS, 0, make(c, *prepared, base_spy));
		for (const std::size_t batch : {std::size_t{1}, K - 1, K, K + 1, SHARDS}) {
			RecordingSpy spy;
			std::size_t widest = 0;
			const auto result = run(corpus, SHARDS, batch, make(c, *prepared, spy), &widest);
			CHECK_LE(widest, std::max<std::size_t>(batch, 3));
			same_hits(result, base);
			CHECK_TRUE(spy.data == base_spy.data);
			CHECK_TRUE(spy.weights == base_spy.weights);
		}
		// Non-vacuity: reading every index's statistics as one index's weighs the same hits differently.
		if (c.sorter == nullptr && c.collapse_key == engine::BAD_VALUENO && c.maxitems > 0 && std::string(c.name) != "no match") {
			RecordingSpy one_spy;
			auto request = make(c, *prepared, one_spy);
			const auto as_one = engine::sync_wait(engine::search(std::span<engine::Shard* const>(corpus.engines.data(), SHARDS), request));
			if (as_one.hits.size() == base.hits.size()) {
				for (std::size_t i = 0; i < base.hits.size(); ++i) {
					if (as_one.hits[i].weight != base.hits[i].weight) { ++differing; break; }
				}
			}
		}
	}
	CHECK_GE(differing, std::size_t{5});   // the queries that weigh read different statistics per index
}

// A document weighs the same in a wide search as in a search of its own index alone.
void test_a_weight_is_that_of_the_documents_index() {
	Corpus corpus;
	for (const auto& query : {engine::Query::term("f", "text"), engine::Query::term("f", "rare"),
		engine::Query::combine(engine::Op::OR, engine::Query::term("f", "rare"), engine::Query::term("f", "tail")),
		engine::Query::wildcard("f", "ta"), engine::Query::wildcard("f", "t", 0, engine::Expansion::error, 0, engine::Op::OR)}) {
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 1000;
		request.check_at_least = engine::CHECK_ALL;
		RecordingSpy wide;
		request.spy = &wide;
		run(corpus, SHARDS, K, request);
		std::map<std::string, double> wide_weight;
		for (std::size_t i = 0; i < wide.data.size(); ++i) { wide_weight[wide.data[i]] = wide.weights[i]; }
		std::size_t compared = 0;
		for (std::uint32_t index = 0; index <= INDEX_OF[SHARDS - 1]; ++index) {
			std::vector<engine::Shard*> own;
			for (std::size_t i = 0; i < SHARDS; ++i) {
				if (INDEX_OF[i] == index) { own.push_back(corpus.engines[i]); }
			}
			RecordingSpy alone;
			auto own_request = request;
			own_request.spy = &alone;
			engine::sync_wait(engine::search(own, own_request));
			for (std::size_t i = 0; i < alone.data.size(); ++i) {
				REQUIRE(wide_weight.count(alone.data[i]) == 1);
				CHECK_EQ(wide_weight[alone.data[i]], alone.weights[i]);
				++compared;
			}
		}
		std::fprintf(stderr, "    weights of %zu documents match their index's\n", compared);
		CHECK_EQ(compared, wide_weight.size());
		CHECK_GT(compared, std::size_t{10});
	}
}

// A batch that finds a shard missing numbers the hits over the search's shards, as the host reads them back.
void test_a_missing_index_keeps_the_numbering_of_the_endpoints() {
	Corpus corpus;
	auto prepared = engine::prepare(engine::Query::match_all());
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100;
	// Shard 1 of 3 is missing: the batch searches 0 and 2.
	const engine::BatchRunner runner = [&](std::size_t, std::size_t, const engine::SearchRequest& inner) -> task::task<engine::BatchResult> {
		engine::BatchResult out;
		engine::Shard* const found[] = {corpus.engines[0], corpus.engines[2]};
		out.result = co_await engine::search(found, inner);
		out.positions = {0, 2};
		co_return out;
	};
	const auto result = engine::sync_wait(engine::search_batched(3, 0, {}, runner, request));
	REQUIRE_EQ(result.hits.size(), static_cast<std::size_t>(docs_in(0) + docs_in(2)));
	for (const auto& hit : result.hits) {
		CHECK_NE((hit.did - 1) % 3, 1u);   // never shard 1's slot
	}
	CHECK_EQ(result.hits[0].did, 1u);
	CHECK_EQ(result.hits[1].did, 3u);
	CHECK_EQ(result.hits[2].did, 4u);
}

// A document id across the shards is 64 bits: a search past the old 32-bit edge (42,949 documents of each of the
// 100,000 shards the cap allows) numbers its hits exactly, one batch or several.
void test_an_id_past_32_bits_is_exact() {
	auto prepared = engine::prepare(engine::Query::match_all());
	engine::SearchRequest request;
	request.query = prepared.get();
	constexpr std::size_t total = 100'000;
	const auto expected = [&](std::uint64_t d, std::size_t position) { return (d - 1) * total + position + 1; };
	constexpr std::uint64_t below = 42'949;   // (d - 1) * 100,000 + 100,000 < 2^32
	constexpr std::uint64_t above = 42'950;   // and this one is not
	CHECK_LT(expected(below, total - 1), std::uint64_t{1} << 32);
	CHECK_GT(expected(above, total - 1), std::uint64_t{1} << 32);
	// The batch searched its first and its last shard, so the second of two, document d, is batch id (d - 1) * 2 + 2.
	for (const std::size_t batch : {std::size_t{0}, std::size_t{50'000}}) {
		const engine::BatchRunner runner = [&](std::size_t begin, std::size_t end, const engine::SearchRequest&) -> task::task<engine::BatchResult> {
			engine::BatchResult out;
			out.positions = {begin, end - 1};
			for (const auto d : {below, above}) {
				out.result.hits.push_back({(d - 1) * 2 + 2, 0.0, 0, 100, {}, {}});
			}
			out.result.estimated = 2;
			co_return out;
		};
		request.maxitems = 100;
		const auto result = engine::sync_wait(engine::search_batched(total, batch, {}, runner, request));
		std::vector<std::uint64_t> ids;
		for (const auto& hit : result.hits) { ids.push_back(hit.did); }
		std::vector<std::uint64_t> want;
		if (batch == 0) {
			want = {expected(below, total - 1), expected(above, total - 1)};
		} else {
			// Two batches: each searched its first and its last shard.
			want = {expected(below, 49'999), expected(below, total - 1), expected(above, 49'999), expected(above, total - 1)};
		}
		std::sort(want.begin(), want.end());
		std::sort(ids.begin(), ids.end());
		CHECK_TRUE(ids == want);
	}
}

// The id a host decodes: the shard and the shard's own document, at the old edge and at the 64-bit one.
void test_a_hit_id_decodes_past_32_bits() {
	constexpr std::size_t shards = 100'000;
	for (const std::uint64_t d : {std::uint64_t{1}, std::uint64_t{42'949}, std::uint64_t{42'950}, std::uint64_t{4'000'000'000}}) {
		for (const std::size_t position : {std::size_t{0}, std::size_t{49'999}, shards - 1}) {
			const std::uint64_t id = (d - 1) * shards + position + 1;
			const auto where = split_hit_id(id, shards);
			CHECK_EQ(where.shard, position);
			CHECK_EQ(where.did, static_cast<engine::docid>(d));
		}
	}
	// No such document: zero, and a document number no shard can hold.
	CHECK_THROW(split_hit_id(0, shards), engine::Error);
	CHECK_THROW(split_hit_id((std::uint64_t{1} << 32) * shards + 1, shards), engine::Error);
	// A shard count a search never has is no document either.
	CHECK_THROW(split_hit_id(5, 0), engine::Error);
}

// An index wider than the batch is a batch of its own, and a batch never splits an index.
void test_a_batch_holds_whole_indexes() {
	auto prepared = engine::prepare(engine::Query::match_all());
	engine::SearchRequest request;
	request.query = prepared.get();
	const std::uint32_t index_of[] = {0, 0, 0, 0, 0, 0, 1, 2, 2, 3, 3, 3, 4};   // 6, 1, 2, 3, 1 shards
	std::vector<std::pair<std::size_t, std::size_t>> batches;
	const engine::BatchRunner runner = [&](std::size_t begin, std::size_t end, const engine::SearchRequest& inner) -> task::task<engine::BatchResult> {
		batches.emplace_back(begin, end);
		CHECK_EQ(inner.index_of.size(), end - begin);
		engine::BatchResult out;
		for (auto i = begin; i < end; ++i) { out.positions.push_back(i); }
		co_return out;
	};
	engine::sync_wait(engine::search_batched(13, 4, index_of, runner, request));
	const std::vector<std::pair<std::size_t, std::size_t>> want = {{0, 6}, {6, 9}, {9, 13}};
	CHECK_TRUE(batches == want);
	// An index list that is not one entry per shard is refused.
	CHECK_THROW(engine::sync_wait(engine::search_batched(13, 4, std::span<const std::uint32_t>(index_of, 5), runner, request)), engine::Error);
}

}  // namespace

int main(int argc, char** argv) {
	struct TestCase { const char* name; void (*fn)(); };
	const TestCase cases[] = {
		{"test_a_batched_search_is_the_search_of_all_the_indexes", test_a_batched_search_is_the_search_of_all_the_indexes},
		{"test_a_missing_index_keeps_the_numbering_of_the_endpoints", test_a_missing_index_keeps_the_numbering_of_the_endpoints},
		{"test_the_hits_do_not_depend_on_the_batch_size", test_the_hits_do_not_depend_on_the_batch_size},
		{"test_a_weight_is_that_of_the_documents_index", test_a_weight_is_that_of_the_documents_index},
		{"test_an_id_past_32_bits_is_exact", test_an_id_past_32_bits_is_exact},
		{"test_a_hit_id_decodes_past_32_bits", test_a_hit_id_decodes_past_32_bits},
		{"test_a_batch_holds_whole_indexes", test_a_batch_holds_whole_indexes},
	};
	for (const auto& c : cases) {
		if (argc > 1 && std::string(c.name).find(argv[1]) == std::string::npos) {
			continue;
		}
		const int before = testh::failures;
		try {
			c.fn();
		} catch (const engine::Error& error) {
			std::fprintf(stderr, "  FAIL %s: threw engine::Error %s: %s\n", c.name, engine::to_string(error.kind), error.description().c_str());
			++testh::failures;
		} catch (const std::exception& exc) {
			std::fprintf(stderr, "  FAIL %s: threw %s\n", c.name, exc.what());
			++testh::failures;
		}
		std::fprintf(stderr, "%s %s\n", testh::failures == before ? "ok  " : "FAIL", c.name);
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
