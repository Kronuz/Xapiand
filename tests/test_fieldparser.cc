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

// Unit tests for FieldParser, which splits query terms of the form
//   field:value        (optionally quoted)
//   [start,end]        (a range, also start..end)
//   field:[start,end]
// into their field name, value, quote flavor, and range endpoints. Revived and
// modernized from the retired oldtests fieldparser suite.

#include "test_harness.h"

#include <string_view>

#include "field_parser.h"

static void test_FieldAndBareValue() {
	FieldParser fp("Color:Blue");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), std::string_view("Color"));
	CHECK_EQ(fp.get_value(), std::string_view("Blue"));
	CHECK_FALSE(fp.is_range());
	CHECK_FALSE(fp.is_single_quoted_value());
	CHECK_FALSE(fp.is_double_quoted_value());
}

static void test_BareValueNoField() {
	FieldParser fp("green");
	fp.parse();
	CHECK(fp.get_field_name().empty());
	CHECK_EQ(fp.get_value(), std::string_view("green"));
	CHECK_FALSE(fp.is_range());
}

static void test_DoubleQuotedValue() {
	FieldParser fp("Color:\"dark blue\"");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), std::string_view("Color"));
	CHECK(fp.is_double_quoted_value());
	CHECK_FALSE(fp.is_single_quoted_value());
	CHECK_EQ(fp.get_value(), std::string_view("dark blue"));   // quotes stripped
}

static void test_SingleQuotedValue() {
	FieldParser fp("Color:'light blue'");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), std::string_view("Color"));
	CHECK(fp.is_single_quoted_value());
	CHECK_FALSE(fp.is_double_quoted_value());
	CHECK_EQ(fp.get_value(), std::string_view("light blue"));
}

static void test_BracketRange() {
	FieldParser fp("[100,200]");
	fp.parse();
	CHECK(fp.get_field_name().empty());
	CHECK(fp.is_range());
	CHECK_EQ(fp.range, FieldParser::Range::closed);
	CHECK_EQ(fp.get_start(), std::string_view("100"));
	CHECK_EQ(fp.get_end(), std::string_view("200"));
}

static void test_FieldWithBracketRange() {
	FieldParser fp("Field:[a70d0d,ec500d]");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), std::string_view("Field"));
	CHECK(fp.is_range());
	CHECK_EQ(fp.get_start(), std::string_view("a70d0d"));
	CHECK_EQ(fp.get_end(), std::string_view("ec500d"));
}

static void test_DotDotRange() {
	FieldParser fp("Field:100..200");
	fp.parse();
	CHECK_EQ(fp.get_field_name(), std::string_view("Field"));
	CHECK(fp.is_range());
	CHECK_EQ(fp.get_start(), std::string_view("100"));
	CHECK_EQ(fp.get_end(), std::string_view("200"));
}

static void test_QuotedRangeEndpoints() {
	FieldParser fp("['initial range','end of range']");
	fp.parse();
	CHECK(fp.is_range());
	CHECK_EQ(fp.get_start(), std::string_view("initial range"));
	CHECK_EQ(fp.get_end(), std::string_view("end of range"));
}


TEST_MAIN(test_FieldAndBareValue,test_BareValueNoField,test_DoubleQuotedValue,test_SingleQuotedValue,test_BracketRange,test_FieldWithBracketRange,test_DotDotRange,test_QuotedRangeEndpoints)
