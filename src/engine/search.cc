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


#include "engine/search.h"

#include <algorithm>               // for std::sort, std::min
#include <cmath>                   // for std::isfinite
#include <span>                    // for std::span
#include <unordered_map>           // for std::unordered_map

#include "engine/census.h"         // for ENGINE_CENSUS_SEARCH
#include "engine/error.h"          // for Error, errc
#include "engine/strata/search.h"  // for strata::search


namespace engine {

namespace {

class Prepared final : public PreparedQuery {
public:
	explicit Prepared(const Query& source) : PreparedQuery(source) { }

	bool empty() const override {
		return source().empty();
	}

	std::string description() const override {
		return source().description();
	}
};

}  // namespace


std::unique_ptr<PreparedQuery>
prepare(const Query& query)
{
	ENGINE_CENSUS_QUERY(query);
	return std::make_unique<Prepared>(query);
}


task::task<SearchResult>
search(std::span<Shard* const> shards, const SearchRequest& request)
{
	ENGINE_CENSUS_SEARCH(request, shards.size());
	co_return co_await strata::search(shards, request);
}

int
percent_of(double score, double top)
{
	if (!(top > 0)) {
		return 100;
	}
	if (!std::isfinite(top)) {
		return score >= top ? 100 : 0;
	}
	const double ratio = score / top * 100.0 + 0.5;
	return ratio >= 100.0 ? 100 : ratio > 0.0 ? static_cast<int>(ratio) : 0;
}


namespace {

// A hit of a batch, numbered across all the shards of the search.
struct Candidate {
	Hit hit;
	hitid id;
};

// The id of a batch's hit over all the shards: batch shard i is the search's shard positions[i].
hitid
global_id(hitid batch_id, const std::vector<std::size_t>& positions, std::size_t total)
{
	const std::uint64_t n = positions.size();
	const std::uint64_t index = batch_id - 1;
	const std::uint64_t slot = positions[index % n];
	if (index / n > (std::numeric_limits<hitid>::max() - slot - 1) / total) {
		throw Error(errc::internal, "A document id does not fit across the shards");
	}
	return (index / n) * total + slot + 1;
}

// One search's result with its hit ids numbered over `total` shards, whatever shards were found.
void
renumber(SearchResult& result, const std::vector<std::size_t>& positions, std::size_t total)
{
	if (positions.size() == total) {
		return;   // every shard was there: the ids already are the whole search's
	}
	for (auto& hit : result.hits) {
		hit.did = global_id(hit.did, positions, total);
	}
}

// The end of the batch that starts at `begin`: whole indexes while they fit in `batch` shards, at least one.
std::size_t
batch_end(std::size_t begin, std::size_t total, std::size_t batch, std::span<const std::uint32_t> index_of)
{
	std::size_t end = begin;
	while (end < total) {
		std::size_t next = end + 1;
		if (!index_of.empty()) {
			while (next < total && index_of[next] == index_of[end]) { ++next; }
		}
		if (end != begin && next - begin > batch) {
			break;
		}
		end = next;
		if (end - begin >= batch) {
			break;
		}
	}
	return end;
}

}  // namespace


task::task<SearchResult>
search_batched(std::size_t total, std::size_t batch, std::span<const std::uint32_t> index_of, const BatchRunner& run, const SearchRequest& request)
{
	if (!index_of.empty() && index_of.size() != total) {
		throw Error(errc::invalid, "A search was given an index for some of its shards only");
	}
	if (batch == 0 || total <= batch) {
		SearchRequest whole = request;
		whole.index_of = index_of;
		auto only = co_await run(0, total, whole);
		if (!only.result.hits.empty()) {
			renumber(only.result, only.positions, total);
		}
		co_return std::move(only.result);
	}

	// Every batch is asked for the top of the whole window (or, under a collapse, for all it kept, whose
	// count is the total), with the keys the merge ranks by.
	const bool collapse = request.collapse_key != BAD_VALUENO && request.collapse_max != 0;
	const bool descending = request.order == DocidOrder::descending;
	const std::uint64_t window = static_cast<std::uint64_t>(request.first) + request.maxitems;
	SearchRequest inner = request;
	inner.first = 0;
	inner.maxitems = collapse ? std::numeric_limits<doccount>::max() : static_cast<doccount>(std::min<std::uint64_t>(window, std::numeric_limits<doccount>::max()));
	inner.report_keys = true;

	const auto above = [descending](const Candidate& a, const Candidate& b) {
		if (const int c = a.hit.key.compare(b.hit.key); c != 0) { return c < 0; }
		if (a.hit.weight != b.hit.weight) { return a.hit.weight > b.hit.weight; }
		return descending ? a.id > b.id : a.id < b.id;
	};

	std::vector<Candidate> top;
	std::uint64_t estimated = 0;
	double heaviest = 0;
	for (std::size_t begin = 0, end; begin < total; begin = end) {
		end = batch_end(begin, total, batch, index_of);
		SearchRequest batch_request = inner;
		if (!index_of.empty()) {
			batch_request.index_of = index_of.subspan(begin, end - begin);
		}
		auto done = co_await run(begin, end, batch_request);
		estimated += done.result.estimated;
		heaviest = std::max(heaviest, done.result.top);
		for (auto& hit : done.result.hits) {
			const auto id = global_id(hit.did, done.positions, total);
			top.push_back({std::move(hit), id});
		}
		if (!collapse && top.size() > window) {
			std::partial_sort(top.begin(), top.begin() + static_cast<std::ptrdiff_t>(window), top.end(), above);
			top.resize(window);
		}
	}
	std::sort(top.begin(), top.end(), above);

	if (collapse) {
		// The best `collapse_max` hits of each value, as one search keeps them; a hit without a value never collapses.
		std::unordered_map<std::string, doccount> seen;
		std::size_t kept = 0;
		for (std::size_t i = 0; i < top.size(); ++i) {
			if (!top[i].hit.value.empty() && ++seen[top[i].hit.value] > request.collapse_max) {
				continue;
			}
			if (kept != i) {
				top[kept] = std::move(top[i]);
			}
			++kept;
		}
		top.resize(kept);
		estimated = kept;
	}

	SearchResult result;
	result.estimated = static_cast<doccount>(std::min<std::uint64_t>(estimated, std::numeric_limits<doccount>::max()));
	result.top = heaviest;
	for (std::uint64_t i = request.first; i < top.size() && i < window; ++i) {
		auto& hit = top[i].hit;
		hit.did = top[i].id;
		hit.rank = static_cast<doccount>(i);
		hit.percent = percent_of(hit.weight, heaviest);
		hit.key.clear();
		hit.value.clear();
		result.hits.push_back(std::move(hit));
	}
	co_return result;
}

}  // namespace engine
