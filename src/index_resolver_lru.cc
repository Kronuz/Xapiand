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

#include "index_resolver_lru.h"

#include "database/flags.h"       // for  DB_*
#include "database/handler.h"     // for DatabaseHandler
#include "database/utils.h"       // for UNKNOWN_REVISION
#include "manager.h"              // XapiandManager::dispatch_command, XapiandManager::resolve_index_endpoints
#include "opts.h"                 // for opts::*
#include "reserved/schema.h"      // RESERVED_*
#include "reserved/fields.h"      // ID_FIELD_NAME, ...
#include "server/discovery.h"     // for primary_updater
#include "serialise.h"            // for KEYWORD_STR



#define L_SHARDS L_NOTHING

// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_SHARDS
// #define L_SHARDS L_DARK_SLATE_BLUE


IndexSettingsShard::IndexSettingsShard() :
	version(UNKNOWN_REVISION),
	modified(false)
{
}

IndexSettings::IndexSettings() :
	version(UNKNOWN_REVISION),
	loaded(false),
	saved(false),
	modified(false),
	stalled(std::chrono::steady_clock::time_point::min()),
	num_shards(0),
	num_replicas_plus_master(0)
{
}


IndexSettings::IndexSettings(Xapian::rev version, bool loaded, bool saved, bool modified, std::chrono::steady_clock::time_point stalled, size_t num_shards, size_t num_replicas_plus_master, const std::vector<IndexSettingsShard>& shards) :
	version(version),
	loaded(loaded),
	saved(saved),
	modified(modified),
	stalled(stalled),
	num_shards(num_shards),
	num_replicas_plus_master(num_replicas_plus_master),
	shards(shards)
{
#ifndef NDEBUG
	size_t replicas_size = 0;
	for (auto& shard : shards) {
		auto replicas_size_ = shard.nodes.size();
		assert(replicas_size_ != 0 && (!replicas_size || replicas_size == replicas_size_));
		replicas_size = replicas_size_;
	}
#endif
}


std::string
IndexSettings::__repr__() const {
	std::vector<std::string> qq;
	for (auto& ss : shards) {
		std::vector<std::string> q;
		for (auto& s : ss.nodes) {
			q.push_back(repr(s));
		}
		qq.push_back(strings::format("[{}]", strings::join(q, ", ")));
	}
	return strings::format("[{}]", strings::join(qq, ", "));
}


constexpr int CONFLICT_RETRIES = 10;   // Number of tries for resolving version conflicts


void
settle_replicas(IndexSettings& index_settings, std::vector<std::shared_ptr<const Node>>& nodes, size_t num_replicas_plus_master)
{
	L_CALL("settle_replicas(<index_settings>, {})", num_replicas_plus_master);

	size_t total_nodes = Node::total_nodes();
	if (num_replicas_plus_master > total_nodes) {
		num_replicas_plus_master = total_nodes;
	}
	for (auto& shard : index_settings.shards) {
		auto shard_nodes_size = shard.nodes.size();
		assert(shard_nodes_size);
		if (shard_nodes_size < num_replicas_plus_master) {
			std::unordered_set<std::string> used;
			for (size_t i = 0; i < shard_nodes_size; ++i) {
				used.insert(strings::lower(shard.nodes[i]));
			}
			if (nodes.empty()) {
				nodes = Node::nodes();
			}
			auto primary = strings::lower(shard.nodes[0]);
			size_t idx = 0;
			for (const auto& node : nodes) {
				if (node->lower_name() == primary) {
					break;
				}
				++idx;
			}
			auto nodes_size = nodes.size();
			for (auto n = shard_nodes_size; n < num_replicas_plus_master; ++n) {
				std::shared_ptr<const Node> node;
				do {
					node = nodes[++idx % nodes_size];
					assert(idx < nodes_size * 2);
				} while (used.count(node->lower_name()));
				shard.nodes.push_back(node->name());
				used.insert(node->lower_name());
			}
			shard.modified = true;
			index_settings.saved = false;
		} else if (shard_nodes_size > num_replicas_plus_master) {
			assert(num_replicas_plus_master);
			shard.nodes.resize(num_replicas_plus_master);
			shard.modified = true;
			index_settings.saved = false;
		}
	}
}


std::vector<IndexSettingsShard>
calculate_shards(size_t routing_key, std::vector<std::shared_ptr<const Node>>& nodes, size_t num_shards)
{
	L_CALL("calculate_shards({}, {})", routing_key, num_shards);

	std::vector<IndexSettingsShard> shards;
	if (Node::total_nodes()) {
		if (routing_key < num_shards) {
			routing_key += num_shards;
		}
		for (size_t s = 0; s < num_shards; ++s) {
			IndexSettingsShard shard;
			if (nodes.empty()) {
				nodes = Node::nodes();
			}
			size_t idx = (routing_key - s) % nodes.size();
			auto node = nodes[idx];
			shard.nodes.push_back(node->name());
			shard.modified = true;
			shards.push_back(std::move(shard));
		}
	}
	return shards;
}


void
update_primary(const std::string& unsharded_normalized_path, IndexSettings& index_settings, std::shared_ptr<const Node> primary_node)
{
	L_CALL("update_primary({}, <index_settings>)", repr(unsharded_normalized_path));

	auto now = std::chrono::steady_clock::now();

	if (index_settings.stalled > now) {
		return;
	}

	bool updated = false;
	size_t shard_num = 0;
	for (auto& shard : index_settings.shards) {
		++shard_num;
		auto it_b = shard.nodes.begin();
		auto it_e = shard.nodes.end();
		auto it = it_b;
		for (; it != it_e; ++it) {
			auto node = Node::get_node(*it);
			if (node && !node->empty()) {
				if (node->is_active() || (primary_node && *node == *primary_node)) {
					break;
				}
			}
		}
		if (it != it_b && it != it_e) {
			if (primary_node) {
				auto normalized_path = index_settings.shards.size() > 1 ? strings::format("{}/.__{}", unsharded_normalized_path, shard_num) : unsharded_normalized_path;
				auto from_node = Node::get_node(*it_b);
				auto to_node = Node::get_node(*it);
				L_INFO("Primary shard {} moved from node {}{}" + INFO_COL + " to {}{}",
					repr(normalized_path),
					from_node->col().ansi(), from_node->name(),
					to_node->col().ansi(), to_node->name());
				std::swap(*it, *it_b);
				updated = true;
				shard.modified = true;
				index_settings.saved = false;
			} else if (index_settings.stalled == std::chrono::steady_clock::time_point::min()) {
				index_settings.stalled = now + std::chrono::milliseconds(opts.database_stall_time);
				break;
			} else if (index_settings.stalled <= now) {
				auto node = Node::get_node(*it_b);
				if (node->last_seen() <= index_settings.stalled) {
					auto normalized_path = index_settings.shards.size() > 1 ? strings::format("{}/.__{}", unsharded_normalized_path, shard_num) : unsharded_normalized_path;
					XapiandManager::dispatch_command(XapiandManager::Command::ELECT_PRIMARY, normalized_path);
				}
				index_settings.stalled = now + std::chrono::milliseconds(opts.database_stall_time);
			}
		}
	}

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		if (updated) {
			index_settings.stalled = std::chrono::steady_clock::time_point::min();
			primary_updater()->debounce(unsharded_normalized_path, index_settings.shards.size(), unsharded_normalized_path);
		}
	}
#endif
}


void
save_shards(const std::string& unsharded_normalized_path, size_t num_replicas_plus_master, IndexSettingsShard& shard)
{
	L_CALL("save_shards(<shard>)");

	if (shard.modified) {
		Endpoint endpoint(".xapiand/indices");
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		assert(!endpoints.empty());
		DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
		MsgPack obj({
			{ RESERVED_IGNORE, SCHEMA_FIELD_NAME },
			{ ID_FIELD_NAME, {
				{ RESERVED_TYPE,  KEYWORD_STR },
			} },
			{ "number_of_shards", {
				{ RESERVED_INDEX, "none" },
				{ RESERVED_TYPE,  "positive" },
			} },
			{ "number_of_replicas", {
				{ RESERVED_INDEX, "none" },
				{ RESERVED_TYPE,  "positive" },
				{ RESERVED_VALUE, num_replicas_plus_master - 1 },
			} },
			{ "shards", {
				{ RESERVED_INDEX, "field_terms" },
				{ RESERVED_TYPE,  "array/keyword" },
				{ RESERVED_VALUE, shard.nodes },
			} },
		});
		auto info = db_handler.update(unsharded_normalized_path, shard.version, false, true, obj, false, msgpack_type).first;
		shard.version = info.version;
		shard.modified = false;
	}
}


void
save_settings(const std::string& unsharded_normalized_path, IndexSettings& index_settings)
{
	L_CALL("save_settings(<index_settings>)");

	assert(index_settings.shards.size() == index_settings.num_shards);

	bool settings_saved_old = index_settings.saved;

	if (index_settings.num_shards == 1) {
		save_shards(unsharded_normalized_path, index_settings.num_replicas_plus_master, index_settings.shards[0]);
		index_settings.saved = true;
		index_settings.loaded = true;
	} else if (index_settings.num_shards != 0) {
		if (!index_settings.shards[0].nodes.empty()) {
			if (index_settings.modified) {
				Endpoint endpoint(".xapiand/indices");
				auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
				assert(!endpoints.empty());
				DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
				MsgPack obj({
					{ RESERVED_IGNORE, SCHEMA_FIELD_NAME },
					{ ID_FIELD_NAME, {
						{ RESERVED_TYPE,  KEYWORD_STR },
					} },
					{ "number_of_shards", {
						{ RESERVED_INDEX, "none" },
						{ RESERVED_TYPE,  "positive" },
						{ RESERVED_VALUE, index_settings.num_shards },
					} },
					{ "number_of_replicas", {
						{ RESERVED_INDEX, "none" },
						{ RESERVED_TYPE,  "positive" },
						{ RESERVED_VALUE, index_settings.num_replicas_plus_master - 1 },
					} },
					{ "shards", {
						{ RESERVED_INDEX, "field_terms" },
						{ RESERVED_TYPE,  "array/keyword" },
					} },
				});
				auto info = db_handler.update(unsharded_normalized_path, index_settings.version, false, true, obj, false, msgpack_type).first;
				index_settings.version = info.version;
				index_settings.modified = false;
			}
		}
		size_t shard_num = 0;
		for (auto& shard : index_settings.shards) {
			if (!shard.nodes.empty()) {
				auto shard_normalized_path = strings::format("{}/.__{}", unsharded_normalized_path, ++shard_num);
				save_shards(shard_normalized_path, index_settings.num_replicas_plus_master, shard);
			}
		}
		index_settings.saved = true;
		index_settings.loaded = true;
	}
#ifdef XAPIAND_CLUSTERING
	if (!settings_saved_old && index_settings.saved) {
		settings_updater()->debounce(unsharded_normalized_path,  index_settings.version, unsharded_normalized_path);
	}
#endif
}


IndexSettingsShard
load_replicas(const Endpoint& endpoint, const MsgPack& obj)
{
	L_CALL("load_replicas(<obj>)");

	IndexSettingsShard shard;

	auto it = obj.find(VERSION_FIELD_NAME);
	if (it != obj.end()) {
		auto& version_val = it.value();
		if (!version_val.is_number()) {
			THROW(Error, "Inconsistency in '{}' configured for {}: Invalid version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
		}
		shard.version = version_val.u64();
	}

	it = obj.find("shards");
	if (it != obj.end()) {
		auto& replicas_val = it.value();
		if (!replicas_val.is_array()) {
			THROW(Error, "Inconsistency in 'shards' configured for {}: Invalid array", repr(endpoint.to_string()));
		}
		for (auto& node_name_val : replicas_val) {
			if (!node_name_val.is_string()) {
				THROW(Error, "Inconsistency in 'shards' configured for {}: Invalid node name", repr(endpoint.to_string()));
			}
			shard.nodes.push_back(node_name_val.str());
		}
	}

	return shard;
}


IndexSettings
load_settings(const std::string& unsharded_normalized_path)
{
	L_CALL("load_settings(<index_endpoints>, {})", repr(unsharded_normalized_path));

	auto nodes = Node::nodes();
	assert(!nodes.empty());

	Endpoint endpoint(".xapiand/indices");

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			IndexSettings index_settings;

			auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
			if(endpoints.empty()) {
				continue;
			}

			DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
			auto document = db_handler.get_document(unsharded_normalized_path);
			auto obj = document.get_obj();

			auto it = obj.find(VERSION_FIELD_NAME);
			if (it != obj.end()) {
				auto& version_val = it.value();
				if (!version_val.is_number()) {
					THROW(Error, "Inconsistency in '{}' configured for {}: Invalid version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
				}
				index_settings.version = version_val.u64();
			} else {
				auto version_ser = document.get_value(DB_SLOT_VERSION);
				if (version_ser.empty()) {
					THROW(Error, "Inconsistency in '{}' configured for {}: No version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
				}
				index_settings.version = sortable_unserialise(version_ser);
			}

			it = obj.find("number_of_replicas");
			if (it != obj.end()) {
				auto& n_replicas_val = it.value();
				if (!n_replicas_val.is_number()) {
					THROW(Error, "Inconsistency in 'number_of_replicas' configured for {}: Invalid number", repr(endpoint.to_string()));
				}
				index_settings.num_replicas_plus_master = n_replicas_val.u64() + 1;
			}

			it = obj.find("number_of_shards");
			if (it != obj.end()) {
				auto& n_shards_val = it.value();
				if (!n_shards_val.is_number()) {
					THROW(Error, "Inconsistency in 'number_of_shards' configured for {}: Invalid number", repr(endpoint.to_string()));
				}
				index_settings.num_shards = n_shards_val.u64();
				size_t replicas_size = 0;
				for (size_t shard_num = 1; shard_num <= index_settings.num_shards; ++shard_num) {
					auto shard_normalized_path = strings::format("{}/.__{}", unsharded_normalized_path, shard_num);
					auto replica_document = db_handler.get_document(shard_normalized_path);
					auto shard = load_replicas(endpoint, replica_document.get_obj());
					auto replicas_size_ = shard.nodes.size();
					if (replicas_size_ == 0 || replicas_size_ > index_settings.num_replicas_plus_master || (replicas_size && replicas_size != replicas_size_)) {
						THROW(Error, "Inconsistency in number of replicas configured for {}", repr(endpoint.to_string()));
					}
					replicas_size = replicas_size_;
					index_settings.shards.push_back(std::move(shard));
				}
			}

			if (!index_settings.num_shards) {
				auto shard = load_replicas(endpoint, obj);
				auto replicas_size_ = shard.nodes.size();
				if (replicas_size_ == 0 || replicas_size_ > index_settings.num_replicas_plus_master) {
					THROW(Error, "Inconsistency in number of replicas configured for {}", repr(endpoint.to_string()));
				}
				index_settings.shards.push_back(std::move(shard));
				index_settings.num_shards = 1;
			}

			index_settings.loaded = true;
			return index_settings;
		} catch (const Xapian::DocNotFoundError&) {
			break;
		} catch (const Xapian::DatabaseNotFoundError&) {
			break;
		} catch (const Xapian::DatabaseNotAvailableError&) {
			if (t == 0) { throw; }
		}
	}

	return {};
}


MsgPack
shards_to_obj(const std::vector<IndexSettingsShard>& shards)
{
	MsgPack nodes = MsgPack::ARRAY();
	for (auto& shard : shards) {
		MsgPack node_replicas = MsgPack::ARRAY();
		for (auto name : shard.nodes) {
			auto node = Node::get_node(name);
			node_replicas.append(MsgPack({
				{ "node", node ? MsgPack(node->name()) : MsgPack::NIL() },
			}));
		}
		nodes.append(std::move(node_replicas));
	}
	return nodes;
}


IndexResolverLRU::IndexResolverLRU(ssize_t resolver_cache_size, std::chrono::milliseconds resolver_cache_timeout)
	: resolve_index_lru(resolver_cache_size, resolver_cache_timeout)
{
}


std::vector<std::vector<std::shared_ptr<const Node>>>
IndexResolverLRU::resolve_nodes(const IndexSettings& index_settings)
{
	L_CALL("IndexResolverLRU::resolve_nodes({})", shards_to_obj(index_settings.shards).to_string());

	std::vector<std::vector<std::shared_ptr<const Node>>> nodes;
	for (auto& shard : index_settings.shards) {
		std::vector<std::shared_ptr<const Node>> node_replicas;
		for (auto name : shard.nodes) {
			auto node = Node::get_node(name);
			node_replicas.push_back(std::move(node));
		}
		nodes.push_back(std::move(node_replicas));
	}
	return nodes;
}


IndexSettings
IndexResolverLRU::resolve_index_settings(std::string_view normalized_path, bool writable, [[maybe_unused]] bool primary, const MsgPack* settings, std::shared_ptr<const Node> primary_node, bool reload, bool rebuild, bool clear)
{
	L_CALL("IndexResolverLRU::resolve_index_settings({}, {}, {}, {}, {}, {}, {}, {})", repr(normalized_path), writable, primary, settings ? settings->to_string() : "null", primary_node ? repr(primary_node->to_string()) : "null", reload, rebuild, clear);

	auto strict = opts.strict;

	if (settings) {
		if (settings->is_map()) {
			auto strict_it = settings->find(RESERVED_STRICT);
			if (strict_it != settings->end()) {
				auto& strict_val = strict_it.value();
				if (strict_val.is_boolean()) {
					strict = strict_val.as_boolean();
				} else {
					THROW(ClientError, "Data inconsistency, '{}' must be boolean", RESERVED_STRICT);
				}
			}

			auto settings_it = settings->find(RESERVED_SETTINGS);
			if (settings_it != settings->end()) {
				settings = &settings_it.value();
			} else {
				settings = nullptr;
			}
		} else {
			settings = nullptr;
		}
	}

	IndexSettings index_settings;

	if (strings::startswith(normalized_path, ".xapiand/")) {
		// Everything inside .xapiand has the primary shard inside
		// the current leader and replicas everywhere.
		if (settings) {
			THROW(ClientError, "Cannot modify settings of cluster indices.");
		}

		// Primary databases in .xapiand are always in the master (or local, if master is unavailable)
		primary_node = Node::get_primary_node();
		if (!primary_node->is_active()) {
			L_WARNING("Primary node {}{}" + WARNING_COL + " is not active!", primary_node->col().ansi(), primary_node->to_string());
		}
		IndexSettingsShard shard;
		shard.nodes.push_back(primary_node->name());
		for (const auto& node : Node::nodes()) {
			if (!Node::is_superset(node, primary_node)) {
				shard.nodes.push_back(node->name());
			}
		}

		if (normalized_path == ".xapiand/indices") {
			// .xapiand/indices have the default number of shards
			for (size_t i = 0; i < opts.num_shards; ++i) {
				index_settings.shards.push_back(shard);
			}
			index_settings.num_shards = opts.num_shards;
		} else {
			// Everything else inside .xapiand has a single shard
			// (.xapiand/nodes, .xapiand/indices/.__N, .xapiand/* etc.)
			index_settings.shards.push_back(shard);
			index_settings.num_shards = 1;
		}

		return index_settings;
	}

	std::unique_lock<std::mutex> lk(resolve_index_lru_mtx);

	if (primary_node) {
		reload = true;
		rebuild = true;
	}

	auto it_e = resolve_index_lru.end();
	auto it = it_e;

	if (!settings && !reload && !rebuild && !clear) {
		it = resolve_index_lru.find(std::string(normalized_path));
		if (it != it_e) {
			index_settings = it->second;
			if (!writable || index_settings.saved) {
				return index_settings;
			}
		}
	}

	bool store_lru = false;

	auto unsharded = unsharded_path(normalized_path);
	std::string unsharded_normalized_path = std::string(unsharded.first);

	if (!reload) {
		it = resolve_index_lru.find(unsharded_normalized_path);
	}

	if (it != it_e) {
		if (clear) {
			resolve_index_lru.erase(it);
			return {};
		}
		index_settings = it->second;
		lk.unlock();
		L_SHARDS("Node settings for {} loaded from LRU", unsharded_normalized_path);
	} else {
		lk.unlock();
		index_settings = load_settings(unsharded_normalized_path);
		store_lru = true;
		if (!index_settings.shards.empty()) {
			for (auto& shard : index_settings.shards) {
				if (shard.nodes.empty()) {
					rebuild = true;  // There were missing replicas, rebuild!
					break;
				}
			}
			L_SHARDS("Node settings for {} loaded", unsharded_normalized_path);
		} else {
			index_settings.num_shards = opts.num_shards;
			index_settings.num_replicas_plus_master = opts.num_replicas + 1;
			index_settings.modified = true;
			index_settings.saved = false;
			L_SHARDS("Node settings for {} initialized", unsharded_normalized_path);
		}
	}

	assert(Node::total_nodes());

	if (settings) {
		auto num_shards = index_settings.num_shards;
		auto num_replicas_plus_master = index_settings.num_replicas_plus_master;

		auto num_shards_it = settings->find("number_of_shards");
		if (num_shards_it != settings->end()) {
			auto& num_shards_val = num_shards_it.value();
			if (num_shards_val.is_number()) {
				num_shards = num_shards_val.u64();
				if (num_shards == 0 || num_shards > 9999UL) {
					THROW(ClientError, "Invalid 'number_of_shards' setting");
				}
			} else {
				THROW(ClientError, "Data inconsistency, 'number_of_shards' must be integer");
			}
		} else if (writable) {
			if (strict && !index_settings.loaded) {
				THROW(MissingTypeError, "Value of 'number_of_shards' is missing");
			}
		}

		auto num_replicas_it = settings->find("number_of_replicas");
		if (num_replicas_it != settings->end()) {
			auto& num_replicas_val = num_replicas_it.value();
			if (num_replicas_val.is_number()) {
				num_replicas_plus_master = num_replicas_val.u64() + 1;
				if (num_replicas_plus_master == 0 || num_replicas_plus_master > 9999UL) {
					THROW(ClientError, "Invalid 'number_of_replicas' setting");
				}
			} else {
				THROW(ClientError, "Data inconsistency, 'number_of_replicas' must be numeric");
			}
		} else if (writable) {
			if (strict && !index_settings.loaded) {
				THROW(MissingTypeError, "Value of 'number_of_replicas' is missing");
			}
		}

		if (!index_settings.shards.empty()) {
			if (num_shards != index_settings.num_shards) {
				if (index_settings.loaded) {
					THROW(ClientError, "It is not allowed to change 'number_of_shards' setting");
				}
				rebuild = true;
			}
			if (num_replicas_plus_master != index_settings.num_replicas_plus_master) {
				rebuild = true;
			}
		}

		if (index_settings.num_replicas_plus_master != num_replicas_plus_master) {
			index_settings.num_replicas_plus_master = num_replicas_plus_master;
			index_settings.modified = true;
			index_settings.saved = false;
		}

		if (index_settings.num_shards != num_shards) {
			index_settings.num_shards = num_shards;
			index_settings.modified = true;
			index_settings.saved = false;
			index_settings.shards.clear();
		}
	} else if (writable) {
		if (strict && !index_settings.loaded) {
			THROW(MissingTypeError, "Index settings are missing");
		}
	}

	if (rebuild || index_settings.shards.empty()) {
		L_SHARDS("    Configuring {} replicas for {} shards", index_settings.num_replicas_plus_master - 1, index_settings.num_shards);

		std::vector<std::shared_ptr<const Node>> node_nodes;
		if (index_settings.shards.empty()) {
			size_t routing_key = jump_consistent_hash(unsharded_normalized_path, Node::total_nodes());
			index_settings.shards = calculate_shards(routing_key, node_nodes, index_settings.num_shards);
			assert(!index_settings.shards.empty());
			index_settings.modified = true;
			index_settings.saved = false;
		}
		settle_replicas(index_settings, node_nodes, index_settings.num_replicas_plus_master);

		if (writable) {
			update_primary(unsharded_normalized_path, index_settings, primary_node);
		}

		store_lru = true;
	}

	if (!index_settings.shards.empty()) {
		if (writable && !index_settings.saved) {
			save_settings(unsharded_normalized_path, index_settings);
			store_lru = true;
		}

		IndexSettings shard_settings;

		if (store_lru) {
			lk.lock();
			resolve_index_lru[unsharded_normalized_path] = IndexSettings(
				index_settings.version,
				index_settings.loaded,
				index_settings.saved,
				index_settings.modified,
				index_settings.stalled,
				index_settings.num_shards,
				index_settings.num_replicas_plus_master,
				index_settings.shards);
			size_t shard_num = 0;
			for (auto& shard : index_settings.shards) {
				assert(!shard.nodes.empty());
				auto shard_normalized_path = strings::format("{}/.__{}", unsharded_normalized_path, ++shard_num);
				std::vector<IndexSettingsShard> shard_shards;
				shard_shards.push_back(shard);
				resolve_index_lru[shard_normalized_path] = IndexSettings(
					shard.version,
					index_settings.loaded,
					index_settings.saved,
					shard.modified,
					index_settings.stalled,
					1,
					index_settings.num_replicas_plus_master,
					shard_shards);
				if (shard_normalized_path == normalized_path) {
					shard_settings = resolve_index_lru[shard_normalized_path];
				}
			}
			lk.unlock();
		} else {
			size_t shard_num = 0;
			for (auto& shard : index_settings.shards) {
				assert(!shard.nodes.empty());
				auto shard_normalized_path = strings::format("{}/.__{}", unsharded_normalized_path, ++shard_num);
				if (shard_normalized_path == normalized_path) {
					std::vector<IndexSettingsShard> shard_shards;
					shard_shards.push_back(shard);
					shard_settings = IndexSettings(
						shard.version,
						index_settings.loaded,
						index_settings.saved,
						shard.modified,
						index_settings.stalled,
						1,
						index_settings.num_replicas_plus_master,
						shard_shards);
					break;
				}
			}
		}

		if (!shard_settings.shards.empty()) {
			return shard_settings;
		}
	}
	return index_settings;
}


Endpoints
IndexResolverLRU::resolve_index_endpoints(const Endpoint& endpoint, bool writable, bool primary, const MsgPack* settings)
{
	L_CALL("IndexResolverLRU::resolve_index_endpoints({}, {}, {}, {})", repr(endpoint.to_string()), writable, primary, settings ? settings->to_string() : "null");

	auto unsharded = unsharded_path(endpoint.path);
	std::string unsharded_normalized_path_holder;
	if (unsharded.second) {
		unsharded_normalized_path_holder = std::string(unsharded.first);
	}
	auto& unsharded_normalized_path = unsharded.second ? unsharded_normalized_path_holder : endpoint.path;

	bool rebuild = false;
	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Endpoints endpoints;

			auto index_settings = resolve_index_settings(unsharded_normalized_path, writable, primary, settings, nullptr, t != CONFLICT_RETRIES, rebuild, false);
			auto nodes = resolve_nodes(index_settings);
			bool retry = !rebuild;
			rebuild = false;

			int n_shards = nodes.size();
			size_t shard_num = 0;
			for (const auto& shard_nodes : nodes) {
				auto path = n_shards == 1 ? unsharded_normalized_path : strings::format("{}/.__{}", unsharded_normalized_path, ++shard_num);
				if (!unsharded.second || path == endpoint.path) {
					Endpoint node_endpoint;
					for (const auto& node : shard_nodes) {
						node_endpoint = Endpoint(path, node);
						if (writable) {
							if (Node::is_active(node)) {
								L_SHARDS("Active writable node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							rebuild = retry;
							break;
						} else {
							if (Node::is_active(node)) {
								L_SHARDS("Active node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							if (primary) {
								L_SHARDS("Inactive primary node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							L_SHARDS("Inactive node ignored (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
						}
					}
					endpoints.add(node_endpoint);
					if (rebuild || unsharded.second) {
						break;
					}
				}
			}

			if (!rebuild) {
				return endpoints;
			}
		} catch (const Xapian::DocVersionConflictError&) {
			if (--t == 0) { throw; }
		}
	}
}


void
IndexResolverLRU::invalidate_settings(const std::string& uri)
{
	L_CALL("IndexResolverLRU::invalidate_settings({})", repr(uri));

	std::unique_lock<std::mutex> lk(resolve_index_lru_mtx);
	Endpoint endpoint(uri);
	auto unsharded = unsharded_path(endpoint.path);
	std::string unsharded_normalized_path = std::string(unsharded.first);
	auto it_e = resolve_index_lru.end();
	auto it = resolve_index_lru.find(unsharded_normalized_path);
	if (it != it_e) {
		auto index_settings = it->second;
		if (index_settings.num_shards > 1) {
			for (size_t i = 1; i <= index_settings.num_shards; ++i) {
				resolve_index_lru.erase(strings::format("{}/.__{}", unsharded_normalized_path, i));
			}
		}
		resolve_index_lru.erase(it);
	}
}
