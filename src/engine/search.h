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

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint8_t
#include <limits>                  // for std::numeric_limits
#include <memory>                  // for std::unique_ptr
#include <optional>                // for std::optional
#include <span>                    // for std::span
#include <string>                  // for std::string
#include <vector>                  // for std::vector

#include "engine/callbacks.h"      // for KeyMaker, MatchSpy
#include "engine/query.h"          // for Query
#include "engine/shard.h"          // for Shard
#include "engine/types.h"          // for docid, doccount, termcount, valueno
#include "task.hh"                 // for task::task


namespace engine {

// A query as the engine reads it, once, before any shard is chosen. A text node is analysed here,
// so a query that analyses to nothing (only stop words) is known to match nothing without a shard.
class PreparedQuery {
public:
	virtual ~PreparedQuery() = default;

	virtual bool empty() const = 0;
	virtual std::string description() const = 0;
};

// Reads the query. Raises what the engine's query parser raises on text it cannot parse.
std::unique_ptr<PreparedQuery> prepare(const Query& query);

// How documents with equal rank are ordered.
enum class DocidOrder : std::uint8_t {
	ascending,
	descending,
	dont_care,
};

// "More like these": the terms that best set the top hits apart are added to the query.
// The top hits are taken by relevance over all documents, in ascending docid order: the request's window, collapse,
// cutoffs, sorter, spy and docid order do not apply to them. `nearest` and `fuzzy` each use their own settings.
struct Similar {
	doccount n_rset = 5;                  // top hits taken as relevant
	doccount n_eset = 32;                 // terms picked from them
	termcount n_term = 10;                // terms of the added query that must match (an elite set)
	std::vector<std::string> prefixes;    // only terms with one of these prefixes; empty allows all
};

// A `check_at_least` that asks the engine to look at every matching document.
inline constexpr doccount CHECK_ALL = std::numeric_limits<doccount>::max();

struct SearchRequest {
	// Not null; must outlive the search.
	const PreparedQuery* query = nullptr;

	doccount first = 0;                   // hits skipped
	doccount maxitems = 10;               // hits returned
	doccount check_at_least = 0;          // documents the engine looks at, for the estimate and the spy; CHECK_ALL looks at all

	const KeyMaker* sorter = nullptr;     // orders hits by key, then by relevance; borrowed
	valueno collapse_key = BAD_VALUENO;   // keeps `collapse_max` hits per value of this slot
	doccount collapse_max = 0;
	double percent_threshold = 0;         // drops hits under this percent of the best weight
	double weight_threshold = 0;          // drops hits under this weight
	DocidOrder order = DocidOrder::ascending;
	MatchSpy* spy = nullptr;              // sees the matching documents; borrowed, holds its results afterwards

	std::optional<Similar> fuzzy;         // adds an elite set of the top hits' terms to the query (OR)
	std::optional<Similar> nearest;       // replaces the query by an elite set of the top hits' terms

	// Names this node in the ids that correlate the phases of a search across nodes.
	std::string origin;
};

struct Hit {
	docid did = 0;                        // the document's id across the shards
	double weight = 0;
	doccount rank = 0;                    // position in the full ordering, from 0
	int percent = 0;                      // weight as a percentage of the best weight
};

// What a search found; holds no engine resource.
struct SearchResult {
	std::vector<Hit> hits;                // the requested window
	doccount estimated = 0;               // matching documents, estimated

	std::size_t size() const noexcept {
		return hits.size();
	}
};

// Searches the shards as one index. Hit ids are the documents' ids across the shards, interleaved
// the way the shards number them. No shards, or an empty query, find nothing. The shards must be
// of the engine that implements this, stay open and be used by this search only until it ends.
task::task<SearchResult> search(std::span<Shard* const> shards, const SearchRequest& request);

}  // namespace engine
