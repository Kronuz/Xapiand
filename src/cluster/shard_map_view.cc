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

#include "cluster/shard_map_view.h"

#include <algorithm>    // for std::find
#include <utility>      // for std::move

namespace cluster {

std::vector<NodeId>
replica_order(const ShardMapView& map, const ShardReplicas& shard)
{
	std::vector<NodeId> order;
	order.reserve(shard.replicas.size());
	const auto self = map.self();
	if (std::find(shard.replicas.begin(), shard.replicas.end(), self) != shard.replicas.end()) {
		order.push_back(self);
	}
	for (const bool up : {true, false}) {
		for (const auto node : shard.replicas) {
			if (node != self && map.up(node) == up) {
				order.push_back(node);
			}
		}
	}
	return order;
}


void
StaticShardMap::set(std::string index, std::vector<ShardReplicas> shards)
{
	indexes_[std::move(index)] = std::move(shards);
}


void
StaticShardMap::set_down(NodeId node, bool down)
{
	if (down) {
		down_.insert(node);
	} else {
		down_.erase(node);
	}
}


task::task<std::vector<ShardReplicas>>
StaticShardMap::shards(std::string_view index) const
{
	const auto found = indexes_.find(index);
	co_return found != indexes_.end() ? found->second : std::vector<ShardReplicas>{};
}


task::task<std::vector<std::string>>
StaticShardMap::indexes(std::string_view prefix) const
{
	std::vector<std::string> found;
	for (auto it = indexes_.lower_bound(prefix); it != indexes_.end() && it->first.starts_with(prefix); ++it) {
		found.push_back(it->first);
	}
	co_return found;
}

}  // namespace cluster
