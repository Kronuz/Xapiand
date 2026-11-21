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

// Unit tests for the engine seam's Query and callbacks: the shape a query keeps, what it matches
// on a strata shard, and how the host callbacks (sources, key makers, spies) behave in a search.

#include "test_harness.h"

#include <cstdio>
#include <cstdlib>

#include "engine/error.h"

#include "test_engine_query/support.h"

namespace {

// The stop words are read from the source tree.
const int data_path = (::setenv("XAPIAND_DATA_PATH", XAPIAND_SOURCE_DIR, 1), 0);

}  // namespace

using namespace detail;

// An engine error that escapes a test is a failure with its text, not an abort.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const engine::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw engine error %s\n", #fn, exc.reason().c_str()); \
			++testh::failures; \
		} \
	}

GUARDED(test_leaves_and_ops)
GUARDED(test_emptiness)
GUARDED(test_registered_sources)
GUARDED(test_unserialize_garbage)
GUARDED(test_expansions)
GUARDED(test_expansion_limits)
GUARDED(test_append)
GUARDED(test_text)
GUARDED(test_posting_source)
GUARDED(test_key_maker)
GUARDED(test_match_spy)
GUARDED(test_source_ownership)
GUARDED(test_wildcard_patterns)
GUARDED(test_fuzzy_limit)
GUARDED(test_binary_equals_list)
GUARDED(test_append_chain)
GUARDED(test_depth_cap)
GUARDED(test_chain_cap)
GUARDED(test_corrupt_stored_value)
GUARDED(test_dsl_not_is_none_of)
GUARDED(test_dsl_one_character_keyword_is_a_term)
GUARDED(test_range_aggregation_open_bounds)

TEST_MAIN(test_leaves_and_ops_guarded, test_emptiness_guarded, test_registered_sources_guarded, test_unserialize_garbage_guarded, test_expansions_guarded, test_expansion_limits_guarded, test_append_guarded, test_text_guarded, test_posting_source_guarded, test_key_maker_guarded, test_match_spy_guarded, test_source_ownership_guarded, test_wildcard_patterns_guarded, test_fuzzy_limit_guarded, test_binary_equals_list_guarded, test_append_chain_guarded, test_depth_cap_guarded, test_chain_cap_guarded, test_corrupt_stored_value_guarded, test_dsl_not_is_none_of_guarded, test_dsl_one_character_keyword_is_a_term_guarded, test_range_aggregation_open_bounds_guarded)
