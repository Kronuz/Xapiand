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


// Unit tests for the host-side text parser: queryparser's tree, lowered to an engine query. The
// recorded A/B decisions (parse-ab.jsonl, the 706 rows the first run inside Xapiand ruled on) must
// come out as recorded, and the lowering must give the tree queryparser itself describes.

#include "test_harness.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "engine/query.h"
#include "engine/text.h"
#include "engine/xapian/query.h"
#include "exception_engine.h"
#include "jsonl.hh"
#include "xapian.h"

namespace {

using engine::Op;
using engine::Query;
using engine::Stem;
using engine::xapian::to_xapian;
namespace text = engine::text;

// The stop words are read from the source tree.
const int data_path = (::setenv("XAPIAND_DATA_PATH", XAPIAND_SOURCE_DIR, 1), 0);

constexpr unsigned base = text::PHRASE | text::LOVEHATE | text::FUZZY;

engine::TextConfig
config(unsigned flags, Op op = Op::INVALID, const char* stop = "", const char* stem = "", Stem strategy = Stem::none)
{
	engine::TextConfig c;
	c.prefix = "XTfield";
	c.default_op = op;
	c.flags = flags;
	c.stop_language = stop;
	c.stem_language = stem;
	c.stem = strategy;
	c.partial_max_expansion = 50;
	c.fuzzy_max_expansion = 0;
	return c;
}

std::string
described(const std::string& text, const engine::TextConfig& c)
{
	return to_xapian(engine::parse_text(text, c)).get_description();
}

// One row of a vector file as a text configuration; false when it sets a flag the engine has no word for.
bool
row_config(std::map<std::string, std::string>& r, engine::TextConfig& c)
{
	static const std::map<std::string, unsigned> flags = {
		{ "boolean", text::BOOLEAN }, { "phrase", text::PHRASE }, { "lovehate", text::LOVEHATE },
		{ "wildcard", text::WILDCARD }, { "wildcard_single", text::WILDCARD_SINGLE },
		{ "wildcard_multi", text::WILDCARD_MULTI }, { "partial", text::PARTIAL }, { "fuzzy", text::FUZZY },
		{ "cjk_ngram", text::CJK_NGRAM },
	};
	static const std::map<std::string, Op> ops = {
		{ "unset", Op::INVALID }, { "and", Op::AND }, { "or", Op::OR }, { "near", Op::NEAR }, { "phrase", Op::PHRASE },
		{ "elite_set", Op::ELITE_SET }, { "synonym", Op::SYNONYM }, { "max", Op::MAX },
	};
	static const std::map<std::string, Stem> stems = {
		{ "unset", Stem::none }, { "none", Stem::none }, { "some", Stem::some }, { "all", Stem::all }, { "all_z", Stem::all_z },
	};
	c = engine::TextConfig();
	std::istringstream fs(r["flags"]);
	for (std::string f; std::getline(fs, f, '|');) {
		auto it = flags.find(f);
		if (it == flags.end()) {
			return false;
		}
		c.flags |= it->second;
	}
	auto op = ops.find(r["default_op"]);
	auto stem = stems.find(r["stem"]);
	if (op == ops.end() || stem == stems.end() || r["stop_strategy"] != "default") {
		return false;
	}
	c.default_op = op->second;
	c.stem = stem->second;
	c.stop_language = r["stopper"] == "none" ? "" : r["stopper"];
	c.stem_language = r["stem_lang"] == "none" ? "" : r["stem_lang"];
	c.prefix = r["prefix"];
	c.partial_max_expansion = static_cast<engine::termcount>(std::stoul(r["partial_max"]));
	c.fuzzy_max_expansion = static_cast<engine::termcount>(std::stoul(r["fuzzy_max"]));
	return true;
}

// The rows of a vector file, or none when it cannot be opened.
std::vector<std::map<std::string, std::string>>
rows(const char* file)
{
	std::vector<std::map<std::string, std::string>> result;
	std::ifstream in(std::string(QUERYPARSER_VECTORS) + "/" + file);
	for (std::string line; std::getline(in, line);) {
		result.push_back(jsonl::row(line));
	}
	return result;
}

}  // namespace


void
test_ab_decisions_stand()
{
	// Every recorded decision, through the lowering and into Xapian's description format.
	auto all = rows("parse-ab.jsonl");
	REQUIRE_EQ(all.size(), 706U);
	std::size_t errors = 0;
	for (auto& r : all) {
		engine::TextConfig c;
		REQUIRE(row_config(r, c));
		if (r["error"].empty()) {
			CHECK_EQ(described(r["query"], c), r["description"]);
		} else {
			++errors;
			std::string message;
			try {
				engine::parse_text(r["query"], c);
			} catch (const QueryParserError& exc) {
				CHECK_TRUE(exc.kind == engine::errc::syntax);
				message = exc.reason();
			}
			CHECK_EQ(message, r["error"]);
		}
	}
	CHECK_EQ(errors, 25U);
}

void
test_lowering_is_the_tree()
{
	// The engine query describes as the tree queryparser built, for every row of the larger vector files.
	std::size_t read = 0, failed = 0;
	for (const char* file : { "parse-v2.jsonl", "parse-v3.jsonl", "parse-ab.jsonl" }) {
		auto all = rows(file);
		REQUIRE(!all.empty());
		for (auto& r : all) {
			engine::TextConfig c;
			if (!row_config(r, c)) {
				continue;
			}
			std::string tree;
			try {
				const auto lowered = engine::parse_text(r["query"], c, &tree);
				CHECK_EQ(to_xapian(lowered).get_description(), tree);
				++read;
			} catch (const QueryParserError&) {
				++failed;
			}
		}
	}
	CHECK_GT(read, 5000U);
	CHECK_GT(failed, 0U);
}

void
test_errors_are_engine_errors()
{
	// A syntax error is a client error of kind syntax, with queryparser's message.
	try {
		engine::parse_text("AND AND", config(base | text::BOOLEAN, Op::AND));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
		CHECK_FALSE(exc.reason().empty());
	}

	// NEAR over phrases is a client mistake too, no longer a server error.
	try {
		engine::parse_text("\"a b\" \"c d\"", config(base, Op::NEAR));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
	}
}

void
test_unreadable_settings_are_invalid()
{
	CHECK_THROW(engine::parse_text("quick", config(base, Op::INVALID, "", "no-such-language", Stem::some)), InvalidTextConfigError);
	CHECK_THROW(engine::parse_text("quick", config(base, Op::AND_NOT)), InvalidTextConfigError);
	try {
		engine::parse_text("quick", config(base, Op::FILTER));
	} catch (const InvalidTextConfigError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::invalid);
	}

	// The reason carries the language as given, braces and all.
	try {
		engine::parse_text("quick", config(base, Op::INVALID, "", "no{}such", Stem::some));
		CHECK_TRUE(false);
	} catch (const InvalidTextConfigError& exc) {
		CHECK_EQ(exc.reason(), "Language code no{}such unknown");
	}
}

void
test_word_breaks_are_not_read()
{
	// WORD_BREAKS is ignored: the text reads as it does without it.
	CHECK_EQ(described("quick brown", config(base | text::WORD_BREAKS)), described("quick brown", config(base)));
}

void
test_text_without_words_matches_nothing()
{
	CHECK_TRUE(engine::parse_text("", config(base)).empty());
	CHECK_TRUE(engine::parse_text(" \t ", config(base)).empty());
	CHECK_TRUE(engine::parse_text("!!! ???", config(base, Op::INVALID, "en")).empty());
	CHECK_FALSE(engine::parse_text("quick", config(base, Op::INVALID, "en")).empty());

	// Stop words go from a group with other words and stay in a group of their own.
	const auto stopped = engine::Query::term("XTfieldquick", 1, 2);
	CHECK_EQ(described("the quick", config(base, Op::INVALID, "en")), to_xapian(stopped).get_description());
	CHECK_FALSE(engine::parse_text("the and of", config(base, Op::INVALID, "en")).empty());
}

void
test_nesting_is_not_recursive()
{
	// 200,000 brackets: the tree is built and lowered without the call stack.
	const std::string deep = std::string(200000, '(') + "a" + std::string(200000, ')');
	CHECK_FALSE(engine::parse_text(deep, config(base | text::BOOLEAN, Op::AND)).empty());

	// Brackets that each hold an operator: the lowered tree is as deep as the text.
	std::string nested;
	std::string closing;
	for (int i = 0; i < 50000; ++i) {
		nested += (i % 2 ? "a OR (" : "a AND (");
		closing += ")";
	}
	const auto lowered = engine::parse_text(nested + "z" + closing, config(base | text::BOOLEAN, Op::OR));
	CHECK_FALSE(lowered.empty());
}

void
test_near_of_a_boolean_expression()
{
	// _near over "(barack OR michelle) AND obama" reached a heap overflow in the Xapian parser. NEAR
	// takes words only, so the bracket is a client error: the answer is a 400, not a corrupted heap.
	try {
		engine::parse_text("(barack OR michelle) AND obama", config(base, Op::NEAR));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
	}
	// With BOOLEAN the operators join the groups, and NEAR joins only the words between them.
	CHECK_EQ(described("(barack OR michelle) AND obama", config(base | text::BOOLEAN, Op::NEAR)),
		"Query(((XTfieldbarack@1 OR XTfieldmichelle@2) AND XTfieldobama@3))");
}

TEST_MAIN(test_ab_decisions_stand, test_lowering_is_the_tree, test_errors_are_engine_errors, test_unreadable_settings_are_invalid, test_word_breaks_are_not_read, test_text_without_words_matches_nothing, test_nesting_is_not_recursive, test_near_of_a_boolean_expression)
