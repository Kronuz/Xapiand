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

#include <atomic>                  // for std::atomic
#include <cstddef>                 // for std::size_t
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "engine/census.h"  // for ENGINE_CENSUS_QUERY, ENGINE_CENSUS_SEARCH
#include "engine/xapian/callbacks.h"  // for KeyMakerAdapter, MatchSpyAdapter
#include "engine/xapian/errors.h"  // for rethrow_translated
#include "engine/xapian/query.h"   // for to_xapian
#include "engine/xapian/search.h"  // for live_search_objects
#include "engine/xapian/shard.h"   // for Shard
#include "xapian.h"                // for Xapian::Enquire, MSet


namespace engine::xapian {

namespace {

std::atomic<std::size_t> live{0};

// Counts the objects that derive from it.
struct Live {
	Live() noexcept { live.fetch_add(1, std::memory_order_relaxed); }
	Live(const Live&) = delete;
	Live& operator=(const Live&) = delete;
	~Live() { live.fetch_sub(1, std::memory_order_relaxed); }
};

class Prepared final : public PreparedQuery, private Live {
public:
	Prepared(const Query& source, Xapian::Query query) : PreparedQuery(source), query(std::move(query)) { }

	bool empty() const override {
		return query.empty();
	}

	std::string description() const override {
		return query.get_description();
	}

	const Xapian::Query query;
};

// What one search makes: the combined database and the Xapian side of the callbacks.
struct Scope : private Live {
	Xapian::Database db;
	std::optional<KeyMakerAdapter> sorter;
	std::optional<MatchSpyAdapter> spy;
};

Xapian::Enquire::docid_order
to_xapian(DocidOrder order)
{
	switch (order) {
		case DocidOrder::ascending: return Xapian::Enquire::ASCENDING;
		case DocidOrder::descending: return Xapian::Enquire::DESCENDING;
		case DocidOrder::dont_care: return Xapian::Enquire::DONT_CARE;
	}
	return Xapian::Enquire::ASCENDING;
}

// Correlates the two phases of a search across nodes; unique per search and per node.
std::string
next_query_id(const std::string& origin)
{
	static std::atomic_size_t seq{0};
	return origin + ":" + std::to_string(seq.fetch_add(1, std::memory_order_relaxed) + 1);
}

SearchResult
run(std::span<engine::Shard* const> shards, const SearchRequest& request, const Xapian::Query& query)
{
	Scope scope;
	for (auto* shard : shards) {
		const auto& database = *dynamic_cast<Shard&>(*shard).database();
		if (shards.size() == 1) {
			scope.db = database;
		} else {
			scope.db.add_database(database);
		}
	}

	Xapian::Enquire enquire(scope.db);
	enquire.set_collapse_key(request.collapse_key, request.collapse_max);
	enquire.set_docid_order(to_xapian(request.order));
	if (request.spy) {
		scope.spy.emplace(*request.spy);
		enquire.add_matchspy(&*scope.spy);
	}
	if (request.sorter) {
		scope.sorter.emplace(*request.sorter);
		enquire.set_sort_by_key_then_relevance(&*scope.sorter, false);
	}

	enquire.set_query(query);

	if (!query.empty()) {  // prepare_mset requires a non-empty query
		enquire.prepare_mset(next_query_id(request.origin), nullptr, nullptr);
	}
	auto mset = enquire.get_mset(request.first, request.maxitems, request.check_at_least);

	SearchResult result;
	result.estimated = mset.get_matches_estimated();
	result.hits.reserve(mset.size());
	const auto end = mset.end();
	for (auto it = mset.begin(); it != end; ++it) {
		result.hits.push_back({*it, it.get_weight(), it.get_rank(), it.get_percent()});
	}
	return result;
}

}  // namespace


std::size_t
live_search_objects() noexcept
{
	return live.load(std::memory_order_relaxed);
}

}  // namespace engine::xapian


namespace engine {

std::unique_ptr<PreparedQuery>
prepare(const Query& query)
{
	ENGINE_CENSUS_QUERY(query);
	try {
		return std::make_unique<xapian::Prepared>(query, xapian::to_xapian(query));
	} catch (...) {
		xapian::rethrow_translated();
	}
}


}  // namespace engine


namespace engine::xapian {

task::task<SearchResult>
search(std::span<engine::Shard* const> shards, const SearchRequest& request)
{
	try {
		const auto& prepared = dynamic_cast<const Prepared&>(*request.query);
		if (shards.empty() || prepared.empty()) {
			co_return SearchResult{};
		}
		co_return run(shards, request, prepared.query);
	} catch (...) {
		rethrow_translated();
	}
}

}  // namespace engine::xapian
