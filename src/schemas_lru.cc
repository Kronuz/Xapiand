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

#include "database/handler.h"
#include "reserved/schema.h"                      // for RESERVED_RECURSE, RESERVED_ENDPOINT, ...
#include "log.h"
#include "opts.h"


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


static const std::string reserved_schema(RESERVED_SCHEMA);


template <typename ErrorType>
inline std::pair<const MsgPack*, const MsgPack*>
SchemasLRU::validate_schema(const MsgPack& object, const char* prefix, std::string_view& foreign, std::string_view& foreign_path, std::string_view& foreign_id)
{
	L_CALL("SchemasLRU::validate_schema({})", repr(object.to_string()));

	auto checked = Schema::check<ErrorType>(object, prefix, true, true, true);
	if (checked.first) {
		foreign = checked.first->str_view();
		split_path_id(foreign, foreign_path, foreign_id);
		if (foreign_path.empty() || foreign_id.empty()) {
			THROW(ErrorType, "{}'{}' must contain index and docid [{}]", prefix, RESERVED_ENDPOINT, repr(foreign));
		}
	}
	return checked;
}


MsgPack
SchemasLRU::get_shared(const Endpoint& endpoint, std::string_view id, std::shared_ptr<std::unordered_set<std::string>> context)
{
	L_CALL("SchemasLRU::get_shared({}, {}, {})", repr(endpoint.to_string()), repr(id), context ? std::to_string(context->size()) : "nullptr");

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
		DatabaseHandler _db_handler(Endpoints{endpoint}, DB_OPEN | DB_DISABLE_WAL, HTTP_GET, context);
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
		context->erase(path);
		return obj;
	} catch (...) {
		context->erase(path);
		throw;
	}
}


std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj, bool write, bool require_foreign)
{
	L_CALL("SchemasLRU::get(<db_handler>, {})", obj ? repr(obj->to_string()) : "nullptr");

	std::string_view foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	bool exchanged;
	const MsgPack* schema_obj = nullptr;

	const auto& local_schema_path = db_handler->endpoints[0].path;
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

	if (foreign_path.empty()) {
		// Foreign schema not passed by the user in '_schema', load schema instead.
		if (local_schema_ptr) {
			// Schema found in cache.
			schema_ptr = local_schema_ptr;
		} else {
			// Schema not found in cache, try loading from metadata.
			bool new_metadata = false;
			auto str_schema = db_handler->get_metadata(reserved_schema);
			if (str_schema.empty()) {
				new_metadata = true;
				schema_ptr = Schema::get_initial_schema();
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				schema_ptr->lock();
			}

			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (!exchanged) {
				schema_ptr = local_schema_ptr;
			}

			if (new_metadata && write) {
				// New LOCAL schema:
				if (require_foreign) {
					THROW(ForeignSchemaError, "Schema of {} must use a foreign schema", repr(db_handler->endpoints.to_string()));
				}
				try {
					// Try writing (only if there's no metadata there alrady)
					if (!db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						// or fallback to load from metadata (again).
						local_schema_ptr = schema_ptr;
						str_schema = db_handler->get_metadata(reserved_schema);
						if (str_schema.empty()) {
							schema_ptr = Schema::get_initial_schema();
						} else {
							schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
							schema_ptr->lock();
						}
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (!exchanged) {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
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
		schema_ptr = std::make_shared<MsgPack>(MsgPack({
			{ RESERVED_TYPE, "foreign/object" },
			{ RESERVED_ENDPOINT, foreign },
		}));
		schema_ptr->lock();
		{
			std::lock_guard<std::mutex> lk(smtx);
			exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
		}
		if (exchanged) {
			if (write) {
				try {
					if (!db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						// It doesn't matter if new metadata cannot be set
						// it should continue with newly created foreign
						// schema, as requested by user.
					}
				} catch(...) {
					// On error, try reverting
					std::lock_guard<std::mutex> lk(smtx);
					(*this)[local_schema_path].compare_exchange_strong(schema_ptr, local_schema_ptr);
					throw;
				}
			}
		}
	}

	// Try validating loaded/created schema as LOCAL or FOREIGN
	validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
	if (!foreign_path.empty()) {
		// FOREIGN Schema, get from the cache or use `get_shared()`
		// to load from `foreign_path/foreign_id` endpoint:
		const auto foreign_schema_path = std::string(foreign);
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = (*this)[foreign_schema_path].load();
		}
		if (foreign_schema_ptr) {
			// found in cache
			schema_ptr = foreign_schema_ptr;
		} else {
			try {
				schema_ptr = std::make_shared<const MsgPack>(get_shared(foreign_path, foreign_id, db_handler->context));
				if (schema_ptr->empty()) {
					schema_ptr = Schema::get_initial_schema();
				} else {
					schema_ptr->lock();
				}
				if (!schema_ptr->is_map()) {
					THROW(Error, "Schema of {} must be map [{}]", repr(db_handler->endpoints.to_string()), repr(schema_ptr->to_string()));
				}
			} catch (const ForeignSchemaError&) {
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DocNotFoundError&) {
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DatabaseNotFoundError&) {
				schema_ptr = Schema::get_initial_schema();
			} catch (const Xapian::DatabaseOpeningError&) {
				schema_ptr = Schema::get_initial_schema();
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[foreign_schema_path].compare_exchange_strong(foreign_schema_ptr, schema_ptr);
			}
			if (!exchanged) {
				schema_ptr = foreign_schema_ptr;
			}
		}
	}

	if ((schema_obj != nullptr) && schema_obj->is_map()) {
		MsgPack o = *schema_obj;
		// Initialize schema (non-foreign, non-recursive, ensure there's "version" and "schema"):
		o.erase(RESERVED_ENDPOINT);
		auto it = o.find(RESERVED_TYPE);
		if (it != o.end()) {
			auto &type = it.value();
			auto sep_types = required_spc_t::get_types(type.str_view());
			sep_types[SPC_FOREIGN_TYPE] = FieldType::EMPTY;
			type = required_spc_t::get_str_type(sep_types);
		}
		o[RESERVED_RECURSE] = false;
		if (o.find(ID_FIELD_NAME) == o.end()) {
			o[VERSION_FIELD_NAME] = DB_VERSION_SCHEMA;
		}
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
			return std::make_tuple(std::move(schema_ptr), std::move(mut_schema), std::string(foreign));
		}
	}

	return std::make_tuple(std::move(schema_ptr), nullptr, std::string(foreign));
}


bool
SchemasLRU::set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema, bool require_foreign)
{
	L_CALL("SchemasLRU::set(<db_handler>, <old_schema>, {})", new_schema ? repr(new_schema->to_string()) : "nullptr");

	bool exchanged;
	bool failure = false;
	std::string_view foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;
	bool new_metadata = false;

	const auto& local_schema_path = db_handler->endpoints[0].path;
	std::shared_ptr<const MsgPack> local_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		local_schema_ptr = (*this)[local_schema_path].load();
	}

	validate_schema<Error>(*new_schema, "Schema metadata is corrupt: ", foreign, foreign_path, foreign_id);
	if (foreign_path.empty()) {
		// LOCAL new schema.
		if (local_schema_ptr) {
			// found in cache
			schema_ptr = local_schema_ptr;
		} else {
			auto str_schema = db_handler->get_metadata(reserved_schema);
			if (str_schema.empty()) {
				new_metadata = true;
				schema_ptr = new_schema;
			} else {
				schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				schema_ptr->lock();
			}
			{
				std::lock_guard<std::mutex> lk(smtx);
				exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
			}
			if (!exchanged) {
				schema_ptr = local_schema_ptr;
			}

			old_schema = schema_ptr;  // renew old_schema since lru didn't already have it

			if (new_metadata) {
				// New LOCAL schema:
				if (require_foreign) {
					THROW(ForeignSchemaError, "Schema of {} must use a foreign schema", repr(db_handler->endpoints.to_string()));
				}
				try {
					if (!db_handler->set_metadata(reserved_schema, schema_ptr->serialise(), false, false)) {
						str_schema = db_handler->get_metadata(reserved_schema);
						if (str_schema.empty()) {
							THROW(Error, "Cannot set metadata: {}", repr(reserved_schema));
						}
						new_metadata = false;
						local_schema_ptr = schema_ptr;
						schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
						schema_ptr->lock();
						{
							std::lock_guard<std::mutex> lk(smtx);
							exchanged = (*this)[local_schema_path].compare_exchange_strong(local_schema_ptr, schema_ptr);
						}
						if (!exchanged) {
							schema_ptr = local_schema_ptr;
						}
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
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
				if (*schema_ptr != *new_schema) {
					try {
						db_handler->set_metadata(reserved_schema, new_schema->serialise());
					} catch(...) {
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
			const auto foreign_schema_path = std::string(foreign);
			std::shared_ptr<const MsgPack> foreign_schema_ptr;
			{
				std::lock_guard<std::mutex> lk(smtx);
				foreign_schema_ptr = (*this)[foreign_schema_path].load();
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
			if (*local_schema_ptr != *new_schema) {
				try {
					db_handler->set_metadata(reserved_schema, new_schema->serialise());
				} catch(...) {
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
	const auto foreign_schema_path = std::string(foreign);
	std::shared_ptr<const MsgPack> foreign_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		foreign_schema_ptr = (*this)[foreign_schema_path].load();
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
			exchanged = (*this)[foreign_schema_path].compare_exchange_strong(foreign_schema_ptr, new_schema);
		}
		if (exchanged) {
			if (*foreign_schema_ptr != *new_schema) {
				try {
					DatabaseHandler _db_handler(Endpoints{Endpoint{foreign_path}}, DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL, HTTP_PUT, db_handler->context);
					auto needle = foreign_id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
					auto new_schema_copy = *new_schema;
					// FIXME: Process the foreign_path's subfields instead of ignoring.
					_db_handler.update(foreign_id.substr(0, needle), 0, true, new_schema_copy, false, false, msgpack_type, false);
				} catch(...) {
					// On error, try reverting
					std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
					std::lock_guard<std::mutex> lk(smtx);
					(*this)[foreign_schema_path].compare_exchange_strong(aux_new_schema, foreign_schema_ptr);
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
	std::string_view foreign, foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;

	const auto& local_schema_path = db_handler->endpoints[0].path;
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
		const auto foreign_schema_path = std::string(foreign);
		std::shared_ptr<const MsgPack> foreign_schema_ptr;
		{
			std::lock_guard<std::mutex> lk(smtx);
			foreign_schema_ptr = (*this)[foreign_schema_path].load();
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

	const auto foreign_schema_path = std::string(foreign);
	std::shared_ptr<const MsgPack> foreign_schema_ptr;
	{
		std::lock_guard<std::mutex> lk(smtx);
		foreign_schema_ptr = (*this)[foreign_schema_path].load();
	}

	old_schema = foreign_schema_ptr;
	return false;
}
