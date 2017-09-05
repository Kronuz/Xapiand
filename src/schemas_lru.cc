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
	L_CALL(this, "SchemasLRU::validate_metadata(...)");

	try {
		const auto& schema_obj = local_schema_ptr->at(DB_SCHEMA);
		try {
			const auto& type = schema_obj.at(RESERVED_TYPE);
			if (!type.is_array() || type.size() != SPC_SIZE_TYPES) {
				THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s must be array of %zu integers", RESERVED_META, db_handler->endpoints.to_string().c_str(), RESERVED_TYPE, SPC_SIZE_TYPES);
			}
			const auto& value = schema_obj.at(RESERVED_VALUE);
			if (type.at(SPC_FOREIGN_TYPE).u64() == toUType(FieldType::FOREIGN)) {
				try {
					const auto aux_schema_str = value.str();
					split_path_id(aux_schema_str, schema_path, schema_id);
					if (schema_path.empty() || schema_id.empty()) {
						THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s in %s must contain index and docid [%s]", RESERVED_META, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE, DB_SCHEMA, aux_schema_str.c_str());
					}
				} catch (const msgpack::type_error&) {
					THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s in %s must be string because is foreign", RESERVED_META, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE, DB_SCHEMA);
				}
			} else if (!value.is_map()) {
				THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s in %s must be object because is not foreign", RESERVED_META, db_handler->endpoints.to_string().c_str(), RESERVED_VALUE, DB_SCHEMA);
			}
		} catch (const std::out_of_range&) {
			THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s must have %s and %s", RESERVED_META, db_handler->endpoints.to_string().c_str(), DB_SCHEMA, RESERVED_TYPE, RESERVED_VALUE);
		} catch (const msgpack::type_error&) {
			THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. %s must be map instead of %s", RESERVED_META, db_handler->endpoints.to_string().c_str(), DB_SCHEMA, schema_obj.getStrType().c_str());
		}
	} catch (const std::out_of_range&) {
		THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. It must contain %s", RESERVED_META, db_handler->endpoints.to_string().c_str(), DB_SCHEMA);
	} catch (const msgpack::type_error&) {
		THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one. It must be map instead of %s", RESERVED_META, db_handler->endpoints.to_string().c_str(), local_schema_ptr->getStrType().c_str());
	}
}


inline std::shared_ptr<const MsgPack>
SchemasLRU::validate_string_meta_schema(const MsgPack& value, const std::array<FieldType, SPC_SIZE_TYPES>& sep_types, std::string& schema_path, std::string& schema_id)
{
	L_CALL(this, "SchemasLRU::validate_string_meta_schema(%s, %s, ...)", repr(value.to_string()).c_str(), required_spc_t::get_str_type(sep_types).c_str());

	const auto aux_schema_str = value.str();
	split_path_id(aux_schema_str, schema_path, schema_id);
	if (schema_path.empty() || schema_id.empty()) {
		THROW(ClientError, "%s in %s must contain index and docid [%s]", RESERVED_VALUE, RESERVED_SCHEMA, aux_schema_str.c_str());
	}
	MsgPack new_schema({ {
		DB_SCHEMA, {
			{ RESERVED_TYPE,  sep_types },
			{ RESERVED_VALUE, value     },
		}
	} });
	new_schema.lock();
	return std::make_shared<const MsgPack>(std::move(new_schema));
}


inline std::shared_ptr<const MsgPack>
SchemasLRU::validate_object_meta_schema(const MsgPack& value, const std::array<FieldType, SPC_SIZE_TYPES>& sep_types, std::string& schema_path, std::string& schema_id)
{
	L_CALL(this, "SchemasLRU::validate_object_meta_schema(%s, %s, ...)", repr(value.to_string()).c_str(), required_spc_t::get_str_type(sep_types).c_str());

	switch (value.getType()) {
		case MsgPack::Type::STR:
			if (sep_types[SPC_FOREIGN_TYPE] != FieldType::FOREIGN) {
				THROW(ClientError, "%s must be map because is not foreign", RESERVED_SCHEMA);
			}
			return validate_string_meta_schema(value, sep_types, schema_path, schema_id);
		case MsgPack::Type::MAP: {
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
				THROW(ClientError, "%s must be string because is foreign", RESERVED_SCHEMA);
			}
			MsgPack new_schema({ {
				DB_SCHEMA, {
					{ RESERVED_TYPE,  sep_types },
					{ RESERVED_VALUE, value     },
				}
			} });
			new_schema.lock();
			return std::make_shared<const MsgPack>(std::move(new_schema));
		}
		default:
			THROW(ClientError, "%s in %s must be string or map", RESERVED_VALUE, RESERVED_SCHEMA);
	}
}


inline bool
SchemasLRU::get_strict(const MsgPack& obj, bool flag_strict)
{
	auto it = obj.find(RESERVED_STRICT);
	if (it == obj.end()) {
		return flag_strict;
	}

	try {
		return it.value().boolean();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "%s must be bool", RESERVED_STRICT);
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
	if (local_schema_ptr) {
		validate_metadata(db_handler, local_schema_ptr, schema_path, schema_id);
	} else {
		const auto str_schema = db_handler->get_metadata(RESERVED_META);
		std::shared_ptr<const MsgPack> aux_schema_ptr;
		if (str_schema.empty()) {
			if (obj && obj->is_map()) {
				created = true;
				const auto it = obj->find(RESERVED_SCHEMA);
				if (it == obj->end()) {
					aux_schema_ptr = Schema::get_initial_schema();
				} else {
					// Update strict for root.
					bool strict = get_strict(*obj, default_spc.flags.strict);
					const auto& meta_schema = it.value();
					switch (meta_schema.getType()) {
						case MsgPack::Type::STR: {
							if (strict) {
								THROW(MissingTypeError, "Type of field %s is missing", repr(RESERVED_SCHEMA).c_str());
							}
							std::array<FieldType, SPC_SIZE_TYPES> sep_types = { { FieldType::FOREIGN, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY } };
							aux_schema_ptr = validate_string_meta_schema(meta_schema, sep_types, schema_path, schema_id);
							break;
						}
						case MsgPack::Type::MAP: {
							auto it_t = meta_schema.find(RESERVED_TYPE);
							if (it_t == meta_schema.end()) {
								if (strict) {
									THROW(MissingTypeError, "Type of field %s is missing", repr(RESERVED_SCHEMA).c_str());
								}
								if (meta_schema.size() == 1) {
									auto it_v = meta_schema.find(RESERVED_VALUE);
									if (it_v == meta_schema.end()) {
										it_v = meta_schema.find(DB_SCHEMA);
										if (it_v == meta_schema.end()) {
											THROW(ClientError, "%s only must contain %s or %s", RESERVED_SCHEMA, RESERVED_VALUE, DB_SCHEMA);
										} else {
											std::array<FieldType, SPC_SIZE_TYPES> sep_types = { { FieldType::EMPTY, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY } };
											aux_schema_ptr = validate_object_meta_schema(it_v.value(), sep_types, schema_path, schema_id);
										}
									} else {
										const auto& value = it_v.value();
										std::array<FieldType, SPC_SIZE_TYPES> sep_types = { { value.is_string() ? FieldType::FOREIGN : FieldType::EMPTY, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY } };
										aux_schema_ptr = validate_object_meta_schema(value, sep_types, schema_path, schema_id);
									}
								} else {
									THROW(ClientError, "%s only must contain %s or %s", RESERVED_SCHEMA, RESERVED_VALUE, DB_SCHEMA);
								}
							} else {
								auto it_v = meta_schema.find(RESERVED_VALUE);
								if (it_v == meta_schema.end() || meta_schema.size() > 2) {
									THROW(ClientError, "%s only must contain %s and %s", RESERVED_SCHEMA, RESERVED_VALUE, RESERVED_TYPE);
								}
								const auto& type = it_t.value();
								if (type.is_string()) {
									auto sep_types = required_spc_t::get_types(type.str());
									if (sep_types[SPC_OBJECT_TYPE] != FieldType::OBJECT) {
										if (strict) {
											THROW(MissingTypeError, "Type of field %s is not completed", repr(RESERVED_SCHEMA).c_str());
										}
										sep_types[SPC_OBJECT_TYPE] = FieldType::OBJECT;
									}
									aux_schema_ptr = validate_object_meta_schema(it_v.value(), sep_types, schema_path, schema_id);
								} else {
									THROW(ClientError, "%s in %s must be string", RESERVED_TYPE, RESERVED_SCHEMA);
								}
							}
							break;
						}
						default:
							THROW(ClientError, "%s must be string or map instead of %s", RESERVED_SCHEMA, meta_schema.getStrType().c_str());
					}
				}
			} else {
				aux_schema_ptr = Schema::get_initial_schema();
			}
		} else {
			try {
				aux_schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				validate_metadata(db_handler, aux_schema_ptr, schema_path, schema_id);
			} catch (const msgpack::unpack_error& e) {
				THROW(Error, "Metadata %s is corrupt in %s, you need provide a new one [%s]", RESERVED_META, db_handler->endpoints.to_string().c_str(), e.what());
			}
		}
		aux_schema_ptr->lock();

		if (!atom_local_schema->compare_exchange_strong(local_schema_ptr, aux_schema_ptr)) {
			if (created) {
				if (!db_handler->set_metadata(RESERVED_META, local_schema_ptr->serialise(), false)) {
					THROW(ClientError, "Cannot set metadata: %s", RESERVED_META);
				}
				created = false;
			}
			validate_metadata(db_handler, local_schema_ptr, schema_path, schema_id);
		} else if (created) {
			if (!db_handler->set_metadata(RESERVED_META, aux_schema_ptr->serialise(), false)) {
				THROW(ClientError, "Cannot set metadata: %s", RESERVED_META);
			}
		}
	}

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
		if (std::get<1>(info_local_schema)->compare_exchange_strong(old_schema, new_schema)) {
			db_handler->set_metadata(RESERVED_META, new_schema->serialise());
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
			DatabaseHandler _db_handler(Endpoints(Endpoint(schema_path)), DB_WRITABLE | DB_SPAWN | DB_NOWAL, HTTP_PUT, db_handler->context);
			MsgPack shared_schema = *new_schema;
			shared_schema[RESERVED_RECURSE] = false;
			// FIXME: Process the schema_path instead of sustract it.
			_db_handler.index(schema_id.substr(0, schema_id.rfind(DB_OFFSPRING_UNION)), true, shared_schema, false, msgpack_type);
			return true;
		} else {
			old_schema = aux_schema;
		}
	}

	return false;
}
