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

#include "test_msgpack.h"

#include "gtest/gtest.h"


TEST(MsgPack, correct_cpp) {
	EXPECT_EQ(test_correct_cpp(), 0);
}


TEST(MsgPack, constructors) {
	EXPECT_EQ(test_constructors(), 0);
}


TEST(MsgPack, assigment) {
	EXPECT_EQ(test_assigment(), 0);
}


TEST(MsgPack, iterator) {
	EXPECT_EQ(test_iterator(), 0);
}


TEST(MsgPack, serialise) {
	EXPECT_EQ(test_serialise(), 0);
}


TEST(MsgPack, unserialise) {
	EXPECT_EQ(test_unserialise(), 0);
}


TEST(MsgPack, explore) {
	EXPECT_EQ(test_explore(), 0);
}


TEST(MsgPack, copy) {
	EXPECT_EQ(test_copy(), 0);
}


TEST(MsgPack, reference) {
	EXPECT_EQ(test_reference(), 0);
}


TEST(MsgPack, path) {
	EXPECT_EQ(test_path(), 0);
}


TEST(MsgPack, erase) {
	EXPECT_EQ(test_msgpack_erase(), 0);
}


TEST(MsgPack, reserve) {
	EXPECT_EQ(test_reserve(), 0);
}


TEST(MsgPack, keys) {
	EXPECT_EQ(test_keys(), 0);
}


TEST(MsgPack, change_keys) {
	EXPECT_EQ(test_change_keys(), 0);
}


TEST(MsgPack, update_map) {
	EXPECT_EQ(test_map(), 0);
}
