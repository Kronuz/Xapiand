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

#include "schemas_lru.h"

#include "database/handler.h"                     // for DatabaseHandler
#include "database/utils.h"                       // for unsharded_path
#include "log.h"                                  // for L_CALL
#include "manager.h"                              // for XapiandManager::resolve_index_endpoints
#include "opts.h"                                 // for opts.strict
#include "reserved/schema.h"                      // for RESERVED_RECURSE, RESERVED_ENDPOINT, ...
#include "string.hh"                              // for string::format, string::replace
#include "url_parser.h"                           // for urldecode


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #define L_SCHEMA L_YELLOW_GREEN


#ifndef L_SCHEMA
#define L_SCHEMA L_NOTHING
#endif

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


static inline MsgPack
get_shared(const Endpoint& endpoint, std::string_view id, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("get_shared({}, {}, {})", repr(endpoint.to_string()), repr(id), context ? std::to_string(context->size()) : "nullptr");

	auto path = endpoint.path;
	if (!context) {
		context = std::make_shared<std::unordered_set<std::string>>();
	}

	try {
		if (context->size() > MAX_SCHEMA_RECURSION) {
			THROW(ClientError, "Maximum recursion reached: {}", endpoint.to_string());
		}
		if (!context->insert(path).second) {
			THROW(ClientError, "Cyclic schema reference detected: {}", endpoint.to_string());
		}
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		DatabaseHandler _db_handler(endpoints, DB_OPEN, HTTP_GET, context);
		std::string_view selector;
		auto needle = id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
		if (needle != std::string_view::npos) {
			selector = id.substr(id[needle] == '.' ? needle + 1 : needle);
			id = id.substr(0, needle);
		}
		auto term_id = path == ".xapiand/index" ? prefixed(id, "Q", 'K') : _db_handler.get_prefixed_term_id(id);
		auto doc = _db_handler.get_document_term(term_id);
		auto o = doc.get_obj();
		if (!selector.empty()) {
			o = o.select(selector);
		}
		auto it = o.find(SCHEMA_FIELD_NAME);  // If there's a "schema" field inside, extract it
		if (it != o.end()) {
			o = it.value();
		}
		o = MsgPack({
			{ RESERVED_RECURSE, false },
			{ SCHEMA_FIELD_NAME, o },
		});
		Schema::check<Error>(o, "Foreign schema is invalid: ", false, false);
		context->erase(path);
		return o;
	} catch (...) {
		context->erase(path);
		throw;
	}
}


static inline void
save_shared(const Endpoint& endpoint, std::string_view id, MsgPack schema, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("save_shared({}, {}, <schema>, {})", repr(endpoint.to_string()), repr(id), context ? std::to_string(context->size()) : "nullptr");

	auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
	DatabaseHandler _db_handler(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN, HTTP_PUT, context);
	auto needle = id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
	// FIXME: Process the subfields instead of ignoring.
	_db_handler.update(id.substr(0, needle), 0, false, schema, true, false, msgpack_type);
}


SchemasLRU::SchemasLRU(ssize_t max_size) :
	local_schemas(max_size),
	foreign_schemas(max_size)
{
}


std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj)
{
	/**
	 * Returns schema, mut_schema and foreign_uri
	 */
	L_CALL("SchemasLRU::get(<db_handler>, {})", obj ? repr(obj->to_string()) : "nullptr");

	std::string foreign_uri, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool exchanged;
	const MsgPack* schema_obj = nullptr;

	// We first try to load schema from the LRU cache
	std::shared_ptr<const MsgPack> local_schema_ptr;
	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = local_schemas[local_schema_path].load();
	}

	// Check if passed object specifies a foreign schema
	if (obj && obj->is_map()) {
		const auto it = obj->find(reserved_schema);
		if (it != obj->end()) {
			schema_obj = &it.value();
			validate_schema<Error>(*schema_obj, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
		}
	}

	if (foreign_path.empty()) {
		// Whatever was passed by the user doesn't specify a foreign schema
		if (local_schema_ptr) {
			// Schema was in the cache
			L_SCHEMA("GET: Schema {} found in cache", repr(local_schema_path));
			schema_ptr = local_schema_ptr;
		} else {
			// Schema needs to be read
			L_SCHEMA("GET: Schema {} not found in cache, try loading from metadata", repr(local_schema_path));
			bool initial_schema = false;
			std::string schema_ser;
			try {
				schema_ser = db_handler->get_metadata(reserved_schema);
			} catch (const Xapian::DocNotFoundError&) {
			} catch (const Xapian::DatabaseNotFoundError&) {
			} catch (...) {
				L_EXC("Exception");
			}
			if (schema_ser.empty()) {
				initial_schema = true;
				if (local_schema_path != ".xapiand") {
					// Implement foreign schemas in .xapiand/index by default:
					schema_ptr = std::make_shared<MsgPack>(MsgPack({
						{ RESERVED_TYPE, "foreign/object" },
						{ RESERVED_ENDPOINT, string::format(".xapiand/index/{}", string::replace(local_schema_path, "/", "%2F")) },
					}));
					schema_ptr->lock();
				} else {
					schema_ptr = Schema::get_initial_schema();
				}
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
				schema_ptr->lock();
			}

			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Local Schema {} added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
			} else {
				// Read object couldn't be stored in cache,
				// so we use the schema now currently in cache
				schema_ptr = local_schema_ptr;
			}
		}
	} else {
		// The user explicitly specifies using a foreign schema,
		// so we fabricate a new foreign schema object
		L_SCHEMA("GET: Foreign Schema {}", repr(local_schema_path));
		schema_ptr = std::make_shared<MsgPack>(MsgPack({
			{ RESERVED_TYPE, "foreign/object" },
			{ RESERVED_ENDPOINT, foreign_uri },
		}));
		if (local_schema_ptr && *schema_ptr == *local_schema_ptr) {
			schema_ptr = local_schema_ptr;
		} else {
			schema_ptr->lock();
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Foreign Schema {} added to LRU", repr(local_schema_path));
			} else {
				// Fabricated schema couldn't be stored in cache,
				// we simply ignore the fact we weren't able to store it in cache
			}
		}
	}

	// Now we check if the schema points to a foreign schema
	validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	if (!foreign_path.empty()) {
		bool initial_schema = false;
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = foreign_schemas[foreign_uri].load();
		}
		if (foreign_schema_ptr) {
			// Foreign Schema was in the cache
			L_SCHEMA("GET: Foreign Schema {} found in cache", repr(foreign_uri));
			schema_ptr = foreign_schema_ptr;
		} else {
			// Foreign Schema needs to be read
			L_SCHEMA("GET: Foreign Schema {} not found in cache, try loading from {} {}", repr(foreign_uri), repr(foreign_path), repr(foreign_id));
			try {
				schema_ptr = std::make_shared<const MsgPack>(get_shared(Endpoint{foreign_path}, foreign_id, db_handler->context));
				schema_ptr->lock();
			} catch (const ClientError&) {
				throw;
			} catch (const Error&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DocNotFoundError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DatabaseNotFoundError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (...) {
				L_EXC("Exception");
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = foreign_schemas[foreign_uri].compare_exchange_strong(foreign_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Foreign Schema {} added to LRU{}", repr(foreign_path), initial_schema ? " (initial schema)" : "");
			} else {
				schema_ptr = foreign_schema_ptr;
			}
		}
	}

	if (schema_obj && schema_obj->is_map()) {
		MsgPack o = *schema_obj;
		// Initialize schema (non-foreign, non-recursive, ensure there's "schema"):
		o.erase(RESERVED_ENDPOINT);
		auto it = o.find(RESERVED_TYPE);
		if (it != o.end()) {
			auto &type = it.value();
			auto sep_types = required_spc_t::get_types(type.str_view());
			sep_types[SPC_FOREIGN_TYPE] = FieldType::EMPTY;
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

	std::string foreign_uri, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool exchanged;
	bool failure = false;

	// We first try to load schema from the LRU cache
	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = local_schemas[local_schema_path].load();
	}

	// Check if passed object specifies a foreign schema
	validate_schema<Error>(*new_schema, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);

	if (foreign_path.empty()) {
		// Whatever was passed by the user doesn't specify a foreign schema
		if (local_schema_ptr) {
			// Schema was in the cache
			L_SCHEMA("SET: Schema {} found in cache", repr(local_schema_path));
			schema_ptr = local_schema_ptr;

			if (schema_ptr->get_flags() == 0) {
				// But we still need to save the metadata
				L_SCHEMA("SET: Cached Local Schema {}, write schema metadata", repr(local_schema_path));
				try {
					if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false)) {
						schema_ptr->set_flags(1);
					} else {
						L_SCHEMA("SET: Metadata for Cached Schema {} wasn't overwriten, try reloading from metadata", repr(local_schema_path));
						std::string schema_ser;
						try {
							schema_ser = db_handler->get_metadata(reserved_schema);
						} catch (const Xapian::DocNotFoundError&) {
						} catch (const Xapian::DatabaseNotFoundError&) {
						} catch (...) {
							L_EXC("Exception");
						}
						if (schema_ser.empty()) {
							THROW(Error, "Cannot set metadata: {}", repr(reserved_schema));
						}
						local_schema_ptr = schema_ptr;
						schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
						schema_ptr->lock();
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (exchanged) {
							L_SCHEMA("SET: Cached Schema {} re-added to LRU", repr(local_schema_path));
						} else {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
						L_SCHEMA("SET: Metadata for Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::lock_guard<std::mutex> lk(smtx);
						local_schemas[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		} else {
			// Schema needs to be read
			L_SCHEMA("SET: Schema {} not found in cache, try loading from metadata", repr(local_schema_path));
			bool initial_schema = false;
			std::string schema_ser;
			try {
				schema_ser = db_handler->get_metadata(reserved_schema);
			} catch (const Xapian::DocNotFoundError&) {
			} catch (const Xapian::DatabaseNotFoundError&) {
			} catch (...) {
				L_EXC("Exception");
			}
			if (schema_ser.empty()) {
				initial_schema = true;
				schema_ptr = new_schema;
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
				schema_ptr->lock();
			}

			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("SET: Local Schema {} added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
			} else {
				// Read object couldn't be stored in cache,
				// so we use the schema now currently in cache
				schema_ptr = local_schema_ptr;
			}

			old_schema = schema_ptr;  // renew old_schema since lru didn't already have it

			if (initial_schema) {
				// New LOCAL schema:
				L_SCHEMA("SET: New Local Schema {}, write schema metadata", repr(local_schema_path));
				try {
					// Try writing (only if there's no metadata there alrady)
					if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false)) {
						schema_ptr->set_flags(1);
					} else {
						L_SCHEMA("SET: Metadata for Schema {} wasn't overwriten, try reloading from metadata", repr(local_schema_path));
						try {
							schema_ser = db_handler->get_metadata(reserved_schema);
						} catch (const Xapian::DocNotFoundError&) {
						} catch (const Xapian::DatabaseNotFoundError&) {
						} catch (...) {
							L_EXC("Exception");
						}
						if (schema_ser.empty()) {
							THROW(Error, "Cannot set metadata: {}", repr(reserved_schema));
						}
						initial_schema = false;
						local_schema_ptr = schema_ptr;
						schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(schema_ser));
						schema_ptr->lock();
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (exchanged) {
							L_SCHEMA("SET: Schema {} re-added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
						} else {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
						L_SCHEMA("SET: Metadata for Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::lock_guard<std::mutex> lk(smtx);
						local_schemas[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		}

		validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
		if (foreign_path.empty()) {
			// LOCAL new schema *and* LOCAL metadata schema.
			if (old_schema != schema_ptr) {
				old_schema = schema_ptr;
				return false;
			}

			if (schema_ptr == new_schema) {
				return true;
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = local_schemas[local_schema_path].compare_exchange_strong(schema_ptr, new_schema);
			}
			if (exchanged) {
				L_SCHEMA("SET: Schema {} added to LRU", repr(local_schema_path));
				if (*schema_ptr != *new_schema) {
					L_SCHEMA("SET: New Local Schema {}, write schema metadata", repr(local_schema_path));
					try {
						if (db_handler->set_metadata(reserved_schema, new_schema->serialise())) {
							new_schema->set_flags(1);
						}
					} catch(...) {
						L_SCHEMA("SET: Metadata for Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
						std::lock_guard<std::mutex> lk(smtx);
						local_schemas[local_schema_path].compare_exchange_strong(aux_new_schema, schema_ptr);
						throw;
					}
				}
				return true;
			}

			validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
			if (foreign_path.empty()) {
				// it faield, but metadata continues to be local
				old_schema = schema_ptr;
				return false;
			}

			failure = true;
		}
	} else {
		// FOREIGN new schema, write the foreign link to metadata:
		if (old_schema != local_schema_ptr) {
			std::shared_ptr<const MsgPack> foreign_schema_ptr;
			{
				std::lock_guard<std::mutex> lk(smtx);
				foreign_schema_ptr = foreign_schemas[foreign_uri].load();
			}
			if (old_schema != foreign_schema_ptr) {
				old_schema = foreign_schema_ptr;
				return false;
			}
		}
		{
			std::lock_guard<std::mutex> lk(smtx);
			exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, new_schema);
		}
		if (exchanged) {
			L_SCHEMA("SET: Schema {} added to LRU", repr(local_schema_path));
			if (*local_schema_ptr != *new_schema) {
				L_SCHEMA("SET: New Foreign Schema {}, write schema metadata", repr(local_schema_path));
				try {
					if (db_handler->set_metadata(reserved_schema, new_schema->serialise())) {
						new_schema->set_flags(1);
					}
				} catch(...) {
					L_SCHEMA("SET: Metadata for Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
					// On error, try reverting
					std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
					std::lock_guard<std::mutex> lk(smtx);
					local_schemas[local_schema_path].compare_exchange_strong(aux_new_schema, local_schema_ptr);
					throw;
				}
			}
			return true;
		}

		validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
		if (foreign_path.empty()) {
			// it faield, but metadata continues to be local
			old_schema = local_schema_ptr;
			return false;
		}

		failure = true;
	}

	// FOREIGN Schema, get from the cache or use `get_shared()`
	// to load from `foreign_path/foreign_id` endpoint:
	std::shared_ptr<const MsgPack> foreign_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		foreign_schema_ptr = foreign_schemas[foreign_uri].load();
	}
	if (old_schema != foreign_schema_ptr) {
		old_schema = foreign_schema_ptr;
		return false;
	}

	if (!failure) {
		if (foreign_schema_ptr == new_schema) {
			return true;
		}
		{
			std::lock_guard<std::mutex> lk(smtx);
			exchanged = foreign_schemas[foreign_uri].compare_exchange_strong(foreign_schema_ptr, new_schema);
		}
		if (exchanged) {
			L_SCHEMA("SET: Foreign Schema {} added to LRU", repr(foreign_uri));
			if (*foreign_schema_ptr != *new_schema) {
				L_SCHEMA("SET: Save Foreign Schema {}", repr(foreign_path));
				try {
					save_shared(Endpoint{foreign_path}, foreign_id, *new_schema, db_handler->context);
					new_schema->set_flags(1);
				} catch(...) {
					L_SCHEMA("SET: Document for Foreign Schema {} wasn't saved, try reverting LRU", repr(foreign_uri));
					// On error, try reverting
					std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
					std::lock_guard<std::mutex> lk(smtx);
					foreign_schemas[foreign_uri].compare_exchange_strong(aux_new_schema, foreign_schema_ptr);
					throw;
				}
			}
			return true;
		}
	}

	old_schema = foreign_schema_ptr;
	return false;
}


bool
SchemasLRU::drop(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema)
{
	L_CALL("SchemasLRU::delete(<db_handler>, <old_schema>)");

	bool exchanged;
	std::string foreign_uri, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = local_schemas[local_schema_path].load();
	}
	if (old_schema != local_schema_ptr) {
		validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
		if (foreign_path.empty()) {
			// it faield, but metadata continues to be local
			old_schema = local_schema_ptr;
			return false;
		}
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = foreign_schemas[foreign_uri].load();
		}
		if (old_schema != foreign_schema_ptr) {
			old_schema = foreign_schema_ptr;
			return false;
		}
	}

	std::shared_ptr<const MsgPack> new_schema = nullptr;
	if (local_schema_ptr == new_schema) {
		return true;
	}
	{
		std::lock_guard<std::mutex> lk(smtx);
		exchanged = local_schemas[local_schema_path].compare_exchange_strong(local_schema_ptr, new_schema);
	}
	if (exchanged) {
		try {
			db_handler->set_metadata(reserved_schema, "");
		} catch(...) {
			// On error, try reverting
			std::lock_guard<std::mutex> lk(smtx);
			local_schemas[local_schema_path].compare_exchange_strong(new_schema, local_schema_ptr);
			throw;
		}
		return true;
	}

	validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign_uri, foreign_path, foreign_id);
	if (foreign_path.empty()) {
		// it faield, but metadata continues to be local
		old_schema = local_schema_ptr;
		return false;
	}

	std::shared_ptr<const MsgPack> foreign_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		foreign_schema_ptr = foreign_schemas[foreign_uri].load();
	}

	old_schema = foreign_schema_ptr;
	return false;
}
