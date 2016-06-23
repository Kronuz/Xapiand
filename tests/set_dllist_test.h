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

#pragma once

#include "test_dllist.h"

#include "gtest/gtest.h"


TEST(DLList, iterators) {
	EXPECT_EQ(test_iterators(), 0);
}


TEST(DLList, push_front) {
	EXPECT_EQ(test_push_front(), 0);
}


TEST(DLList, emplace_front) {
	EXPECT_EQ(test_emplace_front(), 0);
}


TEST(DLList, push_back) {
	EXPECT_EQ(test_push_back(), 0);
}


TEST(DLList, emplace_back) {
	EXPECT_EQ(test_emplace_back(), 0);
}


TEST(DLList, insert) {
	EXPECT_EQ(test_insert(), 0);
}


TEST(DLList, pop_front) {
	EXPECT_EQ(test_pop_front(), 0);
}


TEST(DLList, pop_back) {
	EXPECT_EQ(test_pop_back(), 0);
}


TEST(DLList, erase) {
	EXPECT_EQ(test_erase(), 0);
}


TEST(DLList, single_producer_consumer) {
	EXPECT_EQ(test_single_producer_consumer(), 0);
}


TEST(DLList, multi_push_emplace_front) {
	EXPECT_EQ(test_multi_push_emplace_front(), 0);
}


TEST(DLList, multi_push_emplace_back) {
	EXPECT_EQ(test_multi_push_emplace_back(), 0);
}


TEST(DLList, multi_insert) {
	EXPECT_EQ(test_multi_insert(), 0);
}


TEST(DLList, multi_producers) {
	EXPECT_EQ(test_multi_producers(), 0);
}


TEST(DLList, multi_push_pop_front) {
	EXPECT_EQ(test_multi_push_pop_front(), 0);
}


TEST(DLList, multi_push_pop_back) {
	EXPECT_EQ(test_multi_push_pop_back(), 0);
}


TEST(DLList, multi_insert_erase) {
	EXPECT_EQ(test_multi_insert_erase(), 0);
}


TEST(DLList, multiple_producers_single_consumer) {
	EXPECT_EQ(test_multiple_producers_single_consumer(), 0);
}


TEST(DLList, single_producer_multiple_consumers) {
	EXPECT_EQ(test_single_producer_multiple_consumers(), 0);
}
