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

// The open-index pool defaults to what 1% of RAM holds at ~40 KiB per open index; an explicit size wins.

#include "test_harness.h"

#include "opts.h"

namespace {

constexpr std::uint64_t GiB = std::uint64_t{1} << 30;

void test_the_default_follows_ram() {
	CHECK_EQ(database_pool_size_for(0, 16 * GiB), 4194u);
	CHECK_EQ(database_pool_size_for(0, 64 * GiB), 16777u);
	CHECK(database_pool_size_for(0, 16 * GiB) > database_pool_size_for(0, 8 * GiB));
}

void test_the_default_is_never_zero() {
	CHECK_EQ(database_pool_size_for(0, 0), 1u);
}

void test_an_explicit_size_wins() {
	CHECK_EQ(database_pool_size_for(5000, 1 * GiB), 5000u);
	CHECK_EQ(database_pool_size_for(3, 64 * GiB), 3u);
}

}  // namespace

TEST_MAIN(
	test_the_default_follows_ram,
	test_the_default_is_never_zero,
	test_an_explicit_size_wins
)
