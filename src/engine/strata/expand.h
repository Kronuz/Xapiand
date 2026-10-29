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

#include "engine/document.h"       // for KEY_FIELD
#include "engine/query.h"          // for QueryNode


namespace engine::strata {

// A term under a field, as the host names them.
struct FieldTerm {
	std::string field;
	std::string term;
	bool operator<(const FieldTerm& other) const { return field != other.field ? field < other.field : term < other.term; }
};

// The terms of the whole index that a wildcard or edit-distance node matches, sorted, after its
// expansion limit: more than `max_expansion` matches (0 for no limit) fail the search with
// errc::invalid, keep the first terms, or keep the most frequent ones, as the node says. A term
// whose documents are all deleted still counts until a merge removes it. A wildcard with an empty
// pattern (Query::field) takes every field whose name starts with the node's, the key field's keys
// included only when it is named exactly.
std::vector<FieldTerm> expand(const QueryNode& node, std::span<const prospect::segment_snapshot> segments, prospect::term_table& terms);

}  // namespace engine::strata
