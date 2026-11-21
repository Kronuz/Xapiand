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

#pragma once

// What the pieces of the query tests share: running a query on a shard and a shard of terms.

#include "../test_harness.h"

#include <algorithm>
#include <cstdlib>

#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <pthread.h>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/text.h"
#include "../engine_support.h"
#include "exception_engine.h"
#include "length.h"
#include "multivalue/geospatialrange.h"
#include "msgpack.h"
#include "multivalue/range.h"
#include "query_dsl.h"
#include "serialize.h"
#include "serialize_list.h"

namespace detail {

using engine::Op;
using engine::Query;

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

// The ids a query finds in a shard (at most 20), in rank order.
std::vector<engine::docid> ranked(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter = nullptr, engine::MatchSpy* spy = nullptr);

std::set<engine::docid> run(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter = nullptr, engine::MatchSpy* spy = nullptr);

// One document per term, in the field "f".
std::unique_ptr<engine::Shard> terms_db(const char* name, std::initializer_list<const char*> terms);

// shape.cc
void test_leaves_and_ops();
void test_emptiness();
void test_expansions();
void test_expansion_limits();
void test_append();
void test_text();
void test_wildcard_patterns();
void test_fuzzy_limit();
void test_binary_equals_list();

// sources.cc
void test_registered_sources();
void test_unserialize_garbage();
void test_corrupt_stored_value();

// callbacks.cc
void test_posting_source();
void test_key_maker();
void test_match_spy();
void test_source_ownership();

// deep.cc
void test_append_chain();
void test_depth_cap();
void test_chain_cap();

// dsl.cc
void test_dsl_not_is_none_of();
void test_dsl_one_character_keyword_is_a_term();

// range_aggregation.cc
void test_range_aggregation_open_bounds();

}  // namespace detail
