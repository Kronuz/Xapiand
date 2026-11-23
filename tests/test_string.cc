/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_harness.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unistd.h>

static void test_hello() {
	CHECK_EQ(std::string("Hello"), "Hello");
}

static int require_calls;
static bool require_after;

static int next_call() { return ++require_calls; }

static void require_passes() {
	REQUIRE(next_call() == 1);
	REQUIRE_EQ(next_call(), 2);
}

static void require_fails() {
	REQUIRE(next_call() == 0);
	require_after = true;
}

static void require_eq_fails() {
	REQUIRE_EQ(next_call(), 0);
	require_after = true;
}

// Runs fn with stderr captured and returns what it printed.
static std::string capture_stderr(void (*fn)()) {
	std::fflush(stderr);
	auto* tmp = std::tmpfile();
	const int saved = dup(2);
	dup2(fileno(tmp), 2);
	fn();
	std::fflush(stderr);
	dup2(saved, 2);
	close(saved);
	std::rewind(tmp);
	std::string out;
	char buf[256];
	while (auto n = std::fread(buf, 1, sizeof(buf), tmp)) {
		out.append(buf, n);
	}
	std::fclose(tmp);
	return out;
}

static void test_require_evaluates_once() {
	require_calls = 0;
	int checks = testh::checks;
	int failures = testh::failures;
	const auto ok_err = capture_stderr(require_passes);
	const int ok_checks = testh::checks - checks;
	const int ok_failures = testh::failures - failures;
	CHECK(ok_err.empty());
	CHECK_EQ(require_calls, 2);  // one evaluation per macro
	CHECK_EQ(ok_checks, 2);
	CHECK_EQ(ok_failures, 0);

	for (auto fn : {require_fails, require_eq_fails}) {
		require_calls = 0;
		require_after = false;
		failures = testh::failures;
		const auto err = capture_stderr(fn);
		const int new_failures = testh::failures - failures;
		testh::failures = failures;  // the failure is intended
		CHECK_EQ(require_calls, 1);
		CHECK(!require_after);
		CHECK_EQ(new_failures, 1);
		CHECK(err.find("FAIL") != std::string::npos);
		CHECK(err.find("REQUIRE") != std::string::npos);
		CHECK_EQ(std::count(err.begin(), err.end(), '\n'), 1);
	}
}

TEST_MAIN(test_hello, test_require_evaluates_once)
