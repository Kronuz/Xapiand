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

#include "gtest/gtest.h"

#include <string_view>

#include "field_parser.h"

TEST(FieldParser, FieldAndBareValue) {
	FieldParser fp("Color:Blue");
	fp.parse();
	EXPECT_EQ(fp.get_field_name(), std::string_view("Color"));
	EXPECT_EQ(fp.get_value(), std::string_view("Blue"));
	EXPECT_FALSE(fp.is_range());
	EXPECT_FALSE(fp.is_single_quoted_value());
	EXPECT_FALSE(fp.is_double_quoted_value());
}

TEST(FieldParser, BareValueNoField) {
	FieldParser fp("green");
	fp.parse();
	EXPECT_TRUE(fp.get_field_name().empty());
	EXPECT_EQ(fp.get_value(), std::string_view("green"));
	EXPECT_FALSE(fp.is_range());
}

TEST(FieldParser, DoubleQuotedValue) {
	FieldParser fp("Color:\"dark blue\"");
	fp.parse();
	EXPECT_EQ(fp.get_field_name(), std::string_view("Color"));
	EXPECT_TRUE(fp.is_double_quoted_value());
	EXPECT_FALSE(fp.is_single_quoted_value());
	EXPECT_EQ(fp.get_value(), std::string_view("dark blue"));   // quotes stripped
}

TEST(FieldParser, SingleQuotedValue) {
	FieldParser fp("Color:'light blue'");
	fp.parse();
	EXPECT_EQ(fp.get_field_name(), std::string_view("Color"));
	EXPECT_TRUE(fp.is_single_quoted_value());
	EXPECT_FALSE(fp.is_double_quoted_value());
	EXPECT_EQ(fp.get_value(), std::string_view("light blue"));
}

TEST(FieldParser, BracketRange) {
	FieldParser fp("[100,200]");
	fp.parse();
	EXPECT_TRUE(fp.get_field_name().empty());
	EXPECT_TRUE(fp.is_range());
	EXPECT_EQ(fp.range, FieldParser::Range::closed);
	EXPECT_EQ(fp.get_start(), std::string_view("100"));
	EXPECT_EQ(fp.get_end(), std::string_view("200"));
}

TEST(FieldParser, FieldWithBracketRange) {
	FieldParser fp("Field:[a70d0d,ec500d]");
	fp.parse();
	EXPECT_EQ(fp.get_field_name(), std::string_view("Field"));
	EXPECT_TRUE(fp.is_range());
	EXPECT_EQ(fp.get_start(), std::string_view("a70d0d"));
	EXPECT_EQ(fp.get_end(), std::string_view("ec500d"));
}

TEST(FieldParser, DotDotRange) {
	FieldParser fp("Field:100..200");
	fp.parse();
	EXPECT_EQ(fp.get_field_name(), std::string_view("Field"));
	EXPECT_TRUE(fp.is_range());
	EXPECT_EQ(fp.get_start(), std::string_view("100"));
	EXPECT_EQ(fp.get_end(), std::string_view("200"));
}

TEST(FieldParser, QuotedRangeEndpoints) {
	FieldParser fp("['initial range','end of range']");
	fp.parse();
	EXPECT_TRUE(fp.is_range());
	EXPECT_EQ(fp.get_start(), std::string_view("initial range"));
	EXPECT_EQ(fp.get_end(), std::string_view("end of range"));
}
