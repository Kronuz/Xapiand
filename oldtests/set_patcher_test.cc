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

#include "test_patcher.h"

#include "gtest/gtest.h"

#include "utils.h"


TEST(Patcher, Working) {
	EXPECT_EQ(test_patcher_mix(), 0);
	EXPECT_EQ(test_patcher_add(), 0);
	EXPECT_EQ(test_patcher_remove(), 0);
	EXPECT_EQ(test_patcher_replace(), 0);
	EXPECT_EQ(test_patcher_move(), 0);
	EXPECT_EQ(test_patcher_copy(), 0);
	EXPECT_EQ(test_patcher_test(), 0);
	EXPECT_EQ(test_patcher_incr(), 0);
	EXPECT_EQ(test_patcher_decr(), 0);
	EXPECT_EQ(test_patcher_rfc6901(), 0);
}


int main(int argc, char **argv) {
	auto initializer = Initializer::create();
	::testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	initializer.destroy();
	return ret;
}
