/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_sort.h"

#include "gtest/gtest.h"


TEST(SortQueryTest, String) {
	EXPECT_EQ(sort_test_string_levens(), 0);
	EXPECT_EQ(sort_test_string_jaro(), 0);
	EXPECT_EQ(sort_test_string_jaro_w(), 0);
	EXPECT_EQ(sort_test_string_dice(), 0);
	EXPECT_EQ(sort_test_string_jaccard(), 0);
}


TEST(SortQueryTest, Numerical) {
	EXPECT_EQ(sort_test_numerical(), 0);
}


TEST(SortQueryTest, Date) {
	EXPECT_EQ(sort_test_date(), 0);
}


TEST(SortQueryTest, Bool) {
	EXPECT_EQ(sort_test_boolean(), 0);
}


TEST(SortQueryTest, Geo) {
	EXPECT_EQ(sort_test_geo(), 0);
}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
