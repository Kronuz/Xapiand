/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <functional>                         // for std::function
#include <mutex>                              // for mutex, lock_guard, uniqu...
#include <optional>                           // for std::optional
#include <string>                             // for std::string
#include <string_view>                        // for std::string_view
#include <vector>                             // for std::vector
#include <memory>                             // for std::shared_ptr

#include "endpoint.h"                         // for Endpoint
#include "lru.h"                              // for lru::lru
#include "msgpack.h"                          // for MsgPack, object::object
#include "node.h"                             // for Node
#include "single_flight.h"                    // for SingleFlight
#include "task.hh"                            // for task::task
#include "engine/types.h"                  // for engine::rev, docid


struct IndexSettings;


struct IndexSettingsShard {
	engine::rev version;
	bool modified;

	std::vector<std::string> nodes;

	IndexSettingsShard();
};


struct IndexSettings {
	engine::rev version;
	bool loaded;
	bool saved;
	bool modified;

	size_t num_shards;
	size_t num_replicas_plus_master;
	std::vector<IndexSettingsShard> shards;

	IndexSettings();

	IndexSettings(engine::rev version, bool loaded, bool saved, bool modified, size_t num_shards, size_t num_replicas_plus_master, const std::vector<IndexSettingsShard>& shards);

	std::string __repr__() const;
};


class IndexResolverLRU {

public:
	// Test seam, awaited when a first use missed the cache and is about to claim the read.
	std::function<task::task<void>()> before_settings_claim;

	IndexResolverLRU(ssize_t resolver_cache_size, std::chrono::milliseconds resolver_cache_timeout);

	task::task<IndexSettings> resolve_index_settings(std::string_view normalized_slashed_path, bool writable, const MsgPack* settings, bool reload, bool rebuild, bool clear);
	task::task<Endpoints> resolve_index_endpoints(const Endpoint& endpoint, bool writable, const MsgPack* settings);

	void invalidate_settings(const std::string& uri);

	static std::vector<std::vector<std::shared_ptr<const Node>>> resolve_nodes(const IndexSettings& index_settings);

	// Reads of an index's settings started, calls that awaited a read in flight, and the ones awaiting one now.
	std::uint64_t loads() const noexcept { return settings_loads.loads(); }
	std::uint64_t joined() const noexcept { return settings_loads.joined(); }
	std::uint64_t waiting() const noexcept { return settings_loads.waiting(); }

private:
	// The map's lock guards O(log n) lookups and inserts and is never held across a read (D11).
	std::mutex resolve_index_lru_mtx;
	lru::aging_lru<std::string, IndexSettings> resolve_index_lru;

	// Concurrent first uses of an index await one read of its settings, which puts them in the cache before it ends.
	SingleFlight<std::string, IndexSettings> settings_loads;

	task::task<IndexSettings> load_and_publish(const std::string& unsharded_normalized_path);
	std::optional<IndexSettings> cached_settings(const std::string& unsharded_normalized_path);
	void store_settings(const std::string& unsharded_normalized_path, const IndexSettings& index_settings);
};
