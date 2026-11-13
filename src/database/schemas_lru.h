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

#include <functional>            // for std::function
#include <memory>                // for std::shared_ptr
#include <mutex>                 // for std::mutex
#include <optional>              // for std::optional
#include <string_view>           // for std::string_view

#include "database/schema.h"
#include "endpoint.h"
#include "lru.h"
#include "msgpack.h"
#include "single_flight.h"           // for SingleFlight
#include "task.hh"                // for task::task
#include "engine/types.h"                  // for engine::rev, docid


constexpr size_t MAX_SCHEMA_RECURSION = 10;


class DatabaseHandler;


class SchemasLRU {
	mutable std::mutex schemas_mtx;
	lru::aging_lru<std::string, std::shared_ptr<const MsgPack>> schemas;

	mutable std::mutex versions_mtx;
	lru::aging_lru<std::string, engine::rev> versions;

	// What a metadata flight yields: the stored schema document, or that the cache holds the schema now.
	struct MetadataRead {
		std::string ser;
		bool cached = false;
	};

	// One metadata read, and one foreign-schema read, per schema at a time: concurrent first uses of a
	// schema await the one load (D11). Each load puts its schema in the cache before its flight ends.
	SingleFlight<std::string, MetadataRead> metadata_loads;
	SingleFlight<std::string, std::pair<engine::rev, MsgPack>> foreign_loads;

	task::task<MetadataRead> load_metadata(
		const std::string& local_schema_path, const Endpoints& endpoints, int read_flags,
		std::shared_ptr<std::unordered_set<std::string>> context, task::budget* budget,
		const std::string& foreign_uri, std::string_view endpoints_path, const std::shared_ptr<const MsgPack>& new_schema);
	std::optional<MetadataRead> cached_metadata(const std::string& local_schema_path);

	task::task<std::pair<engine::rev, MsgPack>> load_foreign(
		const std::string& foreign_uri, std::string_view foreign_id, const Endpoint& foreign_endpoint, int read_flags,
		std::shared_ptr<std::unordered_set<std::string>> context, task::budget* budget);
	std::optional<std::pair<engine::rev, MsgPack>> cached_foreign(const std::string& foreign_uri);

	task::task<std::tuple<bool, std::shared_ptr<const MsgPack>, std::string, std::string>>
	_update(
		const char* prefix,
		bool writable,
		const std::shared_ptr<const MsgPack>& new_schema, const MsgPack* schema_obj,
		const Endpoints& endpoints, int read_flags, std::shared_ptr<std::unordered_set<std::string>> context,
		task::budget* budget);   // the caller's: its token ends the schema reads' turn waits

public:
	// Test seams, awaited when a first use missed the cache and is about to claim the load.
	std::function<task::task<void>()> before_metadata_claim;
	std::function<task::task<void>()> before_foreign_claim;
	std::function<task::task<void>()> after_metadata_flight;

	SchemasLRU(ssize_t max_size, std::chrono::milliseconds max_age);

	task::task<std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>> get(DatabaseHandler* db_handler, const MsgPack* obj);

	task::task<bool> set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema);

	void updated(const std::string& uri, engine::rev version);

	void cleanup();

	// Reads of schemas started (metadata and foreign), calls that awaited a read in flight, and the ones
	// awaiting one now.
	std::uint64_t loads() const noexcept { return metadata_loads.loads() + foreign_loads.loads(); }
	std::uint64_t joined() const noexcept { return metadata_loads.joined() + foreign_loads.joined(); }
	std::uint64_t waiting() const noexcept { return metadata_loads.waiting() + foreign_loads.waiting(); }

	std::string __repr__() const;

	std::string dump_schemas(int level = 1) const;
};
