/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <span>                    // for std::span
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector

#include "prospect/searcher.hh"    // for prospect::segment_snapshot, term_table

#include "engine/query.h"          // for QueryNode


// The terms a wildcard or edit-distance node stands for (docs/mechanisms/strata-callbacks.md).
namespace engine::strata {

// The bytes a wildcard pattern starts with, up to its first pattern character: every term that
// matches starts with them.
std::string wildcard_prefix(std::string_view pattern, unsigned pattern_flags);

// Whether the term matches the pattern. Without pattern flags the pattern is a prefix; with them
// it matches the whole term, '?' one character and '*' any run of characters, each only when its
// flag is set.
bool wildcard_matches(std::string_view pattern, std::string_view term, unsigned pattern_flags);

// Whether two UTF-8 strings are at most `distance` edits apart: insertions, deletions,
// substitutions and swaps of two neighbouring characters, each one edit.
bool within_edit_distance(std::string_view a, std::string_view b, unsigned distance);

// The terms of the whole index that a wildcard or edit-distance node matches, sorted, after its
// expansion limit: more than `max_expansion` matches (0 for no limit) fail the search with
// errc::invalid, keep the first terms, or keep the most frequent ones, as the node says. A term
// whose documents are all deleted still counts until a merge removes it.
std::vector<std::string> expand(const QueryNode& node, std::span<const prospect::segment_snapshot> segments, prospect::term_table& terms, std::string_view key_prefix);

}  // namespace engine::strata
