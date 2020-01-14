/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <mutex>                              // for mutex, lock_guard, uniqu...
#include <string>                             // for std::string
#include <string_view>                        // for std::string_view
#include <vector>                             // for std::vector
#include <memory>                             // for std::shared_ptr

#include "endpoint.h"                         // for Endpoint
#include "lru.h"                              // for lru::lru
#include "msgpack.h"                          // for MsgPack, object::object
#include "node.h"                             // for Node
#include "xapian.h"                           // for Xapian::rev


struct IndexSettings;


struct IndexSettingsShard {
	Xapian::rev version;
	bool modified;

	std::vector<std::string> nodes;

	IndexSettingsShard();
};


struct IndexSettings {
	Xapian::rev version;
	bool loaded;
	bool saved;
	bool modified;

	std::chrono::steady_clock::time_point stalled;

	size_t num_shards;
	size_t num_replicas_plus_master;
	std::vector<IndexSettingsShard> shards;

	IndexSettings();

	IndexSettings(Xapian::rev version, bool loaded, bool saved, bool modified, std::chrono::steady_clock::time_point stalled, size_t num_shards, size_t num_replicas_plus_master, const std::vector<IndexSettingsShard>& shards);

	std::string __repr__() const;
};


class IndexResolverLRU {

public:
	IndexResolverLRU() = default;
	IndexSettings resolve_index_settings(std::string_view normalized_slashed_path, bool writable, bool primary, const MsgPack* settings, std::shared_ptr<const Node> primary_node, bool reload, bool rebuild, bool clear);
	Endpoints resolve_index_endpoints(const Endpoint& endpoint, bool writable, bool primary, const MsgPack* settings);

	static void invalidate_settings(const std::string& uri);

	static std::vector<std::vector<std::shared_ptr<const Node>>> resolve_nodes(const IndexSettings& index_settings);

private:
    static std::mutex resolve_index_lru_mtx;
    static lru::lru<std::string, IndexSettings> resolve_index_lru;
};
