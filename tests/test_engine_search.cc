/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Unit tests for engine::search: every request field and every part of the result is compared with
// a hand-written Xapian search over the same shards, and the objects a search makes are counted.

#include "test_harness.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "engine/xapian/callbacks.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/query.h"
#include "engine/xapian/search.h"
#include "engine/xapian/shard.h"
#include "engine/xapian/xapian.h"

namespace {

using engine::Op;
using engine::Query;

std::string path_of(const char* name) {
	return (std::filesystem::current_path() / name).string();
}

// Orders by the slot-1 value.
class SlotKey final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView& doc) const override { return doc.value(1); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(); }
	std::string name() const override { return "SlotKey"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view) const override { return std::make_unique<SlotKey>(); }
};

// Throws from key().
class ThrowingKey final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView&) const override { throw std::runtime_error("key"); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<ThrowingKey>(); }
	std::string name() const override { return "ThrowingKey"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view) const override { return std::make_unique<ThrowingKey>(); }
};

// Counts the documents it sees and their weights; optionally throws on the third.
class CountingSpy final : public engine::MatchSpy {
public:
	int seen = 0;
	double weights = 0;
	bool throws = false;

	void see(const engine::DocView& doc, double weight) override {
		if (throws && seen == 2) {
			throw std::runtime_error("spy");
		}
		++seen;
		weights += weight;
		(void)doc.did();
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountingSpy>(); }
	std::string name() const override { return "CountingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialise_results() const override { return std::to_string(seen); }
	void merge_results(std::string_view s) override { seen += std::stoi(std::string(s)); }
	std::string description() const override { return "CountingSpy"; }
};

// A shard of another engine: it implements nothing.
class Foreign final : public engine::Shard {
public:
	template <typename T>
	[[noreturn]] static task::task<T> nope() { throw std::logic_error("foreign"); }

	task::task<engine::WriteResult> add(engine::Document&, engine::WriteArgs) override { nope<engine::WriteResult>(); }
	task::task<engine::WriteResult> replace(engine::docid, engine::Document&, engine::WriteArgs) override { nope<engine::WriteResult>(); }
	task::task<engine::WriteResult> replace_by_key(const std::string&, engine::Document&, engine::WriteArgs) override { nope<engine::WriteResult>(); }
	task::task<void> erase(engine::docid, bool, std::optional<engine::rev>) override { nope<void>(); }
	task::task<engine::docid> erase_by_key(const std::string&, bool, std::optional<engine::rev>) override { nope<engine::docid>(); }
	task::task<void> commit(engine::Transaction) override { nope<void>(); }
	task::task<void> begin_transaction(bool) override { nope<void>(); }
	task::task<void> commit_transaction() override { nope<void>(); }
	task::task<void> cancel_transaction() override { nope<void>(); }
	task::task<void> set_metadata(const std::string&, const std::string&) override { nope<void>(); }
	task::task<void> add_spelling(const std::string&, engine::termcount) override { nope<void>(); }
	task::task<engine::termcount> remove_spelling(const std::string&, engine::termcount) override { nope<engine::termcount>(); }
	task::task<bool> reopen() override { nope<bool>(); }
	task::task<engine::Stats> stats() override { nope<engine::Stats>(); }
	task::task<engine::docid> find_key(const std::string&) override { nope<engine::docid>(); }
	task::task<std::unique_ptr<engine::DocView>> get(engine::docid) override { nope<std::unique_ptr<engine::DocView>>(); }
	task::task<void> scan(engine::docid, const engine::ScanSink&) override { nope<void>(); }
	task::task<std::string> metadata(const std::string&) override { nope<std::string>(); }
	task::task<std::vector<std::string>> metadata_keys() override { nope<std::vector<std::string>>(); }
	engine::rev revision() const override { return 0; }
	engine::doccount doc_count() const override { return 0; }
	std::string uuid() const override { return ""; }
	unsigned refs() const override { return 0; }
};

// A shard of `count` documents: terms t0..t3 (varied frequencies), the shared term, X-prefixed
// "field" terms, a sort key in slot 1 and a group in slot 2. `salt` makes shards differ.
std::unique_ptr<engine::xapian::Shard> build(const std::string& path, int count, int salt) {
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (int i = 1; i <= count; ++i) {
		engine::Document doc;
		doc.add_boolean_term("Q" + std::to_string(salt) + "_" + std::to_string(i));
		doc.add_term("t" + std::to_string((i + salt) % 4), 1 + (i * salt) % 5);
		if (i % 3 == 0) {
			doc.add_term("t" + std::to_string((i + 1) % 4), 2);
		}
		doc.add_term("common", 1);
		if (i % 2 == 0) {
			doc.add_term("even", 2);
		}
		doc.add_term("Xcolor" + std::to_string((i * salt) % 6), 1 + i % 3);
		doc.add_term("Xshape" + std::to_string(i % 5), 1);
		doc.add_term("other" + std::to_string(i % 7), 1);
		const int key = (i * 7 + salt * 3) % 23;
		doc.add_value(1, (key < 10 ? "k0" : "k") + std::to_string(key));
		doc.add_value(2, "g" + std::to_string((i + salt) % 3));
		doc.set_data("d" + std::to_string(i));
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));
	return std::move(opened.shard);
}

struct Fixture {
	std::unique_ptr<engine::xapian::Shard> a, b;

	Fixture() : a(build(path_of("engine_search_a.db"), 30, 1)), b(build(path_of("engine_search_b.db"), 22, 2)) { }

	// The shards as the search takes them.
	std::vector<engine::Shard*> shards(int n) const {
		std::vector<engine::Shard*> list{a.get()};
		if (n > 1) {
			list.push_back(b.get());
		}
		return list;
	}

	Xapian::Database database(int n) const {
		Xapian::Database db;
		if (n == 1) {
			db = *a->database();
		} else {
			db.add_database(*a->database());
			db.add_database(*b->database());
		}
		return db;
	}
};

struct Expect {
	std::vector<engine::Hit> hits;
	engine::doccount estimated = 0;
};

// What a hand-written Xapian search returns for the request.
Expect reference(const Fixture& fixture, int n, const Query& query, const engine::SearchRequest& request) {
	auto db = fixture.database(n);
	const auto xq = engine::xapian::to_xapian(query);
	Xapian::Enquire enquire(db);
	enquire.set_collapse_key(request.collapse_key, request.collapse_max);
	enquire.set_cutoff(request.percent_threshold, request.weight_threshold);
	enquire.set_docid_order(request.order == engine::DocidOrder::descending ? Xapian::Enquire::DESCENDING : Xapian::Enquire::ASCENDING);
	std::optional<engine::xapian::MatchSpyAdapter> spy;
	std::optional<engine::xapian::KeyMakerAdapter> sorter;
	if (request.spy) {
		spy.emplace(*request.spy);
		enquire.add_matchspy(&*spy);
	}
	if (request.sorter) {
		sorter.emplace(*request.sorter);
		enquire.set_sort_by_key_then_relevance(&*sorter, false);
	}
	auto rset_of = [&](engine::doccount count) {
		Xapian::Enquire e(db);
		e.set_query(xq);
		Xapian::RSet rset;
		auto m = e.get_mset(0, count, Xapian::doccount{0});
		for (auto it = m.begin(); it != m.end(); ++it) {
			rset.add_document(*it);
		}
		return rset;
	};
	struct Prefixes final : Xapian::ExpandDecider {
		std::vector<std::string> list;
		bool operator()(const std::string& term) const override {
			for (const auto& p : list) {
				if (term.compare(0, p.size(), p) == 0) { return true; }
			}
			return list.empty();
		}
	};
	auto q = xq;
	if (request.nearest) {
		Prefixes d; d.list = request.nearest->prefixes;
		auto eset = enquire.get_eset(request.nearest->n_eset, rset_of(request.nearest->n_rset), &d);
		q = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), request.nearest->n_term);
	}
	if (request.fuzzy) {
		Prefixes d; d.list = request.fuzzy->prefixes;
		auto eset = enquire.get_eset(request.fuzzy->n_eset, rset_of(request.fuzzy->n_rset), &d);
		q = Xapian::Query(Xapian::Query::OP_OR, q, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), request.fuzzy->n_term));
	}
	enquire.set_query(q);
	auto mset = enquire.get_mset(request.first, request.maxitems, request.check_at_least);
	Expect expect;
	expect.estimated = mset.get_matches_estimated();
	for (auto it = mset.begin(); it != mset.end(); ++it) {
		expect.hits.push_back({*it, it.get_weight(), it.get_rank(), it.get_percent()});
	}
	return expect;
}

engine::SearchResult run(const Fixture& fixture, int n, const Query& query, engine::SearchRequest request) {
	auto prepared = engine::prepare(query);
	request.query = prepared.get();
	request.origin = "test";
	auto shards = fixture.shards(n);
	return engine::sync_wait(engine::search(shards, request));
}

// The search and the reference agree on every hit and on the estimate; returns the hits found.
size_t agree(const Fixture& fixture, int n, const Query& query, engine::SearchRequest request) {
	auto got = run(fixture, n, query, request);
	auto want = reference(fixture, n, query, request);
	CHECK_EQ(got.estimated, want.estimated);
	CHECK_EQ(got.hits.size(), want.hits.size());
	CHECK_EQ(got.size(), want.hits.size());
	for (size_t i = 0; i < got.hits.size() && i < want.hits.size(); ++i) {
		CHECK_EQ(got.hits[i].did, want.hits[i].did);
		CHECK_TRUE(got.hits[i].weight == want.hits[i].weight);
		CHECK_EQ(got.hits[i].rank, want.hits[i].rank);
		CHECK_EQ(got.hits[i].percent, want.hits[i].percent);
	}
	return got.hits.size();
}

Query some_query() {
	return Query::combine(Op::OR, {Query::term("t1"), Query::term("t2", 2), Query::term("even")});
}

void test_ranking_and_window() {
	Fixture fixture;
	for (int n : {1, 2}) {
		engine::SearchRequest request;
		request.maxitems = 100;
		CHECK_EQ(agree(fixture, n, Query::match_all(), request), n == 1 ? 30u : 52u);
		CHECK_TRUE(agree(fixture, n, some_query(), request) > 10);
		request.first = 3;
		request.maxitems = 7;
		request.check_at_least = 100;
		CHECK_EQ(agree(fixture, n, some_query(), request), 7u);
		request.first = 1000;
		CHECK_EQ(agree(fixture, n, some_query(), request), 0u);
		request = {};
		request.maxitems = 0;
		request.check_at_least = 100;
		CHECK_EQ(agree(fixture, n, some_query(), request), 0u);
		CHECK_TRUE(run(fixture, n, some_query(), request).estimated > 0);
	}
	// the ids across two shards interleave the way the shards number them
	engine::SearchRequest request;
	request.maxitems = 100;
	auto both = run(fixture, 2, Query::term("Q2_1"), request);
	CHECK_EQ(both.hits.size(), 1u);
	CHECK_EQ(both.hits[0].did, 2u);
	auto first = run(fixture, 2, Query::term("Q1_1"), request);
	CHECK_EQ(first.hits.size(), 1u);
	CHECK_EQ(first.hits[0].did, 1u);
	CHECK_EQ(run(fixture, 2, Query::term("Q2_2"), request).hits[0].did, 4u);
}

void test_sort_collapse_cutoff_order() {
	Fixture fixture;
	SlotKey key;
	for (int n : {1, 2}) {
		engine::SearchRequest request;
		request.maxitems = 100;
		request.sorter = &key;
		CHECK_TRUE(agree(fixture, n, some_query(), request) > 10);
		request.first = 2;
		request.maxitems = 9;
		CHECK_EQ(agree(fixture, n, some_query(), request), 9u);

		request = {};
		request.maxitems = 100;
		request.collapse_key = 2;
		request.collapse_max = 1;
		request.check_at_least = 100;
		CHECK_EQ(agree(fixture, n, some_query(), request), 3u);
		request.collapse_max = 2;
		CHECK_EQ(agree(fixture, n, some_query(), request), 6u);

		request = {};
		request.maxitems = 100;
		request.percent_threshold = 60;
		const auto strict = agree(fixture, n, some_query(), request);
		request.percent_threshold = 0;
		const auto loose = agree(fixture, n, some_query(), request);
		CHECK_TRUE(strict > 0 && strict < loose);
		request.weight_threshold = 1.0;
		agree(fixture, n, some_query(), request);

		request = {};
		request.maxitems = 100;
		request.order = engine::DocidOrder::descending;
		agree(fixture, n, Query::match_all(), request);
		request.order = engine::DocidOrder::dont_care;
		agree(fixture, n, some_query(), request);
	}
	// the order is observable: ascending and descending differ on equal weights
	engine::SearchRequest up, down;
	up.maxitems = down.maxitems = 100;
	down.order = engine::DocidOrder::descending;
	auto a = run(fixture, 2, Query::match_all(), up);
	auto d = run(fixture, 2, Query::match_all(), down);
	CHECK_TRUE(a.hits.front().did != d.hits.front().did);
	// a sorter orders by its keys: the first hit holds the smallest key
	engine::SearchRequest sorted;
	sorted.maxitems = 100;
	sorted.sorter = &key;
	auto s = run(fixture, 1, Query::match_all(), sorted);
	std::string previous;
	for (const auto& hit : s.hits) {
		auto k = fixture.a->database()->get_document(hit.did).get_value(1);
		CHECK_TRUE(previous <= k);
		previous = k;
	}
}

void test_spy() {
	Fixture fixture;
	for (int n : {1, 2}) {
		CountingSpy got, want;
		engine::SearchRequest request;
		request.maxitems = 5;
		request.check_at_least = 1000;
		request.spy = &got;
		run(fixture, n, some_query(), request);
		request.spy = &want;
		reference(fixture, n, some_query(), request);
		CHECK_TRUE(got.seen > 10);
		CHECK_EQ(got.seen, want.seen);
		CHECK_TRUE(got.weights == want.weights);
	}
	// a spy borrowed by one search sees that search and nothing else
	CountingSpy spy;
	engine::SearchRequest request;
	request.maxitems = 3;
	request.check_at_least = 1000;
	request.spy = &spy;
	run(fixture, 1, Query::term("t1"), request);
	const int after_one = spy.seen;
	CHECK_TRUE(after_one > 0);
	run(fixture, 1, Query::term("t1"), request);
	CHECK_EQ(spy.seen, 2 * after_one);
}

// CHECK_ALL makes the spy see every match; a small check_at_least leaves it a sample.
void test_spy_check_all() {
	Fixture fixture;
	for (int n : {1, 2}) {
		CountingSpy all, some, sample;
		engine::SearchRequest request;
		request.maxitems = 0;
		request.check_at_least = engine::CHECK_ALL;
		request.spy = &all;
		run(fixture, n, some_query(), request);
		request.check_at_least = 1000;
		request.spy = &some;
		run(fixture, n, some_query(), request);
		request.check_at_least = 1;
		request.spy = &sample;
		run(fixture, n, some_query(), request);
		CHECK_TRUE(all.seen > 10);
		CHECK_EQ(all.seen, some.seen);
		CHECK_TRUE(sample.seen <= all.seen);
	}
}


void test_similar() {
	Fixture fixture;
	for (int n : {1, 2}) {
		for (bool filtered : {false, true}) {
			engine::Similar similar;
			similar.n_rset = 4;
			similar.n_eset = 12;
			similar.n_term = 3;
			if (filtered) {
				similar.prefixes = {"Xcolor", "Xshape"};
			}
			engine::SearchRequest request;
			request.maxitems = 100;
			request.check_at_least = 100;
			request.nearest = similar;
			CHECK_TRUE(agree(fixture, n, Query::term("t1"), request) > 0);
			request.nearest.reset();
			request.fuzzy = similar;
			CHECK_TRUE(agree(fixture, n, Query::term("t1"), request) > 0);
			request.nearest = similar;
			CHECK_TRUE(agree(fixture, n, Query::term("t1"), request) > 0);
			request.sorter = nullptr;
			request.first = 1;
			request.maxitems = 6;
			agree(fixture, n, some_query(), request);
		}
	}
	// the prefix filter is observable: the elite set holds only terms with the prefixes
	engine::Similar only_other;
	only_other.n_rset = 4;
	only_other.n_eset = 40;
	only_other.n_term = 40;
	only_other.prefixes = {"other"};
	engine::Similar only_color = only_other;
	only_color.prefixes = {"Xcolor"};
	engine::SearchRequest a, b;
	a.maxitems = b.maxitems = 100;
	a.nearest = only_other;
	b.nearest = only_color;
	auto ra = run(fixture, 1, Query::term("t1"), a);
	auto rb = run(fixture, 1, Query::term("t1"), b);
	bool differ = ra.hits.size() != rb.hits.size();
	for (size_t i = 0; !differ && i < ra.hits.size(); ++i) {
		differ = ra.hits[i].did != rb.hits[i].did;
	}
	CHECK_TRUE(differ);
}

// nearest and fuzzy each use their own settings, and the relevant set ignores the request's
// collapse, cutoffs and docid order.
void test_similar_relevant_set() {
	Fixture fixture;
	for (int n : {1, 2}) {
		engine::Similar wide, narrow;
		wide.n_rset = narrow.n_rset = 6;
		wide.n_eset = narrow.n_eset = 20;
		wide.n_term = 8;
		narrow.n_term = 2;
		engine::SearchRequest request;
		request.maxitems = 100;
		request.check_at_least = 100;
		request.nearest = wide;
		request.fuzzy = narrow;
		CHECK_TRUE(agree(fixture, n, some_query(), request) > 0);
		request.nearest = narrow;
		request.fuzzy = wide;
		CHECK_TRUE(agree(fixture, n, some_query(), request) > 0);

		for (auto order : {engine::DocidOrder::descending, engine::DocidOrder::dont_care}) {
			for (bool both : {false, true}) {
				request = {};
				request.maxitems = 100;
				request.check_at_least = 100;
				request.nearest = wide;
				if (both) {
					request.fuzzy = narrow;
				}
				request.collapse_key = 2;
				request.collapse_max = 1;
				request.percent_threshold = 60;
				request.weight_threshold = 1.0;
				request.order = order;
				agree(fixture, n, some_query(), request);
				agree(fixture, n, Query::match_all(), request);
			}
		}

		// one class of "other" terms per document: the relevant set decides the hits
		engine::Similar classes;
		classes.n_eset = 10;
		classes.n_term = 10;
		classes.prefixes = {"other"};
		const auto hits_of = [&](engine::SearchRequest request) {
			request.maxitems = 100;
			request.check_at_least = 100;
			request.nearest = classes;
			return agree(fixture, n, some_query(), request);
		};
		engine::SearchRequest request2;
		classes.n_rset = 3;
		request2.order = engine::DocidOrder::descending;
		hits_of(request2);
		// equal weights: the ascending order picks the relevant documents
		request2.maxitems = request2.check_at_least = 100;
		request2.nearest = classes;
		CHECK_TRUE(agree(fixture, n, Query::match_all(), request2) > 0);
		request2 = {};
		request2.collapse_key = 2;
		request2.collapse_max = 1;
		hits_of(request2);
		classes.n_rset = 30;
		request2 = {};
		request2.percent_threshold = 90;
		hits_of(request2);
		request2 = {};
		request2.weight_threshold = 4.0;
		hits_of(request2);
	}
}

void test_empty_and_errors() {
	Fixture fixture;
	engine::SearchRequest request;
	request.maxitems = 10;
	// a query that matches nothing finds nothing, without a shard
	auto nothing = engine::prepare(Query());
	CHECK_TRUE(nothing->empty());
	request.query = nothing.get();
	auto result = engine::sync_wait(engine::search(std::vector<engine::Shard*>{fixture.a.get()}, request));
	CHECK_EQ(result.hits.size(), 0u);
	CHECK_EQ(result.estimated, 0u);
	auto all = engine::prepare(Query::match_all());
	CHECK_FALSE(all->empty());
	CHECK_TRUE(!all->description().empty());
	// no shards find nothing
	request.query = all.get();
	result = engine::sync_wait(engine::search(std::span<engine::Shard* const>{}, request));
	CHECK_EQ(result.hits.size(), 0u);
	CHECK_EQ(result.estimated, 0u);
	// a shard of another engine is refused
	Foreign foreign;
	bool refused = false;
	try {
		engine::sync_wait(engine::search(std::vector<engine::Shard*>{&foreign}, request));
	} catch (const std::bad_cast&) {
		refused = true;
	}
	CHECK_TRUE(refused);
	// a wildcard that expands past its limit fails the search with Xapian's error
	bool failed = false;
	auto wild = engine::prepare(Query::wildcard("t", 1, engine::Expansion::error));
	request.query = wild.get();
	try {
		engine::sync_wait(engine::search(std::vector<engine::Shard*>{fixture.a.get()}, request));
	} catch (const Xapian::Error&) {
		failed = true;
	}
	CHECK_TRUE(failed);
}

// Everything a search makes is destroyed, whether it finds, fails or finds nothing.
void test_search_does_not_leak() {
	Fixture fixture;
	CHECK_EQ(engine::xapian::live_search_objects(), 0u);
	{
		auto held = engine::prepare(some_query());
		CHECK_EQ(engine::xapian::live_search_objects(), 1u);
	}
	CHECK_EQ(engine::xapian::live_search_objects(), 0u);

	SlotKey key;
	CountingSpy spy;
	engine::Similar similar;
	similar.prefixes = {"X"};
	for (int n : {1, 2}) {
		engine::SearchRequest request;
		request.maxitems = 100;
		request.sorter = &key;
		request.spy = &spy;
		request.check_at_least = 100;
		request.nearest = similar;
		request.fuzzy = similar;
		run(fixture, n, some_query(), request);
		CHECK_EQ(engine::xapian::live_search_objects(), 0u);
		request = {};
		run(fixture, n, Query(), request);
		CHECK_EQ(engine::xapian::live_search_objects(), 0u);
	}

	// while the prepared query is held it alone is alive; a finished search adds nothing
	auto prepared = engine::prepare(some_query());
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 10;
	request.nearest = similar;
	request.fuzzy = similar;
	auto shards = fixture.shards(2);
	engine::sync_wait(engine::search(shards, request));
	CHECK_EQ(engine::xapian::live_search_objects(), 1u);

	// a search cut short by a callback that throws leaves nothing behind, and the error arrives as thrown
	ThrowingKey bad_key;
	request.nearest.reset();
	request.fuzzy.reset();
	request.sorter = &bad_key;
	bool thrown = false;
	try {
		engine::sync_wait(engine::search(shards, request));
	} catch (const std::runtime_error& error) {
		thrown = std::string(error.what()) == "key";
	}
	CHECK_TRUE(thrown);
	CHECK_EQ(engine::xapian::live_search_objects(), 1u);

	CountingSpy bad_spy;
	bad_spy.throws = true;
	request.sorter = nullptr;
	request.spy = &bad_spy;
	request.check_at_least = 100;
	thrown = false;
	try {
		engine::sync_wait(engine::search(shards, request));
	} catch (const std::runtime_error& error) {
		thrown = std::string(error.what()) == "spy";
	}
	CHECK_TRUE(thrown);
	CHECK_EQ(engine::xapian::live_search_objects(), 1u);

	// the shard is still usable afterwards
	request.spy = nullptr;
	CHECK_TRUE(engine::sync_wait(engine::search(shards, request)).hits.size() > 0);
	prepared.reset();
	CHECK_EQ(engine::xapian::live_search_objects(), 0u);
}

// The result holds no engine resource: it outlives the shards that produced it.
void test_result_outlives_shards() {
	engine::SearchResult result;
	{
		Fixture fixture;
		engine::SearchRequest request;
		request.maxitems = 5;
		result = run(fixture, 2, some_query(), request);
	}
	CHECK_EQ(result.hits.size(), 5u);
	CHECK_TRUE(result.hits[0].did > 0);
	CHECK_EQ(engine::xapian::live_search_objects(), 0u);
}

}  // namespace

TEST_MAIN(test_ranking_and_window, test_sort_collapse_cutoff_order, test_spy, test_spy_check_all, test_similar, test_similar_relevant_set, test_empty_and_errors, test_search_does_not_leak, test_result_outlives_shards)
