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


inline void
SchemasLRU::validate_metadata(DatabaseHandler* db_handler, const std::shared_ptr<const MsgPack>& local_schema_ptr, std::string& schema_path, std::string& schema_id)
{
	L_CALL(this, "SchemasLRU::validate_metadata(<db_handler>, %s, %s, %s)", repr(local_schema_ptr->to_string()).c_str(), repr(schema_path).c_str(), repr(schema_id).c_str());

	const auto& schema_obj = *local_schema_ptr;
	try {
		const auto& type = schema_obj.at(RESERVED_TYPE);
		if (!type.is_string()) {
			THROW(Error, "Metadata '%s' is corrupt in %s: '%s' must be string", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_TYPE);
		}
		const auto& sep_type = required_spc_t::get_types(type.str());
		if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
			try {
				const auto& foreign_value = schema_obj.at(RESERVED_VALUE);
				const auto aux_schema_str = foreign_value.str();
				split_path_id(aux_schema_str, schema_path, schema_id);
				if (schema_path.empty() || schema_id.empty()) {
					THROW(Error, "Metadata '%s' is corrupt in %s: '%s' must contain index and docid [%s]", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE, aux_schema_str.c_str());
				}
			} catch (const std::out_of_range&) {
				THROW(Error, "Metadata '%s' is corrupt in %s: must have '%s' and '%s'", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_TYPE, RESERVED_VALUE);
			} catch (const msgpack::type_error&) {
				THROW(Error, "Metadata '%s' is corrupt in %s: '%s' must be string because is foreign", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE);
			}
		} else {
			const auto& schema_value = schema_obj.at(SCHEMA_FIELD_NAME);
			if (!schema_value.is_map() || sep_type[SPC_OBJECT_TYPE] != FieldType::OBJECT) {
				THROW(Error, "Metadata '%s' is corrupt in %s: '%s' must be object because is not foreign", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE);
			}
		}
	} catch (const std::out_of_range&) {
		try{
			const auto& schema_value = schema_obj.at(SCHEMA_FIELD_NAME);
			if (!schema_value.is_map()) {
				THROW(Error, "Metadata '%s' is corrupt in %s: '%s' must be object because is not foreign", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE);
			}
		} catch (const std::out_of_range&) {
			THROW(Error, "Metadata '%s' is corrupt in %s: must have '%s'", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), SCHEMA_FIELD_NAME);
		}
	} catch (const msgpack::type_error&) {
		THROW(Error, "Metadata '%s' is corrupt in %s: must be object instead of %s", RESERVED_SCHEMA, db_handler->endpoints.to_string().c_str(), schema_obj.getStrType().c_str());
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

	std::string schema_path, schema_id;
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
			if (!db_handler->set_metadata(RESERVED_SCHEMA, schema_ptr->serialise(), false)) {
				THROW(ClientError, "Cannot set metadata: '%s'", RESERVED_SCHEMA);
			}
		}
	}

	validate_metadata(db_handler, schema_ptr, schema_path, schema_id);
	return std::make_tuple(created, atom_local_schema, std::move(schema_path), std::move(schema_id));
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
	} catch (const DocNotFoundError&) {
		context->erase(hash);
		THROW(DocNotFoundError, "In shared schema %s document not found: %s", repr(endpoint.to_string()).c_str(), id.c_str());
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

	const auto& schema_path = std::get<2>(info_local_schema);

	if (schema_path.empty()) {
		// LOCAL Schema, loaded in `info_local_schema[1]`:
		return std::get<1>(info_local_schema)->load();
	} else {
		// FOREIGN Schema, get from the cache or use `get_shared()`
		// to load from `schema_path/schema_id` endpoint:
		const auto& schema_id = std::get<3>(info_local_schema);
		const auto shared_schema_hash = std::hash<std::string>{}(schema_path + schema_id);
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
				schema_ptr = std::make_shared<const MsgPack>(get_shared(schema_path, schema_id, db_handler->context));
			}
			schema_ptr->lock();
			if (!atom_shared_schema->compare_exchange_strong(shared_schema_ptr, schema_ptr)) {
				schema_ptr = shared_schema_ptr;
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

	const auto& schema_path = std::get<2>(info_local_schema);

	if (schema_path.empty()) {
		// LOCAL Schema, update cache and save it to `metadata._meta`:
		if (std::get<1>(info_local_schema)->compare_exchange_strong(old_schema, new_schema)) {
			db_handler->set_metadata(RESERVED_SCHEMA, new_schema->serialise());
			return true;
		}
	} else {
		// FOREIGN Schema, update cache and save it to `schema_path/schema_id` endpoint:
		const auto& schema_id = std::get<3>(info_local_schema);
		const auto shared_schema_hash = std::hash<std::string>{}(schema_path + schema_id);
		atomic_shared_ptr<const MsgPack>* atom_shared_schema;
		{
			std::lock_guard<std::mutex> lk(smtx);
			atom_shared_schema = &(*this)[shared_schema_hash];
		}
		std::shared_ptr<const MsgPack> aux_schema;
		if (atom_shared_schema->load()) {
			aux_schema = old_schema;
		}
		if (atom_shared_schema->compare_exchange_strong(aux_schema, new_schema)) {
			MsgPack shared_schema = *new_schema;
			shared_schema[SCHEMA_FIELD_NAME][RESERVED_RECURSE] = false;
			DatabaseHandler _db_handler(Endpoints(Endpoint(schema_path)), DB_WRITABLE | DB_SPAWN | DB_NOWAL, HTTP_PUT, db_handler->context);
			// FIXME: Process the schema_path instead of sustract it.
			_db_handler.index(schema_id.substr(0, schema_id.find(DB_OFFSPRING_UNION)), true, shared_schema, false, msgpack_type);
			return true;
		} else {
			old_schema = aux_schema;
		}
	}

	return false;
}
