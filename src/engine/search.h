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

#include <compare>                 // for operator<=>
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint8_t, std::uint64_t
#include <functional>              // for std::function
#include <limits>                  // for std::numeric_limits
#include <map>                     // for std::map
#include <memory>                  // for std::unique_ptr
#include <span>                    // for std::span
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "engine/callbacks.h"      // for KeyMaker, MatchSpy
#include "engine/query.h"          // for Query
#include "engine/shard.h"          // for Shard
#include "engine/types.h"          // for docid, doccount, termcount, valueno
#include "task.hh"                 // for task::task


namespace engine {

// A query as the engine reads it, once, before any shard is chosen. The host has already read its
// text, so a query that has no words is known to match nothing without a shard.
class PreparedQuery {
public:
	explicit PreparedQuery(Query source) : source_(std::move(source)) { }
	virtual ~PreparedQuery() = default;

	virtual bool empty() const = 0;
	virtual std::string description() const = 0;

	// The query as it was given to prepare(), for the engines that read it themselves.
	const Query& source() const noexcept {
		return source_;
	}

private:
	Query source_;
};

// Reads the query. Raises what the engine's query parser raises on text it cannot parse.
std::unique_ptr<PreparedQuery> prepare(const Query& query);

// A term under a field, as a query names it.
struct FieldTerm {
	std::string field;
	std::string term;
	auto operator<=>(const FieldTerm&) const = default;
};

// A term's frequencies over an index, deleted documents included: documents holding it, and the sum of its wdf.
struct TermStatistics {
	std::uint64_t df = 0;
	std::uint64_t cf = 0;
	bool operator==(const TermStatistics&) const = default;
};

// What a search weighs one index with, gathered from its shards and summed (docs/mechanisms/distributed-search.md):
// a search over some of the shards that takes the sum weighs as one search over all of them.
struct IndexStatistics {
	std::uint64_t documents = 0;          // deleted documents included, as the weights count them
	std::uint64_t total_length = 0;
	std::map<std::string, std::map<std::string, TermStatistics, std::less<>>, std::less<>> terms;   // by field, then term
	// The terms each expanding node of the query (expanding_nodes()) matches in the index, sorted: as gathered, then
	// as the node's limit keeps them once finish() has run.
	std::vector<std::vector<FieldTerm>> expansions;

	const TermStatistics* find(std::string_view field, std::string_view term) const;

	// Sums the figures and joins the expansions; raises errc::invalid for statistics of another query.
	IndexStatistics& operator+=(const IndexStatistics& other);
	bool operator==(const IndexStatistics&) const = default;
};

// The wildcard and edit-distance nodes of a query, in the order statistics number them: depth first, each place a
// node appears counted.
std::vector<const QueryNode*> expanding_nodes(const Query& query);

// The terms an expanding node keeps of those it matches in an index (sorted), under its limit: past `max_expansion`
// it raises errc::invalid, keeps the first or keeps the most frequent by `df`, the term's frequency in the index.
std::vector<FieldTerm> limit_expansion(const QueryNode& node, std::vector<FieldTerm> matched, const std::function<std::uint64_t(const FieldTerm&)>& df);

// Applies each expanding node's limit to an index's summed statistics, as one search over the whole index applies it.
void finish(const PreparedQuery& query, IndexStatistics& statistics);

// How documents with equal rank are ordered.
enum class DocidOrder : std::uint8_t {
	ascending,
	descending,
	dont_care,
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
	DocidOrder order = DocidOrder::ascending;
	MatchSpy* spy = nullptr;              // sees the matching documents; borrowed, holds its results afterwards

	// The time slice the search runs in; borrowed, outlives the search. A search with one yields its
	// loop at window boundaries and raises task::cancelled when the budget's token is set; without
	// one it runs to its end where it is awaited.
	task::budget* budget = nullptr;

	// Each hit also carries its sort key and collapse value, which a merge across batches ranks by.
	bool report_keys = false;

	// The index each shard given belongs to, one entry per shard: adjacent equal entries are the shards of one
	// index. A relevance weight reads the statistics of its own index, so it does not depend on the other indexes
	// searched with it. Empty: all the shards are one index. Borrowed; outlives the search.
	std::span<const std::uint32_t> index_of;

	// The statistics each index weighs with and the terms its expanding nodes take, gathered over all of the index's
	// shards and finished; an index's entry is at its `index_of` value (entry 0 when `index_of` is empty). Empty: each
	// index's own, read from the shards given. Borrowed; outlives the search.
	std::span<const IndexStatistics> statistics;
};

struct Hit {
	hitid did = 0;                        // the document's id across the shards
	double weight = 0;
	doccount rank = 0;                    // position in the full ordering, from 0
	int percent = 0;                      // weight as a percentage of the best weight
	std::string key;                      // the sorter's key; only with `report_keys`
	std::string value;                    // the collapse slot's value; only with `report_keys`
};

// What a search found; holds no engine resource.
struct SearchResult {
	std::vector<Hit> hits;                // the requested window
	doccount estimated = 0;               // matching documents, estimated
	double top = 0;                       // the heaviest weight, which `percent` is a share of

	std::size_t size() const noexcept {
		return hits.size();
	}
};

// Searches the shards as one set. Hit ids are the documents' ids across the shards, interleaved
// the way the shards number them: document d of shard i of n is (d - 1) * n + i + 1. A weight reads the
// statistics of its own index (`SearchRequest::index_of`). No shards, or an empty query, find nothing.
// The shards must all be of one engine, stay open and be used by this search only until it ends.
// prepare() may reject a query shape first, with errc::invalid or errc::internal.
task::task<SearchResult> search(std::span<Shard* const> shards, const SearchRequest& request);

// The statistics of the shards for the query, one per index (`index_of` as in SearchRequest, in the order the
// indexes first appear): the frequencies of every term the query names and of every term its expanding nodes match.
// A node's share of a search answers its shards' with these; the coordinator sums them by index and finishes them.
task::task<std::vector<IndexStatistics>> gather(std::span<Shard* const> shards, const PreparedQuery& query, std::span<const std::uint32_t> index_of);

// A hit's weight as a percentage of the top weight, in [0, 100]; against an infinite top weight a hit
// is 100 if it is infinite too and 0 otherwise.
int percent_of(double score, double top);

// What one batch of a wide search returned: the result numbered over the shards that were searched,
// and each searched shard's position in the whole search (ascending, within the batch's range). A
// shard that does not exist is skipped, so there can be fewer positions than the range holds.
struct BatchResult {
	SearchResult result;
	std::vector<std::size_t> positions;
};

// Merges the results of searches over disjoint sets of a search's `total` shards into the result one search over
// all of them returns: each part is searched under part() and add()ed with its shards' positions, in any order.
class Merge {
public:
	Merge(std::size_t total, const SearchRequest& request);

	// The request each part runs under: the top of the whole window (under a collapse, all it keeps), with the keys
	// the merge ranks by. Its `index_of` is the whole search's; a caller gives each part its own.
	const SearchRequest& part() const noexcept {
		return part_;
	}

	void add(BatchResult done);
	SearchResult finish();

private:
	struct Candidate {
		Hit hit;
		hitid id;
	};

	bool above(const Candidate& a, const Candidate& b) const;

	std::size_t total_;
	SearchRequest request_;
	SearchRequest part_;
	bool collapse_;
	std::uint64_t window_;
	std::vector<Candidate> top_;
	std::uint64_t estimated_ = 0;
	double heaviest_ = 0;
};

// Searches shards [begin, end) of the whole search under `request`. The host holds them open for the
// call and no longer: this is the one seam between a wide search and how its indexes are kept.
using BatchRunner = std::function<task::task<BatchResult>(std::size_t begin, std::size_t end, const SearchRequest& request)>;

// Searches `total` shards, at most `batch` of them held at once: each batch is searched as it starts and its hits
// merged into the top of the whole. `index_of` says which index each shard belongs to (empty: all one index, one
// shard each is also fine); a batch holds whole indexes, so one wider than `batch` is a batch alone. A relevance
// weight reads the statistics of the hit's own index, so hits, their order, weights, totals and the spy's results
// are the same whatever `batch` is. Hit ids are numbered by position (hit id = (d - 1) * total + position + 1).
// A batch of 0, or no more shards than a batch, is one search. Raises errc::invalid when `index_of` is not empty
// and not `total` long.
task::task<SearchResult> search_batched(std::size_t total, std::size_t batch, std::span<const std::uint32_t> index_of, const BatchRunner& run, const SearchRequest& request);

}  // namespace engine
