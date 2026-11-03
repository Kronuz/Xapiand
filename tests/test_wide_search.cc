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


// A wide search (docs/mechanisms/wide-search.md): the indexes are searched in batches of at most K held at once
// and merged. Over index counts that straddle K (1, K-1, K, K+1, 3K+2) a batched search gives the hits, their
// order and ids, the totals and the spy's results of one search over all the indexes: the sort by key, the
// collapse, both docid orders and a window. Weights are read against a batch's statistics, so the queries that
// are compared by weight weigh nothing (match_all) and the ones that weigh are ordered by a key.

#include "test_harness.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "database/flags.h"
#include "database/pool.h"
#include "database/shard.h"
#include "engine/callbacks.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace {

constexpr std::size_t K = 4;
constexpr int DOCS = 6;
constexpr int WRITE = DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT;
constexpr engine::valueno GROUP_SLOT = 2;   // the collapse value; absent on every seventh document
constexpr engine::valueno TIED_SLOT = 3;    // a sort key many documents share
constexpr engine::valueno UNIQUE_SLOT = 4;  // a sort key no two documents share

std::string two(int n) {
	char buffer[16];
	std::snprintf(buffer, sizeof buffer, "%04d", n);
	return buffer;
}

// 3K+2 indexes of DOCS documents each, built once; a case searches the first n.
struct Corpus {
	DatabasePool pool{64};
	std::vector<ReferencedShardEndpoint> entries;
	std::vector<std::unique_ptr<Shard>> shards;
	std::vector<engine::Shard*> engines;

	Corpus() {
		for (std::size_t i = 0; i < 3 * K + 2; ++i) {
			const std::string path = "wide_test_" + std::to_string(i) + ".db";
			std::filesystem::remove_all(path);
			entries.push_back(pool.spawn(Endpoint(path)));
			shards.push_back(std::make_unique<Shard>(*entries.back().get(), WRITE));
			for (int j = 0; j < DOCS; ++j) {
				const int n = static_cast<int>(i) * DOCS + j;
				engine::Document doc;
				doc.set_key("k" + std::to_string(n));
				doc.add_term(doc.field("f"), "body", 2);
				doc.add_term(doc.field("f"), (n % 2) != 0 ? "odd" : "even");
				doc.add_term(doc.field("f"), "text", static_cast<engine::termcount>(1 + n % 3));
				if (n % 7 != 6) {
					doc.add_value(GROUP_SLOT, "g" + std::to_string((n * 5 + j) % 4));
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
		entries.clear();
		for (std::size_t i = 0; i < 3 * K + 2; ++i) {
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

// Records the data of every document it is shown, and the weight total.
class RecordingSpy final : public engine::MatchSpy {
public:
	std::vector<std::string> data;
	void see(const engine::DocView& doc, double) override { data.push_back(doc.data()); }
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<RecordingSpy>(); }
	std::string name() const override { return "RecordingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<RecordingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "RecordingSpy"; }
};

// Searches the first n indexes in batches of `batch` (0: one search of all of them).
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
	auto result = engine::sync_wait(engine::search_batched(n, batch, runner, request));
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
	bool weighs = false;   // the weights are compared too (all equal: nothing is read against a batch's statistics)
};

void compare(Corpus& corpus, const Case& c, std::size_t n) {
	auto prepared = engine::prepare(c.query);
	auto make = [&](RecordingSpy& spy) {
		engine::SearchRequest request;
		request.query = prepared.get();
		request.first = c.first;
		request.maxitems = c.maxitems;
		request.check_at_least = engine::CHECK_ALL;
		request.sorter = c.sorter;
		request.collapse_key = c.collapse_key;
		request.collapse_max = c.collapse_max;
		request.order = c.order;
		request.spy = &spy;
		return request;
	};
	RecordingSpy whole_spy, batched_spy;
	std::size_t widest = 0;
	const auto whole = run(corpus, n, 0, make(whole_spy));
	const auto batched = run(corpus, n, K, make(batched_spy), &widest);
	// The reference is also the engine's own search over every shard, with no merge in the way.
	RecordingSpy direct_spy;
	const auto direct = engine::sync_wait(engine::search(std::span<engine::Shard* const>(corpus.engines.data(), n), make(direct_spy)));

	std::fprintf(stderr, "    %s n=%zu: %zu hits of %u, widest batch %zu\n", c.name, n, batched.hits.size(), batched.estimated, widest);
	CHECK_LE(widest, K);
	CHECK_EQ(whole.hits.size(), direct.hits.size());
	CHECK_EQ(whole.estimated, direct.estimated);
	REQUIRE_EQ(batched.hits.size(), whole.hits.size());
	CHECK_EQ(batched.estimated, whole.estimated);
	for (std::size_t i = 0; i < whole.hits.size(); ++i) {
		CHECK_EQ(batched.hits[i].did, whole.hits[i].did);
		CHECK_EQ(batched.hits[i].did, direct.hits[i].did);
		CHECK_EQ(batched.hits[i].rank, whole.hits[i].rank);
		if (c.weighs) {
			CHECK_EQ(batched.hits[i].weight, whole.hits[i].weight);
			CHECK_EQ(batched.hits[i].percent, whole.hits[i].percent);
		}
	}
	// The spy sees every match once, in the same order.
	CHECK_EQ(batched_spy.data.size(), whole_spy.data.size());
	CHECK_TRUE(batched_spy.data == whole_spy.data);
	CHECK_TRUE(whole_spy.data == direct_spy.data);
}

void test_a_batched_search_is_the_search_of_all_the_indexes() {
	Corpus corpus;
	const SlotKey tied(TIED_SLOT);
	const SlotKey unique(UNIQUE_SLOT);
	std::vector<Case> cases;
	cases.push_back({"match_all", engine::Query::match_all(), nullptr, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 0, 10, true});
	cases.push_back({"match_all window", engine::Query::match_all(), nullptr, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 7, 9, true});
	cases.push_back({"match_all descending", engine::Query::match_all(), nullptr, engine::BAD_VALUENO, 0, engine::DocidOrder::descending, 3, 12, true});
	cases.push_back({"match_all tied key", engine::Query::match_all(), &tied, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 0, 15, true});
	cases.push_back({"match_all tied key descending", engine::Query::match_all(), &tied, engine::BAD_VALUENO, 0, engine::DocidOrder::descending, 2, 15, true});
	cases.push_back({"match_all collapse", engine::Query::match_all(), nullptr, GROUP_SLOT, 2, engine::DocidOrder::ascending, 0, 10, true});
	cases.push_back({"match_all collapse window", engine::Query::match_all(), nullptr, GROUP_SLOT, 1, engine::DocidOrder::ascending, 1, 3, true});
	cases.push_back({"match_all collapse tied key", engine::Query::match_all(), &tied, GROUP_SLOT, 2, engine::DocidOrder::ascending, 0, 10, true});
	cases.push_back({"term unique key", engine::Query::term("f", "even"), &unique, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 0, 20});
	cases.push_back({"term unique key window", engine::Query::term("f", "text"), &unique, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 5, 11});
	cases.push_back({"term unique key collapse", engine::Query::term("f", "body"), &unique, GROUP_SLOT, 2, engine::DocidOrder::ascending, 0, 8});
	cases.push_back({"no match", engine::Query::term("f", "absent"), nullptr, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 0, 10, true});
	cases.push_back({"zero window", engine::Query::match_all(), nullptr, engine::BAD_VALUENO, 0, engine::DocidOrder::ascending, 0, 0, true});
	for (const std::size_t n : {std::size_t{1}, K - 1, K, K + 1, 3 * K + 2}) {
		for (const auto& c : cases) {
			compare(corpus, c, n);
		}
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
	const auto result = engine::sync_wait(engine::search_batched(3, 0, runner, request));
	REQUIRE_EQ(result.hits.size(), static_cast<std::size_t>(2 * DOCS));
	for (const auto& hit : result.hits) {
		CHECK_NE((hit.did - 1) % 3, 1u);   // never shard 1's slot
	}
	CHECK_EQ(result.hits[0].did, 1u);
	CHECK_EQ(result.hits[1].did, 3u);
	CHECK_EQ(result.hits[2].did, 4u);
}

// A document id past 32 bits across the shards is refused rather than wrapped.
void test_an_id_that_does_not_fit_is_refused() {
	auto prepared = engine::prepare(engine::Query::match_all());
	engine::SearchRequest request;
	request.query = prepared.get();
	const engine::BatchRunner runner = [&](std::size_t begin, std::size_t end, const engine::SearchRequest&) -> task::task<engine::BatchResult> {
		engine::BatchResult out;
		out.result.hits.push_back({2u, 0.0, 0, 100, {}, {}});   // document 2 of a batch that searched two shards
		out.result.estimated = 1;
		out.positions = {begin, end - 1};
		co_return out;
	};
	// The numbering is arithmetic, so no 4.5 billion shards are needed: the batch's second shard is the one before its end.
	bool refused = false;
	try {
		engine::sync_wait(engine::search_batched(std::size_t{5'000'000'000}, std::size_t{4'500'000'000}, runner, request));
	} catch (const engine::Error& error) {
		refused = error.kind == engine::errc::internal;
	}
	CHECK_TRUE(refused);
}

}  // namespace

int main(int argc, char** argv) {
	struct TestCase { const char* name; void (*fn)(); };
	const TestCase cases[] = {
		{"test_a_batched_search_is_the_search_of_all_the_indexes", test_a_batched_search_is_the_search_of_all_the_indexes},
		{"test_a_missing_index_keeps_the_numbering_of_the_endpoints", test_a_missing_index_keeps_the_numbering_of_the_endpoints},
		{"test_an_id_that_does_not_fit_is_refused", test_an_id_that_does_not_fit_is_refused},
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
