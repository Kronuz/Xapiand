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


#ifndef L_SCHEMA
#define L_SCHEMA L_NOTHING
#endif

static const std::string reserved_schema(RESERVED_SCHEMA);


static inline std::string_view
unsharded_path(std::string_view path)
{
	auto pos = path.find("/.__");
	return pos == std::string::npos ? path : path.substr(0, pos);
}


template <typename ErrorType>
static inline std::pair<const MsgPack*, const MsgPack*>
validate_schema(const MsgPack& object, const char* prefix, std::string& foreign, std::string& foreign_path, std::string& foreign_id)
{
	L_CALL("validate_schema({})", repr(object.to_string()));

	auto checked = Schema::check<ErrorType>(object, prefix, true, true);
	if (checked.first) {
		foreign = checked.first->str();
		std::string_view foreign_path_view, foreign_id_view;
		split_path_id(foreign, foreign_path_view, foreign_id_view);
		if (foreign_path_view.empty() || foreign_id_view.empty()) {
			THROW(ErrorType, "{}'{}' must contain index and docid [{}]", prefix, RESERVED_ENDPOINT, repr(foreign));
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
			THROW(Error, "Maximum recursion reached: {}", endpoint.to_string());
		}
		if (!context->insert(path).second) {
			THROW(Error, "Cyclic schema reference detected: {}", endpoint.to_string());
		}
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		DatabaseHandler _db_handler(endpoints, DB_OPEN, HTTP_GET, context);
		std::string_view selector;
		auto needle = id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
		if (needle != std::string_view::npos) {
			selector = id.substr(id[needle] == '.' ? needle + 1 : needle);
			id = id.substr(0, needle);
		}
		auto doc = _db_handler.get_document(id);
		auto obj = doc.get_obj();
		if (!selector.empty()) {
			obj = obj.select(selector);
		}
		Schema::check<Error>(obj, "Foreign schema is invalid: ", false, false);
		context->erase(path);
		return obj;
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
	_db_handler.update(id.substr(0, needle), 0, false, schema, true, false, msgpack_type, false);
}


std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj, bool write, bool require_foreign)
{
	L_CALL("SchemasLRU::get(<db_handler>, {})", obj ? repr(obj->to_string()) : "nullptr");

	std::string foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool exchanged;
	const MsgPack* schema_obj = nullptr;

	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = (*this)[local_schema_path].load();
	}

	if ((obj != nullptr) && obj->is_map()) {
		const auto it = obj->find(reserved_schema);
		if (it != obj->end()) {
			schema_obj = &it.value();
			validate_schema<Error>(*schema_obj, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
		}
	}

	// Implement foreign schemas in .xapiand/index by default:
	std::string foreign_holder;
	if (require_foreign && foreign_path.empty()) {
		if (local_schema_path != ".xapiand") {
			foreign_holder = string::format(".xapiand/index/{}", string::replace(local_schema_path, "/", "%2F"));
			foreign = foreign_holder;
			std::string_view foreign_path_view, foreign_id_view;
			split_path_id(foreign, foreign_path_view, foreign_id_view);
			foreign_path = urldecode(foreign_path_view);
			foreign_id = urldecode(foreign_id_view);
		}
	}

	if (foreign_path.empty()) {
		// Foreign schema not passed by the user in '_schema', load schema instead.
		if (local_schema_ptr) {
			L_SCHEMA("GET: Schema {} found in cache", repr(local_schema_path));
			schema_ptr = local_schema_ptr;
		} else {
			L_SCHEMA("GET: Schema {} not found in cache, try loading from metadata", repr(local_schema_path));
			bool initial_schema = false;
			auto str_schema = db_handler->get_metadata(reserved_schema);
			if (str_schema.empty()) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				schema_ptr->lock();
			}

			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Local Schema {} added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
			} else {
				schema_ptr = local_schema_ptr;
			}

			if (initial_schema && write) {
				initial_schema = false;
				// New LOCAL schema:
				if (require_foreign) {
					THROW(ForeignSchemaError, "Schema of {} must use a foreign schema", repr(db_handler->endpoints.to_string()));
				}
				L_SCHEMA("GET: New Local Schema {}, write schema metadata", repr(local_schema_path));
				try {
					// Try writing (only if there's no metadata there alrady)
					if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						schema_ptr->set_flags(1);
					} else {
						L_SCHEMA("GET: Metadata for Foreign Schema {} wasn't overwriten, try reloading from metadata", repr(local_schema_path));
						// or fallback to load from metadata (again).
						local_schema_ptr = schema_ptr;
						str_schema = db_handler->get_metadata(reserved_schema);
						if (str_schema.empty()) {
							initial_schema = true;
							schema_ptr = Schema::get_initial_schema();
						} else {
							schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
							schema_ptr->lock();
						}
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (exchanged) {
							L_SCHEMA("GET: Local Schema {} re-added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
						} else {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
						L_SCHEMA("GET: Metadata for Local Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::lock_guard<std::mutex> lk(smtx);
						(*this)[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		}
	} else {
		// New FOREIGN schema, write the foreign link to metadata:
		L_SCHEMA("GET: Foreign Schema {}{}", repr(local_schema_path), write ? " (writing)" : "");
		schema_ptr = std::make_shared<MsgPack>(MsgPack({
			{ RESERVED_TYPE, "foreign/object" },
			{ RESERVED_ENDPOINT, foreign },
		}));
		if (local_schema_ptr && *schema_ptr == *local_schema_ptr) {
			schema_ptr = local_schema_ptr;
		} else {
			schema_ptr->lock();
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Foreign Schema {} added to LRU", repr(local_schema_path));
				if (write) {
					L_SCHEMA("GET: New Foreign Schema {}, write schema metadata", repr(local_schema_path));
					try {
						if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
							schema_ptr->set_flags(1);
						} else {
							// It doesn't matter if new metadata cannot be set
							// it should continue with newly created foreign
							// schema, as requested by user.
						}
					} catch(...) {
						L_SCHEMA("GET: Metadata for Foreign Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::lock_guard<std::mutex> lk(smtx);
						(*this)[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
						throw;
					}
				}
			}
		}
	}

	// Try validating loaded/created schema as LOCAL or FOREIGN
	validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
	if (!foreign_path.empty()) {
		// FOREIGN Schema, get from the cache or use `get_shared()`
		// to load from `foreign_path/foreign_id` endpoint:
		bool initial_schema = false;
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = (*this)[foreign].load();
		}
		if (foreign_schema_ptr) {
			// found in cache
			L_SCHEMA("GET: Foreign Schema {} found in cache", repr(foreign));
			schema_ptr = foreign_schema_ptr;
		} else {
			L_SCHEMA("GET: Foreign Schema {} not found in cache, try loading from {} {}", repr(foreign), repr(foreign_path), repr(foreign_id));
			try {
				schema_ptr = std::make_shared<const MsgPack>(get_shared(Endpoint{foreign_path}, foreign_id, db_handler->context));
				schema_ptr->lock();
			} catch (const Error&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const ForeignSchemaError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DocNotFoundError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DatabaseNotFoundError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DatabaseOpeningError&) {
				initial_schema = true;
				schema_ptr = Schema::get_initial_schema();
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[foreign].compare_exchange_strong(foreign_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("GET: Foreign Schema {} added to LRU{}", repr(foreign_path), initial_schema ? " (initial schema)" : "");
			} else {
				schema_ptr = foreign_schema_ptr;
			}
		}
	}

	if ((schema_obj != nullptr) && schema_obj->is_map()) {
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
			return std::make_tuple(std::move(schema_ptr), std::move(mut_schema), std::move(foreign));
		}
	}

	return std::make_tuple(std::move(schema_ptr), nullptr, std::move(foreign));
}


bool
SchemasLRU::set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema, bool require_foreign)
{
	L_CALL("SchemasLRU::set(<db_handler>, <old_schema>, {})", new_schema ? repr(new_schema->to_string()) : "nullptr");

	bool exchanged;
	bool failure = false;
	std::string foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;
	bool initial_schema = false;

	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = (*this)[local_schema_path].load();
	}

	validate_schema<Error>(*new_schema, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
	if (foreign_path.empty()) {
		// LOCAL new schema.
		if (local_schema_ptr) {
			L_SCHEMA("SET: Schema {} found in cache", repr(local_schema_path));
			schema_ptr = local_schema_ptr;
			if (schema_ptr->get_flags() == 0) {
				// We still need to save the metadata
				L_SCHEMA("SET: Cached Local Schema {}, write schema metadata", repr(local_schema_path));
				try {
					if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						schema_ptr->set_flags(1);
					} else {
						L_SCHEMA("SET: Metadata for Cached Schema {} wasn't overwriten, try reloading from metadata", repr(local_schema_path));
						auto str_schema = db_handler->get_metadata(reserved_schema);
						if (str_schema.empty()) {
							THROW(Error, "Cannot set metadata: {}", repr(reserved_schema));
						}
						initial_schema = false;
						local_schema_ptr = schema_ptr;
						schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
						schema_ptr->lock();
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (exchanged) {
							L_SCHEMA("SET: Cached Schema {} re-added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
						} else {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
						L_SCHEMA("SET: Metadata for Schema {} wasn't set, try reverting LRU", repr(local_schema_path));
						// On error, try reverting
						std::lock_guard<std::mutex> lk(smtx);
						(*this)[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		} else {
			L_SCHEMA("SET: Schema {} not found in cache, try loading from metadata", repr(local_schema_path));
			auto str_schema = db_handler->get_metadata(reserved_schema);
			if (str_schema.empty()) {
				initial_schema = true;
				schema_ptr = new_schema;
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				schema_ptr->lock();
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (exchanged) {
				L_SCHEMA("SET: Schema {} added to LRU{}", repr(local_schema_path), initial_schema ? " (initial schema)" : "");
			} else {
				schema_ptr = local_schema_ptr;
			}

			old_schema = schema_ptr;  // renew old_schema since lru didn't already have it

			if (initial_schema) {
				// New LOCAL schema:
				if (require_foreign) {
					THROW(ForeignSchemaError, "Schema of {} must use a foreign schema", repr(db_handler->endpoints.to_string()));
				}
				L_SCHEMA("SET: New Local Schema {}, write schema metadata", repr(local_schema_path));
				try {
					if (db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						schema_ptr->set_flags(1);
					} else {
						L_SCHEMA("SET: Metadata for Schema {} wasn't overwriten, try reloading from metadata", repr(local_schema_path));
						str_schema = db_handler->get_metadata(reserved_schema);
						if (str_schema.empty()) {
							THROW(Error, "Cannot set metadata: {}", repr(reserved_schema));
						}
						initial_schema = false;
						local_schema_ptr = schema_ptr;
						schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
						schema_ptr->lock();
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
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
						(*this)[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		}

		validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
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
				exchanged = (*this)[local_schema_path].compare_exchange_strong(schema_ptr, new_schema);
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
						(*this)[local_schema_path].compare_exchange_strong(aux_new_schema, schema_ptr);
						throw;
					}
				}
				return true;
			}

			validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
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
				foreign_schema_ptr = (*this)[foreign].load();
			}
			if (old_schema != foreign_schema_ptr) {
				old_schema = foreign_schema_ptr;
				return false;
			}
		}
		{
			std::lock_guard<std::mutex> lk(smtx);
			exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, new_schema);
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
					(*this)[local_schema_path].compare_exchange_strong(aux_new_schema, local_schema_ptr);
					throw;
				}
			}
			return true;
		}

		validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
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
		foreign_schema_ptr = (*this)[foreign].load();
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
			exchanged = (*this)[foreign].compare_exchange_strong(foreign_schema_ptr, new_schema);
		}
		if (exchanged) {
			L_SCHEMA("SET: Foreign Schema {} added to LRU", repr(foreign));
			if (*foreign_schema_ptr != *new_schema) {
				L_SCHEMA("SET: Save Foreign Schema {}", repr(foreign_path));
				try {
					save_shared(Endpoint{foreign_path}, foreign_id, *new_schema, db_handler->context);
					new_schema->set_flags(1);
				} catch(...) {
					L_SCHEMA("SET: Document for Foreign Schema {} wasn't saved, try reverting LRU", repr(foreign));
					// On error, try reverting
					std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
					std::lock_guard<std::mutex> lk(smtx);
					(*this)[foreign].compare_exchange_strong(aux_new_schema, foreign_schema_ptr);
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
	std::string foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	const auto local_schema_path = std::string(unsharded_path(db_handler->endpoints[0].path));  // FIXME: This should remain a string_view, but LRU's std::unordered_map cannot find std::string_view directly!
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = (*this)[local_schema_path].load();
	}
	if (old_schema != local_schema_ptr) {
		validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
		if (foreign_path.empty()) {
			// it faield, but metadata continues to be local
			old_schema = local_schema_ptr;
			return false;
		}
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = (*this)[foreign].load();
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
		exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, new_schema);
	}
	if (exchanged) {
		try {
			db_handler->set_metadata(reserved_schema, "");
		} catch(...) {
			// On error, try reverting
			std::lock_guard<std::mutex> lk(smtx);
			(*this)[local_schema_path].compare_exchange_strong(new_schema, local_schema_ptr);
			throw;
		}
		return true;
	}

	validate_schema<Error>(*local_schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
	if (foreign_path.empty()) {
		// it faield, but metadata continues to be local
		old_schema = local_schema_ptr;
		return false;
	}

	std::shared_ptr<const MsgPack> foreign_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		foreign_schema_ptr = (*this)[foreign].load();
	}

	old_schema = foreign_schema_ptr;
	return false;
}
