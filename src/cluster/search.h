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

/*
 * A search over a cluster's shards (cluster-release D10; docs/mechanisms/distributed-search.md). The
 * coordinator, the node that received the request, asks one replica of each shard for its statistics,
 * sums them by index and expands the query's wildcard and edit-distance terms once over the sum, then
 * asks the same replicas to search with those statistics and terms and merges their parts: hits,
 * counts and aggregations equal one node's holding every shard. A replica that fails a round is
 * replaced once by the shard's next one; a second failure names the shard in `failed`.
 */

#pragma once

#include <chrono>         // for std::chrono::steady_clock, std::chrono::milliseconds
#include <cstddef>        // for std::size_t
#include <cstdint>        // for std::uint32_t, std::uint64_t
#include <span>           // for std::span
#include <string>         // for std::string
#include <vector>         // for std::vector

#include "cluster/shard_map_view.h"  // for ShardMapView, NodeId, GroupId
#include "engine/search.h"           // for PreparedQuery, SearchRequest, SearchResult, IndexStatistics
#include "engine/shard.h"            // for Shard
#include "task.hh"                   // for task::task

namespace cluster {

using Clock = std::chrono::steady_clock;

// A shard of one node's part: its group, and its place in the whole search.
struct PartShard {
	GroupId group = 0;
	std::uint64_t incarnation = 0;
	std::size_t position = 0;  // among all the search's shards
	std::uint32_t index = 0;   // the index's number in the search, as SearchRequest::index_of counts
};

// The statistics round's ask of one node: the query's statistics over its shards.
struct StatsPart {
	const engine::PreparedQuery* query = nullptr;
	std::vector<PartShard> shards;  // ascending positions
	Clock::time_point deadline;
};

// The search round's ask of one node: its shards searched under `request`, the merge's part request
// with the whole search's finished statistics, by index number. Its spy, when set, is the prototype a
// node clones and serializes the results of.
struct SearchPart {
	const engine::SearchRequest* request = nullptr;
	std::vector<PartShard> shards;  // ascending positions
	Clock::time_point deadline;
};

// A node's part of the search: hits numbered over the part's shards with their keys reported, and the
// serialized results of its spy (empty without one).
struct PartAnswer {
	engine::SearchResult result;
	std::string aggregations;
};

// How a coordinator reaches the nodes: in process for itself, over the shard RPC for the others. A
// failure throws: engine::Error with errc::syntax is the request's and ends the search; anything else
// is the replica's.
class Nodes {
public:
	virtual ~Nodes() = default;

	// One statistics entry per index of the part, in the order its shards name them.
	virtual task::task<std::vector<engine::IndexStatistics>> stats(NodeId node, const StatsPart& part) = 0;
	virtual task::task<PartAnswer> search(NodeId node, const SearchPart& part) = 0;
};

// The shards this node holds, by group: what serves a part here.
class LocalShards {
public:
	virtual ~LocalShards() = default;

	// The shard, open for reading; null when this node does not hold that group at that incarnation.
	virtual task::task<engine::Shard*> shard(GroupId group, std::uint64_t incarnation) = 0;
};

// A part served here, for the coordinator's own shards and for a peer's ask alike. A shard not held
// throws errc::not_found.
task::task<std::vector<engine::IndexStatistics>> serve_stats(LocalShards& local, const StatsPart& part);
task::task<PartAnswer> serve_search(LocalShards& local, const SearchPart& part);

// A shard the search could not read: `_shards.failed`'s entry.
struct ShardFailure {
	std::string index;
	std::size_t shard = 0;  // in the index's shard order
	NodeId node = 0;        // the last replica asked; 0 when it has none
	std::string reason;
};

struct FanoutResult {
	engine::SearchResult result;
	std::size_t total = 0;               // `_shards.total`
	std::size_t successful = 0;          // `_shards.successful`
	std::vector<ShardFailure> failed;    // `_shards.failed`: a result with any is partial

	bool partial() const noexcept {
		return !failed.empty();
	}
};

struct FanoutOptions {
	std::chrono::milliseconds attempt{30'000};  // each ask's deadline, from when it is sent
};

// Searches the indexes' shards as one search over all of them, in the order `indexes` names them
// (an index the map does not hold has no shards). `request` is the search a node would run: its
// spy receives the merged results, and its `index_of` and `statistics` are the coordinator's to set.
// Raises errc::invalid when an expansion passes its limit under Expansion::error, and what a node
// raises with errc::syntax.
task::task<FanoutResult> search(const ShardMapView& map, Nodes& nodes, std::span<const std::string> indexes, const engine::SearchRequest& request, const FanoutOptions& options = {});

}  // namespace cluster
