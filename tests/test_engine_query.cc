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

// Unit tests for the engine seam's Query and callbacks: the shape a query keeps, what it matches
// on a strata shard, and how the host callbacks (sources, key makers, spies) behave in a search.

#include "test_harness.h"

#include <algorithm>
#include <cstdlib>

#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <pthread.h>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/text.h"
#include "engine_support.h"
#include "exception_engine.h"
#include "length.h"
#include "multivalue/geospatialrange.h"
#include "msgpack.h"
#include "multivalue/range.h"
#include "query_dsl.h"
#include "serialise.h"
#include "serialise_list.h"

namespace {

// The stop words are read from the source tree.
const int data_path = (::setenv("XAPIAND_DATA_PATH", XAPIAND_SOURCE_DIR, 1), 0);

using engine::Op;
using engine::Query;

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

constexpr Op binary_ops[] = { Op::AND, Op::OR, Op::AND_NOT, Op::XOR, Op::AND_MAYBE, Op::FILTER };

// The ids a query finds in a shard (at most 20), in rank order.
std::vector<engine::docid> ranked(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter = nullptr, engine::MatchSpy* spy = nullptr) {
	testsupport::Find find;
	find.maxitems = 20;
	find.check_at_least = 20;
	find.sorter = sorter;
	find.spy = spy;
	return testsupport::hit_ids(shard, query, find);
}

std::set<engine::docid> run(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter = nullptr, engine::MatchSpy* spy = nullptr) {
	const auto dids = ranked(shard, query, sorter, spy);
	return std::set<engine::docid>(dids.begin(), dids.end());
}

// One document per term, in the field "f".
std::unique_ptr<engine::Shard> terms_db(const char* name, std::initializer_list<const char*> terms) {
	auto shard = testsupport::scratch_shard(name);
	int n = 0;
	for (auto term : terms) {
		engine::Document doc;
		doc.add_term(doc.field("f"), term);
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(++n));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

}  // namespace

static void test_leaves_and_ops() {
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
static void test_emptiness() {
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

static void test_expansions() {
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

// Xapiand's own value sources find the documents their range names.
static void test_registered_sources() {
	auto shard = testsupport::scratch_shard("engine_query_sources");
	for (int i = 10; i <= 30; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		doc.add_value(5, "0" + std::to_string(i));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));

	// The sources are built from their serialised form: their constructors are private to range.cc.
	auto serialised = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialise(data.begin(), data.end());
	};
	auto range = [&](std::string from, std::string to) { return MultipleValueRange{}.unserialise(serialised({ serialise_length(5), from, to })); };
	auto ge = [&](std::string from) { return MultipleValueGE{}.unserialise(serialised({ serialise_length(5), from })); };
	auto le = [&](std::string to) { return MultipleValueLE{}.unserialise(serialised({ serialise_length(5), to })); };
	const std::vector<std::pair<Query, std::set<engine::docid>>> cases = {
		{ Query::source(range("015", "018")), { 6, 7, 8, 9 } },
		{ Query::source(ge("028")), { 19, 20, 21 } },
		{ Query::source(le("012")), { 1, 2, 3 } },
		{ Query::combine(Op::FILTER, Query::source(range("015", "025")), Query::term("f", "t")), { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } },
	};
	for (const auto& [query, expected] : cases) {
		CHECK_TRUE(run(*shard, query) == expected);
	}
}

// Bytes a source never wrote: random, and valid serialisations with a few bytes flipped, cut or
// extended. Each source must either rebuild or raise the engine's invalid error, and never
// anything else.
static void test_unserialise_garbage() {
	auto serialised = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialise(data.begin(), data.end());
	};
	const std::vector<std::pair<const engine::PostingSource*, std::string>> seeds = {
		{ new MultipleValueRange{}, serialised({ serialise_length(5), "015", "018" }) },
		{ new MultipleValueGE{}, serialised({ serialise_length(5), "015" }) },
		{ new MultipleValueLE{}, serialised({ serialise_length(5), "015" }) },
		{ new GeoSpatialRange{}, serialised({ serialise_length(7), Serialise::ranges_centroids({ range_t(1, 9), range_t(20, 30) }, { Cartesian(1, 0, 0) }) }) },
	};
	Lcg rng{777};
	std::size_t rebuilt = 0, rejected = 0;
	for (const auto& [source, seed] : seeds) {
		for (int i = 0; i < 4000; ++i) {
			std::string bytes;
			switch (rng.next(4)) {
				case 0:
					for (auto n = rng.next(40); n > 0; --n) {
						bytes.push_back(static_cast<char>(rng.next(256)));
					}
					break;
				case 1:
					bytes = seed;
					for (auto n = rng.next(4) + 1; n > 0 && !bytes.empty(); --n) {
						bytes[rng.next(static_cast<std::uint32_t>(bytes.size()))] = static_cast<char>(rng.next(256));
					}
					break;
				case 2:
					bytes = seed.substr(0, rng.next(static_cast<std::uint32_t>(seed.size() + 1)));
					break;
				default:
					bytes = seed;
					for (auto n = rng.next(8); n > 0; --n) {
						bytes.push_back(static_cast<char>(rng.next(256)));
					}
					break;
			}
			try {
				auto rebuilt_source = source->unserialise(bytes);
				CHECK_TRUE(rebuilt_source != nullptr);
				// What rebuilt is usable: it describes and serialises without raising.
				(void)rebuilt_source->description();
				(void)rebuilt_source->serialise();
				++rebuilt;
			} catch (const engine::Error& exc) {
				CHECK_TRUE(exc.kind == engine::errc::invalid);
				++rejected;
			} catch (...) {
				CHECK_TRUE(false);
			}
		}
		delete source;
	}
	CHECK_TRUE(rebuilt > 100 && rejected > 100);
}

// What a wildcard does past its limit is the one thing a description does not show: run it.

// What a wildcard does past its limit is the one thing a description does not show: run it.
static void test_expansion_limits() {
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
static void test_append() {
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
static void test_text() {
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

namespace {

// Documents whose slot-21 value holds a number in [lo, hi]; the weight is the number.
class Between final : public engine::PostingSource {
public:
	Between() = default;
	Between(int lo, int hi) : lo_(lo), hi_(hi) { }

	engine::valueno slot() const override { return 21; }
	bool accept(std::string_view value) const override { return !value.empty() && std::stoi(std::string(value)) >= lo_ && std::stoi(std::string(value)) <= hi_; }
	double weight(std::string_view value) const override { return std::stoi(std::string(value)); }
	double max_weight() const override { return 100; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Between>(lo_, hi_); }
	std::string name() const override { return "Between"; }
	std::string serialise() const override { return std::to_string(lo_) + "," + std::to_string(hi_); }
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view s) const override {
		auto comma = s.find(',');
		if (comma == std::string_view::npos) {
			throw engine::Error(engine::errc::invalid, "Bad serialised Between");
		}
		return std::make_unique<Between>(std::stoi(std::string(s.substr(0, comma))), std::stoi(std::string(s.substr(comma + 1))));
	}
	std::string description() const override { return "Between " + serialise(); }

private:
	int lo_ = 0, hi_ = 0;
};

// Orders documents by the slot-21 value, descending.
class ByValue final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView& doc) const override {
		auto value = doc.value(21);
		for (auto& ch : value) {
			ch = static_cast<char>(0xff - static_cast<unsigned char>(ch));
		}
		return value;
	}
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<ByValue>(); }
	std::string name() const override { return "ByValue"; }
	std::string serialise() const override { return "v"; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view s) const override {
		if (s != "v") {
			throw engine::Error(engine::errc::invalid, "Bad serialised ByValue");
		}
		return std::make_unique<ByValue>();
	}
};

// Sums the weights it sees and remembers the data of the documents.
class Collect final : public engine::MatchSpy {
public:
	void see(const engine::DocView& doc, double weight) override {
		seen.insert(doc.did());
		total_weight += weight;
		data += doc.data();
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<Collect>(); }
	std::string name() const override { return "Collect"; }
	std::string serialise() const override { return "c"; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view s) const override {
		if (s != "c") {
			throw engine::Error(engine::errc::invalid, "Bad serialised Collect");
		}
		return std::make_unique<Collect>();
	}
	std::string serialise_results() const override {
		std::string result;
		for (auto did : seen) {
			result += std::to_string(did) + ",";
		}
		return result;
	}
	void merge_results(std::string_view s) override {
		std::string number;
		for (char ch : s) {
			if (ch == ',') {
				seen.insert(static_cast<engine::docid>(std::stoul(number)));
				number.clear();
			} else {
				number += ch;
			}
		}
	}
	std::string description() const override { return "Collect"; }

	std::set<engine::docid> seen;
	double total_weight = 0;
	std::string data;
};

// Ten documents with the term "t", the numbers 1 to 10 in slot 21 and "d1" to "d10" as data.
std::unique_ptr<engine::Shard> make_db() {
	auto shard = testsupport::scratch_shard("engine_query_ten");
	for (int i = 1; i <= 10; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		doc.add_value(21, std::to_string(i));
		doc.set_data("d" + std::to_string(i));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

}  // namespace

static void test_posting_source() {
	auto db = make_db();

	// The search accepts exactly what the source accepts, through every match path.
	const auto query = Query::source(std::make_unique<Between>(3, 6));
	auto dids = ranked(*db, query);
	CHECK_EQ(dids.size(), 4U);
	CHECK_TRUE((std::set<engine::docid>(dids.begin(), dids.end()) == std::set<engine::docid>{ 3, 4, 5, 6 }));

	// A filter: skip_to and check run when the source meets another term.
	CHECK_TRUE((run(*db, Query::combine(Op::FILTER, Query::term("f", "t"), Query::source(std::make_unique<Between>(8, 9)))) == std::set<engine::docid>{ 8, 9 }));
	CHECK_TRUE((run(*db, Query::combine(Op::AND, Query::source(std::make_unique<Between>(2, 9)), Query::source(std::make_unique<Between>(5, 20)))) == std::set<engine::docid>{ 5, 6, 7, 8, 9 }));

	// The weight is the source's: the highest value ranks first.
	CHECK_EQ(dids.front(), 6U);

	// A malformed serialisation is an invalid argument; a good one rebuilds the source.
	CHECK_THROW(Between().unserialise("bad"), engine::Error);
	CHECK_EQ(Between().unserialise("3,6")->description(), "Between 3,6");
	CHECK_EQ(Between().name(), "Between");
	CHECK_EQ(Between().description(), "Between 0,0");
}

static void test_key_maker() {
	auto db = make_db();
	ByValue sorter;
	auto dids = ranked(*db, Query::term("f", "t"), &sorter);
	CHECK_EQ(dids.size(), 10U);
	// Descending by the number's text: "9", "8", ... "2", then "1" before its extension "10".
	CHECK_EQ(dids.front(), 9U);
	CHECK_EQ(dids.back(), 10U);

	auto rebuilt = sorter.unserialise("v");
	CHECK_TRUE(ranked(*db, Query::term("f", "t"), rebuilt.get()) == dids);
	CHECK_THROW(sorter.unserialise("x"), engine::Error);
}

static void test_match_spy() {
	auto db = make_db();
	Collect collect;
	auto dids = ranked(*db, Query::term("f", "t"), nullptr, &collect);
	CHECK_EQ(dids.size(), 10U);
	CHECK_EQ(collect.seen.size(), 10U);
	CHECK_EQ(collect.data, "d1d2d3d4d5d6d7d8d9d10");
	CHECK_TRUE(collect.total_weight > 0);

	// Results of other shards merge through the serialised form.
	auto remote = collect.unserialise("c");
	auto remote_dids = ranked(*db, Query::combine(Op::AND, Query::term("f", "t"), Query::source(std::make_unique<Between>(9, 10))), nullptr, remote.get());
	CHECK_EQ(remote_dids.size(), 2u);
	Collect merged;
	merged.merge_results(remote->serialise_results());
	CHECK_EQ(merged.seen.size(), remote_dids.size());
	CHECK_TRUE((merged.seen == std::set<engine::docid>{ 9, 10 }));
	CHECK_THROW(collect.unserialise("x"), engine::Error);
}

namespace {

// Counts its live instances, to see whether a search frees what it makes.
class Counted final : public engine::PostingSource {
public:
	static inline int live = 0;

	Counted() { ++live; }
	Counted(const Counted&) = delete;
	~Counted() override { --live; }

	engine::valueno slot() const override { return 21; }
	bool accept(std::string_view value) const override { return !value.empty(); }
	double weight(std::string_view) const override { return 1.0; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Counted>(); }
	std::string name() const override { return "Counted"; }
	std::string serialise() const override { return "c"; }
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view) const override { return std::make_unique<Counted>(); }
	std::string description() const override { return "Counted"; }
};

}  // namespace

// A search hands each shard its own clone of the source and frees it at the end: only the query's
// source stays alive.
static void test_source_ownership() {
	auto db = make_db();
	Counted::live = 0;
	{
		auto query = Query::source(std::make_unique<Counted>());
		CHECK_EQ(Counted::live, 1);
		for (int i = 0; i < 20; ++i) {
			auto copy = query;
			CHECK_EQ(run(*db, copy).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);

		auto combined = Query::combine(Op::FILTER, Query::term("f", "t"), query);
		for (int i = 0; i < 20; ++i) {
			CHECK_EQ(run(*db, combined).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);
	}
	CHECK_EQ(Counted::live, 0);
}

// A pattern's '?' and '*' count only when the pattern flags say so.
static void test_wildcard_patterns() {
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
static void test_fuzzy_limit() {
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
static void test_binary_equals_list() {
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

// A chain of appends (one per key of a flat query object) costs no stack and keeps its meaning.
namespace {

struct ChainJob {
	std::size_t length = 0;
	bool done = false;
	std::string description;
};

void* chain_run(void* arg) {
	auto* job = static_cast<ChainJob*>(arg);
	{
		Query chain;
		for (std::size_t i = 0; i < job->length; ++i) {
			// Every 7th operand matches nothing, as a stop-word-only text does.
			chain = Query::append(Op::AND, chain, i % 7 == 6 ? Query() : Query::term("", "t" + std::to_string(i)));
		}
		job->description = chain.description().substr(0, 40);
		job->done = true;
	}
	return nullptr;
}

}  // namespace

static void test_append_chain() {
	// 600 operands on a 256 KB stack: a frame per level would not fit.
	ChainJob job;
	job.length = 600;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 256 * 1024);
	pthread_t thread;
	CHECK_EQ(pthread_create(&thread, &attr, chain_run, &job), 0);
	pthread_join(thread, nullptr);
	pthread_attr_destroy(&attr);
	CHECK_TRUE(job.done);

	// The same chain, short, is the left fold of its operands: an operand that matches nothing leaves an
	// AND with a query that matches nothing, from which the next operand starts again.
	auto shard = testsupport::scratch_shard("engine_query_fold");
	for (int k = 0; k < 12; ++k) {
		// Each of twelve documents has every term but its own.
		engine::Document doc;
		for (int j = 0; j < 12; ++j) {
			if (j != k) {
				doc.add_term(doc.field("f"), "t" + std::to_string(j));
			}
		}
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(k));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	for (const Op op : { Op::AND, Op::OR }) {
		Query chain;
		Query expected;
		for (int i = 0; i < 12; ++i) {
			auto operand = i % 5 == 4 ? Query() : Query::term("f", "t" + std::to_string(i));
			chain = Query::append(op, chain, operand);
			expected = expected.empty() ? operand : Query::combine(op, expected, operand);
		}
		CHECK_EQ(chain.empty(), expected.empty());
		CHECK_TRUE(run(*shard, chain) == run(*shard, expected));
		// AND: the empty operands 4 and 9 cut the chain, so only t10 AND t11 remain; OR skips the empty ones.
		CHECK_EQ(run(*shard, chain).size(), op == Op::AND ? 10u : 12u);
	}
}

// A tree as deep as the cap converts and describes itself on a small stack, and one level more is a 400.
namespace {

struct DepthJob {
	bool done = false;
	std::string description;
	bool nonempty = false;
};

void* depth_run(void* arg) {
	auto* job = static_cast<DepthJob*>(arg);
	Query tree = Query::term("", "a");
	for (unsigned level = 1; level < engine::MAX_QUERY_DEPTH; ++level) {
		// Alternating operators and a scale: nothing is flattened into a wide node.
		tree = level % 5 == 0 ? Query::scale(tree, 2.0) : Query::combine(level % 2 ? Op::AND : Op::OR, tree, Query::term("", "b"));
	}
	job->description = tree.description();
	job->nonempty = !tree.empty();
	job->done = true;
	return nullptr;
}

}  // namespace

static void test_depth_cap() {
	// 40 KB of stack: a frame per level of a tree at the cap would not fit.
	DepthJob job;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 40 * 1024);
	pthread_t thread;
	CHECK_EQ(pthread_create(&thread, &attr, depth_run, &job), 0);
	pthread_join(thread, nullptr);
	pthread_attr_destroy(&attr);
	CHECK_TRUE(job.done);
	CHECK_TRUE(job.description.size() > engine::MAX_QUERY_DEPTH);
	CHECK_TRUE(job.nonempty);

	// Exactly the cap builds; one more level of any operator throws a client error.
	Query tree = Query::term("", "a");
	for (unsigned level = 1; level < engine::MAX_QUERY_DEPTH; ++level) {
		tree = Query::combine(Op::AND, tree, Query::term("", "b"));
	}
	CHECK_EQ(tree.depth(), engine::MAX_QUERY_DEPTH);
	const auto refused = [](auto&& build, const char* message = "nested too deeply") {
		try {
			build();
		} catch (const QueryDepthError& exc) {
			return std::string(exc.what()).find(message) != std::string::npos && dynamic_cast<const ClientError*>(&exc) != nullptr;
		}
		return false;
	};
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, tree, Query::term("", "b")); }));
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, std::vector<Query>{ tree }, 0); }));
	CHECK_TRUE(refused([&] { Query::scale(tree, 2.0); }));

	// An append adds no level: a flat object with many keys is not nested.
	Query flat;
	for (unsigned i = 0; i < 4 * engine::MAX_QUERY_DEPTH; ++i) {
		flat = Query::append(Op::AND, flat, Query::term("", "t" + std::to_string(i)));
	}
	CHECK_EQ(flat.depth(), 1U);
	CHECK_EQ(Query::append(Op::AND, tree, Query::term("", "c")).depth(), engine::MAX_QUERY_DEPTH);

	// The clause cap: a node per term and one for the operator, appends and shared subtrees included.
	std::vector<Query> wide(engine::MAX_QUERY_CLAUSES - 1, Query::term("", "t"));
	CHECK_EQ(Query::combine(Op::OR, wide).size(), engine::MAX_QUERY_CLAUSES);
	wide.push_back(Query::term("", "t"));
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, wide); }, "too large"));
	Query chain;
	for (unsigned i = 0; i < engine::MAX_QUERY_CLAUSES; ++i) {
		chain = Query::append(Op::AND, chain, Query::term("", "t"));
	}
	CHECK_EQ(chain.size(), engine::MAX_QUERY_CLAUSES);
	CHECK_TRUE(refused([&] { Query::append(Op::AND, chain, Query::term("", "t")); }, "too large"));
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, chain, chain); }, "too large"));
	CHECK_FALSE(flat.empty());
}

// The matcher recurses once per phrase or near group an AND joins, so the query counts the
// links it would chain: AND and FILTER add their operands' up, every other operator takes the deepest.
static void test_chain_cap() {
	const auto phrase = [](const char* a, const char* b, Op op = Op::PHRASE) { return Query::combine(op, std::vector<Query>{ Query::term("", a), Query::term("", b) }, 2); };
	const auto many = [&](unsigned n, Op op = Op::PHRASE) {
		std::vector<Query> list;
		for (unsigned i = 0; i < n; ++i) {
			list.push_back(phrase("a", "b", op));
		}
		return list;
	};
	const auto refused = [](auto&& build) {
		try {
			build();
		} catch (const QueryDepthError& exc) {
			return std::string(exc.what()).find("too many phrases") != std::string::npos && dynamic_cast<const ClientError*>(&exc) != nullptr;
		}
		return false;
	};
	const unsigned cap = engine::MAX_QUERY_CHAIN;
	CHECK_EQ(Query::term("", "a").chain(), 0U);
	CHECK_EQ(phrase("a", "b").chain(), 1U);
	CHECK_EQ(phrase("a", "b", Op::NEAR).chain(), 1U);
	CHECK_EQ(Query::combine(Op::AND, many(cap)).chain(), cap);
	CHECK_EQ(Query::combine(Op::FILTER, many(cap)).chain(), cap);
	for (const auto op : { Op::AND, Op::FILTER }) {
		CHECK_TRUE(refused([&] { Query::combine(op, many(cap + 1)); }));
		CHECK_TRUE(refused([&] { Query::combine(op, many(cap + 1, Op::NEAR)); }));
	}
	// Near groups and phrases are counted together; terms and groups of terms are not links.
	auto mixed = many(cap / 2);
	for (auto& near : many(cap / 2, Op::NEAR)) {
		mixed.push_back(std::move(near));
	}
	CHECK_EQ(Query::combine(Op::AND, mixed).chain(), cap);
	mixed.push_back(phrase("c", "d"));
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, mixed); }));
	std::vector<Query> terms(1000, Query::term("", "t"));
	terms.push_back(Query::combine(Op::OR, terms));
	CHECK_EQ(Query::combine(Op::AND, terms).chain(), 0U);

	// Other operators run their operands apart: an OR of thousands of phrases is no deeper than one.
	for (const auto op : { Op::OR, Op::XOR, Op::AND_MAYBE, Op::AND_NOT, Op::MAX }) {
		CHECK_EQ(Query::combine(op, many(1000), 5).chain(), 1U);
	}
	CHECK_EQ(Query::combine(Op::AND_NOT, Query::combine(Op::AND, many(cap)), Query::combine(Op::AND, many(cap))).chain(), cap);

	// Operands pass their links up through AND, however they are wrapped.
	const auto wrapped = [&](unsigned n) {
		std::vector<Query> list;
		for (unsigned i = 0; i < n; ++i) {
			list.push_back(i % 3 == 0 ? Query::combine(Op::AND, phrase("a", "b"), Query::term("", "c"))
				: i % 3 == 1 ? Query::combine(Op::AND_NOT, phrase("a", "b"), Query::term("", "z"))
				: Query::combine(Op::OR, std::vector<Query>{ phrase("a", "b") }, 0));
		}
		return list;
	};
	CHECK_EQ(Query::combine(Op::AND, wrapped(cap)).chain(), cap);
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, wrapped(cap + 1)); }));
	// An OR between ANDs does not hide the links below it, and sibling ORs do not add up.
	const auto half = Query::combine(Op::AND, many(cap / 2));
	CHECK_EQ(Query::combine(Op::AND, Query::combine(Op::OR, half, Query::term("", "c")), half).chain(), cap);
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, Query::combine(Op::OR, half, Query::term("", "c")), Query::combine(Op::AND, many(cap / 2 + 1))); }));
	CHECK_EQ(Query::combine(Op::OR, half, half).chain(), cap / 2);
	// A chain of appends (the text path) adds up as an AND does.
	Query chain;
	for (unsigned i = 0; i < cap; ++i) {
		chain = Query::append(Op::AND, chain, phrase("a", "b"));
	}
	CHECK_EQ(chain.chain(), cap);
	CHECK_TRUE(refused([&] { Query::append(Op::AND, chain, phrase("a", "b")); }));
	CHECK_EQ(Query::append(Op::OR, chain, phrase("a", "b")).chain(), cap);
	// A scale passes its operand's links through.
	CHECK_EQ(Query::scale(Query::combine(Op::AND, many(cap)), 2.0).chain(), cap);
	// Shared subtrees count each time, as they do for the clause cap.
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, Query::combine(Op::AND, many(cap)), phrase("a", "b")); }));
	// The matcher runs a query at the cap.
	auto shard = testsupport::scratch_shard("engine_query_chain");
	engine::Document doc;
	doc.add_posting(doc.field("f"), "a", 1);
	doc.add_posting(doc.field("f"), "b", 2);
	testsupport::add_keyed(*shard, doc, "K1");
	engine::sync_wait(shard->commit(engine::Transaction::none));
	const auto fphrase = [](Op op) {
		return Query::combine(op, std::vector<Query>{ Query::term("f", "a"), Query::term("f", "b") }, 2);
	};
	std::vector<Query> at_cap(cap, fphrase(Op::PHRASE));
	CHECK_EQ(run(*shard, Query::combine(Op::AND, at_cap)).size(), std::size_t(1));
}

// A stored value that does not decode is not accepted; it must never stop the process.
static void test_corrupt_stored_value() {
	auto serialised = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialise(data.begin(), data.end());
	};
	std::vector<std::unique_ptr<engine::PostingSource>> sources;
	sources.push_back(MultipleValueRange{}.unserialise(serialised({ serialise_length(5), "a", "z" })));
	sources.push_back(MultipleValueGE{}.unserialise(serialised({ serialise_length(5), "a" })));
	sources.push_back(MultipleValueLE{}.unserialise(serialised({ serialise_length(5), "z" })));
	sources.push_back(GeoSpatialRange{}.unserialise(serialised({ serialise_length(5), Serialise::ranges_centroids({ range_t(1, 9), range_t(20, 30) }, { Cartesian(1, 0, 0) }) })));

	const std::string garbage_values[] = { "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", "\x7f", std::string("\x05" "ab", 3), std::string("\x01\x00", 2) };
	for (const auto& source : sources) {
		for (const auto& value : garbage_values) {
			(void)source->accept(value);
			(void)source->weight(value);
		}
		Lcg rng{4242};
		for (int i = 0; i < 20000; ++i) {
			std::string bytes;
			for (auto n = rng.next(14) + 1; n > 0; --n) {
				bytes.push_back(static_cast<char>(rng.next(256)));
			}
			(void)source->accept(bytes);
		}
	}
	CHECK_FALSE(sources[0]->accept(garbage_values[2]));
	CHECK_FALSE(sources[1]->accept(garbage_values[2]));
	CHECK_FALSE(sources[3]->accept(garbage_values[2]));

	// A search over a slot with a corrupt value skips that document and finds the others.
	auto shard = testsupport::scratch_shard("engine_query_corrupt");
	for (int i = 0; i < 5; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		std::vector<std::string> one = { std::string(1, static_cast<char>('b' + i)) };
		doc.add_value(5, i == 2 ? garbage_values[2] : StringList::serialise(one.begin(), one.end()));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	auto found = run(*shard, Query::source(MultipleValueGE{}.unserialise(serialised({ serialise_length(5), "a" }))));
	CHECK_TRUE((found == std::set<engine::docid>{ 1, 2, 4, 5 }));
}


namespace {

// A schema of keyword fields "g" and "e", and the (field, term) a query on them finds in a document.
std::shared_ptr<Schema> keyword_schema() {
	MsgPack props = MsgPack::MAP();
	props["g"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1101 } });
	props["e"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1102 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

std::pair<std::string, std::string> term_of(QueryDSL& dsl, const char* field, const char* value) {
	const auto query = dsl.get_query(MsgPack({ { field, value } }));
	// The DSL appends a clause onto the empty query; the term is the appended side.
	auto node = query.node();
	while (node != nullptr && node->kind == engine::QueryKind::append) {
		node = node->subqueries.back().node();
	}
	if (node == nullptr || node->kind != engine::QueryKind::term) {
		++testh::failures;
		std::fprintf(stderr, "  FAIL: %s=%s is not a term query (kind %d)\n", field, value, query.node() ? static_cast<int>(query.node()->kind) : -1);
		return {};
	}
	return { node->field, node->str };
}

struct Person {
	const char* g;
	const char* e;  // nullptr: the document has no "e"
};

constexpr Person people[] = {
	{ "female", "brown" }, { "female", "blue" }, { "female", "green" },
	{ "male", "brown" }, { "male", "blue" }, { "male", "green" },
	{ "female", nullptr },
};

}  // namespace

// `_not` matches the documents that match none of its subqueries, however many there are, and a
// single subquery may be given as an object. Checked by ids against a filter over the same documents.
static void test_dsl_not_is_none_of() {
	auto schema = keyword_schema();
	QueryDSL dsl(schema);
	auto shard = testsupport::scratch_shard("engine_query_dsl_not");
	for (std::size_t i = 0; i < std::size(people); ++i) {
		engine::Document doc;
		auto [gfield, gterm] = term_of(dsl, "g", people[i].g);
		doc.add_term(doc.field(gfield), gterm);
		if (people[i].e != nullptr) {
			auto [efield, eterm] = term_of(dsl, "e", people[i].e);
			doc.add_term(doc.field(efield), eterm);
		}
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));

	const std::pair<const char*, const char*> subqueries[] = { { "g", "female" }, { "e", "brown" }, { "e", "blue" }, { "g", "male" } };
	const auto has = [](const Person& person, const std::pair<const char*, const char*>& sub) {
		const char* value = std::string_view(sub.first) == "g" ? person.g : person.e;
		return value != nullptr && std::string_view(value) == sub.second;
	};
	const auto found = [&](const MsgPack& query) {
		return run(*shard, dsl.get_query(query));
	};

	// Every non-empty subset of the subqueries, in order: N = 1 to 4.
	std::size_t strict = 0;
	for (unsigned mask = 1; mask < (1u << std::size(subqueries)); ++mask) {
		MsgPack list = MsgPack::ARRAY();
		for (std::size_t k = 0; k < std::size(subqueries); ++k) {
			if (mask & (1u << k)) {
				list.push_back(MsgPack({ { subqueries[k].first, subqueries[k].second } }));
			}
		}
		std::set<engine::docid> expected;
		for (std::size_t i = 0; i < std::size(people); ++i) {
			bool any = false;
			for (std::size_t k = 0; k < std::size(subqueries); ++k) {
				any = any || ((mask & (1u << k)) && has(people[i], subqueries[k]));
			}
			if (!any) {
				expected.insert(static_cast<engine::docid>(i + 1));
			}
		}
		strict += !expected.empty() && expected.size() < std::size(people);
		CHECK_TRUE(found(MsgPack({ { "_not", list } })) == expected);
	}
	CHECK_GT(strict, std::size_t(5));

	// The cases of the audit, by value: ids are 1 female/brown, 2 female/blue, 3 female/green, 4 male/brown,
	// 5 male/blue, 6 male/green, 7 female without "e".
	const auto sub = [](const char* field, const char* value) { return MsgPack({ { field, value } }); };
	const auto list_of = [](std::initializer_list<MsgPack> items) {
		MsgPack list = MsgPack::ARRAY();
		for (const auto& item : items) {
			list.push_back(item);
		}
		return list;
	};
	using Ids = std::set<engine::docid>;
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("e", "brown") }) } })) == (Ids{ 5, 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("e", "brown"), sub("e", "blue") }) } })) == (Ids{ 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("g", "male") }) } })).empty());

	// A bare object is the list of one.
	CHECK_TRUE(found(MsgPack({ { "_not", sub("g", "female") } })) == (Ids{ 4, 5, 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", sub("e", "brown") } })) == (Ids{ 2, 3, 5, 6, 7 }));

	// Inside another clause.
	CHECK_TRUE(found(MsgPack({ { "_and", list_of({ MsgPack({ { "_not", list_of({ sub("e", "brown"), sub("e", "blue") }) } }), sub("g", "male") }) } })) == (Ids{ 6 }));
}

// A keyword value of one character is a term of itself: the check for a trailing "~N" fuzzy distance reads
// the second-to-last character only when there is one.
static void test_dsl_one_character_keyword_is_a_term() {
	auto schema = keyword_schema();
	QueryDSL dsl(schema);
	for (const char* value : { "a", "0", "7" }) {
		CHECK_EQ(term_of(dsl, "g", value).second, std::string(value));
	}
}

namespace {

// Slots of the range corpus: one field per type.
constexpr engine::valueno INT_SLOT = 1201, POS_SLOT = 1202, FLT_SLOT = 1203, DATE_SLOT = 1204;

std::shared_ptr<Schema> range_schema() {
	MsgPack props = MsgPack::MAP();
	props["i"] = MsgPack({ { "_type", "integer" }, { "_slot", 1201 } });
	props["p"] = MsgPack({ { "_type", "positive" }, { "_slot", 1202 } });
	props["f"] = MsgPack({ { "_type", "floating" }, { "_slot", 1203 } });
	props["d"] = MsgPack({ { "_type", "date" }, { "_slot", 1204 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

// A range as the request gives it: an end that is not set is open.
template <typename T>
struct Rng {
	std::optional<T> from, to;
};

template <typename T>
MsgPack ranges_of(const std::vector<Rng<T>>& ranges) {
	MsgPack list = MsgPack::ARRAY();
	std::size_t n = 0;
	for (const auto& r : ranges) {
		MsgPack range = MsgPack::MAP();
		range["_key"] = MsgPack("r" + std::to_string(n++));
		if (r.from) {
			range["_from"] = *r.from;
		}
		if (r.to) {
			range["_to"] = *r.to;
		}
		list.push_back(range);
	}
	return list;
}

// The documents counted in each range ("r0", "r1", ...) by a filter over the values; an empty range has no entry.
template <typename T>
std::map<std::string, std::int64_t> brute_counts(const std::vector<Rng<T>>& ranges, const std::vector<T>& values) {
	std::map<std::string, std::int64_t> counts;
	for (std::size_t n = 0; n < ranges.size(); ++n) {
		for (const auto value : values) {
			if ((!ranges[n].from || value >= *ranges[n].from) && (!ranges[n].to || value < *ranges[n].to)) {
				++counts["r" + std::to_string(n)];
			}
		}
	}
	return counts;
}

// The buckets a range aggregation over `field` makes, one document per serialised value.
std::map<std::string, std::int64_t> range_counts(const char* field, const MsgPack& ranges, engine::valueno slot, const std::vector<std::string>& serialised) {
	const auto config = MsgPack({ { "_aggs", { { "r", { { "_range", { { "_field", field }, { "_ranges", ranges } } } } } } } });
	AggregationMatchSpy spy(config, range_schema());
	for (const auto& value : serialised) {
		const std::vector<std::string> one = { value };
		const testsupport::MapView view({ { slot, StringList::serialise(one.begin(), one.end()) } });
		spy.see(view, 1.0);
	}
	std::map<std::string, std::int64_t> counts;
	for (const auto& bucket : spy.get_aggregation().at("_aggregations").at("r")) {
		counts[std::string(bucket.at("_key").str_view())] = bucket.at("_doc_count").as_i64();
	}
	return counts;
}

// The keys of the buckets a range aggregation makes, in the order the aggregation returns them under `sort`.
std::vector<std::string> range_order(const char* field, const MsgPack& ranges, engine::valueno slot, const std::vector<std::string>& serialised, const MsgPack& sort) {
	const auto config = MsgPack({ { "_aggs", { { "r", { { "_range", { { "_field", field }, { "_ranges", ranges }, { "_sort", sort } } } } } } } });
	AggregationMatchSpy spy(config, range_schema());
	for (const auto& value : serialised) {
		const std::vector<std::string> one = { value };
		const testsupport::MapView view({ { slot, StringList::serialise(one.begin(), one.end()) } });
		spy.see(view, 1.0);
	}
	std::vector<std::string> keys;
	for (const auto& bucket : spy.get_aggregation().at("_aggregations").at("r")) {
		keys.emplace_back(bucket.at("_key").str_view());
	}
	return keys;
}

template <typename T, typename Serialiser>
void check_ranges(const char* field, engine::valueno slot, const std::vector<T>& values, const std::vector<std::vector<Rng<T>>>& requests, Serialiser serialise) {
	std::vector<std::string> serialised;
	for (const auto value : values) {
		serialised.push_back(serialise(value));
	}
	for (const auto& request : requests) {
		const auto got = range_counts(field, ranges_of(request), slot, serialised);
		const auto want = brute_counts(request, values);
		if (got != want) {
			std::fprintf(stderr, "  FAIL: ranges over %s differ from the filter:", field);
			for (const auto& r : request) {
				std::fprintf(stderr, " [%s, %s)", r.from ? std::to_string(*r.from).c_str() : "", r.to ? std::to_string(*r.to).c_str() : "");
			}
			std::fprintf(stderr, "\n");
			++testh::failures;
		}
	}
}

}  // namespace

// A range bound left out covers every value on its side, in the field's own type: counts equal a filter's, at
// the extremes of int64 and with negative bounds, for integer, positive, float and date fields.
static void test_range_aggregation_open_bounds() {
	constexpr auto none = std::nullopt;
	constexpr std::int64_t lo = std::numeric_limits<std::int64_t>::min(), hi = std::numeric_limits<std::int64_t>::max(), big = std::int64_t(1) << 62;
	using I = Rng<std::int64_t>;

	const std::vector<std::int64_t> ints = { lo, -big, -5, 0, 3, 10, big, hi };
	check_ranges<std::int64_t>("i", INT_SLOT, ints, {
		{ I{ none, 10 }, I{ 0, none } },
		{ I{ none, 0 } }, { I{ 0, none } }, { I{ -10, none } }, { I{ none, -10 } },
		{ I{ none, 3 }, I{ 3, none } },
		{ I{ 0, 10 } },
		{ I{ none, none } },
		{ I{ lo, none } }, { I{ none, hi } }, { I{ hi, none } }, { I{ none, lo } },
		{ I{ -big, big } }, { I{ lo, hi } },
		{ I{ none, big }, I{ big, none } },
		{ I{ none, -5 }, I{ -5, 3 }, I{ 3, none } },
	}, [](std::int64_t v) { return Serialise::integer(v); });

	// The case of the audit, with its default keys.
	{
		const std::vector<std::int64_t> values = { 3, 10, big, -big };
		MsgPack list = MsgPack::ARRAY();
		MsgPack to = MsgPack::MAP(), from = MsgPack::MAP();
		to["_to"] = 10;
		from["_from"] = 0;
		list.push_back(to);
		list.push_back(from);
		std::vector<std::string> serialised;
		for (auto v : values) {
			serialised.push_back(Serialise::integer(v));
		}
		const auto got = range_counts("i", list, INT_SLOT, serialised);
		CHECK_EQ(got, (std::map<std::string, std::int64_t>{ { "..10", 2 }, { "0..", 3 } }));
	}

	// A bound beyond int64, or fractional, is placed exactly: past the values it is open or empty.
	{
		const std::vector<std::int64_t> values = { lo, -5, 0, 3, 10, hi };
		std::vector<std::string> serialised;
		for (auto v : values) {
			serialised.push_back(Serialise::integer(v));
		}
		const auto count = [&](std::optional<MsgPack> from, std::optional<MsgPack> to) {
			MsgPack range = MsgPack::MAP();
			range["_key"] = MsgPack("r");
			if (from) {
				range["_from"] = *from;
			}
			if (to) {
				range["_to"] = *to;
			}
			MsgPack list = MsgPack::ARRAY();
			list.push_back(range);
			const auto got = range_counts("i", list, INT_SLOT, serialised);
			return got.empty() ? std::int64_t(0) : got.at("r");
		};
		const MsgPack past_max(std::uint64_t(1) << 63), huge(1e30), neg_huge(-1e30);
		CHECK_EQ(count(none, past_max), std::int64_t(6));
		CHECK_EQ(count(past_max, none), std::int64_t(0));
		CHECK_EQ(count(none, huge), std::int64_t(6));
		CHECK_EQ(count(huge, none), std::int64_t(0));
		CHECK_EQ(count(neg_huge, none), std::int64_t(6));
		CHECK_EQ(count(none, neg_huge), std::int64_t(0));
		CHECK_EQ(count(MsgPack(2.5), MsgPack(10.5)), std::int64_t(2));    // 3 and 10
		CHECK_EQ(count(MsgPack(-0.5), MsgPack(3.0)), std::int64_t(1));    // 0
		CHECK_EQ(count(none, MsgPack(-4.5)), std::int64_t(2));            // lo and -5
	}

	// Positive fields: a negative bound is open (from) or empty (to).
	{
		using U = Rng<std::uint64_t>;
		constexpr std::uint64_t ubig = std::uint64_t(1) << 62, ubigger = std::uint64_t(1) << 63;
		const std::vector<std::uint64_t> values = { 0, 3, 10, ubig, ubigger };
		check_ranges<std::uint64_t>("p", POS_SLOT, values, {
			{ U{ none, 10 }, U{ 3, none } },
			{ U{ none, 0 } }, { U{ 0, none } }, { U{ none, none } },
			{ U{ ubig, none } }, { U{ none, ubig } }, { U{ ubigger, none } },
			{ U{ 3, ubig } },
		}, [](std::uint64_t v) { return Serialise::positive(v); });

		std::vector<std::string> serialised;
		for (auto v : values) {
			serialised.push_back(Serialise::positive(v));
		}
		MsgPack below = MsgPack::MAP(), within = MsgPack::MAP(), empty = MsgPack::MAP();
		below["_key"] = MsgPack("below");
		below["_from"] = -5;
		within["_key"] = MsgPack("within");
		within["_from"] = -5;
		within["_to"] = 4;
		empty["_key"] = MsgPack("empty");
		empty["_to"] = -1;
		MsgPack list = MsgPack::ARRAY();
		list.push_back(below);
		list.push_back(within);
		list.push_back(empty);
		const auto got = range_counts("p", list, POS_SLOT, serialised);
		CHECK_EQ(got, (std::map<std::string, std::int64_t>{ { "below", 5 }, { "within", 2 } }));
	}

	// Default keys show the bounds as written, so ranges that differ as written never share a bucket.
	{
		const auto bound = [](const char* name, MsgPack value) {
			MsgPack r = MsgPack::MAP();
			r[name] = value;
			return r;
		};
		const auto both = [](MsgPack from, MsgPack to) {
			MsgPack r = MsgPack::MAP();
			r["_from"] = from;
			r["_to"] = to;
			return r;
		};
		using Counts = std::map<std::string, std::int64_t>;
		const std::vector<std::int64_t> ints = { 1, 3, 5 };
		std::vector<std::string> serialised;
		for (auto v : ints) {
			serialised.push_back(Serialise::integer(v));
		}
		MsgPack list = MsgPack::ARRAY();
		list.push_back(bound("_from", MsgPack(2.5)));
		list.push_back(bound("_from", MsgPack(3)));
		CHECK_EQ(range_counts("i", list, INT_SLOT, serialised), (Counts{ { "2.5..", 2 }, { "3..", 2 } }));

		const std::vector<std::uint64_t> naturals = { 1, 3, 5 };
		serialised.clear();
		for (auto v : naturals) {
			serialised.push_back(Serialise::positive(v));
		}
		list = MsgPack::ARRAY();
		list.push_back(both(MsgPack(-5), MsgPack(4)));
		list.push_back(bound("_to", MsgPack(4)));
		CHECK_EQ(range_counts("p", list, POS_SLOT, serialised), (Counts{ { "-5..4", 2 }, { "..4", 2 } }));
	}

	// Sorted by key, an open lower bound comes first (below every bound), last when descending.
	{
		const std::vector<std::int64_t> values = { -100, -20, 0, 7 };
		std::vector<std::string> serialised;
		for (auto v : values) {
			serialised.push_back(Serialise::integer(v));
		}
		const auto range = [](const char* key, std::optional<int> from, std::optional<int> to) {
			MsgPack r = MsgPack::MAP();
			r["_key"] = MsgPack(key);
			if (from) {
				r["_from"] = *from;
			}
			if (to) {
				r["_to"] = *to;
			}
			return r;
		};
		MsgPack list = MsgPack::ARRAY();
		list.push_back(range("mid", -5, 5));
		list.push_back(range("high", 5, none));
		list.push_back(range("open", none, -50));
		list.push_back(range("neg", -50, -10));
		const auto order = [&](const char* direction) {
			return range_order("i", list, INT_SLOT, serialised, MsgPack({ { "_key", direction } }));
		};
		CHECK_EQ(order("asc"), (std::vector<std::string>{ "open", "neg", "mid", "high" }));
		CHECK_EQ(order("desc"), (std::vector<std::string>{ "high", "mid", "neg", "open" }));
	}

	// Float and date fields: open sides cover the negative values too.
	{
		using F = Rng<double>;
		const std::vector<double> values = { -100.5, -2.5, 0.0, 1.5, 3.0, 1000.25 };
		check_ranges<double>("f", FLT_SLOT, values, {
			{ F{ none, 0.0 }, F{ 0.0, none } },
			{ F{ none, -2.5 } }, { F{ -2.5, none } }, { F{ none, none } }, { F{ -3.0, 1.5 } }, { F{ 1.5, 3.0 } },
		}, [](double v) { return Serialise::floating(static_cast<long double>(v)); });
		const std::vector<double> times = { -1.0e9, -1.0, 0.0, 1.5e9, 4.0e9 };
		check_ranges<double>("d", DATE_SLOT, times, {
			{ F{ none, 0.0 }, F{ 0.0, none } },
			{ F{ none, -1.0 } }, { F{ -1.0, none } }, { F{ none, none } }, { F{ -1.0e9, 1.5e9 } },
		}, [](double v) { return Serialise::timestamp(v); });
	}
}



// An engine error that escapes a test is a failure with its text, not an abort.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const engine::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw engine error %s\n", #fn, exc.reason().c_str()); \
			++testh::failures; \
		} \
	}

GUARDED(test_leaves_and_ops)
GUARDED(test_emptiness)
GUARDED(test_registered_sources)
GUARDED(test_unserialise_garbage)
GUARDED(test_expansions)
GUARDED(test_expansion_limits)
GUARDED(test_append)
GUARDED(test_text)
GUARDED(test_posting_source)
GUARDED(test_key_maker)
GUARDED(test_match_spy)
GUARDED(test_source_ownership)
GUARDED(test_wildcard_patterns)
GUARDED(test_fuzzy_limit)
GUARDED(test_binary_equals_list)
GUARDED(test_append_chain)
GUARDED(test_depth_cap)
GUARDED(test_chain_cap)
GUARDED(test_corrupt_stored_value)
GUARDED(test_dsl_not_is_none_of)
GUARDED(test_dsl_one_character_keyword_is_a_term)
GUARDED(test_range_aggregation_open_bounds)

TEST_MAIN(test_leaves_and_ops_guarded, test_emptiness_guarded, test_registered_sources_guarded, test_unserialise_garbage_guarded, test_expansions_guarded, test_expansion_limits_guarded, test_append_guarded, test_text_guarded, test_posting_source_guarded, test_key_maker_guarded, test_match_spy_guarded, test_source_ownership_guarded, test_wildcard_patterns_guarded, test_fuzzy_limit_guarded, test_binary_equals_list_guarded, test_append_chain_guarded, test_depth_cap_guarded, test_chain_cap_guarded, test_corrupt_stored_value_guarded, test_dsl_not_is_none_of_guarded, test_dsl_one_character_keyword_is_a_term_guarded, test_range_aggregation_open_bounds_guarded)
