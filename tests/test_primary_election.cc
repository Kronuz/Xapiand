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

#include <string>
#include <utility>
#include <vector>

#include "server/primary_election.h"

using primary_election::Copy;
using primary_election::elect;

static Copy dead() { return Copy{}; }
static Copy live(bool voted, unsigned long long revision = 1, std::string uuid = "A", bool eligible = true) {
	return Copy{true, voted, eligible, std::move(uuid), revision};
}
static Copy missing() { return live(true, 0, ""); }
static Copy gone(std::string uuid) { return Copy{false, true, true, std::move(uuid), 9}; }  // dead, with an old vote

// One replica: the only live copy is promoted, though it is half the copies.
static void test_one_replica_dead_primary() {
	CHECK_EQ(elect({dead(), live(true)}), 1);
}

static void test_primary_alive_keeps_it() {
	CHECK_EQ(elect({live(true), live(true)}), -1);
}

static void test_waits_for_every_live_copy() {
	CHECK_EQ(elect({dead(), live(true, 5), live(false, 9)}), -1);
	CHECK_EQ(elect({dead(), live(true, 5), live(true, 9)}), 2);
}

static void test_dead_replicas_do_not_block() {
	CHECK_EQ(elect({dead(), dead(), live(true, 3)}), 2);
}

static void test_prefers_existing_copy_then_revision() {
	CHECK_EQ(elect({dead(), missing(), live(true, 1)}), 2);
	CHECK_EQ(elect({dead(), live(true, 7), live(true, 4)}), 1);
	CHECK_EQ(elect({dead(), live(true, 4), live(true, 4)}), 1);
}

// A replica that never pulled its copy is still promotable, as an empty copy.
static void test_missing_copy_is_promotable() {
	CHECK_EQ(elect({dead(), missing()}), 1);
}

static void test_ineligible_and_no_copies() {
	CHECK_EQ(elect({dead(), live(true, 9, "A", false), live(true, 1)}), 2);
	CHECK_EQ(elect({dead(), live(true, 9, "A", false)}), -1);
	CHECK_EQ(elect({dead(), dead()}), -1);
	CHECK_EQ(elect({}), -1);
}

// Revisions compare only within one database: a copy of another one loses even
// with a higher revision.
static void test_other_database_is_not_promoted() {
	CHECK_EQ(elect({dead(), live(true, 50, "B"), live(true, 2), live(true, 7)}), 3);
	CHECK_EQ(elect({dead(), live(true, 7), live(true, 50, "B"), live(true, 2)}), 1);
}

// The shard's database is the one most live copies hold; dead copies do not count.
static void test_database_is_the_majority_of_live_copies() {
	CHECK_EQ(elect({dead(), live(true, 9, "B"), live(true, 3), live(true, 4)}), 3);
	CHECK_EQ(elect({gone("A"), gone("A"), live(true, 9, "B"), live(true, 3)}), 2);
}

// A tie between databases goes to the first in shard order.
static void test_database_tie_goes_to_shard_order() {
	CHECK_EQ(elect({dead(), live(true, 9, "B"), live(true, 30)}), 1);
	CHECK_EQ(elect({dead(), live(true, 30), live(true, 9, "B")}), 1);
}

// With the shard's database only on ineligible nodes, an empty copy is promoted
// before a copy of another database.
static void test_empty_copy_before_other_database() {
	CHECK_EQ(elect({dead(), live(true, 5, "A", false), live(true, 3, "A", false), live(true, 9, "B"), missing()}), 4);
	CHECK_EQ(elect({dead(), live(true, 5, "A", false), live(true, 3, "A", false), live(true, 9, "B")}), -1);
}

// A missing copy holds no database: it neither sets nor breaks the majority.
static void test_missing_copies_do_not_vote_a_database() {
	CHECK_EQ(elect({dead(), missing(), missing(), live(true, 9, "B")}), 3);
}

TEST_MAIN(
	test_one_replica_dead_primary,
	test_primary_alive_keeps_it,
	test_waits_for_every_live_copy,
	test_dead_replicas_do_not_block,
	test_prefers_existing_copy_then_revision,
	test_missing_copy_is_promotable,
	test_ineligible_and_no_copies,
	test_other_database_is_not_promoted,
	test_database_is_the_majority_of_live_copies,
	test_database_tie_goes_to_shard_order,
	test_empty_copy_before_other_database,
	test_missing_copies_do_not_vote_a_database
)
