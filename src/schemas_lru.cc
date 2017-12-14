/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "schemas_lru.h"

#include "database_handler.h"
#include "log.h"



template <typename ErrorType>
inline void
SchemasLRU::validate_schema(const MsgPack& object, const char* prefix, std::string& foreign_path, std::string& foreign_id)
{
	L_CALL(this, "SchemasLRU::validate_schema(...)");

	try {
		const auto& type = object.at(RESERVED_TYPE);
		if (!type.is_string()) {
			THROW(ErrorType, "%s'%s' must be string", prefix, RESERVED_TYPE);
		}
		const auto& sep_type = required_spc_t::get_types(type.str());
		if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
			try {
				const auto& foreign_value = object.at(RESERVED_VALUE);
				const auto aux_schema_str = foreign_value.str();
				split_path_id(aux_schema_str, foreign_path, foreign_id);
				if (foreign_path.empty() || foreign_id.empty()) {
					THROW(ErrorType, "%s'%s' must contain index and docid [%s]", prefix, RESERVED_VALUE, aux_schema_str.c_str());
				}
			} catch (const std::out_of_range&) {
				THROW(ErrorType, "%smust have '%s' and '%s'", prefix, RESERVED_TYPE, RESERVED_VALUE);
			} catch (const msgpack::type_error&) {
				THROW(ErrorType, "%s'%s' must be string because is foreign", prefix, RESERVED_VALUE);
			}
		} else {
			const auto& schema_value = object.at(SCHEMA_FIELD_NAME);
			if (!schema_value.is_map() || sep_type[SPC_OBJECT_TYPE] != FieldType::OBJECT) {
				THROW(ErrorType, "%s'%s' must be object because is not foreign", prefix, RESERVED_VALUE);
			}
		}
	} catch (const std::out_of_range&) {
		try{
			const auto& schema_value = object.at(SCHEMA_FIELD_NAME);
			if (!schema_value.is_map()) {
				THROW(ErrorType, "%s'%s' must be object because is not foreign", prefix, RESERVED_VALUE);
			}
		} catch (const std::out_of_range&) {
			THROW(ErrorType, "%smust have '%s'", prefix, SCHEMA_FIELD_NAME);
		}
	} catch (const msgpack::type_error&) {
		THROW(ErrorType, "%smust be object instead of %s", prefix, object.getStrType().c_str());
	}
}


std::tuple<bool, atomic_shared_ptr<const MsgPack>*, std::string, std::string>
SchemasLRU::get_local(DatabaseHandler* db_handler, const MsgPack* obj)
{
	L_CALL(this, "SchemasLRU::get_local(<db_handler>, <obj>)");

	bool created = false;
	const auto local_schema_hash = db_handler->endpoints.hash();

	atomic_shared_ptr<const MsgPack>* atom_local_schema;
	{
		std::lock_guard<std::mutex> lk(smtx);
		atom_local_schema = &(*this)[local_schema_hash];
	}
	auto local_schema_ptr = atom_local_schema->load();

	std::string foreign_path, foreign_id;
	std::shared_ptr<const MsgPack> schema_ptr;
	if (local_schema_ptr) {
		schema_ptr = local_schema_ptr;
	} else {
		const auto str_schema = db_handler->get_metadata(RESERVED_SCHEMA);
		if (str_schema.empty()) {
			created = true;
			schema_ptr = Schema::get_initial_schema();
		} else {
			schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
			schema_ptr->lock();
		}
	}

	if (obj && obj->is_map()) {
		const auto it = obj->find(RESERVED_SCHEMA);
		if (it != obj->end()) {
			Schema schema(schema_ptr);
			schema.update(it.value());
			auto aux_schema_ptr = schema.get_modified_schema();
			if (aux_schema_ptr) {
				schema_ptr = aux_schema_ptr;
				schema_ptr->lock();
			}
		}
	}

	if (local_schema_ptr != schema_ptr) {
		if (!atom_local_schema->compare_exchange_strong(local_schema_ptr, schema_ptr)) {
			schema_ptr = local_schema_ptr;
			created = false;
		}
		if (created) {
			if (!local_schema_ptr || *local_schema_ptr != *schema_ptr) {
				try {
					if (!db_handler->set_metadata(RESERVED_SCHEMA, schema_ptr->serialise(), false)) {
						THROW(ClientError, "Cannot set metadata: '%s'", RESERVED_SCHEMA);
					}
				} catch(...) {
					if (local_schema_ptr != schema_ptr) {
						// On error, try reverting
						atom_local_schema->compare_exchange_strong(schema_ptr, local_schema_ptr);
					}
					throw;
				}
			}
		}
	}

	validate_schema<Error>(*schema_ptr, "Schema metadata is corrupt: ", foreign_path, foreign_id);
	return std::make_tuple(created, atom_local_schema, std::move(foreign_path), std::move(foreign_id));
}


MsgPack
SchemasLRU::get_shared(const Endpoint& endpoint, const std::string& id, std::shared_ptr<std::unordered_set<size_t>> context)
{
	L_CALL(this, "SchemasLRU::get_shared(%s, %s, %s)", repr(endpoint.to_string()).c_str(), id.c_str(), context ? std::to_string(context->size()).c_str() : "nullptr");

	auto hash = endpoint.hash();
	if (!context) {
		context = std::make_shared<std::unordered_set<size_t>>();
	}

	try {
		if (context->size() > MAX_SCHEMA_RECURSION) {
			THROW(Error, "Maximum recursion reached: %s", endpoint.to_string().c_str());
		}
		if (!context->insert(hash).second) {
			THROW(Error, "Cyclic schema reference detected: %s", endpoint.to_string().c_str());
		}
		DatabaseHandler _db_handler(Endpoints(endpoint), DB_OPEN | DB_NOWAL, HTTP_GET, context);
		// FIXME: Process the subfield instead of sustract it.
		auto doc = _db_handler.get_document(id.substr(0, id.rfind(DB_OFFSPRING_UNION)));
		context->erase(hash);
		return doc.get_obj();
	} catch (...) {
		context->erase(hash);
		throw;
	}
}


std::shared_ptr<const MsgPack>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj)
{
	L_CALL(this, "SchemasLRU::get(<db_handler>, <obj>)");

	const auto info_local_schema = get_local(db_handler, obj);

	const auto& foreign_path = std::get<2>(info_local_schema);

	if (foreign_path.empty()) {
		// LOCAL Schema, loaded in `info_local_schema[1]`:
		return std::get<1>(info_local_schema)->load();
	} else {
		// FOREIGN Schema, get from the cache or use `get_shared()`
		// to load from `foreign_path/foreign_id` endpoint:
		const auto& foreign_id = std::get<3>(info_local_schema);
		const auto shared_schema_hash = std::hash<std::string>{}(foreign_path + foreign_id);
		atomic_shared_ptr<const MsgPack>* atom_shared_schema;
		{
			std::lock_guard<std::mutex> lk(smtx);
			atom_shared_schema = &(*this)[shared_schema_hash];
		}
		auto shared_schema_ptr = atom_shared_schema->load();
		std::shared_ptr<const MsgPack> schema_ptr;
		if (shared_schema_ptr) {
			schema_ptr = shared_schema_ptr;
		} else {
			if (std::get<0>(info_local_schema)) {
				schema_ptr = Schema::get_initial_schema();
			} else {
				try {
					schema_ptr = std::make_shared<const MsgPack>(get_shared(foreign_path, foreign_id, db_handler->context));
					if (schema_ptr->empty()) {
						schema_ptr = Schema::get_initial_schema();
					}
				} catch (const CheckoutError&) {
					schema_ptr = Schema::get_initial_schema();
				} catch (const DocNotFoundError&) {
					schema_ptr = Schema::get_initial_schema();
				}
			}
			schema_ptr->lock();
			if (shared_schema_ptr != schema_ptr) {
				if (!atom_shared_schema->compare_exchange_strong(shared_schema_ptr, schema_ptr)) {
					schema_ptr = shared_schema_ptr;
				}
			}
		}
		if (!schema_ptr->is_map()) {
			THROW(Error, "Schema of %s must be map [%s]", repr(db_handler->endpoints.to_string()).c_str(), repr(schema_ptr->to_string()).c_str());
		}
		return schema_ptr;
	}
}


bool
SchemasLRU::set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema)
{
	L_CALL(this, "SchemasLRU::set(<db_handler>, <old_schema>, %s)", new_schema ? repr(new_schema->to_string()).c_str() : "nullptr");

	const auto info_local_schema = get_local(db_handler);

	const auto& foreign_path = std::get<2>(info_local_schema);

	std::string new_foreign_path;
	std::string new_foreign_id;
	validate_schema<ClientError>(*new_schema, "New schema is invalid: ", new_foreign_path, new_foreign_id);

	if (foreign_path.empty() || !new_foreign_path.empty()) {
		// LOCAL Schema, update cache and save it to `metadata._meta`:
		if (old_schema != new_schema) {
			auto& atom_local_schema = std::get<1>(info_local_schema);
			if (atom_local_schema->compare_exchange_strong(old_schema, new_schema)) {
				if (!old_schema || *old_schema != *new_schema) {
					try {
						db_handler->set_metadata(RESERVED_SCHEMA, new_schema->serialise());
					} catch(...) {
						// On error, try reverting
						std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
						atom_local_schema->compare_exchange_strong(aux_new_schema, old_schema);
						throw;
					}
				}
				return true;
			}
		} else {
			return true;
		}
	} else {
		// FOREIGN Schema, update cache and save it to `foreign_path/foreign_id` endpoint:
		const auto& foreign_id = std::get<3>(info_local_schema);
		const auto shared_schema_hash = std::hash<std::string>{}(foreign_path + foreign_id);
		atomic_shared_ptr<const MsgPack>* atom_shared_schema;
		{
			std::lock_guard<std::mutex> lk(smtx);
			atom_shared_schema = &(*this)[shared_schema_hash];
		}
		std::shared_ptr<const MsgPack> aux_schema;
		if (atom_shared_schema->load()) {
			aux_schema = old_schema;
		}
		if (aux_schema != new_schema) {
			if (atom_shared_schema->compare_exchange_strong(aux_schema, new_schema)) {
				MsgPack shared_schema = *new_schema;
				shared_schema[RESERVED_STRICT] = false;
				shared_schema[SCHEMA_FIELD_NAME][RESERVED_RECURSE] = false;
				if (!aux_schema || *aux_schema != shared_schema) {
					try {
						DatabaseHandler _db_handler(Endpoints(Endpoint(foreign_path)), DB_WRITABLE | DB_SPAWN | DB_NOWAL, HTTP_PUT, db_handler->context);
						// FIXME: Process the foreign_path instead of sustract it.
						_db_handler.index(foreign_id.substr(0, foreign_id.find(DB_OFFSPRING_UNION)), true, shared_schema, false, msgpack_type);
					} catch(...) {
						// On error, try reverting
						std::shared_ptr<const MsgPack> aux_new_schema(new_schema);
						atom_shared_schema->compare_exchange_strong(aux_new_schema, aux_schema);
						throw;
					}
				}
				return true;
			} else {
				old_schema = aux_schema;
			}
		} else {
			return true;
		}
	}

	return false;
}
