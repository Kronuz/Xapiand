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

#include "test_patcher.h"

#include "gtest/gtest.h"


TEST(Patcher, mix) {
	EXPECT_EQ(test_mix(), 0);
}


TEST(Patcher, add) {
	EXPECT_EQ(test_add(), 0);
}


TEST(Patcher, remove) {
	EXPECT_EQ(test_remove(), 0);
}


TEST(Patcher, replace) {
	EXPECT_EQ(test_remove(), 0);
}


TEST(Patcher, move) {
	EXPECT_EQ(test_move(), 0);
}


TEST(Patcher, copy) {
	EXPECT_EQ(test_patcher_copy(), 0);
}


TEST(Patcher, test) {
	EXPECT_EQ(test_test(), 0);
}


TEST(Patcher, incr) {
	EXPECT_EQ(test_incr(), 0);
}


TEST(Patcher, decr) {
	EXPECT_EQ(test_decr(), 0);
}


TEST(Patcher, rfc6901) {
	EXPECT_EQ(test_rfc6901(), 0);
}
