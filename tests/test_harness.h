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

#pragma once

// A tiny, dependency-free unit-test harness, matching the style of the standalone
// Kronuz libraries so the whole ecosystem tests the same way (no GoogleTest, no
// FetchContent, no framework build). A test is a `void test_*()` function; list
// the ones to run with TEST_MAIN(...).
//
// By design the CHECK macros only stringize the *expressions* on failure -- they
// never try to stream the values. That keeps them from instantiating a value
// printer on the types under test, which is what makes GoogleTest's universal
// printer trip over msgpack's greedy object adaptor here.

#include <cstdio>
#include <exception>

namespace testh {
inline int checks = 0;
inline int failures = 0;
}  // namespace testh

#define CHECK(cond) \
	do { \
		++testh::checks; \
		if (!(cond)) { \
			std::fprintf(stderr, "  FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			++testh::failures; \
		} \
	} while (0)

#define CHECK_TRUE(cond) CHECK(cond)
#define CHECK_FALSE(cond) CHECK(!(cond))

#define CHECK_OP(a, op, b) \
	do { \
		++testh::checks; \
		if (!((a) op (b))) { \
			std::fprintf(stderr, "  FAIL %s:%d: CHECK(%s %s %s)\n", __FILE__, __LINE__, #a, #op, #b); \
			++testh::failures; \
		} \
	} while (0)

#define CHECK_EQ(a, b) CHECK_OP(a, ==, b)
#define CHECK_NE(a, b) CHECK_OP(a, !=, b)
#define CHECK_LT(a, b) CHECK_OP(a, <, b)
#define CHECK_LE(a, b) CHECK_OP(a, <=, b)
#define CHECK_GT(a, b) CHECK_OP(a, >, b)
#define CHECK_GE(a, b) CHECK_OP(a, >=, b)

// REQUIRE_* abort the current test function (they return void) when they fail,
// for preconditions where continuing would crash or be meaningless.
#define REQUIRE(cond) \
	do { \
		CHECK(cond); \
		if (!(cond)) return; \
	} while (0)

#define REQUIRE_EQ(a, b) \
	do { \
		CHECK_EQ(a, b); \
		if (!((a) == (b))) return; \
	} while (0)

#define CHECK_THROW(expr, exc) \
	do { \
		++testh::checks; \
		bool threw = false; \
		try { (void)(expr); } catch (const exc&) { threw = true; } catch (...) {} \
		if (!threw) { \
			std::fprintf(stderr, "  FAIL %s:%d: CHECK_THROW(%s, %s)\n", __FILE__, __LINE__, #expr, #exc); \
			++testh::failures; \
		} \
	} while (0)

// Run the listed test functions and report. Returns non-zero if any check failed.
#define TEST_MAIN(...) \
	int main() { \
		void (*const tests[])() = { __VA_ARGS__ }; \
		for (auto* t : tests) { t(); } \
		if (testh::failures != 0) { \
			std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks); \
			return 1; \
		} \
		std::printf("OK: %d checks passed\n", testh::checks); \
		return 0; \
	}
