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

// Coverage tests for src/field_parser.cc: every state of the parser, with the field name, value, quote flavor, range kind and
// endpoints it leaves for each form, the level bound, and each syntax error with the text callers show. Behavior that looks
// wrong is pinned as it is today and named "finding".

#include "test_harness.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "field_parser.h"

namespace {

using std::string_view;

bool refused(const std::string& text, const char* why, std::size_t lvl_max = 2) {
	FieldParser fp(text);
	try {
		fp.parse(lvl_max);
	} catch (const FieldParserError& e) {
		return std::string(e.what()).find(why) != std::string::npos;
	}
	return false;
}

void test_empty_and_blank_text_hold_nothing() {
	for (const char* text : { "", "   ", " \t\r\n" }) {
		FieldParser fp(text);
		fp.parse();
		CHECK(fp.get_field_name().empty());
		CHECK(fp.get_value().empty());
		CHECK_FALSE(fp.is_range());
	}
	FieldParser fp("a:");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), string_view("a"));
	CHECK(fp.get_value().empty());
	CHECK(fp.get_values().empty());
}

void test_leading_whitespace_is_skipped_before_a_bare_value() {
	FieldParser fp(" \t\r\nabc");
	fp.parse();
	CHECK(fp.get_field_name().empty());
	CHECK_EQ(fp.get_value(), string_view("abc"));
	CHECK_EQ(fp.get_values(), string_view(" \t\r\nabc"));
}

void test_field_names() {
	FieldParser dotted("a.b:c");
	dotted.parse();
	CHECK_EQ(dotted.get_field_name(), string_view("a.b"));
	CHECK_EQ(dotted.get_value(), string_view("c"));

	FieldParser leading(".a:b");
	leading.parse();
	CHECK_EQ(leading.get_field_name(), string_view(".a"));
	CHECK_EQ(leading.get_value(), string_view("b"));

	// Finding: a space inside a field name is not counted, so the name is cut short ("a b" reads as "a ").
	FieldParser spaced("a b:c");
	spaced.parse();
	CHECK_EQ(spaced.get_field_name(), string_view("a "));
	CHECK_EQ(spaced.get_value(), string_view("c"));

	// Finding: the 1,024-byte bound on a field name never applies (its test runs only on the first byte).
	const std::string long_name(5000, 'x');
	FieldParser longer(long_name + ":v");
	longer.parse();
	CHECK_EQ(longer.get_field_name().size(), static_cast<std::size_t>(5000));
	CHECK_EQ(longer.get_value(), string_view("v"));
}

void test_a_value_keeps_colons_and_single_dots() {
	FieldParser colons("a:b:c");
	colons.parse();
	CHECK_EQ(colons.get_field_name(), string_view("a"));
	CHECK_EQ(colons.get_value(), string_view("b:c"));
	CHECK_EQ(colons.get_values(), string_view("b:c"));

	FieldParser dot("a:1.5");
	dot.parse();
	CHECK_EQ(dot.get_value(), string_view("1.5"));
	CHECK_FALSE(dot.is_range());

	FieldParser bare(".5");
	bare.parse();
	CHECK_EQ(bare.get_value(), string_view(".5"));
}

void test_whitespace_ends_a_value_in_error() {
	CHECK(refused("f:a b", "Syntax error in query"));
	CHECK(refused("f:a\tb", "Syntax error in query"));
	CHECK(refused("f:a\rb", "Syntax error in query"));
	CHECK(refused("f:a\nb", "Syntax error in query"));
	CHECK(refused("f:1..2 3", "Syntax error in query"));
	CHECK(refused("f:1..2\t3", "Syntax error in query"));
}

void test_quoted_values_at_the_start() {
	FieldParser dq("\"a b\"");
	dq.parse();
	CHECK(dq.get_field_name().empty());
	CHECK_EQ(dq.get_value(), string_view("a b"));
	CHECK(dq.is_double_quoted_value());
	CHECK_FALSE(dq.is_single_quoted_value());
	CHECK_EQ(dq.get_double_quoted_value(), string_view("\"a b\""));
	CHECK(dq.get_single_quoted_value().empty());

	FieldParser sq("'a b'");
	sq.parse();
	CHECK_EQ(sq.get_value(), string_view("a b"));
	CHECK(sq.is_single_quoted_value());
	CHECK_FALSE(sq.is_double_quoted_value());
	CHECK_EQ(sq.get_single_quoted_value(), string_view("'a b'"));
	CHECK(sq.get_double_quoted_value().empty());
}

void test_escapes_in_quoted_values() {
	// A backslash is kept, with the byte after it.
	FieldParser dq("f:\"a\\b\"");
	dq.parse();
	CHECK_EQ(dq.get_value(), string_view("a\\b"));
	CHECK_EQ(dq.get_double_quoted_value(), string_view("\"a\\b\""));
	FieldParser sq("f:'a\\b'");
	sq.parse();
	CHECK_EQ(sq.get_value(), string_view("a\\b"));
	CHECK_EQ(sq.get_single_quoted_value(), string_view("'a\\b'"));

	// Finding: an escaped quote closes the string (the byte after the backslash is tested as the closing quote), so the
	// string ends with the backslash and anything after the quote is an error.
	FieldParser edq("f:\"a\\\"");
	edq.parse();
	CHECK_EQ(edq.get_value(), string_view("a\\"));
	CHECK_EQ(edq.get_double_quoted_value(), string_view("\"a\\\""));
	FieldParser esq("f:'a\\'");
	esq.parse();
	CHECK_EQ(esq.get_value(), string_view("a\\"));
	CHECK(refused("f:\"a\\\"b\"", "Unexpected symbol: 'b'"));
}

void test_unterminated_quotes() {
	CHECK(refused("f:\"abc", "Expected symbol: '\"'"));
	CHECK(refused("f:'abc", "Expected symbol: '''"));
	CHECK(refused("f:\"a\\", "EOL while scanning quoted string"));
	CHECK(refused("f:'a\\", "EOL while scanning quoted string"));
	CHECK(refused("\"abc", "Expected symbol"));
}

void test_a_quoted_name_before_a_colon() {
	FieldParser sq("'my field':value");
	sq.parse();
	CHECK_EQ(sq.get_field_name(), string_view("my field"));
	CHECK_EQ(sq.get_value(), string_view("value"));
	CHECK_EQ(sq.get_values(), string_view("value"));
	CHECK_FALSE(sq.is_single_quoted_value());
	CHECK_FALSE(sq.is_double_quoted_value());

	// Finding: the double-quoted name resets the single-quote flag of the value, not its own, so the value reads as
	// double-quoted too.
	FieldParser dq("\"my field\":value");
	dq.parse();
	CHECK_EQ(dq.get_field_name(), string_view("my field"));
	CHECK_EQ(dq.get_value(), string_view("value"));
	CHECK(dq.is_double_quoted_value());
	CHECK_FALSE(dq.is_single_quoted_value());
}

void test_what_may_follow_a_closing_quote() {
	CHECK(refused("\"a\"x", "Unexpected symbol: 'x'"));
	CHECK(refused("\"a\".x", "Unexpected symbol: '.'"));
	CHECK(refused("'a' ", "Unexpected symbol: ' '"));
}

void test_dot_dot_ranges() {
	FieldParser both("1..2");
	both.parse();
	CHECK(both.get_field_name().empty());
	CHECK_EQ(both.range, FieldParser::Range::closed);
	CHECK_EQ(both.get_start(), string_view("1"));
	CHECK_EQ(both.get_end(), string_view("2"));

	FieldParser open_start("..2");
	open_start.parse();
	CHECK_EQ(open_start.range, FieldParser::Range::closed);
	CHECK(open_start.get_start().empty());
	CHECK_EQ(open_start.get_end(), string_view("2"));

	FieldParser field_open_start("f:..2");
	field_open_start.parse();
	CHECK_EQ(field_open_start.get_field_name(), string_view("f"));
	CHECK(field_open_start.get_start().empty());
	CHECK_EQ(field_open_start.get_end(), string_view("2"));

	FieldParser open_end("f:1..");
	open_end.parse();
	CHECK_EQ(open_end.get_start(), string_view("1"));
	CHECK(open_end.get_end().empty());

	FieldParser dotted_end("f:1..2.5");
	dotted_end.parse();
	CHECK_EQ(dotted_end.get_start(), string_view("1"));
	CHECK_EQ(dotted_end.get_end(), string_view("2.5"));
}

void test_dot_dot_levels() {
	FieldParser three("f:1..2..3");
	three.parse();
	CHECK_EQ(three.get_value(0), string_view("1"));
	CHECK_EQ(three.get_value(1), string_view("2"));
	CHECK_EQ(three.get_value(2), string_view("3"));
	CHECK(three.get_value(3).empty());

	// Two dot pairs in a row leave an empty second level.
	FieldParser gap("f:1....2");
	gap.parse();
	CHECK_EQ(gap.get_start(), string_view("1"));
	CHECK(gap.get_end().empty());
	CHECK_EQ(gap.get_value(2), string_view("2"));

	// A bound above LVL_MAX is clamped to it; nine dot pairs are within it.
	FieldParser many("1..2..3..4..5..6..7..8..9..10");
	many.parse(100);
	CHECK_EQ(many.get_value(0), string_view("1"));
	CHECK_EQ(many.get_value(9), string_view("10"));
}

void test_too_many_dot_dot_levels() {
	CHECK(refused("1..2..3..4", "Too many levels!"));              // the default is two levels
	CHECK(refused("a..b", "Too many levels!", 0));                  // from a field
	CHECK(refused("..a", "Too many levels!", 0));                   // from the start
	CHECK(refused("f:..a", "Too many levels!", 0));                 // from the start of a value
	CHECK(refused("f:1..2", "Too many levels!", 0));                // from a value
	CHECK(refused("f:1..2..3", "Too many levels!", 1));             // from the end of a range
	CHECK(refused("f:1....2", "Too many levels!", 1));              // from a dot pair
	CHECK(refused("\"a\"..2", "Too many levels!", 0));              // from a closing quote
	CHECK_FALSE(refused("f:1..2..3", "Too many levels!", 2));
}

void test_quoted_dot_dot_endpoints() {
	FieldParser quoted("\"a\"..2");
	quoted.parse();
	CHECK_EQ(quoted.range, FieldParser::Range::closed);
	CHECK_EQ(quoted.get_start(), string_view("a"));
	CHECK(quoted.is_double_quoted_value(0));
	CHECK_EQ(quoted.get_end(), string_view("2"));
	CHECK_FALSE(quoted.is_double_quoted_value(1));

	FieldParser both("'a b'..'c d'");
	both.parse();
	CHECK_EQ(both.get_start(), string_view("a b"));
	CHECK_EQ(both.get_end(), string_view("c d"));
	CHECK(both.is_single_quoted_value(0));
	CHECK(both.is_single_quoted_value(1));
	CHECK_EQ(both.get_single_quoted_value(1), string_view("'c d'"));
}

void test_bracket_kinds() {
	struct Case {
		const char* text;
		FieldParser::Range range;
	};
	const Case cases[] = {
		{ "[10,20]", FieldParser::Range::closed },
		{ "(10,20)", FieldParser::Range::open },
		{ "(10,20]", FieldParser::Range::closed_right },
		{ "[10,20)", FieldParser::Range::closed_left },
	};
	for (const auto& c : cases) {
		FieldParser fp(c.text);
		fp.parse();
		CHECK(fp.is_range());
		CHECK(fp.range == c.range);
		CHECK_EQ(fp.get_start(), string_view("10"));
		CHECK_EQ(fp.get_end(), string_view("20"));
	}
	FieldParser field("f:(1,2)");
	field.parse();
	CHECK_EQ(field.get_field_name(), string_view("f"));
	CHECK_EQ(field.range, FieldParser::Range::open);
	CHECK_EQ(field.get_values(), string_view("(1,2)"));
}

void test_empty_bracket_ends() {
	FieldParser no_start("[,5]");
	no_start.parse();
	CHECK(no_start.get_start().empty());
	CHECK_EQ(no_start.get_end(), string_view("5"));

	FieldParser no_end("[1,]");
	no_end.parse();
	CHECK_EQ(no_end.get_start(), string_view("1"));
	CHECK(no_end.get_end().empty());

	struct Case {
		const char* text;
		FieldParser::Range range;
	};
	const Case cases[] = {
		{ "[]", FieldParser::Range::closed },
		{ "()", FieldParser::Range::open },
		{ "(]", FieldParser::Range::closed_right },
		{ "[)", FieldParser::Range::closed_left },
	};
	for (const auto& c : cases) {
		FieldParser fp(c.text);
		fp.parse();
		CHECK(fp.range == c.range);
		CHECK(fp.get_start().empty());
		CHECK(fp.get_end().empty());
	}
}

void test_bracket_levels() {
	FieldParser three("[1,2,3]");
	three.parse();
	CHECK_EQ(three.get_value(0), string_view("1"));
	CHECK_EQ(three.get_value(1), string_view("2"));
	CHECK_EQ(three.get_value(2), string_view("3"));

	FieldParser spaced("[1 2,3]");
	spaced.parse();
	CHECK_EQ(spaced.get_start(), string_view("1 2"));

	CHECK(refused("[1,2,3]", "Too many levels!", 1));
	CHECK(refused("[,,]", "Too many levels!", 1));
	CHECK(refused("['a','b','c']", "Too many levels!", 1));
}

void test_unterminated_brackets() {
	CHECK(refused("[", "Syntax error in query"));
	CHECK(refused("(", "Syntax error in query"));
	CHECK(refused("f:[", "Syntax error in query"));
	CHECK(refused("[1", "Expected symbol: ']'"));
	CHECK(refused("[1,2", "Expected symbol: ']'"));
}

void test_quoted_bracket_endpoints() {
	FieldParser sq("['a b','c d']");
	sq.parse();
	CHECK_EQ(sq.range, FieldParser::Range::closed);
	CHECK_EQ(sq.get_start(), string_view("a b"));
	CHECK_EQ(sq.get_end(), string_view("c d"));
	CHECK(sq.is_single_quoted_value(0));
	CHECK(sq.is_single_quoted_value(1));
	CHECK_EQ(sq.get_single_quoted_value(0), string_view("'a b'"));
	CHECK_EQ(sq.get_single_quoted_value(1), string_view("'c d'"));

	FieldParser dq("[\"x\",\"y\"]");
	dq.parse();
	CHECK_EQ(dq.get_start(), string_view("x"));
	CHECK_EQ(dq.get_end(), string_view("y"));
	CHECK(dq.is_double_quoted_value(0));
	CHECK_EQ(dq.get_double_quoted_value(1), string_view("\"y\""));

	FieldParser open_quoted("('a')");
	open_quoted.parse();
	CHECK_EQ(open_quoted.range, FieldParser::Range::open);
	CHECK_EQ(open_quoted.get_start(), string_view("a"));
	FieldParser left("['a')");
	left.parse();
	CHECK_EQ(left.range, FieldParser::Range::closed_left);
	FieldParser right("('a']");
	right.parse();
	CHECK_EQ(right.range, FieldParser::Range::closed_right);
}

void test_escapes_in_quoted_bracket_endpoints() {
	FieldParser dq("[\"a\\b\"]");
	dq.parse();
	CHECK_EQ(dq.get_start(), string_view("a\\b"));
	CHECK_EQ(dq.get_double_quoted_value(), string_view("\"a\\b\""));
	FieldParser sq("['a\\b']");
	sq.parse();
	CHECK_EQ(sq.get_start(), string_view("a\\b"));
	CHECK_EQ(sq.get_single_quoted_value(), string_view("'a\\b'"));

	// Finding: as in a quoted value, an escaped quote closes the string.
	FieldParser escaped("['a\\']");
	escaped.parse();
	CHECK_EQ(escaped.get_start(), string_view("a\\"));
	CHECK(refused("['a\\'b']", "Unexpected symbol: 'b'"));
	CHECK(refused("['a\\", "EOL while scanning quoted string"));
	CHECK(refused("[\"a\\", "EOL while scanning quoted string"));
}

void test_what_may_follow_a_quoted_bracket_endpoint() {
	CHECK(refused("['a'x]", "Unexpected symbol: 'x'"));
	CHECK(refused("[\"a\" ]", "Unexpected symbol: ' '"));
}

void test_getters_past_the_last_level() {
	FieldParser fp("a:1");
	fp.parse();
	CHECK(fp.get_value(1).empty());
	CHECK_FALSE(fp.is_double_quoted_value(3));
	CHECK_FALSE(fp.is_single_quoted_value(2));
	CHECK(fp.get_double_quoted_value(1).empty());
	CHECK(fp.get_single_quoted_value(2).empty());
	CHECK(fp.get_start().empty());
	CHECK(fp.get_end().empty());
}

// Finding (not run: it never returns): in the state that scans a quoted bracket endpoint, the end of the text is
// `case '\0': break;` and the loop does not advance past it, so `['abc` spins forever.

}  // namespace

TEST_MAIN(
	test_empty_and_blank_text_hold_nothing, test_leading_whitespace_is_skipped_before_a_bare_value, test_field_names,
	test_a_value_keeps_colons_and_single_dots, test_whitespace_ends_a_value_in_error, test_quoted_values_at_the_start,
	test_escapes_in_quoted_values, test_unterminated_quotes, test_a_quoted_name_before_a_colon,
	test_what_may_follow_a_closing_quote, test_dot_dot_ranges, test_dot_dot_levels, test_too_many_dot_dot_levels,
	test_quoted_dot_dot_endpoints, test_bracket_kinds, test_empty_bracket_ends, test_bracket_levels,
	test_unterminated_brackets, test_quoted_bracket_endpoints, test_escapes_in_quoted_bracket_endpoints,
	test_what_may_follow_a_quoted_bracket_endpoint, test_getters_past_the_last_level)
