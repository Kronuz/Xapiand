/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "test_msgpack.h"

#include "gtest/gtest.h"

#include "utils.h"


TEST(MsgPackTest, Working) {
	EXPECT_EQ(test_msgpack_constructors(), 0);
	EXPECT_EQ(test_msgpack_assigment(), 0);
	EXPECT_EQ(test_msgpack_iterator(), 0);
	EXPECT_EQ(test_msgpack_serialise(), 0);
	EXPECT_EQ(test_msgpack_unserialise(), 0);
	EXPECT_EQ(test_msgpack_explore(), 0);
	EXPECT_EQ(test_msgpack_copy(), 0);
	EXPECT_EQ(test_msgpack_reference(), 0);
	EXPECT_EQ(test_msgpack_path(), 0);
	EXPECT_EQ(test_msgpack_erase(), 0);
	EXPECT_EQ(test_msgpack_reserve(), 0);
	EXPECT_EQ(test_msgpack_keys(), 0);
	EXPECT_EQ(test_msgpack_change_keys(), 0);
	EXPECT_EQ(test_msgpack_map(), 0);
	EXPECT_EQ(test_msgpack_array(), 0);
}


int main(int argc, char **argv) {
	auto initializer = Initializer::create();
	::testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	initializer.destroy();
	return ret;
}
