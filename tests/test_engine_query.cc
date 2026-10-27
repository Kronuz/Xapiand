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
#include <set>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/xapian/callbacks.h"
#include "engine/xapian/query.h"
#include "snowball_stem.h"
#include "stopper.h"
#include "xapian.h"

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

	// Binary and list forms are kept apart: Xapian builds them differently.
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
		Xapian::QueryParser::stem_strategy strategy = c.strategy == engine::Stem::all_z ? Xapian::QueryParser::STEM_ALL_Z : Xapian::QueryParser::STEM_SOME;
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

// Xapian's exceptions are not std::exception: report them as a failure of the test.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const Xapian::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw %s\n", #fn, exc.get_description().c_str()); \
			++testh::failures; \
		} \
	}

GUARDED(test_leaves_and_ops)
GUARDED(test_expansions)
GUARDED(test_random_trees)
GUARDED(test_append)
GUARDED(test_text)
GUARDED(test_posting_source)
GUARDED(test_key_maker)
GUARDED(test_match_spy)

TEST_MAIN(test_leaves_and_ops_guarded, test_expansions_guarded, test_random_trees_guarded, test_append_guarded, test_text_guarded, test_posting_source_guarded, test_key_maker_guarded, test_match_spy_guarded)
