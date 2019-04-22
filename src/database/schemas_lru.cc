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
#include "opts.h"                                 // for opts.strict
#include "reserved/schema.h"                      // for RESERVED_RECURSE, RESERVED_ENDPOINT, ...
#include "serialise.h"                            // for KEYWORD_STR
#include "server/discovery.h"                     // for schema_updater
#include "string.hh"                              // for string::format, string::replace
#include "url_parser.h"                           // for urldecode

#define L_SCHEMA L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_SCHEMA
// #define L_SCHEMA L_STACKED_GREY

using namespace std::chrono_literals;


static const std::string reserved_schema(RESERVED_SCHEMA);


template <typename ErrorType>
static inline std::pair<const MsgPack*, const MsgPack*>
validate_schema(const MsgPack& object, const char* prefix, std::string& foreign_uri, std::string& foreign_path, std::string& foreign_id)
{
	L_CALL("validate_schema({})", repr(object.to_string()));

	auto checked = Schema::check<ErrorType>(object, prefix, true, true);
	if (checked.first) {
		foreign_uri = checked.first->str();
		std::string_view foreign_path_view, foreign_id_view;
		split_path_id(foreign_uri, foreign_path_view, foreign_id_view);
		if (foreign_path_view.empty() || foreign_id_view.empty()) {
			THROW(ErrorType, "{}'{}' must contain index and docid [{}]", prefix, RESERVED_ENDPOINT, repr(foreign_uri));
		}
		foreign_path = urldecode(foreign_path_view);
		foreign_id = urldecode(foreign_id_view);
	}
	return checked;
}


static inline std::pair<Xapian::rev, MsgPack>
get_shared(const Endpoint& endpoint, std::string_view id, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("get_shared({}, {}, {})", repr(endpoint.to_string()), repr(id), context ? std::to_string(context->size()) : "nullptr");

	auto path = endpoint.path;
	if (!context) {
		context = std::make_shared<std::unordered_set<std::string>>();
	}
	if (context->size() > MAX_SCHEMA_RECURSION) {
		THROW(ClientError, "Maximum recursion reached: {}", endpoint.to_string());
	}
	if (!context->insert(path).second) {
		if (path == ".xapiand/indices") {
			// Return default .xapiand/index (chicken and egg problem)
			return std::make_pair(0, MsgPack({
				{ RESERVED_RECURSE, false },
				{ SCHEMA_FIELD_NAME, {
					{ ID_FIELD_NAME, {
						{ RESERVED_STORE, false },
						{ RESERVED_TYPE,  KEYWORD_STR },
					} },
				} },
			}));
		}
		THROW(ClientError, "Cyclic schema reference detected: {}", endpoint.to_string());
	}
	try {
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint);
		if (endpoints.empty()) {
			THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
		}
		DatabaseHandler _db_handler(endpoints, DB_OPEN, context);
		std::string_view selector;
		auto needle = id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
		if (needle != std::string_view::npos) {
			selector = id.substr(id[needle] == '.' ? needle + 1 : needle);
			id = id.substr(0, needle);
		}
		auto document = _db_handler.get_document(id);
		auto version_ser = document.get_value(DB_SLOT_VERSION);
		Xapian::rev version = version_ser.empty() ? 0 : sortable_unserialise(version_ser);
		auto o = document.get_obj();
		if (selector.empty()) {
			// If there's no selector use "schema":
			o = o[SCHEMA_FIELD_NAME];
		} else {
			o = o.select(selector);
		}
		o = MsgPack({
			{ RESERVED_RECURSE, false },
			{ SCHEMA_FIELD_NAME, o },
		});
		Schema::check<Error>(o, "Foreign schema is invalid: ", false, false);
		context->erase(path);
		return std::make_pair(version, o);
	} catch (...) {
		context->erase(path);
		throw;
	}
}


static inline Xapian::rev
save_shared(const Endpoint& endpoint, std::string_view id, MsgPack schema, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("save_shared({}, {}, <schema>, {})", repr(endpoint.to_string()), repr(id), context ? std::to_string(context->size()) : "nullptr");

	auto& path = endpoint.path;
	if (!context) {
		context = std::make_shared<std::unordered_set<std::string>>();
	}
	if (context->size() > MAX_SCHEMA_RECURSION) {
		THROW(ClientError, "Maximum recursion reached: {}", endpoint.to_string());
	}
	if (!context->insert(path).second) {
		if (path == ".xapiand/indices") {
			// Ignore .xapiand/index (chicken and egg problem)
			return 0;
		}
		THROW(ClientError, "Cyclic schema reference detected: {}", endpoint.to_string());
	}
	try {
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		if (endpoints.empty()) {
			THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
		}
		DatabaseHandler _db_handler(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN, context);
		auto needle = id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
		// FIXME: Process the subfields instead of ignoring.
		auto did = _db_handler.update(id.substr(0, needle), 0, false, schema, false, msgpack_type).first;
		context->erase(path);

		Document document(did, &_db_handler);
		auto version_ser = document.get_value(DB_SLOT_VERSION);
		Xapian::rev version = version_ser.empty() ? 0 : sortable_unserialise(version_ser);
		return version;
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


std::tuple<bool, std::shared_ptr<const MsgPack>, std::string>
SchemasLRU::_update([[maybe_unused]] const char* prefix, DatabaseHandler* db_handler, const std::shared_ptr<const MsgPack>& new_schema, const MsgPack* schema_obj, bool writable)
{
	L_CALL("SchemasLRU::_update(<db_handler>, {}, {})", new_schema ? repr(new_schema->to_string()) : "nullptr", schema_obj ? repr(schema_obj->to_string()) : "nullptr");

	assert(db_handler);
	assert(!db_handler->endpoints.empty());

	std::string foreign_uri, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool failure = false;

	// We first try to load schema from the LRU cache
	std::shared_ptr<const MsgPack> local_schema_ptr;
	const auto endpoints_path = unsharded_path(db_handler->endpoints[0].path);
	const auto local_schema_path = std::string(endpoints_path) + "/";  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	L_SCHEMA("{}" + LIGHT_GREY + "[{}]{}{}{}", prefix, repr(local_schema_path), new_schema ? " new_schema=" : schema_obj ? " schema_obj=" : "", new_schema ? new_schema->to_string() : schema_obj ? schema_obj->to_string() : "", writable ? " " + DARK_STEEL_BLUE + "(writable)" + STEEL_BLUE : "");
	{
		std::lock_guard<std::mutex> lk(schemas_mtx);
		local_schema_ptr = schemas[local_schema_path];
	}

	if (new_schema) {
		// Now we check if the schema points to a foreign schema
		validate_schema<Error>(*new_schema, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	} else if (schema_obj) {
		// Check if passed object specifies a foreign schema
		validate_schema<Error>(*schema_obj, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	}

	// Whatever was passed by the user doesn't specify a foreign schema,
	// or there it wasn't passed anything.
	if (local_schema_ptr) {
		// Schema was in the cache
		L_SCHEMA("{}" + DARK_GREEN + "Schema [{}] found in cache: " + DIM_GREY + "{}", prefix, repr(local_schema_path), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr");
		if (!foreign_uri.empty()) {
			schema_ptr = std::make_shared<MsgPack>(MsgPack({
				{ RESERVED_TYPE, "foreign/object" },
				{ RESERVED_ENDPOINT, foreign_uri },
			}));
			if (schema_ptr == local_schema_ptr || *schema_ptr == *local_schema_ptr) {
				schema_ptr = local_schema_ptr;
				L_SCHEMA("{}" + GREEN + "Local Schema [{}] already had the same foreign link in the LRU: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			} else {
				schema_ptr->lock();
				assert(schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[local_schema_path];
				if (!schema || schema == local_schema_ptr) {
					schema = schema_ptr;
					L_SCHEMA("{}" + GREEN + "Local Schema [{}] added new foreign link to the LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(local_schema_path), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
				} else {
					local_schema_ptr = schema;
					assert(local_schema_ptr);
					if (schema_ptr == local_schema_ptr || *schema_ptr == *local_schema_ptr) {
						schema_ptr = local_schema_ptr;
						L_SCHEMA("{}" + GREEN + "Local Schema [{}] couldn't add new foreign link but it already was the same foreign link in the LRU: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
					} else {
						L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] couldn't add new foreign link to the LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(local_schema_path), schema_ptr->to_string(), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr");
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
			schema_ser = db_handler->get_metadata(reserved_schema);
		} catch (const Xapian::DocNotFoundError&) {
		} catch (const Xapian::DatabaseNotFoundError&) {
		} catch (...) {
			L_EXC("Exception");
		}
		if (schema_ser.empty()) {
			if (!foreign_uri.empty()) {
				schema_ptr = std::make_shared<MsgPack>(MsgPack({
					{ RESERVED_TYPE, "foreign/object" },
					{ RESERVED_ENDPOINT, foreign_uri },
				}));
				schema_ptr->lock();
				L_SCHEMA("{}" + LIGHT_CORAL + "Schema [{}] couldn't be loaded from metadata, create a new foreign link: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			} else if (endpoints_path != ".xapiand/nodes") {
				// Implement foreign schemas in .xapiand/index by default:
				schema_ptr = std::make_shared<MsgPack>(MsgPack({
					{ RESERVED_TYPE, "foreign/object" },
					{ RESERVED_ENDPOINT, string::format(".xapiand/indices/{}", string::replace(endpoints_path, "/", "%2F")) },
				}));
				schema_ptr->lock();
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create a new default foreign link: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			} else if (new_schema) {
				schema_ptr = new_schema;
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create from new schema: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			} else {
				schema_ptr = Schema::get_initial_schema();
				L_SCHEMA("{}" + LIGHT_CORAL + "Local Schema [{}] couldn't be loaded from metadata, create a new initial schema: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			}
		} else {
			schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
			schema_ptr->lock();
			schema_ptr->set_flags(1);
			L_SCHEMA("{}" + GREEN + "Local Schema [{}] was loaded from metadata: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
		}
		assert(schema_ptr);
		std::lock_guard<std::mutex> lk(schemas_mtx);
		auto& schema = schemas[local_schema_path];
		if (!schema || schema == local_schema_ptr) {
			schema = schema_ptr;
			L_SCHEMA("{}" + GREEN + "Local Schema [{}] was added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(local_schema_path), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
		} else {
			local_schema_ptr = schema;
			// Read object couldn't be stored in cache,
			// so we use the schema now currently in cache
			assert(local_schema_ptr);
			if (schema_ptr == local_schema_ptr || *schema_ptr == *local_schema_ptr) {
				schema_ptr = local_schema_ptr;
				L_SCHEMA("{}" + GREEN + "Local Schema [{}] already had the same object in the LRU: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			} else {
				L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] couldn't be added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(local_schema_path), schema_ptr->to_string(), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr");
				schema_ptr = local_schema_ptr;
				failure = true;
			}
		}
	}

	// If we still need to save the metadata, we save it:
	if (writable && schema_ptr->get_flags() == 0) {
		try {
			// Try writing (only if there's no metadata there alrady)
			if (!local_schema_ptr || (schema_ptr == local_schema_ptr || *schema_ptr == *local_schema_ptr)) {
				std::string schema_ser;
				try {
					schema_ser = db_handler->get_metadata(reserved_schema);
				} catch (const Xapian::DocNotFoundError&) {
				} catch (const Xapian::DatabaseNotFoundError&) {
				} catch (...) {
					L_EXC("Exception");
				}
				if (schema_ser.empty()) {
					db_handler->set_metadata(reserved_schema, schema_ptr->serialise());
					schema_ptr->set_flags(1);
					L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] new metadata was written: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
				} else if (local_schema_ptr && schema_ser == local_schema_ptr->serialise()) {
					db_handler->set_metadata(reserved_schema, schema_ptr->serialise());
					schema_ptr->set_flags(1);
					L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] metadata was overwritten: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
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
						L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded and added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(local_schema_path), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
					} else {
						local_schema_ptr = schema;
						assert(local_schema_ptr);
						if (schema_ptr == local_schema_ptr || *schema_ptr == *local_schema_ptr) {
							schema_ptr = local_schema_ptr;
							L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded but already had the same object in the LRU: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
						} else {
							L_SCHEMA("{}" + DARK_RED + "Local Schema [{}] metadata wasn't overwritten, it was reloaded but couldn't be added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(local_schema_path), schema_ptr->to_string(), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr");
							schema_ptr = local_schema_ptr;
						}
					}
					failure = true;
				}
			} else {
				db_handler->set_metadata(reserved_schema, schema_ptr->serialise());
				schema_ptr->set_flags(1);
				L_SCHEMA("{}" + YELLOW_GREEN + "Local Schema [{}] metadata was written: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			}
		} catch (...) {
			if (local_schema_ptr && (schema_ptr != local_schema_ptr && *schema_ptr != *local_schema_ptr)) {
				// On error, try reverting
				assert(local_schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[local_schema_path];
				if (!schema || schema == schema_ptr) {
					schema = local_schema_ptr;
					L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written, and was reverted: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(local_schema_path), schema_ptr->to_string(), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr");
				} else {
					schema_ptr = schema;
					L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written, and couldn't be reverted: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(local_schema_path), local_schema_ptr ? local_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
				}
			} else {
				L_SCHEMA("{}" + RED + "Local Schema [{}] metadata couldn't be written: " + DIM_GREY + "{}", prefix, repr(local_schema_path), schema_ptr->to_string());
			}
			throw;
		}
	}

	if (new_schema && !foreign_uri.empty()) {
		return std::make_tuple(failure, std::move(schema_ptr), std::move(foreign_uri));
	}

	// Now we check if the schema points to a foreign schema
	validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	if (!foreign_uri.empty()) {
		// FOREIGN Schema, get from the cache or load from `foreign_path/foreign_id` endpoint:
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(schemas_mtx);
			foreign_schema_ptr = schemas[foreign_uri];
		}
		if (foreign_schema_ptr && (!new_schema || *new_schema == *foreign_schema_ptr)) {
			// Same Foreign Schema was in the cache
			schema_ptr = foreign_schema_ptr;
			L_SCHEMA("{}" + DARK_GREEN + "Foreign Schema [{}] found in cache: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
		} else if (new_schema) {
			L_SCHEMA("{}" + DARK_TURQUOISE + "Foreign Schema [{}] {} try using new schema", prefix, repr(foreign_uri), foreign_schema_ptr ? "found in cache, but it was different so" : "not found in cache,");
			schema_ptr = new_schema;
			assert(schema_ptr);
			std::lock_guard<std::mutex> lk(schemas_mtx);
			auto& schema = schemas[foreign_uri];
			if (!schema || schema == foreign_schema_ptr) {
				schema = schema_ptr;
				L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] new schema was added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
			} else {
				foreign_schema_ptr = schema;
				assert(foreign_schema_ptr);
				if (schema_ptr == foreign_schema_ptr || *schema_ptr == *foreign_schema_ptr) {
					schema_ptr = foreign_schema_ptr;
					L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] already had the same object in LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
				} else {
					L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] new schema couldn't be added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr");
					schema_ptr = foreign_schema_ptr;
					failure = true;
				}
			}
		} else {
			// Foreign Schema needs to be read
			L_SCHEMA("{}" + DARK_TURQUOISE + "Foreign Schema [{}] {} try loading from {} id={}", prefix, repr(foreign_uri), foreign_schema_ptr ? "found in cache, but it was different so" : "not found in cache,", repr(foreign_path), repr(foreign_id));
			try {
				auto shared = get_shared(Endpoint{foreign_path}, foreign_id, db_handler->context);
				schema_ptr = std::make_shared<const MsgPack>(shared.second);
				schema_ptr->lock();
				schema_ptr->set_flags(shared.first);
				L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was loaded: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
			} catch (const ClientError&) {
				L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be loaded (client error)", prefix, repr(foreign_uri));
				throw;
			} catch (const Error&) {
				if (new_schema) {
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (error), create from new schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), new_schema->to_string());
					schema_ptr = new_schema;
				} else {
					auto initial_schema_ptr = Schema::get_initial_schema();
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (error), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), initial_schema_ptr->to_string());
					schema_ptr = initial_schema_ptr;
				}
			} catch (const Xapian::DocNotFoundError&) {
				if (new_schema) {
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (document was not found), create from new schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), new_schema->to_string());
					schema_ptr = new_schema;
				} else {
					auto initial_schema_ptr = Schema::get_initial_schema();
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (document was not found), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), initial_schema_ptr->to_string());
					schema_ptr = initial_schema_ptr;
				}
			} catch (const Xapian::DatabaseNotFoundError&) {
				if (new_schema) {
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (database was not there), create from new schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), new_schema->to_string());
					schema_ptr = new_schema;
				} else {
					auto initial_schema_ptr = Schema::get_initial_schema();
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (database was not there), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), initial_schema_ptr->to_string());
					schema_ptr = initial_schema_ptr;
				}
			} catch (...) {
				L_EXC("Exception");
				if (new_schema) {
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (exception), create from new schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), new_schema->to_string());
					schema_ptr = new_schema;
				} else {
					auto initial_schema_ptr = Schema::get_initial_schema();
					L_SCHEMA("{}" + LIGHT_CORAL + "Foreign Schema [{}] couldn't be loaded (exception), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), initial_schema_ptr->to_string());
					schema_ptr = initial_schema_ptr;
				}
			}
			assert(schema_ptr);
			std::lock_guard<std::mutex> lk(schemas_mtx);
			auto& schema = schemas[foreign_uri];
			if (!schema || schema == foreign_schema_ptr) {
				schema = schema_ptr;
				L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] was added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
			} else {
				foreign_schema_ptr = schema;
				assert(foreign_schema_ptr);
				if (schema_ptr == foreign_schema_ptr || *schema_ptr == *foreign_schema_ptr) {
					schema_ptr = foreign_schema_ptr;
					L_SCHEMA("{}" + GREEN + "Foreign Schema [{}] couldn't be added but already was the same object in LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
				} else {
					L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr");
					schema_ptr = foreign_schema_ptr;
					failure = true;
				}
			}
		}
		// If we still need to save the schema document, we save it:
		if (writable && schema_ptr->get_flags() == 0) {
			try {
				auto version = save_shared(Endpoint{foreign_path}, foreign_id, *schema_ptr, db_handler->context);
				schema_updater()->debounce(foreign_uri, version, foreign_uri);
				schema_ptr->set_flags(version);
				L_SCHEMA("{}" + YELLOW_GREEN + "Foreign Schema [{}] was saved to {} id={}: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string());
			} catch (const Xapian::DocVersionConflictError&) {
				// Foreign Schema needs to be read
				try {
					auto shared = get_shared(Endpoint{foreign_path}, foreign_id, db_handler->context);
					schema_ptr = std::make_shared<const MsgPack>(shared.second);
					schema_ptr->lock();
					schema_ptr->set_flags(shared.first);
					L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={}, it was reloaded: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string());
				} catch (const ClientError&) {
					L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (client error)", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id));
					throw;
				} catch (const Error&) {
					if (new_schema) {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (error), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (error), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), initial_schema_ptr->to_string());
						schema_ptr = initial_schema_ptr;
					}
				} catch (const Xapian::DocNotFoundError&) {
					if (new_schema) {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (document was not found), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (document was not found), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), initial_schema_ptr->to_string());
						schema_ptr = initial_schema_ptr;
					}
				} catch (const Xapian::DatabaseNotFoundError&) {
					if (new_schema) {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (database was not there), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (database was not there), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), initial_schema_ptr->to_string());
						schema_ptr = initial_schema_ptr;
					}
				} catch (...) {
					L_EXC("Exception");
					if (new_schema) {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (exception), create from new schema: " + DIM_GREY + "{}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), new_schema->to_string());
						schema_ptr = new_schema;
					} else {
						auto initial_schema_ptr = Schema::get_initial_schema();
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] couldn't be saved to {} id={} and couldn't be reloaded (exception), create a new initial schema: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), repr(foreign_path), repr(foreign_id), schema_ptr->to_string(), initial_schema_ptr->to_string());
						schema_ptr = initial_schema_ptr;
					}
				}
				assert(schema_ptr);
				std::lock_guard<std::mutex> lk(schemas_mtx);
				auto& schema = schemas[foreign_uri];
				if (!schema || schema == foreign_schema_ptr) {
					schema = schema_ptr;
					L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] for new initial schema was added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
				} else {
					foreign_schema_ptr = schema;
					assert(foreign_schema_ptr);
					if (schema_ptr == foreign_schema_ptr || *schema_ptr == *foreign_schema_ptr) {
						schema_ptr = foreign_schema_ptr;
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] for new initial schema already had the same object in the LRU: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
					} else {
						L_SCHEMA("{}" + DARK_RED + "Foreign Schema [{}] for new initial schema couldn't be added to LRU: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr");
						schema_ptr = foreign_schema_ptr;
					}
				}
				failure = true;
			} catch (...) {
				if (foreign_schema_ptr != schema_ptr) {
					// On error, try reverting
					assert(foreign_schema_ptr);
					std::lock_guard<std::mutex> lk(schemas_mtx);
					auto& schema = schemas[foreign_uri];
					if (!schema || schema == schema_ptr) {
						schema = foreign_schema_ptr;
						L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved, and was reverted: " + DIM_GREY + "{} " + LIGHT_GREY + "-->" + DIM_GREY + " {}", prefix, repr(foreign_uri), schema_ptr->to_string(), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr");
					} else {
						schema_ptr = schema;
						L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved, and couldn't be reverted: " + DIM_GREY + "{} " + LIGHT_GREY + "==>" + DIM_GREY + " {}", prefix, repr(foreign_uri), foreign_schema_ptr ? foreign_schema_ptr->to_string() : "nullptr", schema_ptr->to_string());
					}
				} else {
					L_SCHEMA("{}" + RED + "Foreign Schema [{}] couldn't be saved: " + DIM_GREY + "{}", prefix, repr(foreign_uri), schema_ptr->to_string());
				}
				throw;
			}
		}
	}

	return std::make_tuple(failure, std::move(schema_ptr), std::move(foreign_uri));
}


std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj)
{
	/**
	 * Returns schema, mut_schema and foreign_uri
	 */
	L_CALL("SchemasLRU::get(<db_handler>, {})", obj ? repr(obj->to_string()) : "nullptr");

	const MsgPack* schema_obj = nullptr;
	if (obj && obj->is_map()) {
		const auto it = obj->find(reserved_schema);
		if (it != obj->end()) {
			schema_obj = &it.value();
		}
	}

	auto up = _update("GET: ", db_handler, nullptr, schema_obj, false);
	auto schema_ptr = std::get<1>(up);
	auto foreign_uri = std::get<2>(up);

	if (schema_obj && schema_obj->is_map()) {
		MsgPack o = *schema_obj;
		// Initialize schema (non-foreign, non-recursive, ensure there's "schema"):
		o.erase(RESERVED_ENDPOINT);
		auto it = o.find(RESERVED_TYPE);
		if (it != o.end()) {
			auto &type = it.value();
			auto sep_types = required_spc_t::get_types(type.str_view());
			sep_types[SPC_FOREIGN_TYPE] = FieldType::empty;
			type = required_spc_t::get_str_type(sep_types);
		}
		o[RESERVED_RECURSE] = false;
		if (opts.strict && o.find(ID_FIELD_NAME) == o.end()) {
			THROW(MissingTypeError, "Type of field '{}' for the foreign schema is missing", ID_FIELD_NAME);
		}
		if (o.find(SCHEMA_FIELD_NAME) == o.end()) {
			o[SCHEMA_FIELD_NAME] = MsgPack::MAP();
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

	auto up = _update("SET: ", db_handler, new_schema, nullptr, (db_handler->flags & DB_WRITABLE) == DB_WRITABLE);
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

	std::lock_guard<std::mutex> lk(versions_mtx);
	auto it = versions.find_and_relink(uri);
	if (it == versions.end()) {
		versions.emplace(uri, version);
	} else {
		it->second = version;
	}

	L_RED("Schema updated: {} at {}", uri, version);
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
	return string::format(STEEL_BLUE + "<SchemasLRU>");
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
		std::lock_guard<std::mutex> lk(schemas_mtx);
		for (auto& schema : schemas) {
			ret += indent + indent;
			ret += string::format("<Schema {}>", repr(schema.first));
			ret.push_back('\n');
		}
	}

	return ret;
}
