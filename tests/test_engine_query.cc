/*
 * Copyright (c) 2026 Dubalu LLC
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

// Unit tests for the engine seam's Query and callbacks: a query built through the seam must
// translate to the Xapian query the same Xapian calls build, and the host callbacks must behave
// the same behind their Xapian adapters.

#include "test_harness.h"

#include <cstdint>
#include <pthread.h>
#include <set>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/xapian/callbacks.h"
#include "engine/xapian/query.h"
#include "length.h"
#include "multivalue/geospatialrange.h"
#include "multivalue/range.h"
#include "serialise.h"
#include "serialise_list.h"
#include "engine/xapian/snowball_stem.h"
#include "engine/xapian/stopper.h"
#include "engine/xapian/xapian.h"

namespace {

using engine::Op;
using engine::Query;
using engine::xapian::to_xapian;

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

// A seam query and the Xapian query built by the Xapian calls it stands for.
struct Pair {
	Query seam;
	Xapian::Query xapian;
};

constexpr Op binary_ops[] = { Op::AND, Op::OR, Op::AND_NOT, Op::XOR, Op::AND_MAYBE, Op::FILTER };
constexpr Op list_ops[] = { Op::AND, Op::OR, Op::SYNONYM, Op::MAX, Op::XOR };

Xapian::Query::op xop(Op op) {
	return to_xapian(op);
}

Pair random_pair(Lcg& rng, int depth) {
	switch (rng.next(depth > 0 ? 9 : 3)) {
		case 0: {
			auto term = "t" + std::to_string(rng.next(6));
			auto wqf = rng.next(3) + 1;
			auto pos = rng.next(3);
			return { Query::term(term, wqf, pos), Xapian::Query(term, wqf, pos) };
		}
		case 1:
			return { Query(), Xapian::Query() };
		case 2:
			return { Query::match_all(), Xapian::Query(std::string()) };
		case 3: {
			auto op = binary_ops[rng.next(std::size(binary_ops))];
			auto a = random_pair(rng, depth - 1);
			auto b = random_pair(rng, depth - 1);
			return { Query::combine(op, a.seam, b.seam), Xapian::Query(xop(op), a.xapian, b.xapian) };
		}
		case 4: {
			auto op = list_ops[rng.next(std::size(list_ops))];
			std::vector<Query> seam;
			std::vector<Xapian::Query> xapian;
			for (auto n = rng.next(4); n > 0; --n) {
				auto p = random_pair(rng, depth - 1);
				seam.push_back(p.seam);
				xapian.push_back(p.xapian);
			}
			return { Query::combine(op, seam), Xapian::Query(xop(op), xapian.begin(), xapian.end()) };
		}
		case 5: {
			auto a = random_pair(rng, depth - 1);
			auto factor = 0.5 + rng.next(4);
			return { Query::scale(a.seam, factor), Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, a.xapian, factor) };
		}
		case 6: {
			std::vector<Query> seam;
			std::vector<Xapian::Query> xapian;
			for (auto n = rng.next(5); n > 0; --n) {
				auto p = random_pair(rng, 0);
				seam.push_back(p.seam);
				xapian.push_back(p.xapian);
			}
			auto n = rng.next(4);
			return { Query::combine(Op::ELITE_SET, seam, n), Xapian::Query(Xapian::Query::OP_ELITE_SET, xapian.begin(), xapian.end(), n) };
		}
		case 7: {
			// append skips an accumulated query that matches nothing, once Xapian has simplified it.
			auto a = random_pair(rng, depth - 1);
			auto b = random_pair(rng, depth - 1);
			auto op = binary_ops[rng.next(std::size(binary_ops))];
			return { Query::append(op, a.seam, b.seam), a.xapian.empty() ? b.xapian : Xapian::Query(xop(op), a.xapian, b.xapian) };
		}
		default: {
			auto op = rng.next(2) != 0 ? Op::NEAR : Op::PHRASE;
			std::vector<Query> seam;
			std::vector<Xapian::Query> xapian;
			for (auto n = rng.next(3) + 2; n > 0; --n) {
				auto term = "w" + std::to_string(rng.next(5));
				seam.push_back(Query::term(term));
				xapian.push_back(Xapian::Query(term));
			}
			auto window = static_cast<engine::termcount>(seam.size() + rng.next(4));
			return { Query::combine(op, seam, window), Xapian::Query(xop(op), xapian.begin(), xapian.end(), window) };
		}
	}
}

}  // namespace

static void test_leaves_and_ops() {
	CHECK_EQ(to_xapian(Query::term("a", 2, 3)).get_description(), Xapian::Query("a", 2, 3).get_description());
	CHECK_EQ(to_xapian(Query::match_all()).get_description(), Xapian::Query(std::string()).get_description());
	CHECK_EQ(to_xapian(Query()).get_description(), Xapian::Query().get_description());
	CHECK_TRUE(Query().empty());
	CHECK_FALSE(Query::match_all().empty());
	CHECK_FALSE(Query::term("a").empty());

	// The binary and list forms translate to the same Xapian query (test_binary_equals_list).
	auto a = Query::term("a"), b = Query::term("b"), c = Query::term("c");
	CHECK_EQ(to_xapian(Query::combine(Op::AND, a, b)).get_description(),
		Xapian::Query(Xapian::Query::OP_AND, Xapian::Query("a"), Xapian::Query("b")).get_description());
	const std::vector<Xapian::Query> xs = { Xapian::Query("a"), Xapian::Query("b"), Xapian::Query("c") };
	CHECK_EQ(to_xapian(Query::combine(Op::OR, { a, b, c })).get_description(),
		Xapian::Query(Xapian::Query::OP_OR, xs.begin(), xs.end()).get_description());
	CHECK_EQ(to_xapian(Query::combine(Op::PHRASE, { a, b, c }, 5)).get_description(),
		Xapian::Query(Xapian::Query::OP_PHRASE, xs.begin(), xs.end(), 5).get_description());
	CHECK_EQ(to_xapian(Query::combine(Op::NEAR, { a, b }, 3)).get_description(),
		Xapian::Query(Xapian::Query::OP_NEAR, xs.begin(), xs.begin() + 2, 3).get_description());
	CHECK_EQ(to_xapian(Query::combine(Op::ELITE_SET, { a, b, c }, 2)).get_description(),
		Xapian::Query(Xapian::Query::OP_ELITE_SET, xs.begin(), xs.end(), 2).get_description());
	CHECK_EQ(to_xapian(Query::scale(a, 2.5)).get_description(),
		Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, Xapian::Query("a"), 2.5).get_description());
}

static void test_expansions() {
	CHECK_EQ(to_xapian(Query::wildcard("ab")).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, "ab", 0, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::Query::OP_SYNONYM).get_description());
	CHECK_EQ(to_xapian(Query::wildcard("ab", 50, engine::Expansion::most_frequent, 0, Op::SYNONYM)).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, "ab", 50, Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT, Xapian::Query::OP_SYNONYM).get_description());
	CHECK_EQ(to_xapian(Query::wildcard("a?c*e", 7, engine::Expansion::first, engine::PATTERN_SINGLE | engine::PATTERN_MULTI, Op::OR)).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, "a?c*e", 7, Xapian::Query::WILDCARD_LIMIT_FIRST | Xapian::Query::WILDCARD_PATTERN_SINGLE | Xapian::Query::WILDCARD_PATTERN_MULTI, Xapian::Query::OP_OR).get_description());
	CHECK_EQ(to_xapian(Query::edit_distance("Xabc", 0, engine::Expansion::error, Op::SYNONYM, 2, 3)).get_description(),
		Xapian::Query(Xapian::Query::OP_EDIT_DISTANCE, "Xabc", 0, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::Query::OP_SYNONYM, 2, 3).get_description());
}

namespace {

std::vector<Xapian::docid> run(const Xapian::Database& db, const Xapian::Query& query, Xapian::KeyMaker* sorter = nullptr, Xapian::MatchSpy* spy = nullptr) {
	Xapian::Enquire enquire(db);
	enquire.set_query(query);
	if (sorter) {
		enquire.set_sort_by_key_then_relevance(sorter, false);
	}
	if (spy) {
		enquire.add_matchspy(spy);
	}
	std::vector<Xapian::docid> dids;
	auto mset = enquire.get_mset(0, 20, 20);
	for (auto it = mset.begin(); it != mset.end(); ++it) {
		dids.push_back(*it);
	}
	return dids;
}

}  // namespace

// Xapiand's own value sources cross the remote protocol by name through the registry, and come
// back as the same filter.
static void test_registered_sources() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (int i = 10; i <= 30; ++i) {
		Xapian::Document doc;
		doc.add_term("t");
		doc.add_value(5, "0" + std::to_string(i));
		db.add_document(doc);
	}
	Xapian::Registry registry;
	engine::xapian::register_posting_source(registry, GeoSpatialRange{});
	engine::xapian::register_posting_source(registry, MultipleValueRange{});
	engine::xapian::register_posting_source(registry, MultipleValueGE{});
	engine::xapian::register_posting_source(registry, MultipleValueLE{});

	// The sources are built from their serialised form: their constructors are private to range.cc.
	auto serialised = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialise(data.begin(), data.end());
	};
	auto range = [&](std::string from, std::string to) { return MultipleValueRange{}.unserialise(serialised({ serialise_length(5), from, to })); };
	auto ge = [&](std::string from) { return MultipleValueGE{}.unserialise(serialised({ serialise_length(5), from })); };
	auto le = [&](std::string to) { return MultipleValueLE{}.unserialise(serialised({ serialise_length(5), to })); };
	const std::vector<std::pair<Query, std::set<Xapian::docid>>> cases = {
		{ Query::source(range("015", "018")), { 6, 7, 8, 9 } },
		{ Query::source(ge("028")), { 19, 20, 21 } },
		{ Query::source(le("012")), { 1, 2, 3 } },
		{ Query::combine(Op::FILTER, Query::source(range("015", "025")), Query::term("t")), { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } },
	};
	for (const auto& [query, expected] : cases) {
		auto local = to_xapian(query);
		auto dids = run(db, local);
		CHECK_EQ(std::set<Xapian::docid>(dids.begin(), dids.end()), expected);
		auto remote = Xapian::Query::unserialise(local.serialise(), registry);
		auto remote_dids = run(db, remote);
		CHECK_EQ(std::set<Xapian::docid>(remote_dids.begin(), remote_dids.end()), expected);
	}

	// A registry that does not know the source cannot rebuild the query.
	Xapian::Registry empty_registry;
	bool raised = false;
	try {
		Xapian::Query::unserialise(to_xapian(cases[0].first).serialise(), empty_registry);
	} catch (const Xapian::Error&) {
		raised = true;
	}
	CHECK_TRUE(raised);

	// Bytes the source never wrote are a network error.
	const auto* prototype = registry.get_posting_source("MultipleValueRange");
	CHECK_TRUE(prototype != nullptr);
	bool network_error = false;
	try {
		prototype->unserialise_with_registry("x", registry);
	} catch (const Xapian::NetworkError&) {
		network_error = true;
	}
	CHECK_TRUE(network_error);
}

// Bytes a source never wrote: random, and valid serialisations with a few bytes flipped, cut or
// extended. Each source must either rebuild or raise the engine's invalid error, and never
// anything else (the remote server turns that one error into a network error).
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
static void test_expansion_limits() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	const std::vector<std::vector<int>> docs_of_term = { {}, { 1, 2, 3, 4, 5 }, { 6, 7 }, { 8 }, { 9, 10, 11, 12 } };
	for (int did = 1; did <= 12; ++did) {
		Xapian::Document doc;
		doc.add_term("zz");
		for (std::size_t t = 1; t < docs_of_term.size(); ++t) {
			for (int member : docs_of_term[t]) {
				if (member == did) {
					doc.add_term("ab" + std::to_string(t));
				}
			}
		}
		db.add_document(doc);
	}
	auto matched = [&](engine::Expansion limit) {
		auto dids = run(db, to_xapian(Query::wildcard("ab", 2, limit)));
		return std::set<Xapian::docid>(dids.begin(), dids.end());
	};
	CHECK_EQ(matched(engine::Expansion::first), (std::set<Xapian::docid>{ 1, 2, 3, 4, 5, 6, 7 }));
	CHECK_EQ(matched(engine::Expansion::most_frequent), (std::set<Xapian::docid>{ 1, 2, 3, 4, 5, 9, 10, 11, 12 }));
	bool raised = false;
	try {
		matched(engine::Expansion::error);
	} catch (const Xapian::WildcardError&) {
		raised = true;
	}
	CHECK_TRUE(raised);
}

// The seam simplifies as Xapian does, and translates to the same query, over random trees.
static void test_random_trees() {
	Lcg rng{12345};
	int empties = 0;
	for (int i = 0; i < 2000; ++i) {
		auto pair = random_pair(rng, 3);
		const auto translated = to_xapian(pair.seam);
		CHECK_EQ(translated.get_description(), pair.xapian.get_description());
		CHECK_EQ(pair.seam.empty(), pair.xapian.empty());
		CHECK_EQ(translated.empty(), pair.xapian.empty());
		empties += pair.xapian.empty();
	}
	CHECK_TRUE(empties > 100 && empties < 1900);
}

// append skips an accumulated query that matches nothing only once the text has been read.
static void test_append() {
	engine::TextConfig config;
	config.prefix = "XT";
	config.flags = engine::text::PHRASE;
	auto stops = Query::text("the", config);
	auto word = Query::term("w");
	CHECK_FALSE(stops.empty());
	CHECK_EQ(to_xapian(Query::append(Op::AND, Query(), word)).get_description(), Xapian::Query("w").get_description());
	CHECK_EQ(to_xapian(Query::append(Op::AND, word, Query())).get_description(),
		Xapian::Query(Xapian::Query::OP_AND, Xapian::Query("w"), Xapian::Query()).get_description());
}

static Xapian::Query parse(const std::string& text, Xapian::Query::op default_op, bool set_op, const std::string& stop, const std::string& stem, Xapian::QueryParser::stem_strategy strategy, unsigned flags, const std::string& prefix) {
	Xapian::QueryParser parser;
	if (set_op) {
		parser.set_default_op(default_op);
	}
	if (!stop.empty()) {
		parser.set_stopper(getStopper(stop).get());
	}
	if (!stem.empty()) {
		parser.set_stemmer(makeStem(stem));
		parser.set_stemming_strategy(strategy);
	}
	parser.set_max_expansion(50, Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT, Xapian::QueryParser::FLAG_PARTIAL);
	parser.set_max_expansion(0, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::QueryParser::FLAG_FUZZY);
	return parser.parse_query(text, flags, prefix);
}

static void test_text() {
	struct Case {
		std::string text;
		Op op;
		std::string stop, stem;
		engine::Stem strategy;
		unsigned flags;
	};
	const Case cases[] = {
		{ "hello world", Op::OR, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::BOOLEAN },
		{ "hello AND world OR \"big dog\"", Op::AND, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::BOOLEAN },
		{ "running dogs jumped", Op::AND, "", "english", engine::Stem::some, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::BOOLEAN },
		{ "running dogs jumped", Op::OR, "", "english", engine::Stem::all_z, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "\"Running Dogs\" jumped", Op::AND, "", "english", engine::Stem::some, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "\"Running Dogs\" jumped", Op::AND, "", "english", engine::Stem::all_z, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "\"Running Dogs\" jumped", Op::AND, "", "english", engine::Stem::all, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "\"Running Dogs\" jumped", Op::AND, "", "english", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "+must -never maybe", Op::AND, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "he*", Op::OR, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::WILDCARD },
		{ "hel", Op::OR, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY | engine::text::PARTIAL },
		{ "helo~ wrld~", Op::OR, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "near words here", Op::NEAR, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "a phrase of words", Op::PHRASE, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
		{ "x y z", Op::INVALID, "", "", engine::Stem::none, engine::text::PHRASE | engine::text::LOVEHATE | engine::text::FUZZY },
	};
	for (const auto& c : cases) {
		engine::TextConfig config;
		config.prefix = "XTfield";
		config.default_op = c.op;
		config.flags = c.flags;
		config.stop_language = c.stop;
		config.stem_language = c.stem;
		config.stem = c.strategy;
		config.partial_max_expansion = 50;
		config.fuzzy_max_expansion = 0;

		unsigned xflags = 0;
		struct { unsigned seam, xapian; } map[] = {
			{ engine::text::BOOLEAN, Xapian::QueryParser::FLAG_BOOLEAN }, { engine::text::PHRASE, Xapian::QueryParser::FLAG_PHRASE },
			{ engine::text::LOVEHATE, Xapian::QueryParser::FLAG_LOVEHATE }, { engine::text::WILDCARD, Xapian::QueryParser::FLAG_WILDCARD },
			{ engine::text::PARTIAL, Xapian::QueryParser::FLAG_PARTIAL }, { engine::text::FUZZY, Xapian::QueryParser::FLAG_FUZZY },
		};
		for (auto m : map) {
			if (c.flags & m.seam) {
				xflags |= m.xapian;
			}
		}
		Xapian::QueryParser::stem_strategy strategy = Xapian::QueryParser::STEM_NONE;
		switch (c.strategy) {
			case engine::Stem::none: strategy = Xapian::QueryParser::STEM_NONE; break;
			case engine::Stem::some: strategy = Xapian::QueryParser::STEM_SOME; break;
			case engine::Stem::all: strategy = Xapian::QueryParser::STEM_ALL; break;
			case engine::Stem::all_z: strategy = Xapian::QueryParser::STEM_ALL_Z; break;
		}
		const bool set_op = c.op != Op::INVALID;
		auto expected = parse(c.text, set_op ? xop(c.op) : Xapian::Query::OP_OR, set_op, c.stop, c.stem, strategy, xflags, "XTfield");
		auto got = to_xapian(Query::text(c.text, config));
		CHECK_EQ(got.get_description(), expected.get_description());
		CHECK_FALSE(got.empty() && !expected.empty());
	}

	// Text the parser rejects raises the Xapian error, as the query language did before.
	engine::TextConfig bad;
	bad.prefix = "XT";
	bad.flags = engine::text::BOOLEAN | engine::text::PHRASE;
	bool raised = false;
	try {
		to_xapian(Query::text("AND AND", bad));
	} catch (const Xapian::QueryParserError&) {
		raised = true;
	}
	CHECK_TRUE(raised);
}

namespace {

// Documents whose slot-1 value holds a number in [lo, hi]; the weight is the number.
class Between final : public engine::PostingSource {
public:
	Between() = default;
	Between(int lo, int hi) : lo_(lo), hi_(hi) { }

	engine::valueno slot() const override { return 1; }
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

// Orders documents by the slot-1 value, descending, and counts how often it is asked.
class ByValue final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView& doc) const override {
		auto value = doc.value(1);
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

Xapian::WritableDatabase make_db() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (int i = 1; i <= 10; ++i) {
		Xapian::Document doc;
		doc.add_term("t");
		doc.add_value(1, std::to_string(i));
		doc.set_data("d" + std::to_string(i));
		db.add_document(doc);
	}
	return db;
}

}  // namespace

static void test_posting_source() {
	auto db = make_db();

	// The adapter accepts exactly what the source accepts, through every match path.
	auto query = to_xapian(Query::source(std::make_unique<Between>(3, 6)));
	auto dids = run(db, query);
	CHECK_EQ(dids.size(), 4U);
	CHECK_EQ(std::set<Xapian::docid>(dids.begin(), dids.end()), (std::set<Xapian::docid>{ 3, 4, 5, 6 }));

	// A filter: skip_to and check run when the source meets another term.
	auto filtered = to_xapian(Query::combine(Op::FILTER, Query::term("t"), Query::source(std::make_unique<Between>(8, 9))));
	auto filtered_dids = run(db, filtered);
	CHECK_EQ(std::set<Xapian::docid>(filtered_dids.begin(), filtered_dids.end()), (std::set<Xapian::docid>{ 8, 9 }));
	auto both = to_xapian(Query::combine(Op::AND, Query::source(std::make_unique<Between>(2, 9)), Query::source(std::make_unique<Between>(5, 20))));
	auto both_dids = run(db, both);
	CHECK_EQ(std::set<Xapian::docid>(both_dids.begin(), both_dids.end()), (std::set<Xapian::docid>{ 5, 6, 7, 8, 9 }));

	// The weight is the source's: the highest value ranks first.
	CHECK_EQ(dids.front(), 6U);

	// It crosses the remote protocol by name through a registry.
	Xapian::Registry registry;
	engine::xapian::register_posting_source(registry, Between());
	auto rebuilt = Xapian::Query::unserialise(query.serialise(), registry);
	CHECK_EQ(run(db, rebuilt), dids);

	// A malformed serialisation is a network error, not an engine error, on the Xapian side.
	const auto* prototype = registry.get_posting_source("Between");
	CHECK_TRUE(prototype != nullptr);
	bool network_error = false;
	try {
		prototype->unserialise_with_registry("bad", registry);
	} catch (const Xapian::NetworkError&) {
		network_error = true;
	}
	CHECK_TRUE(network_error);
	CHECK_EQ(prototype->name(), "Between");
	CHECK_EQ(prototype->get_description(), "Between 0,0");
}

static void test_key_maker() {
	auto db = make_db();
	ByValue seam_sorter;
	engine::xapian::KeyMakerAdapter borrowed(seam_sorter);
	auto dids = run(db, Xapian::Query("t"), &borrowed);
	CHECK_EQ(dids.size(), 10U);
	// Descending by the number's text: "9", "8", ... "2", then "1" before its extension "10".
	CHECK_EQ(dids.front(), 9U);
	CHECK_EQ(dids.back(), 10U);

	Xapian::Registry registry;
	engine::xapian::register_key_maker(registry, ByValue());
	const auto* prototype = registry.get_key_maker("ByValue");
	CHECK_TRUE(prototype != nullptr);
	std::unique_ptr<Xapian::KeyMaker> rebuilt(prototype->unserialise("v", registry));
	CHECK_EQ(run(db, Xapian::Query("t"), rebuilt.get()), dids);
	bool network_error = false;
	try {
		prototype->unserialise("x", registry);
	} catch (const Xapian::NetworkError&) {
		network_error = true;
	}
	CHECK_TRUE(network_error);
}

static void test_match_spy() {
	auto db = make_db();
	Collect collect;
	engine::xapian::MatchSpyAdapter adapter(collect);
	auto dids = run(db, Xapian::Query("t"), nullptr, &adapter);
	CHECK_EQ(dids.size(), 10U);
	CHECK_EQ(collect.seen.size(), 10U);
	CHECK_EQ(collect.data, "d1d2d3d4d5d6d7d8d9d10");
	CHECK_TRUE(collect.total_weight > 0);

	// Results of other shards merge through the serialised form.
	Xapian::Registry registry;
	engine::xapian::register_match_spy(registry, Collect());
	const auto* prototype = registry.get_match_spy("Collect");
	CHECK_TRUE(prototype != nullptr);
	std::unique_ptr<Xapian::MatchSpy> remote(prototype->unserialise("c", registry));
	auto remote_dids = run(db, Xapian::Query(Xapian::Query::OP_AND, Xapian::Query("t"), Xapian::Query(Xapian::Query::OP_VALUE_GE, 1, "9")), nullptr, remote.get());
	CHECK_FALSE(remote_dids.empty());
	Collect merged;
	engine::xapian::MatchSpyAdapter merging(merged);
	merging.merge_results(remote->serialise_results());
	CHECK_EQ(merged.seen.size(), remote_dids.size());
	CHECK_EQ(adapter.name(), "Collect");
	CHECK_EQ(adapter.get_description(), "Collect");
}

namespace {

// Counts its live instances, to see whether a translation frees what it makes.
class Counted final : public engine::PostingSource {
public:
	static inline int live = 0;

	Counted() { ++live; }
	Counted(const Counted&) = delete;
	~Counted() override { --live; }

	engine::valueno slot() const override { return 1; }
	bool accept(std::string_view value) const override { return !value.empty(); }
	double weight(std::string_view) const override { return 1.0; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Counted>(); }
	std::string name() const override { return "Counted"; }
	std::string serialise() const override { return "c"; }
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view) const override { return std::make_unique<Counted>(); }
	std::string description() const override { return "Counted"; }
};

}  // namespace

// Translating a source query hands Xapian its own adapter and source; Xapian frees them with the
// query. Only the seam query's source stays alive.
static void test_source_ownership() {
	auto db = make_db();
	Counted::live = 0;
	{
		auto query = Query::source(std::make_unique<Counted>());
		CHECK_EQ(Counted::live, 1);
		for (int i = 0; i < 20; ++i) {
			auto translated = to_xapian(query);
			auto copy = translated;
			CHECK_EQ(run(db, copy).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);

		auto combined = Query::combine(Op::FILTER, Query::term("t"), query);
		for (int i = 0; i < 20; ++i) {
			CHECK_EQ(run(db, to_xapian(combined)).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);

		// Through a registry: the prototype lives with the registry, the rebuilt source with the query.
		{
			Xapian::Registry registry;
			engine::xapian::register_posting_source(registry, Counted());
			for (int i = 0; i < 20; ++i) {
				auto rebuilt = Xapian::Query::unserialise(to_xapian(query).serialise(), registry);
				CHECK_EQ(run(db, rebuilt).size(), 10U);
			}
			CHECK_EQ(Counted::live, 2);
		}
		CHECK_EQ(Counted::live, 1);
	}
	CHECK_EQ(Counted::live, 0);
}

// The adapter tells Xapian what the source says: its bound on the weight, and that a filter may
// accept no document at all.
static void test_adapter_bounds() {
	auto db = make_db();
	engine::xapian::PostingSourceAdapter bounded(std::make_unique<Between>(3, 6));
	CHECK_EQ(bounded.get_maxweight(), 100.0);
	bounded.reset(db, 0);
	CHECK_EQ(bounded.get_maxweight(), 100.0);
	CHECK_EQ(bounded.get_termfreq_min(), 0U);
	CHECK_EQ(bounded.get_termfreq_max(), 10U);

	engine::xapian::PostingSourceAdapter unbounded(std::make_unique<Counted>());
	CHECK_EQ(unbounded.get_maxweight(), 0.0);

	Xapian::Enquire enquire(db);
	enquire.set_query(to_xapian(Query::source(std::make_unique<Between>(3, 6))));
	auto mset = enquire.get_mset(0, 10);
	CHECK_EQ(mset.get_max_possible(), 100.0);
	CHECK_TRUE(mset.get_matches_lower_bound() <= 4U);
}

// A pattern's '?' and '*' reach Xapian only when the pattern flags say so.
static void test_wildcard_patterns() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (auto term : { "alice", "alyce", "alce", "alicce", "bob" }) {
		Xapian::Document doc;
		doc.add_term(term);
		db.add_document(doc);
	}
	auto matched = [&](const std::string& pattern, unsigned flags) {
		auto dids = run(db, to_xapian(Query::wildcard(pattern, 0, engine::Expansion::error, flags)));
		return std::set<Xapian::docid>(dids.begin(), dids.end());
	};
	CHECK_EQ(matched("al?ce", engine::PATTERN_SINGLE), (std::set<Xapian::docid>{ 1, 2 }));
	CHECK_EQ(matched("al?ce", 0), (std::set<Xapian::docid>{}));
	CHECK_EQ(matched("al*e", engine::PATTERN_MULTI), (std::set<Xapian::docid>{ 1, 2, 3, 4 }));
	CHECK_EQ(matched("al*e", 0), (std::set<Xapian::docid>{}));
	CHECK_EQ(matched("al*e", engine::PATTERN_SINGLE), (std::set<Xapian::docid>{}));
	CHECK_EQ(matched("a?*e", engine::PATTERN_SINGLE | engine::PATTERN_MULTI), (std::set<Xapian::docid>{ 1, 2, 3, 4 }));
}

// A fuzzy word that expands past its limit fails the search, where a partial word keeps the most
// frequent terms.
static void test_fuzzy_limit() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (auto term : { "alica", "alicb", "alicc", "alicd", "alice" }) {
		Xapian::Document doc;
		doc.add_term(term);
		db.add_document(doc);
	}
	engine::TextConfig config;
	config.flags = engine::text::FUZZY;
	config.fuzzy_max_expansion = 2;
	bool raised = false;
	try {
		run(db, to_xapian(Query::text("alice~", config)));
	} catch (const Xapian::WildcardError&) {
		raised = true;
	}
	CHECK_TRUE(raised);

	config.fuzzy_max_expansion = 0;
	CHECK_EQ(run(db, to_xapian(Query::text("alice~", config))).size(), 5U);
}

// A binary combine and the same operands as a list translate to the same query, with the same
// matches and weights, so the seam need not keep the two forms apart in the translation.
static void test_binary_equals_list() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (int i = 0; i < 12; ++i) {
		Xapian::Document doc;
		if (i % 2 == 0) { doc.add_term("a", 1); }
		if (i % 3 == 0) { doc.add_term("b", 1 + i % 4); }
		if (i % 4 == 0) { doc.add_term("c", 1); }
		doc.add_term("z");
		db.add_document(doc);
	}
	const std::vector<Query> operands = { Query::term("a"), Query::term("b"), Query::term("nothing"), Query(), Query::match_all(), Query::term("c", 2) };
	auto results = [&](const Query& query) {
		auto translated = to_xapian(query);
		Xapian::Enquire enquire(db);
		enquire.set_query(translated);
		std::vector<std::pair<Xapian::docid, double>> hits;
		auto mset = enquire.get_mset(0, 20);
		for (auto it = mset.begin(); it != mset.end(); ++it) {
			hits.emplace_back(*it, it.get_weight());
		}
		return std::make_pair(translated.get_description(), hits);
	};
	for (auto op : binary_ops) {
		for (const auto& left : operands) {
			for (const auto& right : operands) {
				CHECK_TRUE(results(Query::combine(op, left, right)) == results(Query::combine(op, std::vector<Query>{ left, right })));
			}
		}
	}
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
			chain = Query::append(Op::AND, chain, i % 7 == 6 ? Query() : Query::term("t" + std::to_string(i)));
		}
		job->description = to_xapian(chain).get_description().substr(0, 40);
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

	// The same chain, short, is the left fold Xapiand built before.
	Query chain;
	Xapian::Query expected;
	for (int i = 0; i < 12; ++i) {
		auto operand = i % 5 == 4 ? Query() : Query::term("t" + std::to_string(i));
		auto expected_operand = i % 5 == 4 ? Xapian::Query() : Xapian::Query("t" + std::to_string(i));
		chain = Query::append(i % 2 != 0 ? Op::OR : Op::AND, chain, operand);
		expected = expected.empty() ? expected_operand : Xapian::Query(i % 2 != 0 ? Xapian::Query::OP_OR : Xapian::Query::OP_AND, expected, expected_operand);
	}
	CHECK_EQ(to_xapian(chain).get_description(), expected.get_description());
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
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (int i = 0; i < 5; ++i) {
		Xapian::Document doc;
		doc.add_term("t");
		std::vector<std::string> one = { std::string(1, static_cast<char>('b' + i)) };
		doc.add_value(5, i == 2 ? garbage_values[2] : StringList::serialise(one.begin(), one.end()));
		db.add_document(doc);
	}
	auto found = run(db, to_xapian(Query::source(MultipleValueGE{}.unserialise(serialised({ serialise_length(5), "a" })))));
	CHECK_EQ(std::set<Xapian::docid>(found.begin(), found.end()), (std::set<Xapian::docid>{ 1, 2, 4, 5 }));
}


// Xapian's exceptions are not std::exception: report them as a failure of the test.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const Xapian::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw %s\n", #fn, exc.get_description().c_str()); \
			++testh::failures; \
		} catch (const engine::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw engine error %s\n", #fn, exc.reason().c_str()); \
			++testh::failures; \
		} \
	}

GUARDED(test_leaves_and_ops)
GUARDED(test_registered_sources)
GUARDED(test_unserialise_garbage)
GUARDED(test_expansions)
GUARDED(test_expansion_limits)
GUARDED(test_random_trees)
GUARDED(test_append)
GUARDED(test_text)
GUARDED(test_posting_source)
GUARDED(test_key_maker)
GUARDED(test_match_spy)
GUARDED(test_source_ownership)
GUARDED(test_adapter_bounds)
GUARDED(test_wildcard_patterns)
GUARDED(test_fuzzy_limit)
GUARDED(test_binary_equals_list)
GUARDED(test_append_chain)
GUARDED(test_corrupt_stored_value)

TEST_MAIN(test_leaves_and_ops_guarded, test_registered_sources_guarded, test_unserialise_garbage_guarded, test_expansions_guarded, test_expansion_limits_guarded, test_random_trees_guarded, test_append_guarded, test_text_guarded, test_posting_source_guarded, test_key_maker_guarded, test_match_spy_guarded, test_source_ownership_guarded, test_adapter_bounds_guarded, test_wildcard_patterns_guarded, test_fuzzy_limit_guarded, test_binary_equals_list_guarded, test_append_chain_guarded, test_corrupt_stored_value_guarded)
