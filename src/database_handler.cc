/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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
#include "script.h"                         // for Script
#include "serialise.h"                      // for cast, serialise, type
#include "utils.h"                          // for repr

#if defined(XAPIAND_V8)
#include "v8pp/v8pp.h"                      // for v8pp namespace
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include "chaipp/chaipp.h"                  // for chaipp namespace
#endif


// Reserved words only used in the responses to the user.
constexpr const char RESPONSE_AV_LENGTH[]           = "#av_length";
constexpr const char RESPONSE_BLOB[]                = "#blob";
constexpr const char RESPONSE_CONTENT_TYPE[]        = "#content_type";
constexpr const char RESPONSE_DOC_COUNT[]           = "#doc_count";
constexpr const char RESPONSE_DOC_DEL[]             = "#doc_del";
constexpr const char RESPONSE_DOC_LEN_LOWER[]       = "#doc_len_lower";
constexpr const char RESPONSE_DOC_LEN_UPPER[]       = "#doc_len_upper";
constexpr const char RESPONSE_HAS_POSITIONS[]       = "#has_positions";
constexpr const char RESPONSE_LAST_ID[]             = "#last_id";
constexpr const char RESPONSE_OFFSET[]              = "#offset";
constexpr const char RESPONSE_POS[]                 = "#pos";
constexpr const char RESPONSE_SIZE[]                = "#size";
constexpr const char RESPONSE_TERM_FREQ[]           = "#term_freq";
constexpr const char RESPONSE_TYPE[]                = "#type";
constexpr const char RESPONSE_UUID[]                = "#uuid";
constexpr const char RESPONSE_VOLUME[]              = "#volume";
constexpr const char RESPONSE_WDF[]                 = "#wdf";
constexpr const char RESPONSE_DOCID[]               = "#docid";
constexpr const char RESPONSE_DATA[]                = "#data";
constexpr const char RESPONSE_TERMS[]               = "#terms";
constexpr const char RESPONSE_VALUES[]              = "#values";


const std::string dump_metadata_header ("xapiand-dump-meta");
const std::string dump_schema_header("xapiand-dump-schm");
const std::string dump_documents_header("xapiand-dump-docs");


Xapian::docid
to_docid(const std::string& document_id)
{
	size_t sz = document_id.size();
	if (sz > 2 && document_id[0] == ':' && document_id[1] == ':') {
		std::string did_str(document_id.begin() + 2, document_id.end());
		try {
			return static_cast<Xapian::docid>(strict_stol(did_str));
		} catch (const InvalidArgument& er) {
			THROW(ClientError, "Value %s cannot be cast to integer [%s]", repr(did_str).c_str(), er.what());
		} catch (const OutOfRange& er) {
			THROW(ClientError, "Value %s cannot be cast to integer [%s]", repr(did_str).c_str(), er.what());
		}
	}
	return static_cast<Xapian::docid>(0);
}


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
	if (db_handler) {
		if (db_handler->database) {
			unlock();
		}
	}
}


template<typename F, typename... Args>
void
lock_database::lock(F&& f, Args&&... args)
{
	L_CALL("lock_database::lock(...)");

	if (db_handler) {
		if (db_handler->database) {
			THROW(Error, "lock_database is already locked: %s", repr(db_handler->database->endpoints.to_string()).c_str());
		} else {
			XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags, std::forward<F>(f), std::forward<Args>(args)...);
		}
	}
}


void
lock_database::lock()
{
	L_CALL("lock_database::lock()");

	if (db_handler) {
		if (db_handler->database) {
			THROW(Error, "lock_database is already locked: %s", repr(db_handler->database->endpoints.to_string()).c_str());
		} else {
			XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags);
		}
	}
}


void
lock_database::unlock()
{
	L_CALL("lock_database::unlock(...)");

	if (db_handler) {
		if (db_handler->database) {
			XapiandManager::manager->database_pool.checkin(db_handler->database);
		} else {
			THROW(Error, "lock_database is not locked: %s", repr(db_handler->database->endpoints.to_string()).c_str());
		}
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
	L_CALL("DatabaseHandler::get_schema(<obj>)");
	auto s = XapiandManager::manager->schemas.get(this, obj, obj && (flags & DB_WRITABLE));
	return std::make_shared<Schema>(std::move(std::get<0>(s)), std::move(std::get<1>(s)), std::move(std::get<2>(s)));
}


void
DatabaseHandler::recover_index()
{
	L_CALL("DatabaseHandler::recover_index()");

	XapiandManager::manager->database_pool.recover_database(endpoints, RECOVER_REMOVE_WRITABLE);
	reset(endpoints, flags, HTTP_PUT, context);
}


void
DatabaseHandler::reset(const Endpoints& endpoints_, int flags_, enum http_method method_, const std::shared_ptr<std::unordered_set<size_t>>& context_)
{
	L_CALL("DatabaseHandler::reset(%s, %x, <method>)", repr(endpoints_.to_string()).c_str(), flags_);

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
	L_CALL("DatabaseHandler::repr_wal(%u, %u)", start_revision, end_revision);

	if (endpoints.size() != 1) {
		THROW(ClientError, "It is expected one single endpoint");
	}

	// WAL required on a local writable database, open it.
	lock_database lk_db(this);
	auto wal = std::make_unique<DatabaseWAL>(endpoints[0].path, database.get());
	return wal->repr(start_revision, end_revision);
}
#endif


Document
DatabaseHandler::get_document_term(const std::string& term_id)
{
	L_CALL("DatabaseHandler::get_document_term(%s)", repr(term_id).c_str());

	lock_database lk_db(this);
	auto did = database->find_document(term_id);
	return Document(this, database->get_document(did, database->flags & DB_WRITABLE));
}


#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
std::mutex DatabaseHandler::documents_mtx;
std::unordered_map<size_t, std::shared_ptr<std::pair<size_t, const MsgPack>>> DatabaseHandler::documents;


template<typename Processor>
MsgPack&
DatabaseHandler::call_script(MsgPack& data, const std::string& term_id, size_t script_hash, size_t body_hash, const std::string& script_body, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair)
{
	try {
		auto processor = Processor::compile(script_hash, body_hash, script_body);
		switch (method) {
			case HTTP_PUT:
				old_document_pair = get_document_change_seq(term_id);
				if (old_document_pair) {
					L_INDEX("Script: on_put(%s, %s)", data.to_string(4).c_str(), old_document_pair->second.to_string(4).c_str());
					data = (*processor)["on_put"](data, old_document_pair->second);
				} else {
					L_INDEX("Script: on_put(%s)", data.to_string(4).c_str());
					data = (*processor)["on_put"](data, MsgPack(MsgPack::Type::MAP));
				}
				break;

			case HTTP_PATCH:
			case HTTP_MERGE:
				old_document_pair = get_document_change_seq(term_id);
				if (old_document_pair) {
					L_INDEX("Script: on_patch(%s, %s)", data.to_string(4).c_str(), old_document_pair->second.to_string(4).c_str());
					data = (*processor)["on_patch"](data, old_document_pair->second);
				} else {
					L_INDEX("Script: on_patch(%s)", data.to_string(4).c_str());
					data = (*processor)["on_patch"](data, MsgPack(MsgPack::Type::MAP));
				}
				break;

			case HTTP_DELETE:
				old_document_pair = get_document_change_seq(term_id);
				if (old_document_pair) {
					L_INDEX("Script: on_delete(%s, %s)", data.to_string(4).c_str(), old_document_pair->second.to_string(4).c_str());
					data = (*processor)["on_delete"](data, old_document_pair->second);
				} else {
					L_INDEX("Script: on_delete(%s)", data.to_string(4).c_str());
					data = (*processor)["on_delete"](data, MsgPack(MsgPack::Type::MAP));
				}
				break;

			case HTTP_GET:
				L_INDEX("Script: on_get(%s)", data.to_string(4).c_str());
				data = (*processor)["on_get"](data);
				break;

			case HTTP_POST:
				L_INDEX("Script: on_post(%s)", data.to_string(4).c_str());
				data = (*processor)["on_post"](data);
				break;

			default:
				break;
		}
		return data;
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
	}
}


MsgPack&
DatabaseHandler::run_script(MsgPack& data, const std::string& term_id, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair, const MsgPack& data_script)
{
	L_CALL("DatabaseHandler::run_script(...)");

	if (data_script.is_map()) {
		const auto& type = data_script.at(RESERVED_TYPE);
		const auto& sep_type = required_spc_t::get_types(type.str());
		if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
			THROW(ClientError, "Missing Implementation for Foreign scripts");
		} else {
			auto it_s = data_script.find(RESERVED_CHAI);
			if (it_s == data_script.end()) {
#if defined(XAPIAND_V8)
				const auto& ecma = data_script.at(RESERVED_ECMA);
				return call_script<v8pp::Processor>(data, term_id, ecma.at(RESERVED_HASH).u64(), ecma.at(RESERVED_BODY_HASH).u64(), ecma.at(RESERVED_BODY).str(), old_document_pair);
#else
				THROW(ClientError, "Script type 'ecma' (ECMAScript or JavaScript) not available.");
#endif
			} else {
#if defined(XAPIAND_CHAISCRIPT)
				const auto& chai = it_s.value();
				return call_script<chaipp::Processor>(data, term_id, chai.at(RESERVED_HASH).u64(), chai.at(RESERVED_BODY_HASH).u64(), chai.at(RESERVED_BODY).str(), old_document_pair);
#else
				THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
			}
		}
	}

	return data;
}
#endif


DataType
DatabaseHandler::index(const std::string& document_id, bool stored, const std::string& stored_locator, MsgPack& obj, const std::string& blob, bool commit_, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::index(%s, %s, <stored_locator>, %s, <blob>, %s, <ct_type>)", repr(document_id).c_str(), stored ? "true" : "false", repr(obj.to_string()).c_str(), commit_ ? "true" : "false");

	static UUIDGenerator generator;

	Xapian::Document doc;
	required_spc_t spc_id;
	std::string term_id;
	std::string prefixed_term_id;

	Xapian::docid did = 0;
	std::string doc_uuid;
	std::string doc_id;
	std::string doc_xid;
	if (document_id.empty()) {
		doc_uuid = Unserialise::uuid(generator(opts.uuid_compact).serialise(), static_cast<UUIDRepr>(opts.uuid_repr));
		// Add a new empty document to get its document ID:
		lock_database lk_db(this);
		try {
			did = database->add_document(Xapian::Document(), false, false);
		} catch (const Xapian::DatabaseError&) {
			// Try to recover from DatabaseError (i.e when the index is manually deleted)
			lk_db.unlock();
			recover_index();
			lk_db.lock();
			did = database->add_document(Xapian::Document(), false, false);
		}
		doc_id = std::to_string(did);
	} else {
		doc_xid = document_id;
	}

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	try {
		std::shared_ptr<std::pair<size_t, const MsgPack>> old_document_pair;
		do {
#endif
			auto schema_begins = std::chrono::system_clock::now();
			do {
				schema = get_schema(&obj);
				L_INDEX("Schema: %s", repr(schema->to_string()).c_str());

				// Get term ID.
				spc_id = schema->get_data_id();
				auto id_type = spc_id.get_type();
				if (did != 0) {
					if (id_type == FieldType::UUID || id_type == FieldType::EMPTY) {
						doc_xid = doc_uuid;
					} else {
						doc_xid = doc_id;
					}
				}
				if (id_type == FieldType::EMPTY) {
					auto f_it = obj.find(ID_FIELD_NAME);
					if (f_it != obj.end()) {
						const auto& field = f_it.value();
						if (field.is_map()) {
							auto f_it_end = field.end();
							auto ft_it = field.find(RESERVED_TYPE);
							if (ft_it != f_it_end) {
								const auto& type = ft_it.value();
								if (!type.is_string()) {
									THROW(ClientError, "Data inconsistency, %s must be string", RESERVED_TYPE);
								}
								spc_id.set_types(type.str());
								id_type = spc_id.get_type();
								if (did != 0) {
									if (id_type == FieldType::UUID || id_type == FieldType::EMPTY) {
										doc_xid = doc_uuid;
									} else {
										doc_xid = doc_id;
									}
								}
							}
						}
					}
				} else {
					term_id = Serialise::serialise(spc_id, doc_xid);
					prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
				}

				// Add ID.
				auto id_value = Cast::cast(id_type, doc_xid);
				auto& id_field = obj[ID_FIELD_NAME];
				if (id_field.is_map()) {
					id_field[RESERVED_VALUE] = id_value;
				} else {
					id_field = id_value;
				}

				// Index object.
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
				obj = schema->index(obj, doc, prefixed_term_id, &old_document_pair, this);
#else
				obj = schema->index(obj, doc);
#endif

				// Ensure term ID.
				if (prefixed_term_id.empty()) {
					// Now the schema is full, get specification id.
					spc_id = schema->get_data_id();
					id_type = spc_id.get_type();
					if (did != 0) {
						if (id_type == FieldType::UUID || id_type == FieldType::EMPTY) {
							doc_xid = doc_uuid;
						} else {
							doc_xid = doc_id;
						}
					}
					if (id_type == FieldType::EMPTY) {
						// Index like a namespace.
						const auto type_ser = Serialise::guess_serialise(doc_xid);
						spc_id.set_type(type_ser.first);
						Schema::set_namespace_spc_id(spc_id);
						term_id = type_ser.second;
						prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
					} else {
						term_id = Serialise::serialise(spc_id, doc_xid);
						prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
					}
				}
			} while (!update_schema(schema_begins));

			// Finish document: add data, ID term and ID value.
			if (blob.empty()) {
				L_INDEX("Data: %s", repr(obj.to_string()).c_str());
				doc.set_data(join_data(false, "", obj.serialise(), ""));
			} else {
				L_INDEX("Data: %s", repr(obj.to_string()).c_str());
				auto ct_type_str = ct_type.to_string();
				doc.set_data(join_data(stored, stored_locator, obj.serialise(), serialise_strings({ ct_type_str, blob })));
			}
			doc.add_boolean_term(prefixed_term_id);
			doc.add_value(spc_id.slot, term_id);

			// Index document.
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
			if (set_document_change_seq(prefixed_term_id, std::make_shared<std::pair<size_t, const MsgPack>>(std::make_pair(Document(doc).hash(), obj)), old_document_pair)) {
#endif
				lock_database lk_db(this);
				try {
					try {
						if (did) database->replace_document(did, doc, commit_);
						else did = database->replace_document_term(prefixed_term_id, doc, commit_);
						return std::make_pair(std::move(did), std::move(obj));
					} catch (const Xapian::DatabaseError& exc) {
						// Try to recover from DatabaseError (i.e when the index is manually deleted)
						L_WARNING("ERROR: %s (try recovery)", exc.get_description().c_str());
						lk_db.unlock();
						recover_index();
						lk_db.lock();
						if (did) database->replace_document(did, doc, commit_);
						else did = database->replace_document_term(prefixed_term_id, doc, commit_);
						return std::make_pair(std::move(did), std::move(obj));
					}
				} catch (...) {
					if (did != 0) {
						try {
							database->delete_document(did, false, false);
						} catch (...) { }
					}
					throw;
				}
#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
			}
		} while (true);
	} catch (const MissingTypeError&) { //FIXME: perhaps others erros must be handlend in here
		unsigned doccount;
		{
			lock_database lk_db(this);
			doccount = database->db->get_doccount();
		}
		if (doccount == 0 && schema) {
			auto old_schema = schema->get_const_schema();
			XapiandManager::manager->schemas.drop(this, old_schema);
		}
		if (!prefixed_term_id.empty()) {
			dec_document_change_cnt(prefixed_term_id);
		}
		throw;
	} catch (...) {
		if (!prefixed_term_id.empty()) {
			dec_document_change_cnt(prefixed_term_id);
		}
		throw;
	}
#endif
}


DataType
DatabaseHandler::index(const std::string& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::index(%s, %s, %s, %s, %s/%s)", repr(document_id).c_str(), stored ? "true" : "false", repr(body.to_string()).c_str(), commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "Database is read-only");
	}

	MsgPack obj;
	std::string blob;
	switch (body.getType()) {
		case MsgPack::Type::STR:
			blob = body.str();
			break;
		case MsgPack::Type::MAP:
			obj = body.clone();
			break;
		default:
			THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}

	return index(document_id, stored, "", obj, blob, commit_, ct_type);
}


DataType
DatabaseHandler::patch(const std::string& document_id, const MsgPack& patches, bool commit_, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::patch(%s, <patches>, %s, %s/%s)", repr(document_id).c_str(), commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	if (!patches.is_map() && !patches.is_array()) {
		THROW(ClientError, "Patches must be a JSON or MsgPack");
	}

	auto document = get_document(document_id);

	const auto data = document.get_data();

	auto obj = MsgPack::unserialise(split_data_obj(data));
	apply_patch(patches, obj);

	const auto store = split_data_store(data);
	const auto blob = store.first ? "" : document.get_blob();

	return index(document_id, store.first, store.second, obj, blob, commit_, ct_type);
}


DataType
DatabaseHandler::merge(const std::string& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::merge(%s, %s, <body>, %s, %s/%s)", repr(document_id).c_str(), stored ? "true" : "false", commit_ ? "true" : "false", ct_type.first.c_str(), ct_type.second.c_str());

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

	if (document_id.empty()) {
		THROW(ClientError, "Document must have an 'id'");
	}

	if (!body.is_map()) {
		THROW(ClientError, "Must be a JSON or MsgPack");
	}

	auto document = get_document(document_id);

	const auto data = document.get_data();

	auto obj = MsgPack::unserialise(split_data_obj(data));
	switch (obj.getType()) {
		case MsgPack::Type::STR: {
			const auto blob = body.str();
			return index(document_id, stored, "", obj, blob, commit_, ct_type);
		}
		case MsgPack::Type::MAP: {
			obj.update(body);
			const auto store = split_data_store(data);
			const auto blob = store.first ? "" : document.get_blob(); // only get blob when needed (when it's not stored)
			return index(document_id, store.first, store.second, obj, blob, commit_, ct_type);
		}
		default:
			THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob");
	}
}


void
DatabaseHandler::write_schema(const MsgPack& obj, bool replace)
{
	L_CALL("DatabaseHandler::write_schema(%s)", repr(obj.to_string()).c_str());

	auto schema_begins = std::chrono::system_clock::now();
	bool was_foreign_obj;
	do {
		schema = get_schema();
		was_foreign_obj = schema->write(obj, replace);
		if (!was_foreign_obj && opts.foreign) {
			THROW(ForeignSchemaError, "Schema of %s must use a foreign schema", repr(endpoints.to_string()).c_str());
		}
		L_INDEX("Schema to write: %s %s", repr(schema->to_string()).c_str(), was_foreign_obj ? "(foreign)" : "(local)");
	} while (!update_schema(schema_begins));

	if (was_foreign_obj) {
		MsgPack o = obj;
		o[RESERVED_TYPE] = "object";
		o.erase(RESERVED_ENDPOINT);
		do {
			schema = get_schema();
			was_foreign_obj = schema->write(o, replace);
			L_INDEX("Schema to write: %s (local)", repr(schema->to_string()).c_str());
		} while (!update_schema(schema_begins));
	}
}


void
DatabaseHandler::delete_schema()
{
	L_CALL("DatabaseHandler::write_schema(%s)", repr(obj.to_string()).c_str());

	auto schema_begins = std::chrono::system_clock::now();
	bool done;
	do {
		schema = get_schema();
		auto old_schema = schema->get_const_schema();
		done = XapiandManager::manager->schemas.drop(this, old_schema);
		L_INDEX("Schema to delete: %s", repr(schema->to_string()).c_str());
	} while (!done);
	auto schema_ends = std::chrono::system_clock::now();
	Stats::cnt().add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
}


Xapian::RSet
DatabaseHandler::get_rset(const Xapian::Query& query, Xapian::doccount maxitems)
{
	L_CALL("DatabaseHandler::get_rset(...)");

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
			if (!t) THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database: %s", exc.get_description().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description().c_str());
		}
		database->reopen();
	}

	return rset;
}


std::unique_ptr<Xapian::ExpandDecider>
DatabaseHandler::get_edecider(const similar_field_t& similar)
{
	L_CALL("DatabaseHandler::get_edecider(...)");

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


void
DatabaseHandler::dump_metadata(int fd)
{
	L_CALL("DatabaseHandler::dump_metadata()");

	lock_database lk_db(this);

	XXH32_state_t xxhash;
	XXH32_reset(&xxhash, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_metadata_header);
	XXH32_update(&xxhash, dump_metadata_header.data(), dump_metadata_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(&xxhash, db_endpoints.data(), db_endpoints.size());

	database->dump_metadata(fd, xxhash);

	uint32_t current_hash = XXH32_digest(&xxhash);
	serialise_length(fd, current_hash);
	L_INFO("Dump hash is 0x%08x", current_hash);
}


void
DatabaseHandler::dump_schema(int fd)
{
	L_CALL("DatabaseHandler::dump_schema()");

	schema = get_schema();
	auto saved_schema_ser = schema->get_full().serialise();

	lock_database lk_db(this);

	XXH32_state_t xxhash;
	XXH32_reset(&xxhash, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_schema_header);
	XXH32_update(&xxhash, dump_schema_header.data(), dump_schema_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(&xxhash, db_endpoints.data(), db_endpoints.size());

	serialise_string(fd, saved_schema_ser);
	XXH32_update(&xxhash, saved_schema_ser.data(), saved_schema_ser.size());

	uint32_t current_hash = XXH32_digest(&xxhash);
	serialise_length(fd, current_hash);
	L_INFO("Dump hash is 0x%08x", current_hash);
}


void
DatabaseHandler::dump_documents(int fd)
{
	L_CALL("DatabaseHandler::dump_documents()");

	lock_database lk_db(this);

	XXH32_state_t xxhash;
	XXH32_reset(&xxhash, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_documents_header);
	XXH32_update(&xxhash, dump_documents_header.data(), dump_documents_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(&xxhash, db_endpoints.data(), db_endpoints.size());

	database->dump_documents(fd, xxhash);

	uint32_t current_hash = XXH32_digest(&xxhash);
	serialise_length(fd, current_hash);
	L_INFO("Dump hash is 0x%08x", current_hash);
}


void
DatabaseHandler::restore(int fd)
{
	L_CALL("DatabaseHandler::restore()");

	std::string buffer;
	std::size_t off = 0;

	lock_database lk_db(this);

	XXH32_state_t xxhash;
	XXH32_reset(&xxhash, 0);

	auto header = unserialise_string(fd, buffer, off);
	XXH32_update(&xxhash, header.data(), header.size());
	if (header != dump_documents_header && header != dump_schema_header && header != dump_metadata_header) {
		THROW(ClientError, "Invalid dump", RESERVED_TYPE);
	}

	auto db_endpoints = unserialise_string(fd, buffer, off);
	XXH32_update(&xxhash, db_endpoints.data(), db_endpoints.size());

	// restore metadata (key, value)
	if (header == dump_metadata_header) {
		size_t i = 0;
		do {
			++i;
			auto key = unserialise_string(fd, buffer, off);
			XXH32_update(&xxhash, key.data(), key.size());
			auto value = unserialise_string(fd, buffer, off);
			XXH32_update(&xxhash, value.data(), value.size());
			if (key.empty() && value.empty()) break;

			if (key.empty()) {
				L_WARNING("Metadata with no key ignored [%zu]", ID_FIELD_NAME, i);
				continue;
			}

			L_INFO_HOOK("DatabaseHandler::restore", "Restoring metadata %s = %s", key.c_str(), value.c_str());
			database->set_metadata(key, value, false, false);
		} while (true);
	}

	// restore schema
	if (header == dump_schema_header) {
		auto saved_schema_ser = unserialise_string(fd, buffer, off);
		XXH32_update(&xxhash, saved_schema_ser.data(), saved_schema_ser.size());

		lk_db.unlock();
		if (!saved_schema_ser.empty()) {
			auto saved_schema = MsgPack::unserialise(saved_schema_ser);
			L_INFO_HOOK("DatabaseHandler::restore", "Restoring schema: %s", saved_schema.to_string(4).c_str());
			write_schema(saved_schema, true);
		}
		schema = get_schema();
		lk_db.lock();
	}

	// restore documents (document_id, object, blob)
	if (header == dump_documents_header) {
		lk_db.unlock();
		schema = get_schema();
		lk_db.lock();

		size_t i = 0;
		do {
			++i;
			auto obj_ser = unserialise_string(fd, buffer, off);
			XXH32_update(&xxhash, obj_ser.data(), obj_ser.size());
			auto blob = unserialise_string(fd, buffer, off);
			XXH32_update(&xxhash, blob.data(), blob.size());
			if (obj_ser.empty() && blob.empty()) break;

			Xapian::Document doc;
			required_spc_t spc_id;
			std::string term_id;
			std::string prefixed_term_id;

			std::string ct_type_str;
			if (!blob.empty()) {
				ct_type_str = unserialise_string_at(STORED_BLOB_CONTENT_TYPE, blob);
			}
			auto ct_type = ct_type_t(ct_type_str);

			MsgPack document_id;
			auto obj = MsgPack::unserialise(obj_ser);

			// Get term ID.
			spc_id = schema->get_data_id();
			auto f_it = obj.find(ID_FIELD_NAME);
			if (f_it != obj.end()) {
				const auto& field = f_it.value();
				if (field.is_map()) {
					auto f_it_end = field.end();
					if (spc_id.get_type() == FieldType::EMPTY) {
						auto ft_it = field.find(RESERVED_TYPE);
						if (ft_it != f_it_end) {
							const auto& type = ft_it.value();
							if (!type.is_string()) {
								THROW(ClientError, "Data inconsistency, %s must be string", RESERVED_TYPE);
							}
							spc_id.set_types(type.str());
						}
					}
					auto fv_it = field.find(RESERVED_VALUE);
					if (fv_it != f_it_end) {
						document_id = fv_it.value();
					}
				} else {
					document_id = field;
				}
			}

			if (document_id.is_undefined()) {
				L_WARNING("Document with no '%s' ignored [%zu]", ID_FIELD_NAME, i);
				continue;
			}

			obj = schema->index(obj, doc);

			// Ensure term ID.
			if (prefixed_term_id.empty()) {
				// Now the schema is full, get specification id.
				spc_id = schema->get_data_id();
				if (spc_id.get_type() == FieldType::EMPTY) {
					// Index like a namespace.
					const auto type_ser = Serialise::guess_serialise(document_id);
					spc_id.set_type(type_ser.first);
					Schema::set_namespace_spc_id(spc_id);
					term_id = type_ser.second;
					prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
				} else {
					term_id = Serialise::serialise(spc_id, document_id);
					prefixed_term_id = prefixed(term_id, spc_id.prefix(), spc_id.get_ctype());
				}
			}

			// Finish document: add data, ID term and ID value.
			if (blob.empty()) {
				doc.set_data(join_data(false, "", obj.serialise(), ""));
			} else {
				doc.set_data(join_data(true, "", obj.serialise(), blob));
			}
			doc.add_boolean_term(prefixed_term_id);
			doc.add_value(spc_id.slot, term_id);

			// Index document.
			L_INFO_HOOK("DatabaseHandler::restore", "Restoring document (%zu): %s", i, document_id.to_string().c_str());
			database->replace_document_term(prefixed_term_id, doc, false, false);
		} while (true);

		lk_db.unlock();
		auto schema_begins = std::chrono::system_clock::now();
		while (!update_schema(schema_begins));
		lk_db.lock();
	}

	uint32_t saved_hash = unserialise_length(fd, buffer, off);
	uint32_t current_hash = XXH32_digest(&xxhash);
	if (saved_hash != current_hash) {
		L_WARNING("Invalid dump hash (0x%08x != 0x%08x)", saved_hash, current_hash);
	}

	database->commit(false);
}


MSet
DatabaseHandler::get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& /*suggestions*/)
{
	L_CALL("DatabaseHandler::get_mset(%s, %s)", repr(join_string(e.query, " & ")).c_str(), qdsl ? repr(qdsl->to_string()).c_str() : "null");

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

	// L_DEBUG("query: %s", query.get_description().c_str());

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
			if (!t) THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database: %s", exc.get_description().c_str());
		} catch (const QueryParserError& exc) {
			THROW(ClientError, exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			THROW(ClientError, exc.get_description().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description().c_str());
		} catch (const std::exception& exc) {
			THROW(ClientError, "The search was not performed: %s", exc.what());
		}
		database->reopen();
	}

	return mset;
}


bool
DatabaseHandler::update_schema(std::chrono::time_point<std::chrono::system_clock> schema_begins)
{
	L_CALL("DatabaseHandler::update_schema()");
	bool done = true;
	bool updated = false;

	auto mod_schema = schema->get_modified_schema();
	if (mod_schema) {
		updated = true;
		auto old_schema = schema->get_const_schema();
		done = XapiandManager::manager->schemas.set(this, old_schema, mod_schema);
	}

	if (done) {
		auto schema_ends = std::chrono::system_clock::now();
		if (updated) {
			Stats::cnt().add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
		} else {
			Stats::cnt().add("schema_reads", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
		}
	}

	return done;
}


std::string
DatabaseHandler::get_prefixed_term_id(const std::string& document_id)
{
	L_CALL("DatabaseHandler::get_prefixed_term_id(%s)", repr(document_id).c_str());

	schema = get_schema();

	auto field_spc = schema->get_data_id();
	if (field_spc.get_type() == FieldType::EMPTY) {
		// Search like namespace.
		const auto type_ser = Serialise::guess_serialise(document_id);
		field_spc.set_type(type_ser.first);
		Schema::set_namespace_spc_id(field_spc);
		return prefixed(type_ser.second, field_spc.prefix(), field_spc.get_ctype());
	} else {
		return prefixed(Serialise::serialise(field_spc, document_id), field_spc.prefix(), field_spc.get_ctype());
	}
}


std::vector<std::string>
DatabaseHandler::get_metadata_keys()
{
	L_CALL("DatabaseHandler::get_metadata_keys()");

	lock_database lk_db(this);
	return database->get_metadata_keys();
}


std::string
DatabaseHandler::get_metadata(const std::string& key)
{
	L_CALL("DatabaseHandler::get_metadata(%s)", repr(key).c_str());

	lock_database lk_db(this);
	return database->get_metadata(key);
}


bool
DatabaseHandler::set_metadata(const std::string& key, const std::string& value, bool overwrite)
{
	L_CALL("DatabaseHandler::set_metadata(%s, %s, %s)", repr(key).c_str(), repr(value).c_str(), overwrite ? "true" : "false");

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
	L_CALL("DatabaseHandler::get_document((Xapian::docid)%d)", did);

	lock_database lk_db(this);
	return Document(this, database->get_document(did));
}


Document
DatabaseHandler::get_document(const std::string& document_id)
{
	L_CALL("DatabaseHandler::get_document((std::string)%s)", repr(document_id).c_str());

	auto did = to_docid(document_id);
	if (did) {
		return get_document(did);
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	did = database->find_document(term_id);
	return Document(this, database->get_document(did, database->flags & DB_WRITABLE));
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& document_id)
{
	L_CALL("DatabaseHandler::get_docid(%s)", repr(document_id).c_str());

	auto did = to_docid(document_id);
	if (did) {
		return did;
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	return database->find_document(term_id);
}


void
DatabaseHandler::delete_document(const std::string& document_id, bool commit_, bool wal_)
{
	L_CALL("DatabaseHandler::delete_document(%s)", repr(document_id).c_str());

	auto did = to_docid(document_id);
	if (did) {
		database->delete_document(did, commit_, wal_);
		return;
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	database->delete_document(database->find_document(term_id), commit_, wal_);
}


MsgPack
DatabaseHandler::get_document_info(const std::string& document_id)
{
	L_CALL("DatabaseHandler::get_document_info(%s)", repr(document_id).c_str());

	auto document = get_document(document_id);
	const auto data = document.get_data();

	const auto obj = MsgPack::unserialise(split_data_obj(data));

	MsgPack info;

	info[RESPONSE_DOCID] = document.get_docid();
	info[RESPONSE_DATA] = obj;

#ifdef XAPIAND_DATA_STORAGE
	const auto store = split_data_store(data);
	if (store.first) {
		if (store.second.empty()) {
			info[RESPONSE_BLOB] = nullptr;
		} else {
			const auto locator = storage_unserialise_locator(store.second);
			info[RESPONSE_BLOB] = {
				{ RESPONSE_TYPE, "stored" },
				{ RESPONSE_VOLUME, std::get<0>(locator) },
				{ RESPONSE_OFFSET, std::get<1>(locator) },
				{ RESPONSE_SIZE, std::get<2>(locator) },
				{ RESPONSE_CONTENT_TYPE, std::get<3>(locator) },
			};
		}
	} else
#endif
	{
		const auto blob = split_data_blob(data);
		const auto blob_data = unserialise_string_at(STORED_BLOB_DATA, blob);
		if (blob_data.empty()) {
			info[RESPONSE_BLOB] = nullptr;
		} else {
			auto blob_ct = unserialise_string_at(STORED_BLOB_CONTENT_TYPE, blob);
			info[RESPONSE_BLOB] = {
				{ RESPONSE_TYPE, "local" },
				{ RESPONSE_SIZE, blob_data.size() },
				{ RESPONSE_CONTENT_TYPE, blob_ct },
			};
		}
	}

	info[RESPONSE_TERMS] = document.get_terms();
	info[RESPONSE_VALUES] = document.get_values();

	return info;
}


MsgPack
DatabaseHandler::get_database_info()
{
	L_CALL("DatabaseHandler::get_database_info()");

	lock_database lk_db(this);
	unsigned doccount = database->db->get_doccount();
	unsigned lastdocid = database->db->get_lastdocid();
	MsgPack info;
	info[RESPONSE_UUID] = database->db->get_uuid();
	info[RESPONSE_DOC_COUNT] = doccount;
	info[RESPONSE_LAST_ID] = lastdocid;
	info[RESPONSE_DOC_DEL] = lastdocid - doccount;
	info[RESPONSE_AV_LENGTH] = database->db->get_avlength();
	info[RESPONSE_DOC_LEN_LOWER] =  database->db->get_doclength_lower_bound();
	info[RESPONSE_DOC_LEN_UPPER] = database->db->get_doclength_upper_bound();
	info[RESPONSE_HAS_POSITIONS] = database->db->has_positions();
	return info;
}


bool
DatabaseHandler::commit(bool _wal)
{
	L_CALL("DatabaseHandler::commit(%s)", _wal ? "true" : "false");

	lock_database lk_db(this);
	return database->commit(_wal);
}


bool
DatabaseHandler::reopen()
{
	L_CALL("DatabaseHandler::reopen()");

	lock_database lk_db(this);
	return database->reopen();
}


long long
DatabaseHandler::get_mastery_level()
{
	L_CALL("DatabaseHandler::get_mastery_level()");

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
	L_CALL("DatabaseHandler::init_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto document_id = get_hashed(endpoint.path);

	try {
		if (db_handler.get_metadata(RESERVED_SCHEMA).empty()) {
			db_handler.set_metadata(RESERVED_SCHEMA, Schema::get_initial_schema()->serialise());
		}
		try {
			db_handler.get_document(document_id);
		} catch (const DocNotFoundError&) {
			static const MsgPack obj = {
				{ ID_FIELD_NAME, { { RESERVED_TYPE,  "term"             }, { RESERVED_INDEX, "field"   } } },
				{ "master",      { { RESERVED_VALUE, DOCUMENT_DB_MASTER }, { RESERVED_TYPE,  "term"    }, { RESERVED_INDEX, "field_terms"  } } },
				{ "reference",   { { RESERVED_VALUE, 1                  }, { RESERVED_TYPE,  "integer" }, { RESERVED_INDEX, "field_values" } } },
			};
			db_handler.index(document_id, false, obj, true, msgpack_type);
		}
	} catch (const CheckoutError&) {
		L_CRIT("Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


void
DatabaseHandler::inc_ref(const Endpoint& endpoint)
{
	L_CALL("DatabaseHandler::inc_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto document_id = get_hashed(endpoint.path);

	try {
		try {
			auto document = db_handler.get_document(document_id);
			auto nref = document.get_value("reference").i64() + 1;
			const MsgPack obj = {
				{ ID_FIELD_NAME, { { RESERVED_TYPE,  "term"             }, { RESERVED_INDEX, "field"   } } },
				{ "master",      { { RESERVED_VALUE, DOCUMENT_DB_MASTER }, { RESERVED_TYPE,  "term"    }, { RESERVED_INDEX, "field_terms"  } } },
				{ "reference",   { { RESERVED_VALUE, nref               }, { RESERVED_TYPE,  "integer" }, { RESERVED_INDEX, "field_values" } } },
			};
			db_handler.index(document_id, false, obj, true, msgpack_type);
		} catch (const DocNotFoundError&) {
			// QUESTION: Document not found - should add?
			// QUESTION: This case could happen?
			static const MsgPack obj = {
				{ ID_FIELD_NAME, { { RESERVED_TYPE,  "term"             }, { RESERVED_INDEX, "field"   } } },
				{ "master",      { { RESERVED_VALUE, DOCUMENT_DB_MASTER }, { RESERVED_TYPE,  "term"    }, { RESERVED_INDEX, "field_terms"  } } },
				{ "reference",   { { RESERVED_VALUE, 1                  }, { RESERVED_TYPE,  "integer" }, { RESERVED_INDEX, "field_values" } } },
			};
			db_handler.index(document_id, false, obj, true, msgpack_type);
		}
	} catch (const CheckoutError&) {
		L_CRIT("Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


void
DatabaseHandler::dec_ref(const Endpoint& endpoint)
{
	L_CALL("DatabaseHandler::dec_ref(%s)", repr(endpoint.to_string()).c_str());

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	const auto document_id = get_hashed(endpoint.path);

	try {
		try {
			auto document = db_handler.get_document(document_id);
			auto nref = document.get_value("reference").i64() - 1;
			const MsgPack obj = {
				{ ID_FIELD_NAME, { { RESERVED_TYPE,  "term"             }, { RESERVED_INDEX, "field"   } } },
				{ "master",      { { RESERVED_VALUE, DOCUMENT_DB_MASTER }, { RESERVED_TYPE,  "term"    }, { RESERVED_INDEX, "field_terms"  } } },
				{ "reference",   { { RESERVED_VALUE, nref               }, { RESERVED_TYPE,  "integer" }, { RESERVED_INDEX, "field_values" } } },
			};
			db_handler.index(document_id, false, obj, true, msgpack_type);
			if (nref == 0) {
				// qmtx need a lock
				delete_files(endpoint.path);
			}
		} catch (const DocNotFoundError&) { }
	} catch (const CheckoutError&) {
		L_CRIT("Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return;
	}
}


int
DatabaseHandler::get_master_count()
{
	L_CALL("DatabaseHandler::get_master_count()");

	DatabaseHandler db_handler(Endpoints(Endpoint(".refs")), DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);

	try {
		std::vector<std::string> suggestions;
		query_field_t q_t;
		q_t.limit = 0;
		q_t.query.push_back("master:M");
		auto mset = db_handler.get_mset(q_t, nullptr, nullptr, suggestions);
		return mset.get_matches_estimated();
	} catch (const CheckoutError&) {
		L_CRIT("Cannot open %s database", repr(db_handler.endpoints.to_string()).c_str());
		return - 1;
	}
}


#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
const std::shared_ptr<std::pair<size_t, const MsgPack>>
DatabaseHandler::get_document_change_seq(const std::string& term_id)
{
	L_CALL("DatabaseHandler::get_document_change_seq(%s, %s)", endpoints.to_string().c_str(), repr(term_id).c_str());

	static std::hash<std::string> hash_fn_string;
	auto key = endpoints.hash() ^ hash_fn_string(term_id);

	bool is_local = endpoints[0].is_local();

	std::unique_lock<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = DatabaseHandler::documents.end();
	if (is_local) {
		it = DatabaseHandler::documents.find(key);
	}

	std::shared_ptr<std::pair<size_t, const MsgPack>> current_document_pair;
	if (it == DatabaseHandler::documents.end()) {
		lk.unlock();

		// Get document from database
		try {
			auto current_document = get_document_term(term_id);
			current_document_pair = std::make_shared<std::pair<size_t, const MsgPack>>(std::make_pair(current_document.hash(), current_document.get_obj()));
		} catch (const DocNotFoundError&) { }

		lk.lock();

		if (is_local) {
			it = DatabaseHandler::documents.emplace(key, current_document_pair).first;
			current_document_pair = it->second;
		}
	} else {
		current_document_pair = it->second;
	}

	return current_document_pair;
}


bool
DatabaseHandler::set_document_change_seq(const std::string& term_id, const std::shared_ptr<std::pair<size_t, const MsgPack>>& new_document_pair, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair)
{
	L_CALL("DatabaseHandler::set_document_change_seq(%s, %s, %s, %s)", endpoints.to_string().c_str(), repr(term_id).c_str(), std::to_string(new_document_pair->first).c_str(), old_document_pair ? std::to_string(old_document_pair->first).c_str() : "nullptr");

	static std::hash<std::string> hash_fn_string;
	auto key = endpoints.hash() ^ hash_fn_string(term_id);

	bool is_local = endpoints[0].is_local();

	std::unique_lock<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = DatabaseHandler::documents.end();
	if (is_local) {
		it = DatabaseHandler::documents.find(key);
	}

	std::shared_ptr<std::pair<size_t, const MsgPack>> current_document_pair;
	if (it == DatabaseHandler::documents.end()) {
		if (old_document_pair) {
			lk.unlock();

			// Get document from database
			try {
				auto current_document = get_document_term(term_id);
				current_document_pair = std::make_shared<std::pair<size_t, const MsgPack>>(std::make_pair(current_document.hash(), current_document.get_obj()));
			} catch (const DocNotFoundError&) { }

			lk.lock();

			if (is_local) {
				it = DatabaseHandler::documents.emplace(key, current_document_pair).first;
				current_document_pair = it->second;
			}
		}
	} else {
		current_document_pair = it->second;
	}

	bool accepted = (!old_document_pair || (current_document_pair && old_document_pair->first == current_document_pair->first));

	current_document_pair.reset();
	old_document_pair.reset();

	if (it != DatabaseHandler::documents.end()) {
		if (it->second.use_count() == 1) {
			DatabaseHandler::documents.erase(it);
		} else if (accepted) {
			it->second = new_document_pair;
		}
	}

	return accepted;
}


void
DatabaseHandler::dec_document_change_cnt(const std::string& term_id)
{
	L_CALL("DatabaseHandler::dec_document_change_cnt(%s, %s)", endpoints.to_string().c_str(), repr(term_id).c_str());

	static std::hash<std::string> hash_fn_string;
	auto key = endpoints.hash() ^ hash_fn_string(term_id);

	bool is_local = endpoints[0].is_local();

	std::lock_guard<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = DatabaseHandler::documents.end();
	if (is_local) {
		it = DatabaseHandler::documents.find(key);
	}

	if (it != DatabaseHandler::documents.end()) {
		if (it->second.use_count() == 1) {
			DatabaseHandler::documents.erase(it);
		}
	}
}
#endif



/*  ____                                        _
 * |  _ \  ___   ___ _   _ _ __ ___   ___ _ __ | |_
 * | | | |/ _ \ / __| | | | '_ ` _ \ / _ \ '_ \| __|
 * | |_| | (_) | (__| |_| | | | | | |  __/ | | | |_
 * |____/ \___/ \___|\__,_|_| |_| |_|\___|_| |_|\__|
 *
 */

Document::Document()
	: _hash(0),
	  db_handler(nullptr) { }


Document::Document(const Xapian::Document& doc_, uint64_t hash_)
	: doc(doc_),
	  _hash(hash_),
	  db_handler(nullptr) { }


Document::Document(DatabaseHandler* db_handler_, const Xapian::Document& doc_, uint64_t hash_)
	: doc(doc_),
	  _hash(hash_),
	  db_handler(db_handler_),
	  database(db_handler->database) { }


Document::Document(const Document& doc_)
	: doc(doc_.doc),
	  _hash(doc_._hash),
	  db_handler(doc_.db_handler),
	  database(doc_.database) { }


Document&
Document::operator=(const Document& doc_)
{
	doc = doc_.doc;
	_hash = doc_._hash;
	db_handler = doc_.db_handler;
	database = doc_.database;
	return *this;
}


void
Document::update()
{
	L_CALL("Document::update()");

	if (db_handler && db_handler->database && database != db_handler->database) {
		doc = db_handler->database->get_document(doc.get_docid());
		_hash = 0;
		database = db_handler->database;
	}
}


Xapian::docid
Document::get_docid()
{
	return doc.get_docid();
}


std::string
Document::serialise(size_t retries)
{
	L_CALL("Document::serialise(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.serialise();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return serialise(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


std::string
Document::get_value(Xapian::valueno slot, size_t retries)
{
	L_CALL("Document::get_value(%u, %zu)", slot, retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.get_value(slot);
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_value(slot, --retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


std::string
Document::get_data(size_t retries)
{
	L_CALL("Document::get_data(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		update();
		return doc.get_data();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return get_data(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


std::string
Document::get_blob(size_t retries)
{
	L_CALL("Document::get_blob(%zu)", retries);

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
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


MsgPack
Document::get_terms(size_t retries)
{
	L_CALL("get_terms(%zu)", retries);

	try {
		MsgPack terms;

		lock_database lk_db(db_handler);
		update();

		// doc.termlist_count() disassociates the database in doc.

		const auto it_e = doc.termlist_end();
		for (auto it = doc.termlist_begin(); it != it_e; ++it) {
			auto& term = terms[*it];
			term[RESPONSE_WDF] = it.get_wdf();  // The within-document-frequency of the current term in the current document.
			try {
				auto _term_freq = it.get_termfreq();  // The number of documents which this term indexes.
				term[RESPONSE_TERM_FREQ] = _term_freq;
			} catch (const Xapian::InvalidOperationError&) { }  // Iterator has moved, and does not support random access or doc is not associated with a database.
			if (it.positionlist_count()) {
				auto& term_pos = term[RESPONSE_POS];
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
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


MsgPack
Document::get_values(size_t retries)
{
	L_CALL("get_values(%zu)", retries);

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
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}


MsgPack
Document::get_value(const std::string& slot_name)
{
	L_CALL("Document::get_value(%s)", slot_name.c_str());

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
	L_CALL("Document::get_store()");

	return split_data_store(get_data());
}


MsgPack
Document::get_obj()
{
	L_CALL("Document::get_obj()");

	return MsgPack::unserialise(split_data_obj(get_data()));
}


MsgPack
Document::get_field(const std::string& slot_name)
{
	L_CALL("Document::get_field(%s)", slot_name.c_str());

	return get_field(slot_name, get_obj());
}


MsgPack
Document::get_field(const std::string& slot_name, const MsgPack& obj)
{
	L_CALL("Document::get_field(%s, <obj>)", slot_name.c_str());

	auto itf = obj.find(slot_name);
	if (itf != obj.end()) {
		const auto& value = itf.value();
		if (value.is_map()) {
			auto itv = value.find(RESERVED_VALUE);
			if (itv != value.end()) {
				return itv.value();
			}
		}
		return value;
	}

	return MsgPack(MsgPack::Type::NIL);
}


uint64_t
Document::hash(size_t retries)
{
	try {
		lock_database lk_db(db_handler);
		update();

		if (_hash == 0) {
			// Add hash of values
			const auto iv_e = doc.values_end();
			for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
				_hash ^= xxh64::hash(*iv) * iv.get_valueno();
			}

			// Add hash of terms
			const auto it_e = doc.termlist_end();
			for (auto it = doc.termlist_begin(); it != it_e; ++it) {
				_hash ^= xxh64::hash(*it) * it.get_wdf();
				const auto pit_e = it.positionlist_end();
				for (auto pit = it.positionlist_begin(); pit != pit_e; ++pit) {
					_hash ^= *pit;
				}
			}

			// Add hash of data
			_hash ^= xxh64::hash(doc.get_data());
		}
		return _hash;
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries) {
			return hash(--retries);
		} else {
			THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description().c_str());
		}
	}
}
