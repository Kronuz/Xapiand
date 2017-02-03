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

#include "database.h"                       // for DatabasePool, Database
#include "exception.h"                      // for CheckoutError, ClientError
#include "length.h"                         // for unserialise_length, seria...
#include "log.h"                            // for L_CALL, Log
#include "manager.h"                        // for XapiandManager, XapiandM...
#include "msgpack.h"                        // for MsgPack, object::object, ...
#include "msgpack_patcher.h"                // for apply_patch
#include "multivalue/aggregation.h"         // for AggregationMatchSpy
#include "multivalue/keymaker.h"            // for Multi_MultiValueKeyMaker
#include "query_dsl.h"                      // for QUERYDSL_QUERY, QueryDSL
#include "rapidjson/document.h"             // for Document
#include "schema.h"                         // for Schema, required_spc_t
#include "serialise.h"                      // for cast, serialise, type
#include "utils.h"                          // for repr
#include "v8/exception.h"                   // for Error, ReferenceError
#include "v8/v8pp.h"                        // for Processor::Function, Proc...


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


DatabaseHandler::lock_database::lock_database(DatabaseHandler* db_handler_)
	: db_handler(db_handler_)
{
	lock();
}


DatabaseHandler::lock_database::~lock_database()
{
	unlock();
}


void
DatabaseHandler::lock_database::lock()
{
	if (db_handler) {
		if (db_handler->database) {
			THROW(Error, "lock_database is already locked");
		} else if (!XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags)) {
			THROW(CheckoutError, "Cannot checkout database: %s", repr(db_handler->endpoints.to_string()).c_str());
		}
	}
}


void
DatabaseHandler::lock_database::unlock()
{
	if (db_handler && db_handler->database) {
		XapiandManager::manager->database_pool.checkin(db_handler->database);
	}
}


DatabaseHandler::DatabaseHandler()
	: flags(0),
	  method(HTTP_GET) { }


DatabaseHandler::DatabaseHandler(const Endpoints& endpoints_, int flags_, enum http_method method_)
	: endpoints(endpoints_),
	  flags(flags_),
	  method(method_) { }


std::shared_ptr<Database>
DatabaseHandler::get_database() const noexcept
{
	return database;
}


std::shared_ptr<Schema>
DatabaseHandler::get_schema(const MsgPack* obj) const
{
	L_CALL(this, "DatabaseHandler::get_schema(<obj>)");

	return std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags, obj));
}


void
DatabaseHandler::recover_index()
{
	L_CALL(this, "DatabaseHandler::recover_index()");

	XapiandManager::manager->database_pool.recover_database(endpoints, RECOVER_REMOVE_WRITABLE);
	reset(endpoints, flags, HTTP_PUT);
}


void
DatabaseHandler::reset(const Endpoints& endpoints_, int flags_, enum http_method method_)
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
}


MsgPack
DatabaseHandler::get_document_obj(const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::get_document_obj(%s)", repr(term_id).c_str());

	std::string data;
	{
		lock_database lk(this);
		Xapian::docid did = database->find_document(term_id);
		data = database->get_document(did).get_data();
	}
	return MsgPack::unserialise(::split_data_obj(data));
}


#ifdef XAPIAND_V8
MsgPack
DatabaseHandler::run_script(MsgPack& data, const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::run_script(...)");

	std::string script;
	auto it = data.find(RESERVED_SCRIPT);
	if (it == data.end()) {
		return data;
	} else {
		try {
			script = it.value().as_string();
		} catch (const msgpack::type_error&) {
			THROW(ClientError, "%s must be string", RESERVED_SCRIPT);
		}
	}

	try {
		auto processor = v8pp::Processor::compile(RESERVED_SCRIPT, script);

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
	} catch (const v8pp::ReferenceError&) {
		return data;
	} catch (const v8pp::Error& e) {
		THROW(ClientError, e.what());
	}
}
#endif


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const std::string& store, MsgPack& obj, const std::string& blob, bool commit_, const std::string& ct_type)
{
	L_CALL(this, "DatabaseHandler::index(%s, %s, <store>, %s, <blob>, %s, <ct_type>)", repr(_document_id).c_str(), stored ? "true" : "false", repr(obj.to_string()).c_str(), commit_ ? "true" : "false");

	Xapian::Document doc;

	std::string term_id;
	std::string prefixed_term_id;
	required_spc_t spc_id;

#ifdef XAPIAND_V8
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
					prefixed_term_id = prefixed(term_id, spc_id.prefix, spc_id.get_ctype());
#ifdef XAPIAND_V8
					{
						lock_database lk(this);
						doc_revision = database->get_revision_document(prefixed_term_id);
					}
					obj = run_script(obj, prefixed_term_id);
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
					const auto found = ct_type.find_last_of("/");
					std::string type(ct_type.c_str(), found);
					std::string subtype(ct_type.c_str(), found + 1, ct_type.length());

					auto& ct_field = obj[CT_FIELD_NAME];
					if (!ct_field.is_map() && !ct_field.is_undefined()) {
						ct_field = MsgPack();
					}
					ct_field[RESERVED_TYPE] = TERM_STR;
					ct_field[RESERVED_VALUE] = ct_type;
					ct_field[type][subtype] = nullptr;
				}

				// Index object.
				obj = schema->index(obj, doc);

				if (prefixed_term_id.empty()) {
					// Now the schema is full, get specification id.
					spc_id = schema->get_data_id();
					if (spc_id.get_type() == FieldType::EMPTY) {
						// Index like a namespace.
						static const auto& prefix_id = get_prefix(ID_FIELD_NAME);
						const auto type_ser = Serialise::get_type(id_value);
						term_id = type_ser.second;
						prefixed_term_id = prefixed(term_id, prefix_id, required_spc_t::get_ctype(specification_t::global_type(type_ser.first)));
					} else {
						term_id = Serialise::serialise(spc_id, _document_id);
						prefixed_term_id = prefixed(term_id, spc_id.prefix, spc_id.get_ctype());
					}
#ifdef XAPIAND_V8
					{
						lock_database lk(this);
						doc_revision = database->get_revision_document(prefixed_term_id);
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
				doc.set_data(join_data(stored, store, obj.serialise(), serialise_strings({ prefixed_term_id, ct_type, blob })));
			}

			doc.add_boolean_term(prefixed_term_id);
			doc.add_value(spc_id.slot, term_id);

			try {
				lock_database lk(this);
#ifdef XAPIAND_V8
				if (database->set_revision_document(prefixed_term_id, doc_revision))
#endif
				{
					auto did = database->replace_document_term(prefixed_term_id, doc, commit_);
					return std::make_pair(std::move(did), std::move(obj));
				}
			} catch (const Xapian::DatabaseError&) {
				// Try to recover from DatabaseError (i.e when the index is manually deleted)
				recover_index();
				lock_database lk(this);
#ifdef XAPIAND_V8
				if (database->set_revision_document(prefixed_term_id, doc_revision))
#endif
				{
					auto did = database->replace_document_term(prefixed_term_id, doc, commit_);
					return std::make_pair(std::move(did), std::move(obj));
				}
			}
#ifdef XAPIAND_V8
		} while (true);
	} catch(...) {
		if (!prefixed_term_id.empty()) {
			lock_database lk(this);
			database->dec_count_document(prefixed_term_id);
		}
		throw;
	}
#endif
}


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type)
{
	L_CALL(this, "DatabaseHandler::index(%s, %s, <body>, %s, %s)", repr(_document_id).c_str(), stored ? "true" : "false", commit_ ? "true" : "false", ct_type.c_str());

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
DatabaseHandler::patch(const std::string& _document_id, const MsgPack& patches, bool commit_, const std::string& ct_type)
{
	L_CALL(this, "DatabaseHandler::patch(%s, <patches>, %s, %s)", repr(_document_id).c_str(), commit_ ? "true" : "false", ct_type.c_str());

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
DatabaseHandler::merge(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type)
{
	L_CALL(this, "DatabaseHandler::merge(%s, %s, <body>, %s, %s)", repr(_document_id).c_str(), stored ? "true" : "false", commit_ ? "true" : "false", ct_type.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (_document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
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
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			prefixes.push_back(field_spc.prefix);
		}
	}
	return std::make_unique<FilterPrefixesExpandDecider>(prefixes);
}


MSet
DatabaseHandler::get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& suggestions)
{
	L_CALL(this, "DatabaseHandler::get_mset(...)");

	MSet mset;

	schema = get_schema();

	Xapian::Query query;
	lock_database lk(this);
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
	Xapian::RSet nearest_rset;
	std::unique_ptr<Xapian::ExpandDecider> nearest_edecider;
	if (e.is_nearest) {
		nearest_rset = get_rset(query, e.nearest.n_rset);
		nearest_edecider = get_edecider(e.nearest);
	}

	Xapian::RSet fuzzy_rset;
	std::unique_ptr<Xapian::ExpandDecider> fuzzy_edecider;
	if (e.is_fuzzy) {
		fuzzy_rset = get_rset(query, e.fuzzy.n_rset);
		fuzzy_edecider = get_edecider(e.fuzzy);
	}

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
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const QueryParserError& exc) {
			THROW(ClientError, "%s", exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, "%s", exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, "%s", exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			THROW(ClientError, "%s", exc.get_msg().c_str());
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
		if (!XapiandManager::manager->database_pool.set_schema(endpoints[0], flags, old_schema, mod_schema)) {
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
		static const auto& prefix_id = get_prefix(ID_FIELD_NAME);
		auto type_ser = Serialise::get_type(doc_id);
		return prefixed(type_ser.second, prefix_id, required_spc_t::get_ctype(specification_t::global_type(type_ser.first)));
	} else {
		return prefixed(Serialise::serialise(field_spc, doc_id), field_spc.prefix, field_spc.get_ctype());
	}
}


std::string
DatabaseHandler::get_metadata(const std::string& key)
{
	L_CALL(this, "DatabaseHandler::get_metadata(%s)", repr(key).c_str());

	lock_database lk(this);
	return database->get_metadata(key);
}



void
DatabaseHandler::set_metadata(const std::string& key, const std::string& value)
{
	L_CALL(this, "DatabaseHandler::set_metadata(%s, %s)", repr(key).c_str(), repr(value).c_str());

	lock_database lk(this);
	database->set_metadata(key, value);
}


Document
DatabaseHandler::get_document(const Xapian::docid& did)
{
	L_CALL(this, "DatabaseHandler::get_document((Xapian::docid)%d)", did);

	lock_database lk(this);
	return Document(this, database->get_document(did));
}


Document
DatabaseHandler::get_document(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document((std::string)%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk(this);
	Xapian::docid did = database->find_document(term_id);
	return Document(this, database->get_document(did));
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_docid(%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk(this);
	return database->find_document(term_id);
}


void
DatabaseHandler::delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::delete_document(%s)", repr(doc_id).c_str());

	const auto term_id = get_prefixed_term_id(doc_id);

	lock_database lk(this);
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
			const auto locator = database->storage_unserialise_locator(store.second);
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
		const auto blob_data = unserialise_string_at(2, blob);
		if (blob_data.empty()) {
			info["_blob"] = nullptr;
		} else {
			auto blob_ct = unserialise_string_at(1, blob);
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

	lock_database lk(this);
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
		doc = db_handler->database->get_document(doc.get_docid(), true);
		database = db_handler->database;
	}
}


std::string
Document::serialise()
{
	L_CALL(this, "Document::serialise()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return doc.serialise();
}


std::string
Document::get_value(Xapian::valueno slot)
{
	L_CALL(this, "Document::get_value(%u)", slot);

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return doc.get_value(slot);
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


std::string
Document::get_data()
{
	L_CALL(this, "Document::get_data()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return doc.get_data();
}


std::string
Document::get_blob()
{
	L_CALL(this, "Document::get_blob()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	if (db_handler) {
		return db_handler->database->storage_get_blob(doc);
	} else {
		auto data = doc.get_data();
		return split_data_blob(data);
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
Document::get_terms()
{
	L_CALL(this, "get_terms()");

	MsgPack terms;

	DatabaseHandler::lock_database lk(db_handler);
	update();

	// doc.termlist_count() disassociates the database in doc.

	const auto it_e = doc.termlist_end();
	for (auto it = doc.termlist_begin(); it != it_e; ++it) {
		auto& term = terms[*it];
		term["_wdf"] = it.get_wdf();  // The within-document-frequency of the current term in the current document.
		try {
			auto _term_freq = it.get_termfreq();  // The number of documents which this term indexes.
			term["_term_freq"] = _term_freq;
		} catch (const Xapian::InvalidOperationError&) { } // Iterator has moved, and does not support random access or doc is not associated with a database.
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
}


MsgPack
Document::get_values()
{
	L_CALL(this, "get_values()");

	MsgPack values;

	DatabaseHandler::lock_database lk(db_handler);
	update();

	values.reserve(doc.values_count());
	const auto iv_e = doc.values_end();
	for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
		values[std::to_string(iv.get_valueno())] = *iv;
	}

	return values;
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
