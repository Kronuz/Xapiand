/*
 * Copyright (C) 2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "database_handler.h"

#include <algorithm>                        // for min, move
#include <ctype.h>                          // for isupper, tolower
#include <exception>                        // for exception
#include <stdexcept>                        // for out_of_range

#include "cast.h"                           // for Cast
#include "chaipp/chaipp.h"                  // for chaipp namespace
#include "database.h"                       // for DatabasePool, Database
#include "exception.h"                      // for CheckoutError, ClientError
#include "length.h"                         // for unserialise_length, seria...
#include "log.h"                            // for L_CALL
#include "msgpack.h"                        // for MsgPack, object::object, ...
#include "msgpack_patcher.h"                // for apply_patch
#include "multivalue/aggregation.h"         // for AggregationMatchSpy
#include "multivalue/keymaker.h"            // for Multi_MultiValueKeyMaker
#include "query_dsl.h"                      // for QUERYDSL_QUERY, QueryDSL
#include "rapidjson/document.h"             // for Document
#include "schema.h"                         // for Schema, required_spc_t
#include "schemas_lru.h"                    // for SchemasLRU
#include "serialise.h"                      // for cast, serialise, type
#include "utils.h"                          // for repr
#include "v8pp/v8pp.h"                      // for v8pp namespace


class FilterPrefixesExpandDecider : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

public:
	FilterPrefixesExpandDecider(const std::vector<std::string>& prefixes_)
		: prefixes(prefixes_) { }

	virtual bool operator() (const std::string& term) const override {
		for (const auto& prefix : prefixes) {
			if (startswith(term, prefix)) {
				return true;
			}
		}

		return prefixes.empty();
	}
};


template<typename F, typename... Args>
lock_database::lock_database(DatabaseHandler* db_handler_, F&& f, Args&&... args)
	: db_handler(db_handler_)
{
	lock(std::forward<F>(f), std::forward<Args>(args)...);
}


lock_database::lock_database(DatabaseHandler* db_handler_)
	: db_handler(db_handler_)
{
	lock();
}


lock_database::~lock_database()
{
	unlock();
}


template<typename F, typename... Args>
void
lock_database::lock(F&& f, Args&&... args)
{
	L_CALL(this, "lock_database::lock(...)");

	if (db_handler) {
		if (db_handler->database) {
			THROW(Error, "lock_database is already locked");
		} else {
			XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags, std::forward<F>(f), std::forward<Args>(args)...);
		}
	}
}


void
lock_database::lock()
{
	L_CALL(this, "lock_database::lock()");

	if (db_handler) {
		if (db_handler->database) {
			THROW(Error, "lock_database is already locked");
		} else {
			XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags);
		}
	}
}


void
lock_database::unlock()
{
	L_CALL(this, "lock_database::unlock(...)");

	if (db_handler && db_handler->database) {
		XapiandManager::manager->database_pool.checkin(db_handler->database);
	}
}


DatabaseHandler::DatabaseHandler()
	: flags(0),
	  method(HTTP_GET) { }


DatabaseHandler::DatabaseHandler(const Endpoints& endpoints_, int flags_, enum http_method method_, const std::shared_ptr<std::unordered_set<size_t>>& context_)
	: endpoints(endpoints_),
	  flags(flags_),
	  method(method_),
	  context(context_) { }


std::shared_ptr<Database>
DatabaseHandler::get_database() const noexcept
{
	return database;
}


std::shared_ptr<Schema>
DatabaseHandler::get_schema(const MsgPack* obj)
{
	L_CALL(this, "DatabaseHandler::get_schema(<obj>)");

	return std::make_shared<Schema>(XapiandManager::manager->schemas.get(this, obj));
}


void
DatabaseHandler::recover_index()
{
	L_CALL(this, "DatabaseHandler::recover_index()");

	XapiandManager::manager->database_pool.recover_database(endpoints, RECOVER_REMOVE_WRITABLE);
	reset(endpoints, flags, HTTP_PUT, context);
}


void
DatabaseHandler::reset(const Endpoints& endpoints_, int flags_, enum http_method method_, const std::shared_ptr<std::unordered_set<size_t>>& context_)
{
	L_CALL(this, "DatabaseHandler::reset(%s, %x, <method>)", repr(endpoints_.to_string()).c_str(), flags_);

	if (endpoints_.size() == 0) {
		THROW(ClientError, "It is expected at least one endpoint");
	}

	method = method_;

	if (endpoints != endpoints_ || flags != flags_) {
		endpoints = endpoints_;
		flags = flags_;
	}

	context = context_;
}


#if XAPIAND_DATABASE_WAL
MsgPack
DatabaseHandler::repr_wal(uint32_t start_revision, uint32_t end_revision)
{
	L_CALL(this, "DatabaseHandler::repr_wal(%u, %u)", start_revision, end_revision);

	if (endpoints.size() != 1) {
		THROW(ClientError, "It is expected one single endpoint");
	}

	// WAL required on a local writable database, open it.
	lock_database lk_db(this);
	auto wal = std::make_unique<DatabaseWAL>(endpoints[0].path, database.get());
	return wal->repr(start_revision, end_revision);
}
#endif


MsgPack
DatabaseHandler::get_document_obj(const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::get_document_obj(%s)", repr(term_id).c_str());

	std::string data;
	{
		lock_database lk_db(this);
		Xapian::docid did = database->find_document(term_id);
		data = database->get_document(did, database->flags & DB_WRITABLE).get_data();
	}
	return MsgPack::unserialise(::split_data_obj(data));
}


#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)

template<typename Processor>
MsgPack DatabaseHandler::call_script(MsgPack& data, const std::string& term_id, const std::string& script_name, const std::string& script_body)
{
	try {
		auto processor = Processor::compile(script_name, script_body);
		switch (method) {
			case HTTP_PUT:
				try {
					auto old_data = get_document_obj(term_id);
					return (*processor)["on_put"](data, old_data);
				} catch (const DocNotFoundError&) {
					return data;
				}

			case HTTP_PATCH:
			case HTTP_MERGE:
				try {
					auto old_data = get_document_obj(term_id);
					return (*processor)["on_patch"](data, old_data);
				} catch (const DocNotFoundError&) {
					return data;
				}

			case HTTP_DELETE:
				try {
					auto old_data = get_document_obj(term_id);
					return (*processor)["on_delete"](data, old_data);
				} catch (const DocNotFoundError&) {
					return data;
				}

			case HTTP_GET:
				return (*processor)["on_get"](data);

			case HTTP_POST:
				return (*processor)["on_post"](data);

			default:
				return data;
		}
#if defined(XAPIAND_V8)
	} catch (const v8pp::ReferenceError&) {
		return data;
	} catch (const v8pp::Error& e) {
		THROW(ClientError, e.what());
#endif
#if defined(XAPIAND_CHAISCRIPT)
	} catch (const chaipp::ReferenceError&) {
		return data;
	} catch (const chaipp::Error& e) {
		THROW(ClientError, e.what());
#endif
	} catch (const std::out_of_range&) {
		THROW(MissingTypeError, "Script for %s is missing", repr(script_name).c_str());
	} catch (...) {
		throw;
	}
}


MsgPack
DatabaseHandler::run_script(MsgPack& data, const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::run_script(...)");

	std::string script_type;
	std::string script_name;
	std::string script_body;
	auto it = data.find(RESERVED_SCRIPT);
	if (it == data.end()) {
		return data;
	} else {
		auto& _script = it.value();
		switch (_script.getType()) {
			case MsgPack::Type::STR:
				script_name = _script.as_string();
				break;
			case MsgPack::Type::MAP:
				try {
					script_type = _script.at(RESERVED_TYPE).as_string();
				} catch (const std::out_of_range&) { }
				try {
					script_name = _script.at(RESERVED_VALUE).as_string();
				} catch (const std::out_of_range&) { }
				try {
					script_body = _script.at(RESERVED_BODY).as_string();
				} catch (const std::out_of_range&) { }
				if (script_name.empty() && script_body.empty()) {
					THROW(ClientError, "%s must be a string or a valid script object", RESERVED_SCRIPT);
				}
				break;
			default:
				THROW(ClientError, "%s must be a string or a valid script object", RESERVED_SCRIPT);
		}
	}

	// ECMAScript
	if (script_type == "ecma" || endswith(script_name, ".js") || endswith(script_name, ".es")) {
#if defined(XAPIAND_V8)
		return call_script<v8pp::Processor>(data, term_id, script_name, script_body);
#else
		THROW(ClientError, "Script type 'ecma' (ECMAScript or JavaScript) not available.");
#endif
	}

	// ChaiScript
	if (script_type == "chai" || endswith(script_name, ".chai")) {
#if defined(XAPIAND_CHAISCRIPT)
		return call_script<chaipp::Processor>(data, term_id, script_name, script_body);
#else
		THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
	}

	if (!script_type.empty()) {
		THROW(ClientError, "Script type %s not available.", script_type.c_str());
	}

	// Fallback:
#if defined(XAPIAND_V8)
	return call_script<v8pp::Processor>(data, term_id, script_name, script_body);
#elif defined(XAPIAND_CHAISCRIPT)
	return call_script<chaipp::Processor>(data, term_id, script_name, script_body);
#endif

}
#endif


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const std::string& store, MsgPack& obj, const std::string& blob, bool commit_, const ct_type_t& ct_type)
{
	L_CALL(this, "DatabaseHandler::index(%s, %s, <store>, %s, <blob>, %s, <ct_type>)", repr(_document_id).c_str(), stored ? "true" : "false", repr(obj.to_string()).c_str(), commit_ ? "true" : "false");

	Xapian::Document doc;

	std::string term_id;
	std::string prefixed_term_id;
	required_spc_t spc_id;

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	try {
		short doc_revision;
		do {
#endif
			auto schema_begins = std::chrono::system_clock::now();
			do {
				schema = get_schema(&obj);
				L_INDEX(this, "Schema: %s", repr(schema->to_string()).c_str());

				spc_id = schema->get_data_id();
				if (spc_id.get_type() == FieldType::EMPTY) {
					try {
						const auto& id_field = obj.at(ID_FIELD_NAME);
						if (id_field.is_map()) {
							try {
								spc_id.set_types(id_field.at(RESERVED_TYPE).as_string());
							} catch (const msgpack::type_error&) {
								THROW(ClientError, "Data inconsistency, %s must be string", RESERVED_TYPE);
							}
						}
					} catch (const std::out_of_range&) { }
				} else {
					term_id = Serialise::serialise(spc_id, _document_id);
					prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
					{
						lock_database lk_db(this);
						doc_revision = database->get_document_change_seq(prefixed_term_id);
					}
					if (obj.is_map()) {
						obj = run_script(obj, prefixed_term_id);
						if (!obj.is_map()) {
							THROW(ClientError, "Script must return an object, it returned %s", obj.getStrType().c_str());
						}
					}
#endif
				}

				// Add ID.
				auto& id_field = obj[ID_FIELD_NAME];
				auto id_value = Cast::cast(spc_id.get_type(), _document_id);
				if (id_field.is_map()) {
					id_field[RESERVED_VALUE] = id_value;
				} else {
					id_field = id_value;
				}

				if (blob.empty()) {
					obj.erase(CT_FIELD_NAME);
				} else {
					// Add Content Type if indexing a blob.
					auto& ct_field = obj[CT_FIELD_NAME];
					if (!ct_field.is_map() && !ct_field.is_undefined()) {
						ct_field = MsgPack();
					}
					ct_field[RESERVED_TYPE] = TERM_STR;
					ct_field[RESERVED_VALUE] = ct_type.to_string();
					ct_field[ct_type.first][ct_type.second] = nullptr;
				}

				// Index object.
				obj = schema->index(obj, doc);

				if (prefixed_term_id.empty()) {
					// Now the schema is full, get specification id.
					spc_id = schema->get_data_id();
					if (spc_id.get_type() == FieldType::EMPTY) {
						// Index like a namespace.
						const auto type_ser = Serialise::guess_serialise(_document_id);
						spc_id.set_type(type_ser.first);
						Schema::set_namespace_spc_id(spc_id);
						term_id = type_ser.second;
						prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
					} else {
						term_id = Serialise::serialise(spc_id, _document_id);
						prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
					}
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
					{
						lock_database lk_db(this);
						doc_revision = database->get_document_change_seq(prefixed_term_id);
					}
#endif
				}
				auto update = update_schema();
				if (update.first) {
					auto schema_ends = std::chrono::system_clock::now();
					if (update.second) {
						Stats::cnt().add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
					} else {
						Stats::cnt().add("schema_reads", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
					}
					break;
				}
			} while (true);

			if (blob.empty()) {
				L_INDEX(this, "Data: %s", repr(obj.to_string()).c_str());
				doc.set_data(join_data(false, "", obj.serialise(), ""));
			} else {
				L_INDEX(this, "Data: %s", repr(obj.to_string()).c_str());
				auto ct_type_str = ct_type.to_string();
				doc.set_data(join_data(stored, store, obj.serialise(), serialise_strings({ prefixed_term_id, ct_type_str, blob })));
			}

			doc.add_boolean_term(prefixed_term_id);
			doc.add_value(spc_id.slot, term_id);

			lock_database lk_db(this);
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
			if (database->set_document_change_seq(prefixed_term_id, doc_revision)) {
#endif
				try {
					auto did = database->replace_document_term(prefixed_term_id, doc, commit_);
					return std::make_pair(std::move(did), std::move(obj));
				} catch (const Xapian::DatabaseError&) {
					// Try to recover from DatabaseError (i.e when the index is manually deleted)
					lk_db.unlock();
					recover_index();
					lk_db.lock();
					auto did = database->replace_document_term(prefixed_term_id, doc, commit_);
					return std::make_pair(std::move(did), std::move(obj));
				}
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
			}
		} while (true);
	} catch (...) {
		if (!prefixed_term_id.empty()) {
			lock_database lk_db(this);
			database->dec_document_count(prefixed_term_id);
		}
		throw;
	}
#endif
}


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type)
{
	L_CALL(this, "DatabaseHandler::index(%s, %s, <body>, %s, %s/%s)", repr(_document_id).c_str(), stored ? "true" : "false", commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "Database is read-only");
	}

	if (_document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	MsgPack obj;
	std::string blob;
	switch (body.getType()) {
		case MsgPack::Type::STR:
			blob = body.as_string();
			break;
		case MsgPack::Type::MAP:
			obj = body;
			break;
		default:
			THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}

	return index(_document_id, stored, "", obj, blob, commit_, ct_type);
}


DataType
DatabaseHandler::patch(const std::string& _document_id, const MsgPack& patches, bool commit_, const ct_type_t& ct_type)
{
	L_CALL(this, "DatabaseHandler::patch(%s, <patches>, %s, %s/%s)", repr(_document_id).c_str(), commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (_document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	if (!patches.is_map() && !patches.is_array()) {
		THROW(ClientError, "Patches must be a JSON or MsgPack");
	}

	auto document = get_document(_document_id);

	const auto data = document.get_data();

	auto obj = MsgPack::unserialise(::split_data_obj(data));
	apply_patch(patches, obj);

	const auto store = ::split_data_store(data);
	const auto blob = store.first ? "" : document.get_blob();

	return index(_document_id, store.first, store.second, obj, blob, commit_, ct_type);
}


DataType
DatabaseHandler::merge(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type)
{
	L_CALL(this, "DatabaseHandler::merge(%s, %s, <body>, %s, %s/%s)", repr(_document_id).c_str(), stored ? "true" : "false", commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (_document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	if (!body.is_map()) {
		THROW(ClientError, "Must be a JSON or MsgPack");
	}

	auto document = get_document(_document_id);

	const auto data = document.get_data();

	auto obj = MsgPack::unserialise(::split_data_obj(data));
	switch (obj.getType()) {
		case MsgPack::Type::STR: {
			const auto blob = body.as_string();
			return index(_document_id, stored, "", obj, blob, commit_, ct_type);
		}
		case MsgPack::Type::MAP: {
			obj.update(body);
			const auto store = ::split_data_store(data);
			const auto blob = store.first ? "" : document.get_blob();
			return index(_document_id, store.first, store.second, obj, blob, commit_, ct_type);
		}
		default:
			THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}
}


void
DatabaseHandler::write_schema(const MsgPack& obj)
{
	L_CALL(this, "DatabaseHandler::write_schema(<obj>)");

	auto schema_begins = std::chrono::system_clock::now();
	do {
		schema = get_schema();
		schema->write_schema(obj, method == HTTP_PUT);
		L_INDEX(this, "Schema to write: %s", repr(schema->to_string()).c_str());
		auto update = update_schema();
		if (update.first) {
			auto schema_ends = std::chrono::system_clock::now();
			if (update.second) {
				Stats::cnt().add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
			} else {
				Stats::cnt().add("schema_reads", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
			}
			break;
		}
	} while (true);
}


Xapian::RSet
DatabaseHandler::get_rset(const Xapian::Query& query, Xapian::doccount maxitems)
{
	L_CALL(this, "DatabaseHandler::get_rset(...)");

	Xapian::RSet rset;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			Xapian::Enquire enquire(*database->db);
			enquire.set_query(query);
			auto mset = enquire.get_mset(0, maxitems);
			for (const auto& doc : mset) {
				rset.add_document(doc);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		database->reopen();
	}

	return rset;
}


std::unique_ptr<Xapian::ExpandDecider>
DatabaseHandler::get_edecider(const similar_field_t& similar)
{
	L_CALL(this, "DatabaseHandler::get_edecider(...)");

	// Expand Decider filter.
	std::vector<std::string> prefixes;
	prefixes.reserve(similar.type.size() + similar.field.size());
	for (const auto& sim_type : similar.type) {
		char type = toUType(Unserialise::type(sim_type));
		prefixes.emplace_back(1, type);
		prefixes.emplace_back(1, tolower(type));
	}
	for (const auto& sim_field : similar.field) {
		auto field_spc = schema->get_data_field(sim_field).first;
		if (field_spc.get_type() != FieldType::EMPTY) {
			prefixes.push_back(field_spc.prefix());
		}
	}
	return std::make_unique<FilterPrefixesExpandDecider>(prefixes);
}


MSet
DatabaseHandler::get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& /*suggestions*/)
{
	L_CALL(this, "DatabaseHandler::get_mset(...)");

	MSet mset;

	schema = get_schema();

	Xapian::Query query;
	switch (method) {
		case HTTP_GET: {
			QueryDSL query_object(schema);
			query = query_object.get_query(query_object.make_dsl_query(e));
			break;
		}

		case HTTP_POST: {
			if (qdsl && qdsl->find(QUERYDSL_QUERY) != qdsl->end()) {
				QueryDSL query_object(schema);
				query = query_object.get_query(qdsl->at(QUERYDSL_QUERY));
			} else {
				QueryDSL query_object(schema);
				query = query_object.get_query(query_object.make_dsl_query(e));
			}
			break;
		}

		default:
			break;
	}

	// Configure sorter.
	std::unique_ptr<Multi_MultiValueKeyMaker> sorter;
	if (!e.sort.empty()) {
		sorter = std::make_unique<Multi_MultiValueKeyMaker>();
		std::string field, value;
		for (const auto& sort : e.sort) {
			size_t pos = sort.find(":");
			if (pos == std::string::npos) {
				field.assign(sort);
				value.clear();
			} else {
				field.assign(sort.substr(0, pos));
				value.assign(sort.substr(pos + 1));
			}
			bool descending = false;
			switch (field.at(0)) {
				case '-':
					descending = true;
				case '+':
					field.erase(field.begin());
					break;
			}
			const auto field_spc = schema->get_slot_field(field);
			if (field_spc.get_type() != FieldType::EMPTY) {
				sorter->add_value(field_spc, descending, value, e);
			}
		}
	}

	// Get the collapse key to use for queries.
	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (!e.collapse.empty()) {
		const auto field_spc = schema->get_slot_field(e.collapse);
		collapse_key = field_spc.slot;
	}

	// Configure nearest and fuzzy search:
	std::unique_ptr<Xapian::ExpandDecider> nearest_edecider;
	Xapian::RSet nearest_rset;
	if (e.is_nearest) {
		nearest_edecider = get_edecider(e.nearest);
		lock_database lk_db(this);
		nearest_rset = get_rset(query, e.nearest.n_rset);
	}

	Xapian::RSet fuzzy_rset;
	std::unique_ptr<Xapian::ExpandDecider> fuzzy_edecider;
	if (e.is_fuzzy) {
		fuzzy_edecider = get_edecider(e.fuzzy);
		lock_database lk_db(this);
		fuzzy_rset = get_rset(query, e.fuzzy.n_rset);
	}

	lock_database lk_db(this);
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto final_query = query;
			Xapian::Enquire enquire(*database->db);
			if (collapse_key != Xapian::BAD_VALUENO) {
				enquire.set_collapse_key(collapse_key, e.collapse_max);
			}
			if (aggs) {
				enquire.add_matchspy(aggs);
			}
			if (sorter) {
				enquire.set_sort_by_key_then_relevance(sorter.get(), false);
			}
			if (e.is_nearest) {
				auto eset = enquire.get_eset(e.nearest.n_eset, nearest_rset, nearest_edecider.get());
				final_query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), e.nearest.n_term);
			}
			if (e.is_fuzzy) {
				auto eset = enquire.get_eset(e.fuzzy.n_eset, fuzzy_rset, fuzzy_edecider.get());
				final_query = Xapian::Query(Xapian::Query::OP_OR, final_query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), e.fuzzy.n_term));
			}
			enquire.set_query(final_query);
			mset = enquire.get_mset(e.offset, e.limit, e.check_at_least);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const QueryParserError& exc) {
			THROW(ClientError, exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			THROW(ClientError, exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		} catch (const std::exception& exc) {
			THROW(ClientError, "The search was not performed (%s)", exc.what());
		}
		database->reopen();
	}

	return mset;
}


std::pair<bool, bool>
DatabaseHandler::update_schema()
{
	L_CALL(this, "DatabaseHandler::update_schema()");

	auto mod_schema = schema->get_modified_schema();
	if (mod_schema) {
		auto old_schema = schema->get_const_schema();
		if (!XapiandManager::manager->schemas.set(this, old_schema, mod_schema)) {
			return std::make_pair(false, true);
		}
		return std::make_pair(true, true);
	}
	return std::make_pair(true, false);
}


std::string
DatabaseHandler::get_prefixed_term_id(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_prefixed_term_id(%s)", repr(doc_id).c_str());

	schema = get_schema();

	auto field_spc = schema->get_data_id();
	if (field_spc.get_type() == FieldType::EMPTY) {
		// Search like namespace.
		const auto type_ser = Serialise::guess_serialise(doc_id);
		field_spc.set_type(type_ser.first);
		Schema::set_namespace_spc_id(field_spc);
		return prefixed(type_ser.second, field_spc.prefix(), field_spc.get_ctype());
	} else {
		return prefixed(Serialise::serialise(field_spc, doc_id), field_spc.prefix(), field_spc.get_ctype());
	}
}


std::vector<std::string>
DatabaseHandler::get_metadata_keys()
{
	L_CALL(this, "DatabaseHandler::get_metadata_keys()");

	lock_database lk_db(this);
	return database->get_metadata_keys();
}


std::string
DatabaseHandler::get_metadata(const std::string& key)
{
	L_CALL(this, "DatabaseHandler::get_metadata(%s)", repr(key).c_str());

	lock_database lk_db(this);
	return database->get_metadata(key);
}


bool
DatabaseHandler::set_metadata(const std::string& key, const std::string& value, bool overwrite)
{
	L_CALL(this, "DatabaseHandler::set_metadata(%s, %s, %s)", repr(key).c_str(), repr(value).c_str(), overwrite ? "true" : "false");

	lock_database lk_db(this);
	if (!overwrite) {
		auto old_value = database->get_metadata(key);
		if (!old_value.empty()) {
			return (old_value == value);
		}
	}
	database->set_metadata(key, value);
	return true;
}


Document
DatabaseHandler::get_document(const Xapian::docid& did)
{
	L_CALL(this, "DatabaseHandler::get_document((Xapian::docid)%d)", did);

	lock_database lk_db(this);
	return Document(this, database->get_document(did));
}


Document
DatabaseHandler::get_document(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document((std::string)%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk_db(this);
	Xapian::docid did = database->find_document(term_id);
	return Document(this, database->get_document(did, database->flags & DB_WRITABLE));
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_docid(%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk_db(this);
	return database->find_document(term_id);
}


void
DatabaseHandler::delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::delete_document(%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk_db(this);
	database->delete_document(database->find_document(term_id), commit_, wal_);
}


MsgPack
DatabaseHandler::get_document_info(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document_info(%s)", repr(doc_id).c_str());

	auto document = get_document(doc_id);

	const auto data = document.get_data();

	const auto obj = MsgPack::unserialise(::split_data_obj(data));

	MsgPack info;
	info[ID_FIELD_NAME] = Document::get_field(ID_FIELD_NAME, obj) || document.get_value(ID_FIELD_NAME);
	info[RESERVED_DATA] = obj;

#ifdef XAPIAND_DATA_STORAGE
	const auto store = ::split_data_store(data);
	if (store.first) {
		if (store.second.empty()) {
			info["_blob"] = nullptr;
		} else {
			const auto locator = ::storage_unserialise_locator(store.second);
			const auto ct_type_mp = Document::get_field(CT_FIELD_NAME, obj);
			info["_blob"] = {
				{ "_type", "stored" },
				{ "_content_type", ct_type_mp ? ct_type_mp.as_string() : "unknown" },
				{ "_volume", std::get<0>(locator) },
				{ "_offset", std::get<1>(locator) },
				{ "_size", std::get<2>(locator) },
			};
		}
	} else
#endif
	{
		const auto blob = document.get_blob();
		const auto blob_data = ::unserialise_string_at(2, blob);
		if (blob_data.empty()) {
			info["_blob"] = nullptr;
		} else {
			auto blob_ct = ::unserialise_string_at(1, blob);
			info["_blob"] = {
				{ "_type", "local" },
				{ "_content_type", blob_ct },
				{ "_size", blob_data.size() },
			};
		}
	}

	info[RESERVED_TERMS] = document.get_terms();
	info[RESERVED_VALUES] = document.get_values();

	return info;
}


MsgPack
DatabaseHandler::get_database_info()
{
	L_CALL(this, "DatabaseHandler::get_database_info()");

	lock_database lk_db(this);
	unsigned doccount = database->db->get_doccount();
	unsigned lastdocid = database->db->get_lastdocid();
	MsgPack info;
	info["_uuid"] = database->db->get_uuid();
	info["_doc_count"] = doccount;
	info["_last_id"] = lastdocid;
	info["_doc_del"] = lastdocid - doccount;
	info["_av_length"] = database->db->get_avlength();
	info["_doc_len_lower"] =  database->db->get_doclength_lower_bound();
	info["_doc_len_upper"] = database->db->get_doclength_upper_bound();
	info["_has_positions"] = database->db->has_positions();
	return info;
}


bool
DatabaseHandler::commit(bool _wal)
{
	L_CALL(this, "DatabaseHandler::commit(%s)", _wal ? "true" : "false");

	lock_database lk_db(this);
	return database->commit(_wal);
}


bool
DatabaseHandler::reopen()
{
	L_CALL(this, "DatabaseHandler::reopen()");

	lock_database lk_db(this);
	return database->reopen();
}


long long
DatabaseHandler::get_mastery_level()
{
	L_CALL(this, "DatabaseHandler::get_mastery_level()");

	try {
		lock_database lk_db(this);
		return database->mastery_level;
	} catch (const CheckoutError&) {
		return ::read_mastery(endpoints[0].path, false);
	}
}


void
DatabaseHandler::init_ref(const Endpoint& endpoint)
{
	L_CALL(nullptr, "DatabaseHandler::init_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto doc_id = get_hashed(endpoint.path);

	try {
		try {
			db_handler.get_document(doc_id);
		} catch (const DocNotFoundError&) {
			static const MsgPack obj = {
				{ "_id",       { { "_type",  "term"             }, { "_index", "field"   } } },
				{ "master",    { { "_value", DOCUMENT_DB_MASTER }, { "_type",  "term"    }, { "_index", "field_terms"  } } },
				{ "reference", { { "_value", 1                  }, { "_type",  "integer" }, { "_index", "field_values" } } },
			};
			db_handler.index(doc_id, false, obj, true, msgpack_type);
		}
	} catch (const CheckoutError&) {
		L_CRIT(nullptr, "Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


void
DatabaseHandler::inc_ref(const Endpoint& endpoint)
{
	L_CALL(nullptr, "DatabaseHandler::inc_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto doc_id = get_hashed(endpoint.path);

	try {
		try {
			auto document = db_handler.get_document(doc_id);
			auto nref = document.get_value("reference").as_i64() + 1;
			const MsgPack obj = {
				{ "_id",       { { "_type",  "term"             }, { "_index", "field"   } } },
				{ "master",    { { "_value", DOCUMENT_DB_MASTER }, { "_type",  "term"    }, { "_index", "field_terms"  } } },
				{ "reference", { { "_value", nref               }, { "_type",  "integer" }, { "_index", "field_values" } } },
			};
			db_handler.index(doc_id, false, obj, true, msgpack_type);
		} catch (const DocNotFoundError&) {
			// QUESTION: Document not found - should add?
			// QUESTION: This case could happen?
			static const MsgPack obj = {
				{ "_id",       { { "_type",  "term"             }, { "_index", "field"   } } },
				{ "master",    { { "_value", DOCUMENT_DB_MASTER }, { "_type",  "term"    }, { "_index", "field_terms"  } } },
				{ "reference", { { "_value", 1                  }, { "_type",  "integer" }, { "_index", "field_values" } } },
			};
			db_handler.index(doc_id, false, obj, true, msgpack_type);
		}
	} catch (const CheckoutError&) {
		L_CRIT(nullptr, "Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


void
DatabaseHandler::dec_ref(const Endpoint& endpoint)
{
	L_CALL(nullptr, "DatabaseHandler::dec_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto doc_id = get_hashed(endpoint.path);

	try {
		try {
			auto document = db_handler.get_document(doc_id);
			auto nref = document.get_value("reference").as_i64() - 1;
			const MsgPack obj = {
				{ "_id",       { { "_type",  "term"             }, { "_index", "field"   } } },
				{ "master",    { { "_value", DOCUMENT_DB_MASTER }, { "_type",  "term"    }, { "_index", "field_terms"  } } },
				{ "reference", { { "_value", nref               }, { "_type",  "integer" }, { "_index", "field_values" } } },
			};
			db_handler.index(doc_id, false, obj, true, msgpack_type);
			if (nref == 0) {
				// qmtx need a lock
				delete_files(endpoint.path);
			}
		} catch (const DocNotFoundError&) { }
	} catch (const CheckoutError&) {
		L_CRIT(nullptr, "Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


int
DatabaseHandler::get_master_count()
{
	L_CALL(nullptr, "DatabaseHandler::get_master_count()");

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	try {
		std::vector<std::string> suggestions;
		query_field_t q_t;
		q_t.limit = 0;
		q_t.query.push_back("master:M");
		auto mset = db_handler.get_mset(q_t, nullptr, nullptr, suggestions);
		return mset.get_matches_estimated();
	} catch (const CheckoutError&) {
		L_CRIT(nullptr, "Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return - 1;
	}
}


Document::Document()
	: db_handler(nullptr) { }


Document::Document(const Xapian::Document& doc_)
	: doc(doc_),
	  db_handler(nullptr) { }


Document::Document(DatabaseHandler* db_handler_, const Xapian::Document& doc_)
	: doc(doc_),
	  db_handler(db_handler_),
	  database(db_handler->database) { }


Document::Document(const Document& doc_)
	: doc(doc_.doc),
	  db_handler(doc_.db_handler),
	  database(doc_.database) { }


Document&
Document::operator=(const Document& doc_)
{
	doc = doc_.doc;
	db_handler = doc_.db_handler;
	database = doc_.database;
	return *this;
}


void
Document::update()
{
	L_CALL(this, "Document::update()");

	if (db_handler && db_handler->database && database != db_handler->database) {
		doc = db_handler->database->get_document(doc.get_docid());
		database = db_handler->database;
	}
}


std::string
Document::serialise(size_t retries)
{
	L_CALL(this, "Document::serialise(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.serialise();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return serialise(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


std::string
Document::get_value(Xapian::valueno slot, size_t retries)
{
	L_CALL(this, "Document::get_value(%u, %zu)", slot, retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.get_value(slot);
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_value(slot, --retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


std::string
Document::get_data(size_t retries)
{
	L_CALL(this, "Document::get_data(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.get_data();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_data(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


std::string
Document::get_blob(size_t retries)
{
	L_CALL(this, "Document::get_blob(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		update();
#ifdef XAPIAND_DATA_STORAGE
		if (db_handler) {
			return db_handler->database->storage_get_blob(doc);
		}
#endif
		auto data = doc.get_data();
		return split_data_blob(data);
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_blob(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


MsgPack
Document::get_terms(size_t retries)
{
	L_CALL(this, "get_terms(%zu)", retries);

	try {
		MsgPack terms;

		lock_database lk_db(db_handler);
		update();

		// doc.termlist_count() disassociates the database in doc.

		const auto it_e = doc.termlist_end();
		for (auto it = doc.termlist_begin(); it != it_e; ++it) {
			auto& term = terms[*it];
			term["_wdf"] = it.get_wdf();  // The within-document-frequency of the current term in the current document.
			try {
				auto _term_freq = it.get_termfreq();  // The number of documents which this term indexes.
				term["_term_freq"] = _term_freq;
			} catch (const Xapian::InvalidOperationError&) { }  // Iterator has moved, and does not support random access or doc is not associated with a database.
			if (it.positionlist_count()) {
				auto& term_pos = term["_pos"];
				term_pos.reserve(it.positionlist_count());
				const auto pit_e = it.positionlist_end();
				for (auto pit = it.positionlist_begin(); pit != pit_e; ++pit) {
					term_pos.push_back(*pit);
				}
			}
		}
		return terms;
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_terms(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


MsgPack
Document::get_values(size_t retries)
{
	L_CALL(this, "get_values(%zu)", retries);

	try {
		MsgPack values;

		lock_database lk_db(db_handler);
		update();

		values.reserve(doc.values_count());
		const auto iv_e = doc.values_end();
		for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
			values[std::to_string(iv.get_valueno())] = *iv;
		}
		return values;
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_values(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		}
	}
}


MsgPack
Document::get_value(const std::string& slot_name)
{
	L_CALL(this, "Document::get_value(%s)", slot_name.c_str());

	if (db_handler) {
		auto slot_field = db_handler->get_schema()->get_slot_field(slot_name);
		return Unserialise::MsgPack(slot_field.get_type(), get_value(slot_field.slot));
	} else {
		return MsgPack(MsgPack::Type::NIL);
	}
}


std::pair<bool, std::string>
Document::get_store()
{
	L_CALL(this, "Document::get_store()");

	return ::split_data_store(get_data());
}


MsgPack
Document::get_obj()
{
	L_CALL(this, "Document::get_obj()");

	return MsgPack::unserialise(::split_data_obj(get_data()));
}


MsgPack
Document::get_field(const std::string& slot_name)
{
	L_CALL(this, "Document::get_field(%s)", slot_name.c_str());

	return get_field(slot_name, get_obj());
}


MsgPack
Document::get_field(const std::string& slot_name, const MsgPack& obj)
{
	L_CALL(nullptr, "Document::get_field(%s, <obj>)", slot_name.c_str());

	auto itf = obj.find(slot_name);
	if (itf != obj.end()) {
		const auto& value = itf.value();
		if (value.is_map()) {
			auto itv = value.find(RESERVED_VALUE);
			if (itv != value.end()) {
				const auto& value_ = itv.value();
				if (!value_.empty()) {
					return value_;
				}
			}
		}
		if (!value.empty()) {
			return value;
		}
	}

	return MsgPack(MsgPack::Type::NIL);
}
