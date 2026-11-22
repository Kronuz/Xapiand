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
 * What a search reads of the shard map (docs/mechanisms/distributed-search.md): an index's shards, the
 * nodes holding each one's data, and which of them to ask. The root's namespace records
 * (strata::placement::namespace_record) are its source; the shard-map task implements this view over
 * them, and StaticShardMap stands in until then.
 */

#pragma once

#include <cstddef>      // for std::size_t
#include <cstdint>      // for std::uint64_t
#include <map>          // for std::map
#include <set>          // for std::set
#include <string>       // for std::string
#include <string_view>  // for std::string_view
#include <vector>       // for std::vector

#include "task.hh"      // for task::task

namespace cluster {

using NodeId = std::uint64_t;   // consensus's NodeId, placement's node_id; 0 is no node
using GroupId = std::uint64_t;  // placement's group_id; 0 is no group

// One shard of an index as search reads it: its group, and the nodes that hold its data (a namespace
// record's members less its witnesses), the preferred leader first.
struct ShardReplicas {
	GroupId group = 0;
	std::uint64_t incarnation = 0;
	std::vector<NodeId> replicas;
	bool operator==(const ShardReplicas&) const = default;
};

class ShardMapView {
public:
	virtual ~ShardMapView() = default;

	// This node.
	virtual NodeId self() const = 0;

	// The index's shards in shard order; empty when there is no such index.
	virtual task::task<std::vector<ShardReplicas>> shards(std::string_view index) const = 0;

	// The index paths that start with `prefix` (every one for an empty prefix), sorted: an index
	// pattern's candidates, from the root's records by prefix scan, nested paths included.
	virtual task::task<std::vector<std::string>> indexes(std::string_view prefix) const = 0;

	// False for a node the directory holds `down` (cluster-membership); a search asks it last.
	virtual bool up(NodeId node) const = 0;
};

// The order a search asks a shard's replicas in: this node first when it holds one, then the others
// that are up in the record's order, then the ones that are down.
std::vector<NodeId> replica_order(const ShardMapView& map, const ShardReplicas& shard);

// A shard map held in memory, set whole: one node's indexes, or a placement a test lays out. It
// stands in for the root's records until the shard-map task replaces it.
class StaticShardMap final : public ShardMapView {
public:
	explicit StaticShardMap(NodeId self) : self_(self) { }

	void set(std::string index, std::vector<ShardReplicas> shards);
	void set_down(NodeId node, bool down);

	NodeId self() const override { return self_; }
	task::task<std::vector<ShardReplicas>> shards(std::string_view index) const override;
	task::task<std::vector<std::string>> indexes(std::string_view prefix) const override;
	bool up(NodeId node) const override { return !down_.contains(node); }

private:
	NodeId self_;
	std::map<std::string, std::vector<ShardReplicas>, std::less<>> indexes_;
	std::set<NodeId> down_;
};

}  // namespace cluster
