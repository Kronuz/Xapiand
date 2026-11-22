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


#include "engine/search.h"

#include <algorithm>               // for std::sort, std::min, std::set_union, std::stable_sort
#include <cmath>                   // for std::isfinite
#include <iterator>                // for std::back_inserter
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


task::task<std::vector<IndexStatistics>>
gather(std::span<Shard* const> shards, const PreparedQuery& query, std::span<const std::uint32_t> index_of)
{
	co_return co_await strata::gather(shards, query, index_of);
}


const TermStatistics*
IndexStatistics::find(std::string_view field, std::string_view term) const
{
	const auto by_field = terms.find(field);
	if (by_field == terms.end()) {
		return nullptr;
	}
	const auto found = by_field->second.find(term);
	return found == by_field->second.end() ? nullptr : &found->second;
}


IndexStatistics&
IndexStatistics::operator+=(const IndexStatistics& other)
{
	if (expansions.empty()) {
		expansions.resize(other.expansions.size());
	} else if (!other.expansions.empty() && other.expansions.size() != expansions.size()) {
		throw Error(errc::invalid, "Statistics of another query were added");
	}
	documents += other.documents;
	total_length += other.total_length;
	for (const auto& [field, by_term] : other.terms) {
		auto& mine = terms[field];
		for (const auto& [term, figures] : by_term) {
			auto& sum = mine[term];
			sum.df += figures.df;
			sum.cf += figures.cf;
		}
	}
	for (std::size_t k = 0; k < other.expansions.size(); ++k) {
		std::vector<FieldTerm> joined;
		joined.reserve(expansions[k].size() + other.expansions[k].size());
		std::set_union(expansions[k].begin(), expansions[k].end(), other.expansions[k].begin(), other.expansions[k].end(), std::back_inserter(joined));
		expansions[k] = std::move(joined);
	}
	return *this;
}


std::vector<const QueryNode*>
expanding_nodes(const Query& query)
{
	// Depth first, left to right, without recursion: an append chain is as long as a query allows.
	std::vector<const QueryNode*> found;
	std::vector<const Query*> pending{&query};
	while (!pending.empty()) {
		const auto* node = pending.back()->node();
		pending.pop_back();
		if (node == nullptr) {
			continue;
		}
		if (node->kind == QueryKind::wildcard || node->kind == QueryKind::edit_distance) {
			found.push_back(node);
		}
		for (auto sub = node->subqueries.rbegin(); sub != node->subqueries.rend(); ++sub) {
			pending.push_back(&*sub);
		}
	}
	return found;
}


std::vector<FieldTerm>
limit_expansion(const QueryNode& node, std::vector<FieldTerm> matched, const std::function<std::uint64_t(const FieldTerm&)>& df)
{
	const std::size_t limit = node.parameter;
	if (limit == 0 || matched.size() <= limit) {
		return matched;
	}
	switch (node.limit) {
		case Expansion::error:
			throw Error(errc::invalid, "The pattern \"" + node.field + node.str + "\" expands to more than " + std::to_string(limit) + " terms");
		case Expansion::first:
			matched.resize(limit);
			return matched;
		case Expansion::most_frequent: {
			std::vector<std::pair<std::uint64_t, std::size_t>> by_frequency;
			by_frequency.reserve(matched.size());
			for (std::size_t i = 0; i < matched.size(); ++i) {
				by_frequency.emplace_back(df(matched[i]), i);
			}
			std::stable_sort(by_frequency.begin(), by_frequency.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
			by_frequency.resize(limit);
			std::vector<std::size_t> keep;
			keep.reserve(limit);
			for (const auto& entry : by_frequency) {
				keep.push_back(entry.second);
			}
			std::sort(keep.begin(), keep.end());
			std::vector<FieldTerm> kept;
			kept.reserve(limit);
			for (const auto i : keep) {
				kept.push_back(std::move(matched[i]));
			}
			return kept;
		}
	}
	throw Error(errc::internal, "An expansion limit the engine does not know");
}


void
finish(const PreparedQuery& query, IndexStatistics& statistics)
{
	const auto nodes = expanding_nodes(query.source());
	if (statistics.expansions.size() != nodes.size()) {
		throw Error(errc::invalid, "Statistics of another query were finished");
	}
	for (std::size_t k = 0; k < nodes.size(); ++k) {
		statistics.expansions[k] = limit_expansion(*nodes[k], std::move(statistics.expansions[k]), [&](const FieldTerm& t) {
			const auto* figures = statistics.find(t.field, t.term);
			return figures != nullptr ? figures->df : std::uint64_t{0};
		});
	}
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


Merge::Merge(std::size_t total, const SearchRequest& request)
	: total_(total),
	  request_(request),
	  part_(request),
	  collapse_(request.collapse_key != BAD_VALUENO && request.collapse_max != 0),
	  window_(static_cast<std::uint64_t>(request.first) + request.maxitems)
{
	part_.first = 0;
	part_.maxitems = collapse_ ? std::numeric_limits<doccount>::max() : static_cast<doccount>(std::min<std::uint64_t>(window_, std::numeric_limits<doccount>::max()));
	part_.report_keys = true;
}


// Hits rank by sorter key, then weight (heavier first), then id in the request's order, as one search ranks them.
bool
Merge::above(const Candidate& a, const Candidate& b) const
{
	if (const int c = a.hit.key.compare(b.hit.key); c != 0) { return c < 0; }
	if (a.hit.weight != b.hit.weight) { return a.hit.weight > b.hit.weight; }
	return request_.order == DocidOrder::descending ? a.id > b.id : a.id < b.id;
}


void
Merge::add(BatchResult done)
{
	estimated_ += done.result.estimated;
	heaviest_ = std::max(heaviest_, done.result.top);
	for (auto& hit : done.result.hits) {
		const auto id = global_id(hit.did, done.positions, total_);
		top_.push_back({std::move(hit), id});
	}
	if (!collapse_ && top_.size() > window_) {
		std::partial_sort(top_.begin(), top_.begin() + static_cast<std::ptrdiff_t>(window_), top_.end(), [this](const Candidate& a, const Candidate& b) { return above(a, b); });
		top_.resize(window_);
	}
}


SearchResult
Merge::finish()
{
	std::sort(top_.begin(), top_.end(), [this](const Candidate& a, const Candidate& b) { return above(a, b); });

	std::uint64_t estimated = estimated_;
	if (collapse_) {
		// The best `collapse_max` hits of each value, as one search keeps them; a hit without a value never collapses.
		std::unordered_map<std::string, doccount> seen;
		std::size_t kept = 0;
		for (std::size_t i = 0; i < top_.size(); ++i) {
			if (!top_[i].hit.value.empty() && ++seen[top_[i].hit.value] > request_.collapse_max) {
				continue;
			}
			if (kept != i) {
				top_[kept] = std::move(top_[i]);
			}
			++kept;
		}
		top_.resize(kept);
		estimated = kept;
	}

	SearchResult result;
	result.estimated = static_cast<doccount>(std::min<std::uint64_t>(estimated, std::numeric_limits<doccount>::max()));
	result.top = heaviest_;
	for (std::uint64_t i = request_.first; i < top_.size() && i < window_; ++i) {
		auto& hit = top_[i].hit;
		hit.did = top_[i].id;
		hit.rank = static_cast<doccount>(i);
		hit.percent = percent_of(hit.weight, heaviest_);
		hit.key.clear();
		hit.value.clear();
		result.hits.push_back(std::move(hit));
	}
	top_.clear();
	return result;
}


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

	Merge merge(total, request);
	for (std::size_t begin = 0, end; begin < total; begin = end) {
		end = batch_end(begin, total, batch, index_of);
		SearchRequest batch_request = merge.part();
		if (!index_of.empty()) {
			batch_request.index_of = index_of.subspan(begin, end - begin);
		}
		merge.add(co_await run(begin, end, batch_request));
	}
	co_return merge.finish();
}

}  // namespace engine
