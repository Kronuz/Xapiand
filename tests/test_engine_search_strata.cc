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

// Unit tests for engine::search. One corpus is written to a strata shard set; every query runs through
// the seam and is checked against a model of the corpus: the documents found and the estimate are
// exact, the order is by weight, and the weights (BM25 over the model's statistics) stay within a stated
// tolerance. Then deletes and replacements, segments, shards that interleave, and what it refuses.

#include "test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "engine/error.h"

#include "test_engine_search_strata/support.h"

using namespace detail;

namespace {

// Reports an engine error with its text, which the bare terminate message of an uncaught one lacks.
template <typename F>
void guarded(const char* name, F&& test) {
	if (const char* only = std::getenv("ONLY_TEST"); only != nullptr && std::string(only) != name) {
		return;
	}
	try {
		test();
	} catch (const engine::Error& error) {
		std::fprintf(stderr, "%s: engine error (%s): %s\n", name, engine::to_string(error.kind), error.description().c_str());
		CHECK_TRUE(false);
	}
}

void test_one_shard() { guarded("test_one_shard", test_one_shard_impl); }
void test_positional() { guarded("test_positional", test_positional_impl); }
void test_append() { guarded("test_append", test_append_impl); }
void test_fields() { guarded("test_fields", test_fields_impl); }
void test_two_shards() { guarded("test_two_shards", test_two_shards_impl); }
void test_spy() { guarded("test_spy", test_spy_impl); }
void test_five_shards() { guarded("test_five_shards", test_five_shards_impl); }
void test_no_shards_and_empty() { guarded("test_no_shards_and_empty", test_no_shards_and_empty_impl); }
void test_writes_and_deletes() { guarded("test_writes_and_deletes", test_writes_and_deletes_impl); }
void test_refusals() { guarded("test_refusals", test_refusals_impl); }
void test_invalid_shapes() { guarded("test_invalid_shapes", test_invalid_shapes_impl); }
void test_phrase_alternatives() { guarded("test_phrase_alternatives", test_phrase_alternatives_impl); }
void test_long_chains() { guarded("test_long_chains", test_long_chains_impl); }
void test_snapshot_consistency() { guarded("test_snapshot_consistency", test_snapshot_consistency_impl); }
void test_concurrent_searches() { guarded("test_concurrent_searches", test_concurrent_searches_impl); }
void test_view_holds_nothing() { guarded("test_view_holds_nothing", test_view_holds_nothing_impl); }
void test_pin_after_failed_reopen() { guarded("test_pin_after_failed_reopen", test_pin_after_failed_reopen_impl); }
void test_huge_window() { guarded("test_huge_window", test_huge_window_impl); }
void test_percent_infinite() { guarded("test_percent_infinite", test_percent_infinite_impl); }
void test_collapse() { guarded("test_collapse", test_collapse_impl); }

}  // namespace

TEST_MAIN(test_one_shard, test_positional, test_append, test_fields, test_two_shards, test_spy, test_five_shards, test_no_shards_and_empty, test_writes_and_deletes, test_refusals, test_invalid_shapes, test_phrase_alternatives, test_long_chains, test_concurrent_searches, test_snapshot_consistency, test_view_holds_nothing, test_pin_after_failed_reopen, test_huge_window, test_percent_infinite, test_collapse)
