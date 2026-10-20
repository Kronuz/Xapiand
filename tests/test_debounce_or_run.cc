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

#include "test_harness.h"

#include <vector>

// Counts the past-horizon reports instead of logging and asserting.
static int past_horizon_reports = 0;
#define DEBOUNCE_OR_RUN_PAST_HORIZON() (++past_horizon_reports)
#include "../src/debounce_or_run.h"

// Mirrors the scheduler's enum; debounce_or_run only needs its enumerators.
enum class schedule_result { scheduled, past_horizon, stopping, no_memory };

// Returns the scripted results in order, recording each touch.
struct FakeDebouncer {
	std::vector<schedule_result> results;
	int touches = 0;
	schedule_result debounce(int, int) {
		return results[touches++];
	}
};

static int run(std::vector<schedule_result> results, int& touches, schedule_result& returned) {
	FakeDebouncer debouncer{std::move(results)};
	int runs = 0;
	past_horizon_reports = 0;
	returned = debounce_or_run(debouncer, [&](int) { ++runs; }, 1, 7);
	touches = debouncer.touches;
	return runs;
}

static void test_scheduled_runs_nothing() {
	int touches; schedule_result r;
	CHECK_EQ(run({schedule_result::scheduled}, touches, r), 0);
	CHECK_EQ(touches, 1);
	CHECK(r == schedule_result::scheduled);
}

static void test_stopping_runs_now() {
	int touches; schedule_result r;
	CHECK_EQ(run({schedule_result::stopping}, touches, r), 1);
	CHECK_EQ(touches, 1);
	CHECK(r == schedule_result::stopping);
}

static void test_no_memory_retries_once() {
	int touches; schedule_result r;
	CHECK_EQ(run({schedule_result::no_memory, schedule_result::scheduled}, touches, r), 0);
	CHECK_EQ(touches, 2);
	CHECK(r == schedule_result::scheduled);
	CHECK_EQ(run({schedule_result::no_memory, schedule_result::no_memory}, touches, r), 1);
	CHECK_EQ(touches, 2);
	CHECK(r == schedule_result::no_memory);
}

static void test_past_horizon_runs_nothing() {
	int touches; schedule_result r;
	CHECK_EQ(run({schedule_result::past_horizon}, touches, r), 0);
	CHECK_EQ(touches, 1);
	CHECK(r == schedule_result::past_horizon);
	CHECK_EQ(past_horizon_reports, 1);
	CHECK_EQ(run({schedule_result::no_memory, schedule_result::past_horizon}, touches, r), 0);
	CHECK_EQ(past_horizon_reports, 1);
}

TEST_MAIN(test_scheduled_runs_nothing, test_stopping_runs_now, test_no_memory_retries_once, test_past_horizon_runs_nothing)
