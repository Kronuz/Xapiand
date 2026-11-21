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


// Coverage tests for src/query_dsl.cc: each clause of the query DSL builds the query it names (checked as the tree
// the engine receives, with each field printed by its name), a typed field takes only values of its type, an
// accuracy path (`d._year`) searches the term of that accuracy, a range reaches the range query, a boolean
// expression becomes the DSL object it spells, a sort object becomes the keys it names, and a malformed query is
// a QueryDslError that names the clause.

#include "test_coverage_core.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include "database/schema.h"
#include "database/utils.h"
#include "datetime_msgpack.h"
#include "engine/query.h"
#include "engine_support.h"
#include "exception_engine.h"
#include "length.h"
#include "msgpack.h"
#include "query_dsl.h"
#include "serialize.h"
#include "serialize_list.h"
#include "stopper.h"
#include "xapiand_exception.h"

namespace {

using engine::Query;
using engine::QueryKind;
using engine::QueryNode;

// The stop words come from <data path>/stopwords, which an uninstalled test run does not have: this run installs its own
// English list before anything reads one.
const int stop_words = [] {
	namespace fs = std::filesystem;
	const auto root = fs::current_path() / "coverage_core_query_dsl_data";
	fs::remove_all(root);
	fs::create_directories(root / "stopwords");
	std::ofstream(root / "stopwords" / "en.txt") << "a\nthe\nof\n";
	::setenv("XAPIAND_DATA_PATH", root.c_str(), 1);
	preloadStopWords();
	fs::remove_all(root);  // the table is read once
	return 0;
}();

constexpr engine::valueno SLOT_N = 1301, SLOT_F = 1302, SLOT_K = 1303, SLOT_D = 1304;

const char* const NAMES[] = { "k", "kb", "t", "ts", "n", "p", "f", "b", "d", "dt", "tm", "td", "u", "g", "e", "o.x" };

std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	engine::valueno slot = SLOT_N;
	const auto add = [&](const char* name, const char* type, MsgPack extra = MsgPack::MAP()) {
		MsgPack spec = MsgPack({ { "_type", type }, { "_slot", slot++ } });
		for (auto it = extra.begin(); it != extra.end(); ++it) {
			spec[it->str_view()] = it.value();
		}
		props[name] = spec;
	};
	add("n", "integer");
	add("f", "floating");
	add("k", "keyword");
	add("d", "date");
	add("kb", "keyword", MsgPack({ { "_bool_term", true } }));
	add("t", "text");
	add("ts", "text", MsgPack({ { "_language", "en" }, { "_stem_language", "en" } }));
	add("p", "positive");
	add("b", "boolean");
	add("dt", "datetime");
	add("tm", "time");
	add("td", "timedelta");
	add("u", "uuid");
	add("g", "geo");
	props["e"] = MsgPack({ { "_slot", slot++ } });  // a field without a type
	props["o"] = MsgPack::MAP();
	props["o"]["x"] = MsgPack({ { "_type", "keyword" }, { "_slot", slot++ } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

struct Fixture {
	std::shared_ptr<Schema> schema = make_schema();
	QueryDSL dsl{ schema };
	std::vector<std::pair<std::string, std::string>> labels;  // engine field -> printed name, longest first

	Fixture() {
		for (const char* name : NAMES) {
			const auto spc = schema->get_data_field(name, true).first;
			labels.emplace_back(field_name(spc.prefix(), spc.get_ctype()), name);
			labels.emplace_back(std::string(query_field(spc.prefix())), std::string(name) + ".any");
		}
	}

	std::string label(const std::string& field) const {
		for (const auto& [engine_field, name] : labels) {
			if (field == engine_field) {
				return name;
			}
		}
		return "?" + std::to_string(field.size());
	}

	// The tree of a query: an append onto nothing is its next clause; a field prints as its name.
	std::string show(const Query& q) const {
		const auto* n = q.node();
		if (n == nullptr) {
			return "<nothing>";
		}
		switch (n->kind) {
			case QueryKind::term:
				if (n->field.empty() && n->str.empty()) {
					return "<all>";
				}
				return label(n->field) + "/" + n->str + (n->wqf != 1 ? "^" + std::to_string(n->wqf) : "");
			case QueryKind::wildcard:
				return "wildcard(" + label(n->field) + "/" + n->str + (n->parameter ? " max=" + std::to_string(n->parameter) : "") +
				       (n->pattern_flags ? " flags=" + std::to_string(n->pattern_flags) : "") + ")";
			case QueryKind::edit_distance:
				return "edit_distance(" + label(n->field) + "/" + n->str + " d=" + std::to_string(n->distance) + ")";
			case QueryKind::scale:
				return "(" + show(n->subqueries.front()) + " SCALE " + std::to_string(n->factor) + ")";
			case QueryKind::source:
				return n->src->description();
			case QueryKind::append:
				if (n->subqueries[0].node() == nullptr) {
					return show(n->subqueries[1]);
				}
				[[fallthrough]];
			case QueryKind::combine: {
				std::string out = "(";
				for (std::size_t i = 0; i < n->subqueries.size(); ++i) {
					out += (i ? std::string(" ") + engine::to_string(n->op) + " " : std::string()) + show(n->subqueries[i]);
				}
				return out + (n->parameter ? " " + std::to_string(n->parameter) : "") + ")";
			}
		}
		return "?";
	}

	std::string got(const MsgPack& dsl_query) { return show(dsl.get_query(dsl_query)); }

	// A term of `name` for `bytes`, as it prints.
	static std::string term(const std::string& name, const std::string& bytes) { return name + "/" + bytes; }
};

MsgPack obj(std::initializer_list<std::pair<const char*, MsgPack>> items) {
	MsgPack m = MsgPack::MAP();
	for (const auto& kv : items) {
		m[kv.first] = kv.second;
	}
	return m;
}

MsgPack list(std::initializer_list<MsgPack> items) {
	MsgPack a = MsgPack::ARRAY();
	for (const auto& item : items) {
		a.push_back(item);
	}
	return a;
}

}  // namespace

static void test_terms_of_keywords() {
	Fixture fx;
	SEE(fx.got("*"), std::string("<all>"));
	SEE(fx.got(obj({ { "k", "Female" } })), std::string("k/female"));      // keywords are lower case...
	SEE(fx.got(obj({ { "kb", "Female" } })), std::string("kb/Female"));    // ...unless the field is a boolean term
	SEE(fx.got(obj({ { "o", obj({ { "x", "A" } }) } })), std::string("o.x/a"));
	SEE(fx.got(obj({ { "k", "" } })), std::string("k.any/"));
	SEE(fx.got(obj({ { "k", nullptr } })), std::string("k.any/"));
	SEE(fx.got(obj({ { "k", "*" } })), std::string("wildcard(k.any/)"));
	SEE(fx.got(obj({ { "k", "fem*" } })), std::string("wildcard(k/fem)"));
	SEE(fx.got(obj({ { "k", "f?m*" } })), std::string("wildcard(k/f?m flags=3)"));
	SEE(fx.got(obj({ { "k", "fem**" } })), std::string("(wildcard(k/fem max=50) OR k/fem)"));
	SEE(fx.got(obj({ { "k", "fem~" } })), std::string("edit_distance(k/fem d=2)"));
	SEE(fx.got(obj({ { "k", "fem~1" } })), std::string("edit_distance(k/fem d=1)"));
	SEE(fx.got(obj({ { "k", "fem~x" } })), std::string("k/fem~x"));  // not a distance: part of the word
	SEE(fx.got(obj({ { "k", "a" } })), std::string("k/a"));
	SEE(fx.got(obj({ { "k", obj({ { "_wildcard", "fem" } }) } })), std::string("wildcard(k/fem)"));
	SEE(fx.got(obj({ { "k", obj({ { "_partial", "fem" } }) } })), std::string("(wildcard(k/fem max=50) OR k/fem)"));
	SEE(fx.got(obj({ { "k", obj({ { "_fuzzy", "fem" } }) } })), std::string("edit_distance(k/fem d=2)"));
	SEE(fx.got(obj({ { "k", obj({ { "_value", "Fem" } }) } })), std::string("k/fem"));
	SEE(fx.got(obj({ { "k", obj({ { "_keyword", "Fem" } }) } })), std::string("k/fem"));
	// Several fields join with AND, and a list under a field is its values joined.
	SEE(fx.got(obj({ { "k", "a" }, { "kb", "B" } })), std::string("(k/a AND kb/B)"));
	SEE(fx.got(obj({ { "k", list({ "a", "b" }) } })), std::string("(k/a AND k/b)"));
	SEE(fx.got(obj({ { "#comment", "ignored" }, { "k", "a" } })), std::string("k/a"));
}

static void test_terms_of_typed_fields() {
	Fixture fx;
	SEE(fx.got(obj({ { "n", 5 } })), Fixture::term("n", Serialize::integer(5)));
	SEE(fx.got(obj({ { "n", "5" } })), Fixture::term("n", Serialize::integer(5)));  // text spelled as the number
	SEE(fx.got(obj({ { "n", -5 } })), Fixture::term("n", Serialize::integer(-5)));
	SEE(fx.got(obj({ { "n", obj({ { "_integer", 5.9 } }) } })), Fixture::term("n", Serialize::integer(5)));  // a cast converts
	SEE(fx.got(obj({ { "n", obj({ { "_value", 7 } }) } })), Fixture::term("n", Serialize::integer(7)));
	SEE(fx.got(obj({ { "p", 7 } })), Fixture::term("p", Serialize::positive(7)));
	SEE(fx.got(obj({ { "f", 2.5 } })), Fixture::term("f", Serialize::floating(2.5)));
	SEE(fx.got(obj({ { "f", "2.5" } })), Fixture::term("f", Serialize::floating(2.5)));
	SEE(fx.got(obj({ { "b", true } })), Fixture::term("b", Serialize::boolean(true)));
	CHECK_THROW(fx.got(obj({ { "n", "abc" } })), ClientError);
	CHECK_THROW(fx.got(obj({ { "n", 5.5 } })), ClientError);  // not exactly an integer
	CHECK_THROW(fx.got(obj({ { "p", -1 } })), ClientError);
	// A date is the same instant however it is written.
	const auto by_text = fx.got(obj({ { "d", "2021-03-04" } }));
	const auto by_seconds = fx.got(obj({ { "d", Datetime::timestamp(Datetime::DatetimeParser(std::string_view("2021-03-04"))) } }));
	SEE(by_text, by_seconds);
	CHECK_TRUE(by_text.rfind("d/", 0) == 0);
	CHECK_TRUE(by_text != fx.got(obj({ { "d", "2021-03-05" } })));
	CHECK_TRUE(fx.got(obj({ { "dt", "2021-03-04T05:06:07" } })).rfind("dt/", 0) == 0);
	CHECK_TRUE(fx.got(obj({ { "tm", "10:30:00" } })).rfind("tm/", 0) == 0);
	CHECK_TRUE(fx.got(obj({ { "td", "+01:30:00" } })).rfind("td/", 0) == 0);
	CHECK_TRUE(fx.got(obj({ { "u", "a2d6e3b4-0000-1000-8000-0100000000ab" } })).rfind("u/", 0) == 0);
	// A field of no type, and an object, take the value of any type: searched as a global value.
	CHECK_TRUE(fx.got(obj({ { "e", "word" } })).find("/word") != std::string::npos);
	CHECK_TRUE(fx.got(obj({ { "e", 5 } })).find("/") != std::string::npos);
}

static void test_global_values() {
	Fixture fx;
	// With no field, a value is searched by its own type among the global values.
	const auto text = fx.got("word");
	CHECK_TRUE(text.find("/word") != std::string::npos);
	const auto number = fx.got(5);
	CHECK_TRUE(number.find("/" + Serialize::integer(5)) != std::string::npos);
	CHECK_TRUE(number != fx.got(6));
	CHECK_TRUE(fx.got(true) != fx.got(false));
	CHECK_TRUE(fx.got(2.5).find("/" + Serialize::floating(2.5)) != std::string::npos);
	CHECK_TRUE(fx.got(obj({ { "_value", "" } })) == fx.got(obj({ { "_value", nullptr } })));  // an empty value is the field's null marker
	CHECK_TRUE(fx.got(obj({ { "_value", "*" } })).rfind("wildcard(", 0) == 0);
	CHECK_TRUE(fx.got(obj({ { "_text", "word" } })).find("/word") != std::string::npos);
	CHECK_TRUE(fx.got(obj({ { "_integer", "7" } })) == fx.got(7));
	CHECK_TRUE(fx.got(obj({ { "_value", "word" } })) == text);
}

static void test_text_fields() {
	Fixture fx;
	// The default operator comes from the clause: a bare text is analysed as the engine's text query.
	SEE(fx.got(obj({ { "t", "hello world" } })), fx.got(obj({ { "t", obj({ { "_or", "hello world" } }) } })));
	SEE(fx.got(obj({ { "t", obj({ { "_and", "hello world" } }) } })), std::string("(t/hello AND t/world)"));
	SEE(fx.got(obj({ { "t", obj({ { "_or", "hello world" } }) } })), std::string("(t/hello OR t/world)"));
	SEE(fx.got(obj({ { "t", obj({ { "_phrase", "hello world" } }) } })), std::string("(t/hello PHRASE t/world 11)"));  // the window is the default, 11
	SEE(fx.got(obj({ { "t", obj({ { "_near", "hello world" } }) } })), std::string("(t/hello NEAR t/world 11)"));
	SEE(fx.got(obj({ { "t", obj({ { "_synonym", "hello world" } }) } })), std::string("(t/hello SYNONYM t/world)"));
	SEE(fx.got(obj({ { "t", obj({ { "_max", "hello world" } }) } })), std::string("(t/hello MAX t/world)"));
	SEE(fx.got(obj({ { "t", "hello" } })), std::string("t/hello"));
	SEE(fx.got(obj({ { "t", "hel*" } })), std::string("wildcard(t/hel)"));
	SEE(fx.got(obj({ { "t", "hel**" } })), std::string("(wildcard(t/hel max=50) OR t/hel)"));
	SEE(fx.got(obj({ { "t", obj({ { "_wildcard", "hel" } }) } })), std::string("wildcard(t/hel)"));
	SEE(fx.got(obj({ { "t", obj({ { "_wildcard", "h?l*o" } }) } })), std::string("wildcard(t/h?l*o flags=3)"));
	SEE(fx.got(obj({ { "t", obj({ { "_partial", "hel" } }) } })), std::string("(wildcard(t/hel max=50) OR t/hel)"));
	SEE(fx.got(obj({ { "t", obj({ { "_fuzzy", "hello" } }) } })), std::string("edit_distance(t/hello d=2)"));
	SEE(fx.got(obj({ { "t", obj({ { "_wildcard", "" } }) } })), std::string("t.any/"));
	// A text field with a language stems and drops stop words.
	CHECK_TRUE(fx.got(obj({ { "ts", "running" } })).find("run") != std::string::npos);
	SEE(fx.got(obj({ { "ts", obj({ { "_and", "the running dogs" } }) } })), std::string("(?5/run AND ?5/dog)"));  // "the" is a stop word; ?5 is the stemmed-term prefix
}

static void test_compound_clauses() {
	Fixture fx;
	const auto a = obj({ { "k", "a" } }), b = obj({ { "k", "b" } }), c = obj({ { "k", "c" } });
	SEE(fx.got(obj({ { "_and", list({ a, b }) } })), std::string("(k/a AND k/b)"));
	SEE(fx.got(obj({ { "_or", list({ a, b }) } })), std::string("(k/a OR k/b)"));
	SEE(fx.got(obj({ { "_and_not", list({ a, b }) } })), std::string("(k/a AND_NOT k/b)"));
	SEE(fx.got(obj({ { "_xor", list({ a, b }) } })), std::string("(k/a XOR k/b)"));
	SEE(fx.got(obj({ { "_and_maybe", list({ a, b }) } })), std::string("(k/a AND_MAYBE k/b)"));
	SEE(fx.got(obj({ { "_filter", list({ a, b }) } })), std::string("(k/a FILTER k/b)"));
	SEE(fx.got(obj({ { "_or", list({ a, b, c }) } })), std::string("(k/a OR k/b OR k/c)"));
	SEE(fx.got(obj({ { "_not", list({ a, b }) } })), std::string("(<all> AND_NOT (k/a OR k/b))"));
	SEE(fx.got(obj({ { "_not", a } })), std::string("(<all> AND_NOT k/a)"));  // a single subquery is a list of one
	SEE(fx.got(obj({ { "_and", list({ obj({ { "_or", list({ a, b }) } }), c }) } })), std::string("((k/a OR k/b) AND k/c)"));
	SEE(fx.got(list({ a, b })), std::string("(k/a AND k/b)"));  // a list at the top is its queries joined
	SEE(fx.got(obj({ { "_match_all", nullptr } })), std::string("<all>"));
	SEE(fx.got(obj({ { "_match_none", nullptr } })), std::string("<nothing>"));
	SEE(fx.got(obj({ { "k", obj({ { "_and", "a" } }) } })), std::string("k/a"));
	SEE(fx.got(obj({ { "k", obj({ { "_or", "a" } }) } })), std::string("k/a"));
	// A list under a field repeats the field for each value.
	SEE(fx.got(obj({ { "k", obj({ { "_or", list({ "a", "b" }) } }) } })), std::string("(k/a OR k/b)"));
	SEE(fx.got(obj({ { "k", obj({ { "_and", list({ "a", "b" }) } }) } })), std::string("(k/a AND k/b)"));
	// A wildcard over a list is refused; `_scale_weight` and `_edit_distance` are no operators, in any form.
	CHECK_THROW(fx.got(obj({ { "_wildcard", list({ "a" }) } })), std::exception);
	CHECK_THROWS_WITH(fx.got(obj({ { "_scale_weight", list({ a, 2 }) } })), QueryDslError, "Invalid operator: _scale_weight");
	CHECK_THROWS_WITH(fx.got(obj({ { "_edit_distance", list({ "a" }) } })), QueryDslError, "Invalid operator: _edit_distance");
	CHECK_THROWS_WITH(fx.got(obj({ { "k", obj({ { "_scale_weight", "a" } }) } })), QueryDslError, "Invalid operator: _scale_weight");
	CHECK_THROWS_WITH(fx.got(obj({ { "k", obj({ { "_edit_distance", "a" } }) } })), QueryDslError, "Invalid operator: _edit_distance");
}

static void test_clause_errors() {
	Fixture fx;
	CHECK_THROWS_WITH(fx.got(obj({ { "", 1 } })), QueryDslError, "Invalid field name: must not be empty");
	CHECK_THROWS_WITH(fx.got(obj({ { "_bogus", 1 } })), QueryDslError, "Invalid operator: _bogus");
	CHECK_THROWS_WITH(fx.got(obj({ { "_bogus", list({ 1 }) } })), QueryDslError, "Invalid operator: _bogus");
	CHECK_THROWS_WITH(fx.got(obj({ { "_in", list({ 1 }) } })), QueryDslError, "Invalid operator: _in");
	CHECK_THROWS_WITH(fx.got(obj({ { "_value", list({ 1 }) } })), QueryDslError, "Invalid operator: _value");
	CHECK_THROWS_WITH(fx.got(obj({ { "_and", 5 } })), std::exception, "");
	// Nesting is bounded: 45 levels of clauses pass, past that the query is refused.
	const auto nest = [](int levels) {
		MsgPack q = obj({ { "k", "a" } });
		for (int i = 0; i < levels; ++i) {
			q = obj({ { "_and", list({ q }) } });
		}
		return q;
	};
	SEE(fx.got(nest(20)), std::string(20, '(') + "k/a" + std::string(20, ')'));
	CHECK_THROWS_WITH(fx.got(nest(60)), QueryDslError, "nested too deeply");
	SEE(fx.got(nest(20)), std::string(20, '(') + "k/a" + std::string(20, ')'));  // the depth is released after a refusal
}

static void test_accuracy_terms() {
	Fixture fx;
	const auto by = [&](const char* path, const MsgPack& value) { return fx.got(obj({ { path, value } })); };
	// The serialized bytes of a term, past its field's name.
	const auto tail = [&](const char* path, const MsgPack& value) {
		const auto q = by(path, value);
		return q.substr(q.find('/') + 1);
	};
	const MsgPack when = "2021-03-04T05:06:07.5";
	struct Row { const char* unit; Datetime::tm_t tm; };
	const Row rows[] = {
		{ "d._second", Datetime::tm_t(2021, 3, 4, 5, 6, 7) },    { "d._minute", Datetime::tm_t(2021, 3, 4, 5, 6) },
		{ "d._hour", Datetime::tm_t(2021, 3, 4, 5) },            { "d._day", Datetime::tm_t(2021, 3, 4) },
		{ "d._month", Datetime::tm_t(2021, 3) },                 { "d._year", Datetime::tm_t(2021) },
		{ "d._decade", Datetime::tm_t(2020) },                   { "d._century", Datetime::tm_t(2000) },
		{ "d._millennium", Datetime::tm_t(2000) },
	};
	for (const auto& row : rows) {
		auto tm = row.tm;
		SEE(tail(row.unit, when), Serialize::serialize(tm));
	}
	// Two instants in one unit share the term; in different units they do not.
	SEE(fx.got(obj({ { "d._day", "2021-03-04T01:00:00" } })), fx.got(obj({ { "d._day", "2021-03-04T23:59:59" } })));
	CHECK_TRUE(fx.got(obj({ { "d._day", "2021-03-04" } })) != fx.got(obj({ { "d._day", "2021-03-05" } })));
	// An integer field's accuracy rounds down to a multiple.
	SEE(tail("n._10", 1234), Serialize::integer(1230));
	SEE(by("n._10", 1230), by("n._10", 1239));
	SEE(tail("n._100", 1299), Serialize::integer(1200));
	SEE(tail("n._10", -1234), Serialize::integer(-1240));  // down, toward minus infinity
	// A time's accuracy rounds its seconds, a timedelta's too.
	SEE(tail("tm._tminute", "10:30:45"), Serialize::integer(37800));
	SEE(tail("tm._thour", "10:30:45"), Serialize::integer(36000));
	SEE(tail("tm._tsecond", "10:30:45"), Serialize::integer(37845));
	SEE(tail("td._tdminute", 5445), Serialize::integer(5400));
	SEE(tail("td._tdhour", 5445), Serialize::integer(3600));
	SEE(tail("td._tdminute", -5445), Serialize::integer(-5460));
	// A geo accuracy is the cells of the shape at that level.
	const auto point = obj({ { "_ewkt", "POINT(10 20)" } });
	const auto cells5 = by("g._geo5", point);
	const auto cells8 = by("g._geo8", point);
	CHECK_TRUE(!cells5.empty() && cells5 != cells8);
	CHECK_TRUE(cells5.rfind("g", 0) == 0 || cells5.size() > 1);
	// The accuracy a field type does not have is refused, and so is a malformed one.
	CHECK_THROWS_WITH(fx.got(obj({ { "k._10", "a" } })), std::exception, "expects a value of type integer");
	CHECK_THROWS_WITH(fx.got(obj({ { "n._x", 1 } })), ClientError, "not valid");
	CHECK_THROWS_WITH(fx.got(obj({ { "n._tsecond", 1 } })), std::exception, "");
	// An integer accuracy of 0 is no accuracy: a QueryDslError (a 400) naming the accuracy component like the other refusals, not the modulus's std::invalid_argument (a 500).
	CHECK_THROWS_WITH(fx.got(obj({ { "n._0", 1 } })), QueryDslError, "Invalid field name: _0");
}

static void test_in_ranges() {
	Fixture fx;
	const auto range = [](const MsgPack& from, const MsgPack& to) {
		return obj({ { "_in", obj({ { "_range", obj({ { "_from", from }, { "_to", to } }) } }) } });
	};
	const auto d_in_object = fx.got(obj({ { "n", range(1, 5) } }));
	const auto d_in_text = fx.got(obj({ { "n", obj({ { "_in", "1..5" } }) } }));
	SEE(d_in_text, d_in_object);                            // a text range is the same range as its object
	CHECK_TRUE(d_in_object != fx.got(obj({ { "n", range(1, 6) } })));
	CHECK_TRUE(fx.got(obj({ { "n", obj({ { "_in", "1.." } }) } })) != fx.got(obj({ { "n", obj({ { "_in", "..5" } }) } })));
	SEE(fx.got(obj({ { "n", obj({ { "_in", "1..5" } }) } })), fx.got(obj({ { "n", obj({ { "_in", obj({ { "_range", obj({ { "_from", "1" }, { "_to", "5" } }) } }) } }) } })));
	CHECK_TRUE(fx.got(obj({ { "f", obj({ { "_in", "1.5..2.5" } }) } })) != fx.got(obj({ { "f", obj({ { "_in", "1.5..3.5" } }) } })));
	CHECK_TRUE(!fx.got(obj({ { "d", obj({ { "_in", "2021-01-01..2021-12-31" } }) } })).empty());
	CHECK_TRUE(!fx.got(obj({ { "k", obj({ { "_in", "a..m" } }) } })).empty());
	// Without a field the bounds give the type: numbers, text, dates.
	CHECK_TRUE(fx.got(range(1, 5)) != fx.got(range(1, 6)));
	CHECK_TRUE(!fx.got(obj({ { "_in", "1..5" } })).empty());
	CHECK_TRUE(fx.got(obj({ { "_in", "..5" } })) != fx.got(obj({ { "_in", "1.." } })));
	SEE(fx.got(obj({ { "_in", obj({ { "_range", MsgPack::MAP() } }) } })), std::string("<all>"));  // no bounds: everything
	CHECK_TRUE(!fx.got(range("a", "z")).empty());
	CHECK_TRUE(fx.got(obj({ { "_in", obj({ { "_range", obj({ { "_to", 5 } }) } }) } })) != fx.got(obj({ { "_in", obj({ { "_range", obj({ { "_from", 5 } }) } }) } })));
	// Geometry in a range.
	CHECK_TRUE(!fx.got(obj({ { "g", obj({ { "_in", obj({ { "_ewkt", "POINT(10 20)" } }) } }) } })).empty());
	CHECK_TRUE(!fx.got(obj({ { "_in", obj({ { "_ewkt", "POINT(10 20)" } }) } })).empty());
	// Malformed.
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", "nothing" } }) } })), QueryDslError, "Invalid range");
	CHECK_THROWS_WITH(fx.got(obj({ { "_in", "nothing" } })), QueryDslError, "Invalid range");
	CHECK_THROWS_WITH(fx.got(obj({ { "_in", 5 } })), QueryDslError, "must be object");
	CHECK_THROWS_WITH(fx.got(obj({ { "_in", obj({ { "_range", 5 } }) } })), QueryDslError, "must be object");
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", obj({ { "_range", 5 } }) } }) } })), QueryDslError, "must be object");
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", obj({ { "_range", obj({ { "_from", 1 } }) }, { "x", 1 } }) } }) } })), QueryDslError, "single element");
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", 5 } }) } })), QueryDslError, "single element");
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", obj({ { "foo", 1 } }) } }) } })), QueryDslError, "Invalid format '_in'");
	CHECK_THROWS_WITH(fx.got(obj({ { "n", obj({ { "_in", "a..b" } }) } })), ClientError, "");
}

static void test_make_dsl_query_from_text() {
	Fixture fx;
	const auto dsl = [&](const char* text) { return fx.dsl.make_dsl_query(text).to_string(); };
	const auto want = [](const MsgPack& m) { return m.to_string(); };
	SEE(dsl(""), want("*"));
	SEE(dsl("*"), want("*"));
	SEE(dsl("hello"), want(obj({ { "_value", "hello" } })));
	SEE(dsl("name:smith"), want(obj({ { "name", obj({ { "_value", "smith" } }) } })));
	SEE(dsl("age:18..65"), want(obj({ { "age", obj({ { "_in", obj({ { "_range", obj({ { "_from", "18" }, { "_to", "65" } }) } }) } }) } })));
	SEE(dsl("age:18.."), want(obj({ { "age", obj({ { "_in", obj({ { "_range", obj({ { "_from", "18" } }) } }) } }) } })));
	SEE(dsl("age:..65"), want(obj({ { "age", obj({ { "_in", obj({ { "_range", obj({ { "_to", "65" } }) } }) } }) } })));
	SEE(dsl("a AND b"), want(obj({ { "_and", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } })));
	SEE(dsl("a OR b"), want(obj({ { "_or", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } })));
	SEE(dsl("a XOR b"), want(obj({ { "_xor", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } })));
	SEE(dsl("a MAYBE b"), want(obj({ { "_and_maybe", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } })));
	// `_not` holds a list of one expression, never a list nested in a list, on every toolchain.
	SEE(dsl("NOT a"), want(obj({ { "_not", list({ obj({ { "_value", "a" } }) }) } })));
	// A run of one operator is one clause.
	SEE(dsl("a AND b AND c"), want(obj({ { "_and", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }), obj({ { "_value", "c" } }) }) } })));
	SEE(dsl("a OR b OR c"), want(obj({ { "_or", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }), obj({ { "_value", "c" } }) }) } })));
	SEE(dsl("a XOR b XOR c"), want(obj({ { "_xor", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }), obj({ { "_value", "c" } }) }) } })));
	SEE(dsl("a MAYBE b MAYBE c"), want(obj({ { "_and_maybe", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }), obj({ { "_value", "c" } }) }) } })));
	// Different operators nest, by parentheses or by precedence.
	SEE(dsl("a AND (b OR c)"), want(obj({ { "_and", list({ obj({ { "_value", "a" } }), obj({ { "_or", list({ obj({ { "_value", "b" } }), obj({ { "_value", "c" } }) }) } }) }) } })));
	SEE(dsl("(a OR b) AND c"), want(obj({ { "_and", list({ obj({ { "_or", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } }), obj({ { "_value", "c" } }) }) } })));
	CHECK_TRUE(!dsl("a AND (b AND c)").empty());
	CHECK_TRUE(!dsl("a OR (b OR c)").empty());
	CHECK_TRUE(!dsl("a XOR (b XOR c)").empty());
	CHECK_TRUE(!dsl("a MAYBE (b MAYBE c)").empty());
	CHECK_TRUE(!dsl("(a OR b) OR c").empty());
	CHECK_TRUE(!dsl("(a AND b) AND c").empty());
	CHECK_TRUE(!dsl("(a XOR b) XOR c").empty());
	CHECK_TRUE(!dsl("(a MAYBE b) MAYBE c").empty());
	CHECK_TRUE(!dsl("NOT (a AND b)").empty());
	// Malformed expressions.
	CHECK_THROW(fx.dsl.make_dsl_query("a AND"), QueryDslError);
	CHECK_THROW(fx.dsl.make_dsl_query("AND"), QueryDslError);
	CHECK_THROW(fx.dsl.make_dsl_query("(a"), QueryDslError);
	// Juxtaposition is an OR, not an error.
	SEE(dsl("a b"), want(obj({ { "_or", list({ obj({ { "_value", "a" } }), obj({ { "_value", "b" } }) }) } })));
	CHECK_THROW(fx.dsl.make_dsl_query("a OR"), QueryDslError);
	CHECK_THROW(fx.dsl.make_dsl_query("a XOR"), QueryDslError);
	CHECK_THROW(fx.dsl.make_dsl_query("a MAYBE"), QueryDslError);
	CHECK_THROW(fx.dsl.make_dsl_query("NOT"), QueryDslError);
	// An expression too deep is refused before it is built.
	std::string deep = "x";
	for (int i = 0; i < 80; ++i) {
		deep = "(" + deep + " AND y" + std::to_string(i) + ") OR z";
	}
	CHECK_THROWS_WITH(fx.dsl.make_dsl_query(deep), QueryDslError, "nested too deeply");

	// From the query fields of a request: none is everything, one is its DSL, several are joined by AND.
	query_field_t none;
	SEE(fx.dsl.make_dsl_query(none).to_string(), want("*"));
	query_field_t one;
	one.query = { "a" };
	SEE(fx.dsl.make_dsl_query(one).to_string(), want(obj({ { "_value", "a" } })));
	query_field_t two;
	two.query = { "a", "f:b" };
	SEE(fx.dsl.make_dsl_query(two).to_string(), want(obj({ { "_and", list({ obj({ { "_value", "a" } }), obj({ { "f", obj({ { "_value", "b" } }) } }) }) } })));
}

namespace {

std::string sorted_key(const std::unique_ptr<Multi_MultiValueKeyMaker>& sorter, engine::valueno slot, const std::string& serialized) {
	const std::vector<std::string> one = { serialized };
	return sorter->key(testsupport::MapView({ { slot, StringList::serialize(one.begin(), one.end()) } }));
}

bool has_key(const std::unique_ptr<Multi_MultiValueKeyMaker>& sorter, const char* name) {
	return sorter->serialize().find(serialize_string(name)) != std::string::npos;
}

// has_key of the sorter `sort` builds; a throw is a failed check that prints what it threw, not an abort of the binary.
bool built_key(QueryDSL& dsl, const MsgPack& sort, const char* name) {
	try {
		return has_key(dsl.get_sorter(sort), name);
	} catch (const std::exception& e) {
		std::printf("get_sorter threw: %s\n", e.what());
		return false;
	}
}

}  // namespace

static void test_sorters() {
	Fixture fx;
	// A reference value sorts by distance from it, nearest first; descending puts the farthest first.
	auto asc = fx.dsl.get_sorter(obj({ { "n", obj({ { "_value", 100 } }) } }));
	CHECK_TRUE(has_key(asc, "IntegerKey"));
	CHECK_TRUE(sorted_key(asc, SLOT_N, Serialize::integer(90)) < sorted_key(asc, SLOT_N, Serialize::integer(120)));
	auto desc = fx.dsl.get_sorter(obj({ { "n", obj({ { "_value", 100 }, { "_order", "desc" } }) } }));
	CHECK_TRUE(sorted_key(desc, SLOT_N, Serialize::integer(90)) > sorted_key(desc, SLOT_N, Serialize::integer(120)));
	CHECK_TRUE(asc->serialize() != desc->serialize());
	// A cast word gives the reference value cast to the field's type: nearest first.
	auto cast_int = fx.dsl.get_sorter(obj({ { "n", obj({ { "_integer", "100" } }) } }));
	CHECK_TRUE(sorted_key(cast_int, SLOT_N, Serialize::integer(90)) < sorted_key(cast_int, SLOT_N, Serialize::integer(120)));
	auto cast_flt = fx.dsl.get_sorter(obj({ { "f", obj({ { "_float", "1.5" } }) } }));
	CHECK_TRUE(sorted_key(cast_flt, SLOT_F, Serialize::floating(1.0)) < sorted_key(cast_flt, SLOT_F, Serialize::floating(3.0)));
	auto cast_dt = fx.dsl.get_sorter(obj({ { "dt", obj({ { "_datetime", "2021-03-04T05:06:07" } }) } }));
	CHECK_TRUE(sorted_key(cast_dt, SLOT_D + 6, Serialize::datetime(std::string("2021-03-05T00:00:00"))) < sorted_key(cast_dt, SLOT_D + 6, Serialize::datetime(std::string("2021-04-01T00:00:00"))));
	auto cast_kw = fx.dsl.get_sorter(obj({ { "k", obj({ { "_keyword", "smith" } }) } }));
	CHECK_TRUE(sorted_key(cast_kw, SLOT_K, "smith") < sorted_key(cast_kw, SLOT_K, "zzzzz"));
	auto flt = fx.dsl.get_sorter(obj({ { "f", obj({ { "_value", 1.5 } }) } }));
	CHECK_TRUE(has_key(flt, "FloatKey"));
	CHECK_TRUE(sorted_key(flt, SLOT_F, Serialize::floating(1.0)) < sorted_key(flt, SLOT_F, Serialize::floating(3.0)));
	CHECK_TRUE(has_key(fx.dsl.get_sorter(obj({ { "p", obj({ { "_value", 7 } }) } })), "PositiveKey"));
	CHECK_TRUE(has_key(fx.dsl.get_sorter(obj({ { "d", obj({ { "_value", "2021-03-04" } }) } })), "DateKey"));
	// A boolean reference value is a BoolKey.
	CHECK_TRUE(has_key(fx.dsl.get_sorter(obj({ { "b", obj({ { "_value", true } }) } })), "BoolKey"));
	CHECK_FALSE(has_key(fx.dsl.get_sorter(obj({ { "b", obj({ { "_value", true } }) } })), "DateKey"));
	// A string reference value has a metric: its name, and the reference text (a phonetic metric keeps its code instead).
	struct Metric { const char* word; const char* name; };
	for (const auto& m : { Metric{ "levenshtein", "Levenshtein" }, Metric{ "leven", "Levenshtein" }, Metric{ "jarowinkler", "Jaro_Winkler" },
	                       Metric{ "jarow", "Jaro_Winkler" }, Metric{ "sorensendice", "Sorensen_Dice" }, Metric{ "sorensen", "Sorensen_Dice" },
	                       Metric{ "dice", "Sorensen_Dice" }, Metric{ "jaccard", "Jaccard" }, Metric{ "lcsubstr", "LCSubstr" },
	                       Metric{ "lcs", "LCSubstr" }, Metric{ "lcsubsequence", "LCSubsequence" }, Metric{ "lcsq", "LCSubsequence" },
	                       Metric{ "jaro", "Jaro" }, Metric{ "unknown", "Jaro" } }) {
		auto s = fx.dsl.get_sorter(obj({ { "k", obj({ { "_value", "smith" }, { "_metric", m.word } }) } }));
		CHECK_TRUE(has_key(s, m.name));
		CHECK_TRUE(s->serialize().find("smith") != std::string::npos);
	}
	const auto jaro = fx.dsl.get_sorter(obj({ { "k", obj({ { "_value", "smith" } }) } }))->serialize();  // no metric: Jaro
	for (const char* metric : { "soundex", "sound", "doublemetaphone", "dmetaphone", "metaphone" }) {
		const auto phonetic = fx.dsl.get_sorter(obj({ { "k", obj({ { "_value", "smith" }, { "_metric", metric } }) } }))->serialize();
		CHECK_TRUE(phonetic != jaro);
		CHECK_TRUE(phonetic.find("smith") == std::string::npos);
	}
	CHECK_TRUE(built_key(fx.dsl, obj({ { "k", obj({ { "_text", "smith" } }) } }), "Jaro"));
	CHECK_TRUE(built_key(fx.dsl, obj({ { "u", obj({ { "_value", "a2d6e3b4-0000-1000-8000-0100000000ab" } }) } }), "Jaro"));
	// A geo reference value gives the centroids; one with none adds no key.
	CHECK_TRUE(has_key(fx.dsl.get_sorter(obj({ { "g", obj({ { "_ewkt", "POINT(10 20)" } }) } })), "GeoKey"));
	// The order alone, as an object or a text, sorts by the value itself.
	auto plain = fx.dsl.get_sorter(obj({ { "n", "asc" } }));
	CHECK_TRUE(has_key(plain, "SerializeKey"));
	auto plain_desc = fx.dsl.get_sorter(obj({ { "n", "DESC" } }));
	CHECK_TRUE(plain->serialize() != plain_desc->serialize());
	CHECK_TRUE(sorted_key(plain, SLOT_N, Serialize::integer(1)) < sorted_key(plain, SLOT_N, Serialize::integer(2)));
	CHECK_TRUE(sorted_key(plain_desc, SLOT_N, Serialize::integer(1)) > sorted_key(plain_desc, SLOT_N, Serialize::integer(2)));
	SEE(fx.dsl.get_sorter(obj({ { "n", obj({ { "_order", "asc" } }) } }))->serialize(), plain->serialize());
	SEE(fx.dsl.get_sorter(obj({ { "n", obj({ { "_order", "desc" } }) } }))->serialize(), plain_desc->serialize());
	CHECK_TRUE(has_key(fx.dsl.get_sorter(obj({ { "k", obj({ { "_order", "desc" } }) } })), "SerializeKey"));
	// A list of sorts is the keys in order; a field of no type adds no key.
	auto two = fx.dsl.get_sorter(list({ obj({ { "n", "desc" } }), obj({ { "f", "asc" } }) }));
	SEE(two->serialize(), plain_desc->serialize() + fx.dsl.get_sorter(obj({ { "f", "asc" } }))->serialize());
	SEE(fx.dsl.get_sorter(obj({ { "e", "asc" } }))->serialize(), std::string());
	SEE(fx.dsl.get_sorter(obj({ { "nothing", "asc" } }))->serialize(), std::string());
	// A plain field name, with a sign for the order.
	SEE(fx.dsl.get_sorter(MsgPack("n"))->serialize(), plain->serialize());
	SEE(fx.dsl.get_sorter(MsgPack("+n"))->serialize(), plain->serialize());
	SEE(fx.dsl.get_sorter(MsgPack("-n"))->serialize(), plain_desc->serialize());
	SEE(fx.dsl.get_sorter(MsgPack("e"))->serialize(), std::string());
	SEE(fx.dsl.get_sorter(list({ "n", "-f" }))->serialize(), plain->serialize() + fx.dsl.get_sorter(MsgPack("-f"))->serialize());
}

static void test_sorter_errors() {
	Fixture fx;
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", "sideways" } })), QueryDslError, "must be asc or desc");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", 5 } })), QueryDslError, "Invalid sort for field n");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", obj({ { "_order", 5 } }) } })), QueryDslError, "must be string (asc/desc)");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "k", obj({ { "_metric", 5 } }) } })), QueryDslError, "must be string");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", obj({ { "_value", 1 }, { "_integer", 2 } }) } })), QueryDslError, "already defined");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", obj({ { "_integer", 1 }, { "_value", 2 } }) } })), QueryDslError, "already defined");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "n", obj({ { "_bogus", 1 } }) } })), QueryDslError, "Invalid key");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "g", obj({ { "_order", "asc" } }) } })), QueryDslError, "must define a reference value");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(obj({ { "g", "asc" } })), QueryDslError, "must define a reference value");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(MsgPack("g")), QueryDslError, "must define a reference value");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(MsgPack("")), QueryDslError, "must specify a field");
	CHECK_THROWS_WITH(fx.dsl.get_sorter(MsgPack(5)), QueryDslError, "Invalid format '_sort'");
	CHECK_THROW(fx.dsl.get_sorter(obj({ { "n", obj({ { "_value", "abc" } }) } })), CastError);
}

TEST_MAIN(test_terms_of_keywords, test_terms_of_typed_fields, test_global_values, test_text_fields, test_compound_clauses,
          test_clause_errors, test_accuracy_terms, test_in_ranges, test_make_dsl_query_from_text, test_sorters, test_sorter_errors)
