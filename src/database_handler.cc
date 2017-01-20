/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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
	: db_handler(db_handler_),
	  database(nullptr)
{
	lock();
}


DatabaseHandler::lock_database::lock_database(DatabaseHandler& db_handler)
	: lock_database(&db_handler) { }


DatabaseHandler::lock_database::~lock_database()
{
	unlock();
}


void
DatabaseHandler::lock_database::lock()
{
	if (db_handler && !db_handler->database) {
		if (XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags)) {
			database = &db_handler->database;
		} else {
			THROW(CheckoutError, "Cannot checkout database: %s", repr(db_handler->endpoints.to_string()).c_str());
		}
	}
}


void
DatabaseHandler::lock_database::unlock() noexcept
{
	if (database) {
		if (*database) {
			XapiandManager::manager->database_pool.checkin(*database);
		}
		(*database).reset();
		database = nullptr;
	}
}


DatabaseHandler::DatabaseHandler()
	: flags(0),
	  method(HTTP_GET) { }


DatabaseHandler::DatabaseHandler(const Endpoints &endpoints_, int flags_)
	: endpoints(endpoints_),
	  flags(flags_),
	  method(HTTP_GET) { }


std::shared_ptr<Database>
DatabaseHandler::get_database() const noexcept
{
	return database;
}


std::shared_ptr<Schema>
DatabaseHandler::get_schema() const
{
	if (endpoints.size() == 1) {
		return std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));
	} else {
		for (const auto& endp : endpoints) {
			try {
				return std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endp, flags));
			} catch (const CheckoutError&) {
				continue;
			}
		}
		THROW(CheckoutError, "Cannot checkout none of the databases: %s", repr(endpoints.to_string()).c_str());
	}
}


std::shared_ptr<Schema>
DatabaseHandler::get_fvschema() const
{
	std::shared_ptr<const MsgPack> fvs, fvs_aux;
	for (const auto& e : endpoints) {
		fvs_aux = XapiandManager::manager->database_pool.get_schema(e, flags);	/* Get the first valid schema */
		if (fvs_aux->is_null()) {
			continue;
		}
		if (fvs == nullptr) {
			fvs = fvs_aux;
		} else if (*fvs != *fvs_aux) {
			THROW(ClientError, "Cannot index in several indexes with different schemas");
		}
	}
	return std::make_shared<Schema>(fvs ? fvs : fvs_aux);
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
		DatabaseHandler::lock_database lk(this);  // Try opening database (raises errors)
	}
}


Document
DatabaseHandler::get_document_term(const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::get_document_term(%s)", repr(term_id).c_str());

	DatabaseHandler::lock_database lk(this);
	Xapian::docid did = database->find_document(term_id);
	return Document(this, database->get_document(did));
}


MsgPack
DatabaseHandler::run_script(const MsgPack& data, const std::string& term_id)
{
	L_CALL(this, "DatabaseHandler::run_script(...)");

#if XAPIAND_V8

	std::string script;
	try {
		script = data.at(RESERVED_SCRIPT).as_string();
	} catch (const std::out_of_range&) {
		return data;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "%s must be string", RESERVED_SCRIPT);
	}

	try {
		auto processor = v8pp::Processor::compile("_script", script);

		switch (method) {
			case HTTP_PUT: {
				MsgPack old_data;
				try {
					auto document = get_document_term(term_id);
					old_data = document.get_obj();
				} catch (const DocNotFoundError&) { }
				MsgPack data_ = data;
				return (*processor)["on_put"](data_, old_data);
			}

			case HTTP_PATCH:
			case HTTP_MERGE: {
				MsgPack old_data;
				try {
					auto document = get_document_term(term_id);
					old_data = document.get_obj();
				} catch (const DocNotFoundError&) { }
				MsgPack data_ = data;
				return (*processor)["on_patch"](data_, old_data);
			}

			case HTTP_DELETE: {
				MsgPack old_data;
				try {
					auto document = get_document_term(term_id);
					old_data = document.get_obj();
				} catch (const DocNotFoundError&) { }
				MsgPack data_ = data;
				return (*processor)["on_delete"](data_, old_data);
			}

			case HTTP_GET: {
				MsgPack data_ = data;
				return (*processor)["on_get"](data_);
			}

			case HTTP_POST: {
				MsgPack data_ = data;
				return (*processor)["on_post"](data_);
			}

			default:
				return data;
		}
	} catch (const v8pp::ReferenceError& e) {
		return data;
	} catch (const v8pp::Error& e) {
		THROW(ClientError, e.what());
	}

#else

	return data;

#endif
}


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const std::string& store, const MsgPack& obj, const std::string& blob, bool commit_, const std::string& ct_type, endpoints_error_list* err_list)
{
	L_CALL(this, "DatabaseHandler::index(%s, <obj>, <blob>)", repr(_document_id).c_str());

	L_INDEX(this, "Document to index (%s): %s", repr(_document_id).c_str(), repr(obj.to_string()).c_str());

	// Create a suitable document.
	Xapian::Document doc;

	MsgPack obj_;

	std::string term_id;
	std::string prefixed_term_id;
	required_spc_t spc_id;

	auto schema_begins = std::chrono::system_clock::now();
	do {
		schema = (endpoints.size() == 1) ? get_schema() : get_fvschema();
		L_INDEX(this, "Schema: %s", repr(schema->to_string()).c_str());

		spc_id = schema->get_data_id();
		if (spc_id.sep_types[2] == FieldType::EMPTY) {
			obj_ = obj;
		} else {
			term_id = Serialise::serialise(spc_id, _document_id);
			prefixed_term_id = prefixed(term_id, spc_id.prefix, spc_id.get_ctype());
			obj_ = run_script(obj, prefixed_term_id);
		}

		// Add ID.
		auto& id_field = obj_[ID_FIELD_NAME];
		auto id_value = Cast::cast(spc_id.sep_types[2], _document_id);
		if (id_field.is_map()) {
			id_field[RESERVED_VALUE] = id_value;
		} else {
			id_field = id_value;
		}

		if (blob.empty()) {
			obj_.erase(CT_FIELD_NAME);
		} else {
			// Add Content Type if indexing a blob.
			auto found = ct_type.find_last_of("/");
			std::string type(ct_type.c_str(), found);
			std::string subtype(ct_type.c_str(), found + 1, ct_type.length());

			auto& ct_field = obj_[CT_FIELD_NAME];
			if (!ct_field.is_map() && !ct_field.is_undefined()) {
				ct_field = MsgPack();
			}
			ct_field[RESERVED_TYPE] = TERM_STR;
			ct_field[RESERVED_VALUE] = ct_type;
			ct_field[type][subtype] = nullptr;
		}

		// Index object.
		obj_ = schema->index(obj_, doc);

		if (prefixed_term_id.empty()) {
			// Now the schema is full, get specification id.
			spc_id = schema->get_data_id();
			if (spc_id.get_type() == FieldType::EMPTY) {
				// Index like a namespace.
				static const auto& prefix_id = get_prefix(ID_FIELD_NAME);
				auto type_ser = Serialise::get_type(id_value);
				term_id = type_ser.second;
				prefixed_term_id = prefixed(term_id, prefix_id, required_spc_t::get_ctype(specification_t::global_type(type_ser.first)));
			} else {
				term_id = Serialise::serialise(spc_id, _document_id);
				prefixed_term_id = prefixed(term_id, spc_id.prefix, spc_id.get_ctype());
			}
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
		L_INDEX(this, "Data: %s", repr(obj_.to_string()).c_str());
		doc.set_data(join_data(false, "", obj_.serialise(), ""));
	} else {
		L_INDEX(this, "Data: %s", repr(obj_.to_string()).c_str());
		doc.set_data(join_data(stored, store, obj_.serialise(), serialise_strings({prefixed_term_id, ct_type, blob})));
	}

	doc.add_boolean_term(prefixed_term_id);
	doc.add_value(spc_id.slot, term_id);

	Xapian::docid did;
	const auto _endpoints = endpoints;
	for (const auto& e : _endpoints) {
		endpoints.clear();
		endpoints.add(e);
		try {
			DatabaseHandler::lock_database lk(this);
			did = database->replace_document_term(prefixed_term_id, doc, commit_);
		} catch (const Xapian::Error& err) {
			if (err_list) {
				err_list->operator[](err.get_error_string()).push_back(e.to_string());
			}
		}
	}
	endpoints = std::move(_endpoints);

	return std::make_pair(std::move(did), std::move(obj_));
}


DataType
DatabaseHandler::index(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type, endpoints_error_list* err_list)
{
	L_CALL(this, "DatabaseHandler::index(%s, <body>)", repr(_document_id).c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "Database is read-only");
	}

	if (_document_id.empty()) {
		THROW(Error, "Document must have an 'id'");
	}

	MsgPack obj;
	std::string blob;
	if (body.is_map()) {
		obj = body;
	} else if (body.is_string()) {
		blob = body.as_string();
	} else {
		THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}

	return index(_document_id, stored, "", obj, blob, commit_, ct_type, err_list);
}


DataType
DatabaseHandler::patch(const std::string& _document_id, const MsgPack& patches, bool commit_, const std::string& ct_type, endpoints_error_list* err_list)
{
	L_CALL(this, "DatabaseHandler::patch(%s, <patches>)", repr(_document_id).c_str());

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
	auto obj = document.get_obj();
	apply_patch(patches, obj);

	auto store = document.get_store();
	auto blob = store.first ? "" : document.get_blob();

	return index(_document_id, store.first, store.second, obj, blob, commit_, ct_type, err_list);
}


DataType
DatabaseHandler::merge(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type, endpoints_error_list* err_list)
{
	L_CALL(this, "DatabaseHandler::merge(%s, <obj>)", repr(_document_id).c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (_document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	auto document = get_document(_document_id);
	auto obj = document.get_obj();
	if (body.is_map()) {
		obj.update(body);
		auto store = document.get_store();
		auto blob = store.first ? "" : document.get_blob();

		return index(_document_id, store.first, store.second, obj, blob, commit_, ct_type, err_list);
	} else if (body.is_string()) {
		auto blob = body.as_string();
		return index(_document_id, stored, "", obj, blob, commit_, ct_type, err_list);
	} else {
		THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}
}


void
DatabaseHandler::write_schema(const std::string& body)
{
	L_CALL(this, "DatabaseHandler::write_schema(<body>)");

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "Database is read-only");
	}

	// Create MsgPack object
	rapidjson::Document rdoc;
	json_load(rdoc, body);
	MsgPack obj(rdoc);

	write_schema(obj);
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
	DatabaseHandler::lock_database lk(this);
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
			auto field_spc = schema->get_slot_field(field);
			if (field_spc.get_type() != FieldType::EMPTY) {
				sorter->add_value(field_spc, descending, value, e);
			}
		}
	}

	// Get the collapse key to use for queries.
	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (!e.collapse.empty()) {
		auto field_spc = schema->get_slot_field(e.collapse);
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


Document
DatabaseHandler::get_document(const Xapian::docid& did)
{
	L_CALL(this, "DatabaseHandler::get_document((Xapian::docid)%d)", did);

	DatabaseHandler::lock_database lk(this);
	return Document(this, database->get_document(did));
}


std::pair<bool, bool>
DatabaseHandler::update_schema()
{
	L_CALL(this, "DatabaseHandler::update_schema()");

	auto mod_schema = schema->get_modified_schema();
	if (mod_schema) {
		auto old_schema = schema->get_const_schema();
		for (const auto& e: endpoints) {
			if (!XapiandManager::manager->database_pool.set_schema(e, flags, old_schema, mod_schema)) {
				return std::make_pair(false, true);
			}
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
	if (field_spc.sep_types[2] == FieldType::EMPTY) {
		// Search like namespace.
		static const auto& prefix_id = get_prefix(ID_FIELD_NAME);
		auto type_ser = Serialise::get_type(doc_id);
		return prefixed(type_ser.second, prefix_id, required_spc_t::get_ctype(specification_t::global_type(type_ser.first)));
	} else {
		return prefixed(Serialise::serialise(field_spc, doc_id), field_spc.prefix, field_spc.get_ctype());
	}
}


Document
DatabaseHandler::get_document(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document((std::string)%s)", repr(doc_id).c_str());

	return get_document_term(get_prefixed_term_id(doc_id));
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_docid(%s)", repr(doc_id).c_str());

	auto prefixed_term_id = get_prefixed_term_id(doc_id);

	DatabaseHandler::lock_database lk(this);
	return database->find_document(prefixed_term_id);
}


void
DatabaseHandler::delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::delete_document(%s)", repr(doc_id).c_str());

	auto _id = get_docid(doc_id);

	DatabaseHandler::lock_database lk(this);
	database->delete_document(_id, commit_, wal_);
}


endpoints_error_list
DatabaseHandler::multi_db_delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::multi_db_delete_document(%s)", repr(doc_id).c_str());

	endpoints_error_list err_list;
	auto _endpoints = endpoints;
	for (const auto& e : _endpoints) {
		endpoints.clear();
		endpoints.add(e);
		try {
			auto _id = get_docid(doc_id);
			DatabaseHandler::lock_database lk(this);
			database->delete_document(_id, commit_, wal_);
		} catch (const DocNotFoundError& err) {
			err_list["Document not found"].push_back(e.to_string());
		} catch (const Xapian::Error& err) {
			err_list[err.get_error_string()].push_back(e.to_string());
		}
	}
	endpoints = _endpoints;
	return err_list;
}


void
DatabaseHandler::get_document_info(MsgPack& info, const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document_info(%s, %s)", repr(info.to_string()).c_str(), repr(doc_id).c_str());

	DatabaseHandler::lock_database lk(this);  // optimize nested database locking

	auto document = get_document(doc_id);

	info[ID_FIELD_NAME] = document.get_field(ID_FIELD_NAME) || document.get_value(ID_FIELD_NAME);
	info[RESERVED_DATA] = document.get_obj();

	auto ct_type_mp = document.get_field(CT_FIELD_NAME);

#ifdef XAPIAND_DATA_STORAGE
	auto store = document.get_store();
	if (store.first) {
		if (store.second.empty()) {
			info["_blob"] = nullptr;
		} else {
			auto locator = database->storage_unserialise_locator(store.second);
			info["_blob"] = {
				{"_type", "stored"},
				{"_content_type", ct_type_mp ? ct_type_mp.as_string() : "unknown"},
				{"_volume", std::get<0>(locator)},
				{"_offset", std::get<1>(locator)},
				{"_size", std::get<2>(locator)},
			};
		}
	} else
#endif
	{
		auto blob = document.get_blob();
		auto blob_data = unserialise_string_at(2, blob);
		if (blob_data.empty()) {
			info["_blob"] = nullptr;
		} else {
			auto blob_ct = unserialise_string_at(1, blob);
			info["_blob"] = {
				{"_type", "local"},
				{"_content_type", blob_ct},
				{"_size", blob_data.size()},
			};
		}
	}

	auto& stats_terms = info[RESERVED_TERMS];
	const auto it_e = document.termlist_end();
	for (auto it = document.termlist_begin(); it != it_e; ++it) {
		auto& stat_term = stats_terms[*it];
		stat_term["_wdf"] = it.get_wdf();  // The within-document-frequency of the current term in the current document.
		stat_term["_term_freq"] = it.get_termfreq();  // The number of documents which this term indexes.
		if (it.positionlist_count()) {
			auto& stat_term_pos = stat_term["_pos"];
			const auto pit_e = it.positionlist_end();
			for (auto pit = it.positionlist_begin(); pit != pit_e; ++pit) {
				stat_term_pos.push_back(*pit);
			}
		}
	}

	auto& stats_values = info[RESERVED_VALUES];
	const auto iv_e = document.values_end();
	for (auto iv = document.values_begin(); iv != iv_e; ++iv) {
		stats_values[std::to_string(iv.get_valueno())] = *iv;
	}
}


void
DatabaseHandler::get_database_info(MsgPack& info)
{
	L_CALL(this, "DatabaseHandler::get_database_info(%s)", repr(info.to_string()).c_str());

	DatabaseHandler::lock_database lk(this);
	unsigned doccount = database->db->get_doccount();
	unsigned lastdocid = database->db->get_lastdocid();
	info["_uuid"] = database->db->get_uuid();
	info["_doc_count"] = doccount;
	info["_last_id"] = lastdocid;
	info["_doc_del"] = lastdocid - doccount;
	info["_av_length"] = database->db->get_avlength();
	info["_doc_len_lower"] =  database->db->get_doclength_lower_bound();
	info["_doc_len_upper"] = database->db->get_doclength_upper_bound();
	info["_has_positions"] = database->db->has_positions();
}


void
Document::update()
{
	if (db_handler && db_handler->database && database != db_handler->database) {
		L_CALL(this, "Document::update()");
		database = db_handler->database;
		std::shared_ptr<Database> database_ = database;
		DatabaseHandler* db_handler_ = db_handler;
		*this = database->get_document(get_docid(), true);
		db_handler = db_handler_;
		database = database_;
	}
}


void
Document::update() const
{
	const_cast<Document*>(this)->update();
}


Document::Document()
	: db_handler(nullptr) { }


Document::Document(const Xapian::Document &doc)
	: Xapian::Document(doc),
	  db_handler(nullptr) { }


Document::Document(DatabaseHandler* db_handler_, const Xapian::Document &doc)
	: Xapian::Document(doc),
	  db_handler(db_handler_),
	  database(db_handler->database) { }


std::string
Document::get_value(Xapian::valueno slot) const
{
	L_CALL(this, "Document::get_value()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::get_value(slot);
}


MsgPack
Document::get_value(const std::string& slot_name) const
{
	L_CALL(this, "Document::get_value(%s)", slot_name.c_str());

	auto schema = db_handler->get_schema();
	auto slot_field = schema->get_slot_field(slot_name);

	return Unserialise::MsgPack(slot_field.get_type(), get_value(slot_field.slot));
}


void
Document::add_value(Xapian::valueno slot, const std::string& value)
{
	L_CALL(this, "Document::add_value()");

	Xapian::Document::add_value(slot, value);
}


void
Document::add_value(const std::string& slot_name, const MsgPack& value)
{
	L_CALL(this, "Document::add_value(%s)", slot_name.c_str());

	auto schema = db_handler->get_schema();
	auto slot_field = schema->get_slot_field(slot_name);

	add_value(slot_field.slot, Serialise::MsgPack(slot_field, value));
}


void
Document::remove_value(Xapian::valueno slot)
{
	L_CALL(this, "Document::remove_value()");

	Xapian::Document::remove_value(slot);
}


void
Document::remove_value(const std::string& slot_name)
{
	L_CALL(this, "Document::remove_value(%s)", slot_name.c_str());

	auto schema = db_handler->get_schema();
	auto slot_field = schema->get_slot_field(slot_name);

	remove_value(slot_field.slot);
}


std::string
Document::get_data() const
{
	L_CALL(this, "Document::get_data()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::get_data();
}


void
Document::set_data(const std::string& data)
{
	L_CALL(this, "Document::set_data(%s)", repr(data).c_str());

	Xapian::Document::set_data(data);
}


std::string
Document::get_blob()
{
	L_CALL(this, "Document::get_blob()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return db_handler->database->storage_get_blob(*this);
}


std::pair<bool, std::string>
Document::get_store()
{
	L_CALL(this, "Document::get_store()");

	return ::split_data_store(get_data());
}


void
Document::set_blob(const std::string& blob, bool stored)
{
	L_CALL(this, "Document::set_blob()");

	DatabaseHandler::lock_database lk(db_handler);  // optimize nested database locking
	set_data(::join_data(stored, "", ::split_data_obj(get_data()), blob));
}


MsgPack
Document::get_obj() const
{
	L_CALL(this, "Document::get_obj()");

	return MsgPack::unserialise(::split_data_obj(get_data()));
}


void
Document::set_obj(const MsgPack& obj)
{
	L_CALL(this, "Document::get_obj()");

	DatabaseHandler::lock_database lk(db_handler);  // optimize nested database locking
	auto blob = get_blob();
	auto store = get_store();
	set_data(::join_data(store.first, store.second, obj.serialise(), blob));
}


Xapian::termcount
Document::termlist_count() const
{
	L_CALL(this, "Document::termlist_count()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::termlist_count();
}


Xapian::TermIterator
Document::termlist_begin() const
{
	L_CALL(this, "Document::termlist_begin()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::termlist_begin();
}


Xapian::termcount
Document::values_count() const
{
	L_CALL(this, "Document::values_count()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::values_count();
}


Xapian::ValueIterator
Document::values_begin() const
{
	L_CALL(this, "Document::values_begin()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::values_begin();
}


std::string
Document::serialise() const
{
	L_CALL(this, "Document::serialise()");

	DatabaseHandler::lock_database lk(db_handler);
	update();
	return Xapian::Document::serialise();
}


MsgPack
Document::get_field(const std::string& slot_name) const
{
	L_CALL(this, "Document::get_field(%s)", slot_name.c_str());

	auto obj = get_obj();
	auto itf = obj.find(slot_name);
	if (itf != obj.end()) {
		auto& value = obj.at(*itf);
		if (value.is_map()) {
			auto itv = value.find(RESERVED_VALUE);
			if (itv != value.end()) {
				auto& value_ = value.at(*itv);
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
