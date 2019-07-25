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

#include "database/schemas_lru.h"

#include <cassert>                                // for assert
#include <chrono>                                 // for std::chrono_literals

#include "database/handler.h"                     // for DatabaseHandler
#include "database/utils.h"                       // for unsharded_path
#include "log.h"                                  // for L_CALL
#include "manager.h"                              // for XapiandManager::resolve_index_endpoints
#include "opts.h"                                 // for opts.solo
#include "reserved/schema.h"                      // for RESERVED_RECURSE, RESERVED_FOREIGN, ...
#include "serialise.h"                            // for KEYWORD_STR
#include "server/discovery.h"                     // for schema_updater
#include "strings.hh"                             // for strings::format, strings::replace
#include "url_parser.h"                           // for urldecode

#define L_SCHEMA L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_SCHEMA
// #define L_SCHEMA L_STACKED_GREY

using namespace std::chrono_literals;


static const MsgPack non_strict({{ RESERVED_STRICT, false }});


template <typename ErrorType>
static inline std::pair<const MsgPack*, const MsgPack*>
validate_schema(const MsgPack& object, const char* prefix, std::string& foreign_uri, std::string& foreign_path, std::string& foreign_id)
{
	L_CALL("validate_schema({})", repr(object.to_string()));

	auto checked = Schema::check<ErrorType>(object, prefix, true);
	if (checked.first) {
		foreign_uri = checked.first->str();
		std::string_view foreign_path_view, foreign_id_view;
		split_path_id(foreign_uri, foreign_path_view, foreign_id_view);
		if (foreign_path_view.empty() || foreign_id_view.empty()) {
			THROW(ErrorType, "{}'{}' must contain index and docid [{}]", prefix, RESERVED_FOREIGN, repr(foreign_uri));
		}
		foreign_path = urldecode(foreign_path_view);
		foreign_id = urldecode(foreign_id_view);
	}
	return checked;
}


static inline bool
compare_schema(const MsgPack& a, const MsgPack& b)
{
	return a == b;
}


static inline std::pair<Xapian::rev, MsgPack>
load_shared(std::string_view id, const Endpoint& endpoint, int read_flags, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("load_shared({}, {}, {}, {})", repr(id), repr(endpoint.to_string()), read_flags, context ? std::to_string(context->size()) : "nullptr");

	auto path = endpoint.path;
	if (!context) {
		context = std::make_shared<std::unordered_set<std::string>>();
	}
	if (context->size() > MAX_SCHEMA_RECURSION) {
		THROW(ClientError, "Maximum recursion reached: {}", endpoint.to_string());
	}
	if (!context->insert(path).second) {
		if (path == ".xapiand/indices") {
			// Return initial schema for .xapiand/indices (chicken and egg problem)
			return std::make_pair(0, MsgPack::MAP());
		}
		THROW(ClientError, "Cyclic schema reference detected: {}", endpoint.to_string());
	}
	try {
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint);
		if (endpoints.empty()) {
			THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
		}
		DatabaseHandler _db_handler(endpoints, read_flags, context);
		Xapian::rev version;
		auto document = _db_handler.get_document(id);
		auto obj = document.get_obj();
		auto it = obj.find(VERSION_FIELD_NAME);
		if (it != obj.end()) {
			auto& version_val = it.value();
			if (!version_val.is_number()) {
				THROW(Error, "Inconsistency in '{}' for {}: Invalid version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
			}
			version = version_val.u64();
		} else {
			auto version_ser = document.get_value(DB_SLOT_VERSION);
			if (version_ser.empty()) {
				THROW(Error, "Inconsistency in '{}' for {}: No version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
			}
			version = sortable_unserialise(version_ser);
		}
		obj = obj[SCHEMA_FIELD_NAME];
		Schema::check<Error>(obj, "Foreign schema is invalid: ", false);
		context->erase(path);
		return std::make_pair(version, obj);
	} catch (...) {
		context->erase(path);
		throw;
	}
}


static inline std::pair<Xapian::rev, MsgPack>
save_shared(std::string_view id, const MsgPack& schema, Xapian::rev version, const Endpoint& endpoint, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("save_shared({}, {}, {}. {}, {})", repr(id), schema.to_string(), version, repr(endpoint.to_string()), context ? std::to_string(context->size()) : "nullptr");

	Schema::check<ClientError>(schema, "Foreign schema is invalid: ", false);

	auto& path = endpoint.path;
	if (!context) {
		context = std::make_shared<std::unordered_set<std::string>>();
	}
	if (context->size() > MAX_SCHEMA_RECURSION) {
		THROW(ClientError, "Maximum recursion reached: {}", endpoint.to_string());
	}
	if (!context->insert(path).second) {
		if (path == ".xapiand/indices") {
			// Ignore .xapiand/indices (chicken and egg problem)
			return std::make_pair(0, schema);
		}
		THROW(ClientError, "Cyclic schema reference detected: {}", endpoint.to_string());
	}
	try {
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true, false, &non_strict);
		if (endpoints.empty()) {
			THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
		}
		DatabaseHandler _db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE, context);
		// FIXME: Process the subfields instead of ignoring.
		auto updated = _db_handler.update(id, version, false, true, MsgPack({
			{ RESERVED_IGNORE, SCHEMA_FIELD_NAME },
			{ SCHEMA_FIELD_NAME, schema },
		}), false, msgpack_type);
		auto obj = std::move(updated.second);
		obj = obj[SCHEMA_FIELD_NAME];
		context->erase(path);
		return std::make_pair(updated.first.version, obj);
	} catch (...) {
		context->erase(path);
		throw;
	}
}


SchemasLRU::SchemasLRU(ssize_t max_size) :
	schemas(max_size, 3600s),
	versions(0, 3600s)
{
}


std::tuple<bool, std::shared_ptr<const MsgPack>, std::string, std::string>
SchemasLRU::_update([[maybe_unused]] const char* prefix, bool writable, const std::shared_ptr<const MsgPack>& new_schema, const MsgPack* schema_obj, const Endpoints& endpoints, int read_flags, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("SchemasLRU::_update({}, {}, {}, {}, {}, {}, <context>)", repr(prefix), writable, new_schema ? repr(new_schema->to_string()) : "nullptr", schema_obj ? repr(schema_obj->to_string()) : "nullptr", repr(endpoints.to_string()), read_flags);

	std::string foreign_uri, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool failure = false;

	// We first try to load schema from the LRU cache
	std::shared_ptr<const MsgPack> local_schema_ptr;
	const auto endpoints_path = unsharded_path(endpoints[0].path).first;
	const auto local_schema_path = std::string(endpoints_path) + "/";  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	L_SCHEMA("{}" + LIGHT_GREY + "[{}]{}{}{}", prefix, repr(local_schema_path), new_schema ? " new_schema=" : schema_obj ? " schema_obj=" : "", new_schema ? repr(new_schema->to_string()) : schema_obj ? repr(schema_obj->to_string()) : "", writable ? " (writable)" : "");
	{
		std::lock_guard<std::mutex> lk(schemas_mtx);
		local_schema_ptr = schemas[local_schema_path];
	}

	if (new_schema) {
		// Now we check if the schema points to a foreign schema
		validate_schema<Error>(*new_schema, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	} else if (schema_obj) {
		// Check if passed object specifies a foreign schema
		validate_schema<ClientError>(*schema_obj, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	}

	// Whatever was passed by the user doesn't specify a foreign schema,
	// or there it wasn't passed anything.
	if (local_schema_ptr) {
		// Schema was in the cache
		L_SCHEMA("{}" + DARK_GREEN + "Schema [{}] found in cache (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), local_schema_ptr->get_flags(), repr(local_schema_ptr->to_string()));
		if (!foreign_uri.empty()) {
			schema_ptr = std::make_shared<MsgPack>(MsgPack({
				{ RESERVED_TYPE, "foreign/object" },
				{ RESERVED_FOREIGN, foreign_uri },
			}));
			if (schema_ptr == local_schema_ptr || compare_schema(*schema_ptr, *local_schema_ptr)) {
				schema_ptr = local_schema_ptr;
				L_SCHEMA("{}" + GREEN + "Local Schema [{}] already had the same foreign link in the LRU (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			} else {
				schema_ptr->lock();
				assert(schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[local_schema_path];
				if (!schema || schema == local_schema_ptr) {
					schema = schema_ptr;
					L_SCHEMA("{}" + GREEN + "Local Schema [{}] added new foreign link to the LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(local_schema_ptr->to_string()), repr(schema_ptr->to_string()));
				} else {
					local_schema_ptr = schema;
					assert(local_schema_ptr);
					if (schema_ptr == local_schema_ptr || compare_schema(*schema_ptr, *local_schema_ptr)) {
						schema_ptr = local_schema_ptr;
						L_SCHEMA("{}" + GREEN + "Local Schema [{}] couldn't add new foreign link but it already was the same foreign link in the LRU (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
					} else {
						L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] couldn't add new foreign link to the LRU (version {}): " + DIM_GREY + "{} ==> {}", prefix, repr(local_schema_path), local_schema_ptr->get_flags(), repr(schema_ptr->to_string()), repr(local_schema_ptr->to_string()));
						schema_ptr = local_schema_ptr;
						failure = true;
					}
				}
			}
		} else {
			schema_ptr = local_schema_ptr;
		}
	} else {
		// Schema needs to be read
		L_SCHEMA("{}" + DARK_TURQUOISE + "Local Schema [{}] not found in cache, try loading from metadata", prefix, repr(local_schema_path));
		std::string schema_ser;
		try {
			DatabaseHandler _db_handler(endpoints, read_flags, context);
			schema_ser = _db_handler.get_metadata(std::string_view(RESERVED_SCHEMA));
		} catch (const Xapian::DocNotFoundError&) {
		} catch (const Xapian::DatabaseNotFoundError&) {
		}
		if (schema_ser.empty()) {
			if (!foreign_uri.empty()) {
				schema_ptr = std::make_shared<MsgPack>(MsgPack({
					{ RESERVED_TYPE, "foreign/object" },
					{ RESERVED_FOREIGN, foreign_uri },
				}));
				schema_ptr->lock();
				L_SCHEMA("{}" + LIGHT_CORAL + "Schema [{}] couldn't be loaded from metadata, create a new foreign link (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			} else if (endpoints_path != ".xapiand/nodes") {
				// Implement foreign schemas in .xapiand/indices by default:
				schema_ptr = std::make_shared<MsgPack>(MsgPack({
					{ RESERVED_TYPE, "foreign/object" },
					{ RESERVED_FOREIGN, strings::format(".xapiand/indices/{}", strings::replace(endpoints_path, "/", "%2F")) },
				}));
				schema_ptr->lock();
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create a new default foreign link (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			} else if (new_schema) {
				schema_ptr = new_schema;
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create from new schema (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			} else {
				schema_ptr = Schema::get_initial_schema();
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create a new initial schema (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			}
		} else {
			schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
			schema_ptr->lock();
			schema_ptr->set_flags(1);
			L_SCHEMA("{}" + GREEN + "Local Schema [{}] was loaded from metadata (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
		}
		assert(schema_ptr);
		std::lock_guard<std::mutex> lk(schemas_mtx);
		auto& schema = schemas[local_schema_path];
		if (!schema || schema == local_schema_ptr) {
			schema = schema_ptr;
			L_SCHEMA("{}" + GREEN + "Local Schema [{}] was added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(local_schema_path), schema_ptr->get_flags(), local_schema_ptr ? repr(local_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
			local_schema_ptr = schema;
		} else {
			local_schema_ptr = schema;
			// Read object couldn't be stored in cache,
			// so we use the schema now currently in cache
			assert(local_schema_ptr);
			if (schema_ptr == local_schema_ptr || compare_schema(*schema_ptr, *local_schema_ptr)) {
				schema_ptr = local_schema_ptr;
				L_SCHEMA("{}" + GREEN + "Local Schema [{}] already had the same object in the LRU (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			} else {
				L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] couldn't be added to LRU (version {}): " + DIM_GREY + "{} ==> {}", prefix, repr(local_schema_path), local_schema_ptr->get_flags(), repr(schema_ptr->to_string()), repr(local_schema_ptr->to_string()));
				schema_ptr = local_schema_ptr;
				failure = true;
			}
		}
	}

	// If we still need to save the metadata, we save it:
	if (writable && schema_ptr->get_flags() == 0) {
		try {
			DatabaseHandler _db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE, context);
			// Try writing (only if there's no metadata there alrady)
			if (!local_schema_ptr || (schema_ptr == local_schema_ptr || compare_schema(*schema_ptr, *local_schema_ptr))) {
				std::string schema_ser;
				try {
					schema_ser = _db_handler.get_metadata(std::string_view(RESERVED_SCHEMA));
				} catch (const Xapian::DocNotFoundError&) {
				} catch (const Xapian::DatabaseNotFoundError&) {
				}
				if (schema_ser.empty()) {
					_db_handler.set_metadata(RESERVED_SCHEMA, schema_ptr->serialise());
					schema_ptr->set_flags(1);
					L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] new metadata was written (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
				} else if (local_schema_ptr && schema_ser == local_schema_ptr->serialise()) {
					_db_handler.set_metadata(RESERVED_SCHEMA, schema_ptr->serialise());
					schema_ptr->set_flags(1);
					L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] metadata was overwritten (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
				} else {
					local_schema_ptr = schema_ptr;
					schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
					schema_ptr->lock();
					schema_ptr->set_flags(1);
					assert(schema_ptr);
					std::lock_guard<std::mutex> lk(schemas_mtx);
					auto& schema = schemas[local_schema_path];
					if (!schema || schema == local_schema_ptr) {
						schema = schema_ptr;
						L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded and added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(local_schema_path), schema_ptr->get_flags(), local_schema_ptr ? repr(local_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
					} else {
						local_schema_ptr = schema;
						assert(local_schema_ptr);
						if (schema_ptr == local_schema_ptr || compare_schema(*schema_ptr, *local_schema_ptr)) {
							schema_ptr = local_schema_ptr;
							L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded but already had the same object in the LRU (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
						} else {
							L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded but couldn't be added to LRU (version {}): " + DIM_GREY + "{} ==> {}", prefix, repr(local_schema_path), local_schema_ptr->get_flags(), repr(schema_ptr->to_string()), repr(local_schema_ptr->to_string()));
							schema_ptr = local_schema_ptr;
						}
					}
					failure = true;
				}
			} else {
				_db_handler.set_metadata(RESERVED_SCHEMA, schema_ptr->serialise());
				schema_ptr->set_flags(1);
				L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] metadata was written (version {}): " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
			}
		} catch (...) {
			L_EXC("Error saving local schema: endpoint:{}", repr(endpoints.to_string()));
			if (local_schema_ptr && (schema_ptr != local_schema_ptr && *schema_ptr != *local_schema_ptr)) {
				// On error, try reverting
				assert(local_schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[local_schema_path];
				if (!schema || schema == schema_ptr) {
					schema = local_schema_ptr;
					L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written, and was reverted: " + DIM_GREY + "{} --> {}", prefix, repr(local_schema_path), repr(schema_ptr->to_string()), local_schema_ptr ? repr(local_schema_ptr->to_string()) : "nullptr");
				} else {
					schema_ptr = schema;
					L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written, and couldn't be reverted: " + DIM_GREY + "{} ==> {}", prefix, repr(local_schema_path), local_schema_ptr ? repr(local_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
				}
			} else {
				L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written: " + DIM_GREY + "{}", prefix, repr(local_schema_path), repr(schema_ptr->to_string()));
			}
			throw;
		}
	}

	if (!new_schema || foreign_uri.empty()) {
		bool save_schema = false;
		// Now we check if the schema points to a foreign schema
		validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
		if (!foreign_uri.empty()) {
			// FOREIGN Schema, get from the cache or load from `foreign_path/foreign_id` endpoint:
			std::shared_ptr<const MsgPack> foreign_schema_ptr;
			{
				std::lock_guard<std::mutex> lk(schemas_mtx);
				foreign_schema_ptr = schemas[foreign_uri];
			}
			if (foreign_schema_ptr && (!new_schema || compare_schema(*new_schema, *foreign_schema_ptr))) {
				// Same Foreign Schema was in the cache
				schema_ptr = foreign_schema_ptr;
				assert(schema_ptr);
				L_SCHEMA("{}" + DARK_GREEN + "Foreign Schema [{}] found in cache (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
				save_schema = schema_ptr->get_flags() == 0;
			} else if (new_schema) {
				if (foreign_schema_ptr) {
					new_schema->set_flags(foreign_schema_ptr->get_flags());
					L_SCHEMA("{}" + DARK_TURQUOISE + "Foreign Schema [{}] found in cache, but it was different so try using new schema", prefix, repr(foreign_uri));
				} else {
					L_SCHEMA("{}" + DARK_TURQUOISE + "Foreign Schema [{}] not found in cache, try using new schema", prefix, repr(foreign_uri));
				}
				schema_ptr = new_schema;
				assert(schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[foreign_uri];
				if (!schema || schema == foreign_schema_ptr) {
					if (!context || context->find(foreign_path) == context->end()) {
						schema = schema_ptr;
						L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] new schema was added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
						foreign_schema_ptr = schema;
						save_schema = true;
					} else {
						L_SCHEMA("{}" + DARK_GREEN + "Foreign Schema [{}] new schema wasn't added to LRU (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
					}
				} else {
					foreign_schema_ptr = schema;
					assert(foreign_schema_ptr);
					if (schema_ptr == foreign_schema_ptr || compare_schema(*schema_ptr, *foreign_schema_ptr)) {
						foreign_schema_ptr->set_flags(schema_ptr->get_flags());
						schema_ptr = foreign_schema_ptr;
						L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] already had the same object in LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()));
					} else {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] new schema couldn't be added to LRU: " + DIM_GREY + "{} ==> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
						schema_ptr = foreign_schema_ptr;
						failure = true;
					}
				}
			} else {
				// Foreign Schema needs to be read
				L_SCHEMA("{}" + DARK_TURQUOISE + "Foreign Schema [{}] {} try loading from {} id={}", prefix, repr(foreign_uri), foreign_schema_ptr ? "found in cache, but it was different so" : "not found in cache,", repr(foreign_path), repr(foreign_id));
				try {
					auto shared = load_shared(foreign_id, Endpoint(foreign_path), read_flags, context);
					schema_ptr = std::make_shared<const MsgPack>(shared.second);
					schema_ptr->lock();
					schema_ptr->set_flags(shared.first);
					L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was loaded (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
				} catch (const ClientError&) {
					L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be loaded (client error)", prefix, repr(foreign_uri));
					throw;
				} catch (const Error&) {
					L_EXC("Error loading foreign schema");
					if (new_schema) {
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (error), create from new schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (error), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
						schema_ptr = initial_schema_ptr;
					}
				} catch (const Xapian::DocNotFoundError&) {
					if (new_schema) {
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (document was not found), create from new schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (document was not found), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
						schema_ptr = initial_schema_ptr;
					}
				} catch (const Xapian::DatabaseNotFoundError&) {
					if (new_schema) {
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (database was not there), create from new schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (database was not there), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
						schema_ptr = initial_schema_ptr;
					}
				}
				assert(schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[foreign_uri];
				if (!schema || schema == foreign_schema_ptr) {
					if (!context || context->find(foreign_path) == context->end()) {
						schema = schema_ptr;
						L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
						foreign_schema_ptr = schema;
					} else {
						L_SCHEMA("{}" + DARK_GREEN + "Foreign Schema [{}] wasn't added to LRU (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
					}
				} else {
					foreign_schema_ptr = schema;
					assert(foreign_schema_ptr);
					if (schema_ptr == foreign_schema_ptr || compare_schema(*schema_ptr, *foreign_schema_ptr)) {
						schema_ptr = foreign_schema_ptr;
						L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] couldn't be added but already was the same object in LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()));
					} else {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be added to LRU: " + DIM_GREY + "{} ==> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
						schema_ptr = foreign_schema_ptr;
						failure = true;
					}
				}
			}
			// If we still need to save the schema document, we save it:
			Xapian::rev schema_version = foreign_schema_ptr ? foreign_schema_ptr->get_flags() : UNKNOWN_REVISION;
			if (writable && save_schema) {
				try {
					auto shared = save_shared(foreign_id, *schema_ptr, schema_version, Endpoint(foreign_path), context);
					schema_ptr = std::make_shared<const MsgPack>(shared.second);
					assert(schema_ptr);
					schema_ptr->lock();
					schema_version = shared.first;
					schema_ptr->set_flags(schema_version);

					{
						std::lock_guard<std::mutex> lk(schemas_mtx);
						auto& schema = schemas[foreign_uri];
						if (!schema || schema == foreign_schema_ptr) {
							schema = schema_ptr;
							L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was saved and added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
							foreign_schema_ptr = schema;
						} else {
							foreign_schema_ptr = schema;
							assert(foreign_schema_ptr);
							if (schema_ptr == foreign_schema_ptr || compare_schema(*schema_ptr, *foreign_schema_ptr)) {
								schema_ptr = foreign_schema_ptr;
								L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was saved and couldn't be added but already was the same object in LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()));
							} else {
								L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] was saved and couldn't be added to LRU: " + DIM_GREY + "{} ==> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
								schema_ptr = foreign_schema_ptr;
								failure = true;
							}
						}
					}

#ifdef XAPIAND_CLUSTERING
					if (!opts.solo) {
						if (schema_version) {
							schema_updater()->debounce(foreign_uri, schema_version, foreign_uri);
						}
					}
#endif
					L_SCHEMA("{}" + YELLOW_GREEN + "Foreign Schema [{}] was saved to {} id={} (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
				} catch (const Xapian::DocVersionConflictError&) {
					// Foreign Schema needs to be read
					L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved to {} id={} (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
					try {
						auto shared = load_shared(foreign_id, Endpoint(foreign_path), DB_CREATE_OR_OPEN | DB_WRITABLE, context);
						schema_ptr = std::make_shared<const MsgPack>(shared.second);
						schema_ptr->lock();
						schema_version = shared.first;
						schema_ptr->set_flags(schema_version);
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} was reloaded (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->get_flags(), repr(schema_ptr->to_string()));
					} catch (const ClientError&) {
						L_SCHEMA("{}" + RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (client error)", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id));
						throw;
					} catch (const Error&) {
						L_EXC("Error loading foreign schema");
						if (new_schema) {
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (error), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), new_schema->to_string());
							schema_ptr = new_schema;
						} else {
							auto initial_schema_ptr = Schema::get_initial_schema();
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (error), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
							schema_ptr = initial_schema_ptr;
						}
					} catch (const Xapian::DocNotFoundError&) {
						if (new_schema) {
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (document was not found), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), new_schema->to_string());
							schema_ptr = new_schema;
						} else {
							auto initial_schema_ptr = Schema::get_initial_schema();
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (document was not found), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
							schema_ptr = initial_schema_ptr;
						}
					} catch (const Xapian::DatabaseNotFoundError&) {
						if (new_schema) {
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (database was not there), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), new_schema->to_string());
							schema_ptr = new_schema;
						} else {
							auto initial_schema_ptr = Schema::get_initial_schema();
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] {} id={} couldn't be reloaded (database was not there), create a new initial schema: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), repr(schema_ptr->to_string()), repr(initial_schema_ptr->to_string()));
							schema_ptr = initial_schema_ptr;
						}
					}
					assert(schema_ptr);
					std::lock_guard<std::mutex> lk(schemas_mtx);
					auto& schema = schemas[foreign_uri];
					if (!schema || schema == foreign_schema_ptr) {
						if (!context || context->find(foreign_path) == context->end()) {
							schema = schema_ptr;
							L_SCHEMA("{}" + ORANGE + "Foreign Schema [{}] for new initial schema was added to LRU (version {}): " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
							foreign_schema_ptr = schema;
						} else {
							L_SCHEMA("{}" + DARK_ORANGE + "Foreign Schema [{}] for new initial schema wasn't added to LRU (version {}): " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->get_flags(), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
						}
					} else {
						foreign_schema_ptr = schema;
						assert(foreign_schema_ptr);
						if (schema_ptr == foreign_schema_ptr || compare_schema(*schema_ptr, *foreign_schema_ptr)) {
							foreign_schema_ptr->set_flags(schema_ptr->get_flags());
							schema_ptr = foreign_schema_ptr;
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] for new initial schema already had the same object in the LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()));
						} else {
							L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] for new initial schema couldn't be added to LRU: " + DIM_GREY + "{} ==> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
							schema_ptr = foreign_schema_ptr;
						}
					}
					failure = true;
				} catch (...) {
					L_EXC("Error saving foreign schema: endpoint:{}, id:{}, version: {}", repr(foreign_path), repr(foreign_id), schema_version);
					if (foreign_schema_ptr != schema_ptr) {
						// On error, try reverting
						assert(foreign_schema_ptr);
						std::lock_guard<std::mutex> lk(schemas_mtx);
						auto& schema = schemas[foreign_uri];
						if (!schema || schema == schema_ptr) {
							schema = foreign_schema_ptr;
							L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved, and was reverted: " + DIM_GREY + "{} --> {}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr");
						} else {
							schema_ptr = schema;
							L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved, and couldn't be reverted: " + DIM_GREY + "{} ==> {}", prefix, repr(foreign_uri), foreign_schema_ptr ? repr(foreign_schema_ptr->to_string()) : "nullptr", repr(schema_ptr->to_string()));
						}
					} else {
						L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(schema_ptr->to_string()));
					}
					throw;
				}
			}
		}
	}

	return std::make_tuple(failure, std::move(schema_ptr), std::move(local_schema_path), std::move(foreign_uri));
}


std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj)
{
	/**
	 * Returns schema, mut_schema and foreign_uri
	 */
	L_CALL("SchemasLRU::get(<db_handler>, {})", obj ? repr(obj->to_string()) : "nullptr");

	assert(db_handler);
	assert(!db_handler->endpoints.empty());

	bool writable = false;
	const MsgPack* schema_obj = nullptr;
	if (obj && obj->is_map()) {
		const auto it = obj->find(RESERVED_SCHEMA);
		if (it != obj->end()) {
			writable = has_db_writable(db_handler->flags);
			schema_obj = &it.value();
		}
	}

	auto up = _update("GET: ", writable, nullptr, schema_obj, db_handler->endpoints, DB_OPEN, db_handler->context);
	auto schema_ptr = std::get<1>(up);
	auto local_schema_path = std::get<2>(up);
	auto foreign_uri = std::get<3>(up);

	// The versions LRU contains versions of schemas received as notices
	// from other nodes. In the event such version exists and is newer
	// than the currently loaded version of the schema, we try to reload
	// the new schema or otherwise lower the schema's lifespan in the LRU.
	auto& path = foreign_uri.empty() ? local_schema_path : foreign_uri;
	Xapian::rev schema_version = schema_ptr->get_flags();
	Xapian::rev latest_version = 0;
	{
		std::lock_guard<std::mutex> lk(versions_mtx);
		auto version_it = versions.find(path);
		if (version_it != versions.end()) {
			latest_version = version_it->second;
			if (latest_version <= schema_version) {
				versions.erase(version_it);
			}
		}
	}
	if (latest_version > schema_version) {
		// If the schema was flagged as outdated (newer version exists), try
		// erasing the schema, retry _update, and re-check outdated status.
		bool retry = false;
		{
			std::lock_guard<std::mutex> schemas_lk(schemas_mtx);
			auto schema_it = schemas.find(path);
			if (schema_it != schemas.end() && schema_it.expiration() > std::chrono::steady_clock::now() + 10s) {
				schemas.erase(schema_it);
				retry = true;
			}
		}
		if (retry) {
			L_SCHEMA("GET: " + DARK_CORAL + "Schema {} is outdated, try reloading {{latest_version:{}, schema_version:{}}}", repr(path), latest_version, schema_version);
			up = _update("RETRY GET: ", writable, nullptr, schema_obj, db_handler->endpoints, DB_OPEN | DB_WRITABLE, db_handler->context);
			schema_ptr = std::get<1>(up);
			local_schema_path = std::get<2>(up);
			foreign_uri = std::get<3>(up);
			auto& path_ = foreign_uri.empty() ? local_schema_path : foreign_uri;
			schema_version = schema_ptr->get_flags();
			{
				std::lock_guard<std::mutex> lk(versions_mtx);
				auto version_it = versions.find(path_);
				if (version_it != versions.end()) {
					latest_version = version_it->second;
				}
			}
			if (latest_version > schema_version) {
				L_SCHEMA("GET: " + DARK_RED + "Schema {} is still outdated, relink with a shorter lifespan (10s) {{latest_version:{}, schema_version:{}}}", repr(path_), latest_version, schema_version);
				std::lock_guard<std::mutex> schemas_lk(schemas_mtx);
				auto schema_it = schemas.find(path_);
				if (schema_it != schemas.end() && schema_it.expiration() > std::chrono::steady_clock::now() + 10s) {
					schema_it.relink(10s);
				}
			} else {
				L_SCHEMA("GET: " + GREEN + "Schema {} was outdated but it was reloaded {{latest_version:{}, schema_version:{}}}", repr(path_), latest_version, schema_version);
			}
		} else {
			L_SCHEMA("GET: " + DARK_RED + "Schema {} is still outdated {{latest_version:{}, schema_version:{}}}", repr(path), latest_version, schema_version);
		}
	} else {
		L_SCHEMA("GET: " + GREEN + "Schema {} is current {{latest_version:{}, schema_version:{}}}", repr(path), latest_version, schema_version);
	}

	if (schema_obj && schema_obj->is_map()) {
		MsgPack o = *schema_obj;
		// Initialize schema (non-foreign, non-recursive, ensure there's "schema"):
		o.erase(RESERVED_FOREIGN);
		auto it = o.find(RESERVED_TYPE);
		if (it != o.end()) {
			auto &type = it.value();
			auto sep_types = required_spc_t::get_types(type.str_view());
			sep_types[SPC_FOREIGN_TYPE] = FieldType::empty;
			type = required_spc_t::get_str_type(sep_types);
		}
		Schema schema(schema_ptr, nullptr, "");
		schema.update(o);
		std::unique_ptr<MsgPack> mut_schema;
		schema.swap(mut_schema);
		if (mut_schema) {
			return std::make_tuple(std::move(schema_ptr), std::move(mut_schema), std::move(foreign_uri));
		}
	}

	return std::make_tuple(std::move(schema_ptr), nullptr, std::move(foreign_uri));
}


bool
SchemasLRU::set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema)
{
	L_CALL("SchemasLRU::set(<db_handler>, <old_schema>, {})", new_schema ? repr(new_schema->to_string()) : "nullptr");

	assert(db_handler);
	assert(!db_handler->endpoints.empty());

	bool writable = has_db_writable(db_handler->flags);
	auto up = _update("SET: ", writable, new_schema, nullptr, db_handler->endpoints, db_handler->flags, db_handler->context);
	auto failure = std::get<0>(up);
	auto schema_ptr = std::get<1>(up);

	if (failure) {
		old_schema = schema_ptr;
		return false;
	}
	return true;
}


void
SchemasLRU::updated(const std::string& uri, Xapian::rev version)
{
	L_CALL("SchemasLRU::updated({}, {})", repr(uri), version);

	if (!version) {
		return;
	}

	std::lock_guard<std::mutex> lk(versions_mtx);
	auto emplaced = versions.emplace(uri, version);
	auto& it = emplaced.first;
	auto& latest_version = it->second;
	if (latest_version < version) {
		latest_version = version;
		if (!emplaced.second) {
			it.relink();
		}
		L_SCHEMA("Schema {} updated schema version! {{latest_version:{}}}", repr(uri), latest_version);
	}
}


void
SchemasLRU::cleanup()
{
	L_CALL("SchemasLRU::cleanup()");

	{
		std::lock_guard<std::mutex> lk(schemas_mtx);
		schemas.trim();
	}

	{
		std::lock_guard<std::mutex> lk(versions_mtx);
		versions.trim();
	}
}


std::string
SchemasLRU::__repr__() const
{
	std::lock_guard<std::mutex> versions_lk(versions_mtx);
	return strings::format(STEEL_BLUE + "<SchemasLRU {{versions:{}}}>", versions.size());
}


std::string
SchemasLRU::dump_schemas(int level) const
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret;
	ret += indent;
	ret += __repr__();
	ret.push_back('\n');

	{
		std::lock_guard<std::mutex> schemas_lk(schemas_mtx);
		std::lock_guard<std::mutex> versions_lk(versions_mtx);
		for (auto schema_it = schemas.begin(); schema_it != schemas.end(); ++schema_it) {
			auto& schema = *schema_it;
			ret += indent + indent;
			if (schema.second) {
				Xapian::rev schema_version = schema.second->get_flags();
				std::string outdated;
				auto version_it = versions.find(schema.first);
				if (version_it != versions.end() && version_it->second > schema_version) {
					if (schema_it.expiration() > std::chrono::steady_clock::now() + 10s) {
						outdated = " " + DARK_STEEL_BLUE + "(outdated)" + STEEL_BLUE;
					} else {
						outdated = " " + DARK_ORANGE + "(outdated)" + STEEL_BLUE;
					}
				}
				ret += strings::format("<Schema {} {{version:{}}}{}>", repr(schema.first), schema_version, outdated);
			} else {
				ret += strings::format("<Schema {} {{version:" + RED + "??" + STEEL_BLUE + "}}>", repr(schema.first));
			}
			ret.push_back('\n');
		}
	}

	return ret;
}
