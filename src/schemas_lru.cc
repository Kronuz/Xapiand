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
#include "schema.h"


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

	std::string schema_path_str, schema_id;
	if (local_schema_ptr) {
		switch (local_schema_ptr->getType()) {
			case MsgPack::Type::STR: {
				const auto aux_schema_str = local_schema_ptr->as_string();
				split_path_id(aux_schema_str, schema_path_str, schema_id);
				if (schema_path_str.empty() || schema_id.empty()) {
					THROW(Error, "Metadata %s is corrupt, you need provide a new one. It must contain index and docid [%s]", DB_META_SCHEMA, aux_schema_str.c_str());
				}
				break;
			}
			case MsgPack::Type::MAP:
				break;
			default:
				THROW(Error, "Metadata %s is corrupt, you need provide a new one. It must be string or map [%s]", DB_META_SCHEMA, MsgPackTypes[toUType(local_schema_ptr->getType())]);
		}
	} else {
		DatabaseHandler local_db_handler(db_handler->endpoints, db_handler->flags != -1 ? db_handler->flags : DB_WRITABLE);

		const auto str_schema = local_db_handler.get_metadata(DB_META_SCHEMA);

		std::shared_ptr<const MsgPack> aux_schema_ptr;
		if (str_schema.empty()) {
			created = true;
			if (obj) {
				const auto it = obj->find(RESERVED_SCHEMA);
				if (it != obj->end()) {
					const auto& path = it.value();
					try {
						aux_schema_ptr = std::make_shared<const MsgPack>(path.as_string());
					} catch (const msgpack::type_error&) {
						THROW(ClientError, "%s must be string", RESERVED_SCHEMA);
					}
				} else {
					aux_schema_ptr = Schema::get_initial_schema();
				}
			} else {
				aux_schema_ptr = Schema::get_initial_schema();
			}
		} else {
			try {
				aux_schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
			} catch (const msgpack::unpack_error& e) {
				THROW(Error, "Metadata %s is corrupt, you need provide a new one [%s]", DB_META_SCHEMA, e.what());
			}
		}
		aux_schema_ptr->lock();

		switch (aux_schema_ptr->getType()) {
			case MsgPack::Type::STR: {
				const auto aux_schema_str = aux_schema_ptr->as_string();
				split_path_id(aux_schema_str, schema_path_str, schema_id);
				if (schema_path_str.empty() || schema_id.empty()) {
					if (created) {
						THROW(ClientError, "%s must contain index and docid [%s]", RESERVED_SCHEMA, aux_schema_str.c_str());
					} else {
						THROW(Error, "Metadata %s is corrupt, you need provide a new one. It must contain index and docid [%s]", DB_META_SCHEMA, aux_schema_str.c_str());
					}
				}
				break;
			}
			case MsgPack::Type::MAP:
				break;
			default:
				THROW(Error, "Metadata %s is corrupt, you need provide a new one. It must be string or map [%s]", DB_META_SCHEMA, MsgPackTypes[toUType(aux_schema_ptr->getType())]);
		}

		atom_local_schema->compare_exchange_strong(local_schema_ptr, aux_schema_ptr);
	}

	return std::make_tuple(created, atom_local_schema, std::move(schema_path_str), std::move(schema_id));
}


MsgPack
SchemasLRU::get_shared(const Endpoint& endpoint, const std::string& id, int flags)
{
	L_CALL(this, "SchemasLRU::get_shared(%s, %s, %d)", repr(endpoint.to_string()).c_str(), id.c_str(), flags);

	try {
		DatabaseHandler db_handler;
		db_handler.reset(Endpoints(endpoint), flags != -1 ? flags : DB_OPEN, HTTP_POST);
		auto doc = db_handler.get_document(id);
		return doc.get_obj();
	} catch (const DocNotFoundError&) {
		THROW(DocNotFoundError, "In shared schema %s document not found: %s", repr(endpoint.to_string()).c_str(), id.c_str());
	}
}


std::shared_ptr<const MsgPack>
SchemasLRU::get(DatabaseHandler* db_handler, const MsgPack* obj)
{
	L_CALL(this, "SchemasLRU::get(<db_handler>, <obj>)");

	const auto info_local_schema = get_local(db_handler, obj);

	const auto& schema_path = std::get<2>(info_local_schema);
	if (schema_path.empty()) {
		return std::get<1>(info_local_schema)->load();
	} else {
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
				schema_ptr = std::make_shared<const MsgPack>(get_shared(schema_path, schema_id));
			}
			schema_ptr->lock();
			if (!atom_shared_schema->compare_exchange_strong(shared_schema_ptr, schema_ptr)) {
				schema_ptr = shared_schema_ptr;
			}
		}
		if (!schema_ptr->is_map()) {
			THROW(Error, "Schema must be a map [%s]", repr(db_handler->endpoints.to_string()).c_str());
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
		if (std::get<1>(info_local_schema)->compare_exchange_strong(old_schema, new_schema)) {
			db_handler->set_metadata(DB_META_SCHEMA, new_schema->serialise());
			return true;
		}
	} else {
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
			Endpoints endpoints = Endpoint(schema_path);
			DatabaseHandler db_handler(endpoints, DB_WRITABLE | DB_SPAWN | DB_NOWAL);
			MsgPack shared_schema = *new_schema;
			shared_schema[RESERVED_RECURSE] = false;
			db_handler.index(schema_id, true, shared_schema, false, MSGPACK_CONTENT_TYPE);
			db_handler.reset(endpoints, DB_WRITABLE);
			db_handler.set_metadata(DB_META_SCHEMA, (std::get<1>(info_local_schema)->load()->serialise()));
		} else {
			old_schema = aux_schema;
		}
	}

	return false;
}
