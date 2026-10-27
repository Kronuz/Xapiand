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

// Unit tests for the text adapter: queryparser's tree, translated to an engine query, must give the
// Xapian query that Xapian's own parser gives for the settings Xapiand uses, and its errors must
// carry Xapian's classes. The queries that differ on purpose are pinned as differences.

#include "test_harness.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "engine/query.h"
#include "engine/xapian/query.h"
#include "engine/xapian/text_parser.h"
#include "xapian.h"

namespace {

using engine::Op;
using engine::Query;
using engine::Stem;
using engine::xapian::parse_text_queryparser;
using engine::xapian::to_xapian;
namespace text = engine::text;

// Xapian's parser is the reference, whatever the environment says.
const int unset_switch = (::unsetenv("XAPIAND_QUERYPARSER"), 0);

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
xapian_reading(const std::string& text, const engine::TextConfig& c)
{
	return to_xapian(Query::text(text, c)).get_description();
}

std::string
adapter_reading(const std::string& text, const engine::TextConfig& c)
{
	auto parsed = parse_text_queryparser(text, c);
	if (!parsed || parsed->failed) {
		CHECK_TRUE(false);
		return "<unreadable>";
	}
	return to_xapian(parsed->query).get_description();
}

struct Case {
	const char* text;
	engine::TextConfig config;
};

}  // namespace

void
test_agrees_with_xapian()
{
	const unsigned boolean = base | text::BOOLEAN;
	const unsigned wild = base | text::WILDCARD | text::WILDCARD_SINGLE | text::WILDCARD_MULTI;
	const std::vector<Case> cases = {
		{ "quick brown fox", config(base) },
		{ "\"quick brown\" fox", config(base) },
		{ "+quick -brown fox", config(base) },
		{ "quick~ brown~1 fox~2", config(base) },
		{ "title:obama a-b 1990s-era", config(base) },
		{ "quick OR brown", config(base) },
		{ "quick*  br?wn q*k", config(wild) },
		{ "quick bro", config(base | text::PARTIAL) },
		{ "quick AND (brown OR fox) NOT lazy", config(boolean, Op::AND) },
		{ "quick XOR brown", config(boolean, Op::OR) },
		{ "quick NEAR brown fox ADJ lazy", config(boolean, Op::AND) },
		{ "quick NEAR/3 brown", config(boolean, Op::OR) },
		{ "((quick))", config(boolean, Op::AND) },
		{ "running dogs jumped", config(boolean, Op::AND, "en", "en", Stem::some) },
		{ "running dogs jumped", config(base, Op::INVALID, "en", "en", Stem::all) },
		{ "running dogs jumped", config(base, Op::INVALID, "en", "en", Stem::all_z) },
		{ "running dogs jumped", config(base, Op::INVALID, "", "en", Stem::none) },
		{ "the quick and the dead", config(base, Op::INVALID, "en", "en", Stem::some) },
		{ "a b c d", config(base, Op::NEAR) },
		{ "a b c d", config(base, Op::PHRASE) },
		{ "a b c d", config(base, Op::SYNONYM) },
		{ "a b c d", config(base, Op::MAX) },
		{ "a b c d", config(base, Op::ELITE_SET) },
		{ "", config(base) },
	};
	for (const auto& c : cases) {
		CHECK_EQ(adapter_reading(c.text, c.config), xapian_reading(c.text, c.config));
	}
}

void
test_errors_keep_their_class()
{
	// A syntax error is Xapian::QueryParserError with Xapian's message.
	auto c = config(base | text::BOOLEAN, Op::AND);
	auto parsed = parse_text_queryparser("AND AND", c);
	REQUIRE(parsed.has_value());
	CHECK_TRUE(parsed->failed);
	CHECK_FALSE(parsed->unimplemented);
	std::string message;
	try {
		to_xapian(Query::text("AND AND", c));
	} catch (const Xapian::QueryParserError& exc) {
		message = exc.get_msg();
	}
	CHECK_EQ(parsed->error, message);
	CHECK_THROW(engine::xapian::throw_text_error(*parsed), Xapian::QueryParserError);

	// NEAR over phrases is Xapian::UnimplementedError.
	c = config(base, Op::NEAR);
	parsed = parse_text_queryparser("\"a b\" \"c d\"", c);
	REQUIRE(parsed.has_value());
	CHECK_TRUE(parsed->failed);
	CHECK_TRUE(parsed->unimplemented);
	CHECK_THROW(engine::xapian::throw_text_error(*parsed), Xapian::UnimplementedError);
	CHECK_THROW(to_xapian(Query::text("\"a b\" \"c d\"", c)), Xapian::UnimplementedError);
}

void
test_unreadable_settings_fall_back()
{
	CHECK_FALSE(parse_text_queryparser("quick", config(base | text::WORD_BREAKS)).has_value());
	CHECK_FALSE(parse_text_queryparser("quick", config(base, Op::INVALID, "", "no-such-language", Stem::some)).has_value());
	CHECK_FALSE(parse_text_queryparser("quick", config(base, Op::AND_NOT)).has_value());
}

void
test_long_words_are_kept()
{
	// Xapian keeps a long word; the index side bounds it.
	const std::string word(200, 'a');
	auto c = config(base);
	CHECK_EQ(adapter_reading(word + " b", c), xapian_reading(word + " b", c));
}

void
test_nesting_is_not_recursive()
{
	// 200,000 brackets: the tree is built and translated without the call stack.
	const std::string deep = std::string(200000, '(') + "a" + std::string(200000, ')');
	auto parsed = parse_text_queryparser(deep, config(base | text::BOOLEAN, Op::AND));
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->failed);
}

void
test_known_differences()
{
	// Xapian 2.0 stems a capitalized word under Stem::some; queryparser follows 2.1 and does not.
	auto c = config(base, Op::INVALID, "en", "en", Stem::some);
	CHECK_NE(adapter_reading("Barack", c), xapian_reading("Barack", c));
	CHECK_EQ(adapter_reading("barack", c), xapian_reading("barack", c));
}

TEST_MAIN(test_agrees_with_xapian, test_errors_keep_their_class, test_unreadable_settings_fall_back, test_long_words_are_kept, test_nesting_is_not_recursive, test_known_differences)
