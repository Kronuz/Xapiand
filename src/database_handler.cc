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

#include "database_handler.h"

#include <algorithm>                        // for min, move
#include <array>                            // for std::array
#include <cctype>                           // for tolower
#include <exception>                        // for std::exception
#include <utility>                          // for std::move

#include "cast.h"                           // for Cast
#include "database.h"                       // for Database
#include "database_wal.h"                   // for DatabaseWAL
#include "exception.h"                      // for ClientError
#include "length.h"                         // for serialise_string, unserialise_string
#include "lock_database.h"                  // for lock_database
#include "log.h"                            // for L_CALL
#include "manager.h"                        // for XapiandManager
#include "msgpack.h"                        // for MsgPack
#include "msgpack_patcher.h"                // for apply_patch
#include "multivalue/aggregation.h"         // for AggregationMatchSpy
#include "multivalue/keymaker.h"            // for Multi_MultiValueKeyMaker
#include "opts.h"                           // for opts::
#include "query_dsl.h"                      // for QUERYDSL_QUERY, QueryDSL
#include "rapidjson/document.h"             // for Document
#include "repr.hh"                          // for repr
#include "schema.h"                         // for Schema, required_spc_t
#include "schemas_lru.h"                    // for SchemasLRU
#include "script.h"                         // for Script
#include "serialise.h"                      // for cast, serialise, type

#if defined(XAPIAND_V8)
#include "v8pp/v8pp.h"                      // for v8pp namespace
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include "chaipp/chaipp.h"                  // for chaipp namespace
#endif


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


// Reserved words only used in the responses to the user.
constexpr const char RESPONSE_AV_LENGTH[]           = "#av_length";
constexpr const char RESPONSE_CONTENT_TYPE[]        = "#content_type";
constexpr const char RESPONSE_DATA[]                = "#data";
constexpr const char RESPONSE_DOC_COUNT[]           = "#doc_count";
constexpr const char RESPONSE_DOC_DEL[]             = "#doc_del";
constexpr const char RESPONSE_DOC_LEN_LOWER[]       = "#doc_len_lower";
constexpr const char RESPONSE_DOC_LEN_UPPER[]       = "#doc_len_upper";
constexpr const char RESPONSE_DOCID[]               = "#docid";
constexpr const char RESPONSE_HAS_POSITIONS[]       = "#has_positions";
constexpr const char RESPONSE_LAST_ID[]             = "#last_id";
constexpr const char RESPONSE_OFFSET[]              = "#offset";
constexpr const char RESPONSE_POS[]                 = "#pos";
constexpr const char RESPONSE_RAW_DATA[]            = "#raw_data";
constexpr const char RESPONSE_REVISION[]            = "#revision";
constexpr const char RESPONSE_SIZE[]                = "#size";
constexpr const char RESPONSE_TERM_FREQ[]           = "#term_freq";
constexpr const char RESPONSE_TERMS[]               = "#terms";
constexpr const char RESPONSE_TYPE[]                = "#type";
constexpr const char RESPONSE_UUID[]                = "#uuid";
constexpr const char RESPONSE_VALUES[]              = "#values";
constexpr const char RESPONSE_VOLUME[]              = "#volume";
constexpr const char RESPONSE_WDF[]                 = "#wdf";

constexpr size_t NON_STORED_SIZE_LIMIT = 1024 * 1024;

const std::string dump_metadata_header ("xapiand-dump-meta");
const std::string dump_schema_header("xapiand-dump-schm");
const std::string dump_documents_header("xapiand-dump-docs");


void
committer_commit(std::weak_ptr<Database> weak_database) {
	if (auto database = weak_database.lock()) {
		auto start = std::chrono::system_clock::now();

		std::string error;

		try {
			DatabaseHandler db_handler(database->endpoints, DB_WRITABLE);
			db_handler.commit();
		} catch (const Exception& exc) {
			error = exc.get_message();
		} catch (const Xapian::Error& exc) {
			error = exc.get_description();
		}

		auto end = std::chrono::system_clock::now();

		if (error.empty()) {
			L_DEBUG("Autocommit of %s succeeded after %s", repr(database->endpoints.to_string()), string::from_delta(start, end));
		} else {
			L_WARNING("Autocommit of %s falied after %s: %s", repr(database->endpoints.to_string()), string::from_delta(start, end), error);
		}
	}
}


Xapian::docid
to_docid(std::string_view document_id)
{
	size_t sz = document_id.size();
	if (sz > 2 && document_id[0] == ':' && document_id[1] == ':') {
		std::string_view did_str(document_id.data() + 2, document_id.size() - 2);
		try {
			return static_cast<Xapian::docid>(strict_stol(did_str));
		} catch (const InvalidArgument& er) {
			THROW(ClientError, "Value %s cannot be cast to integer [%s]", repr(did_str), er.what());
		} catch (const OutOfRange& er) {
			THROW(ClientError, "Value %s cannot be cast to integer [%s]", repr(did_str), er.what());
		}
	}
	return static_cast<Xapian::docid>(0);
}


static void
inject_blob(Data& data, const MsgPack& obj)
{
	auto blob_it = obj.find(RESERVED_BLOB);
	if (blob_it == obj.end()) {
		THROW(ClientError, "Data inconsistency, objects in '%s' must contain '%s'", RESERVED_DATA, RESERVED_BLOB);
	}
	auto& blob_value = blob_it.value();
	if (!blob_value.is_string()) {
		THROW(ClientError, "Data inconsistency, '%s' must be a string", RESERVED_BLOB);
	}

	auto content_type_it = obj.find(RESERVED_CONTENT_TYPE);
	if (content_type_it == obj.end()) {
		THROW(ClientError, "Data inconsistency, objects in '%s' must contain '%s'", RESERVED_DATA, RESERVED_CONTENT_TYPE);
	}
	auto& content_type_value = content_type_it.value();
	auto ct_type = ct_type_t(content_type_value.is_string() ? content_type_value.str_view() : "");
	if (ct_type.empty()) {
		THROW(ClientError, "Data inconsistency, '%s' must be a valid content type string", RESERVED_CONTENT_TYPE);
	}

	std::string_view type;
	auto type_it = obj.find(RESERVED_TYPE);
	if (type_it == obj.end()) {
		type = "inplace";
	} else {
		auto& type_value = type_it.value();
		if (!type_value.is_string()) {
			THROW(ClientError, "Data inconsistency, '%s' must be either \"inplace\" or \"stored\"", RESERVED_TYPE);
		}
		type = type_value.str_view();
	}

	if (type == "inplace") {
		auto blob = blob_value.str_view();
		if (blob.size() > NON_STORED_SIZE_LIMIT) {
			THROW(ClientError, "Non-stored object has a size limit of %s", string::from_bytes(NON_STORED_SIZE_LIMIT));
		}
		data.update(ct_type, blob);
	} else if (type == "stored") {
		data.update(ct_type, -1, 0, 0, blob_value.str_view());
	} else {
		THROW(ClientError, "Data inconsistency, '%s' must be either \"inplace\" or \"stored\"", RESERVED_TYPE);
	}
}


static void
inject_data(Data& data, const MsgPack& obj)
{
	auto data_it = obj.find(RESERVED_DATA);
	if (data_it != obj.end()) {
		auto& _data = data_it.value();
		switch (_data.getType()) {
			case MsgPack::Type::STR: {
				auto blob = _data.str_view();
				if (blob.size() > NON_STORED_SIZE_LIMIT) {
					THROW(ClientError, "Non-stored object has a size limit of %s", string::from_bytes(NON_STORED_SIZE_LIMIT));
				}
				data.update("application/octet-stream", blob);
				break;
			}
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED:
				data.erase("application/octet-stream");
				break;
			case MsgPack::Type::MAP:
				inject_blob(data, _data);
				break;
			case MsgPack::Type::ARRAY:
				for (auto& blob : _data) {
					inject_blob(data, blob);
				}
				break;
			default:
				THROW(ClientError, "Data inconsistency, '%s' must be an array or an object", RESERVED_DATA);
		}
	}
}


class FilterPrefixesExpandDecider : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

public:
	FilterPrefixesExpandDecider(std::vector<std::string>  prefixes_)
		: prefixes(std::move(prefixes_)) { }

	bool operator() (const std::string& term) const override {
		for (const auto& prefix : prefixes) {
			if (string::startswith(term, prefix)) {
				return true;
			}
		}

		return prefixes.empty();
	}
};


//  ____        _        _                    _   _                 _ _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| | | | __ _ _ __   __| | | ___ _ __
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_| |/ _` | '_ \ / _` | |/ _ \ '__|
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/  _  | (_| | | | | (_| | |  __/ |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_| |_|\__,_|_| |_|\__,_|_|\___|_|
//

DatabaseHandler::DatabaseHandler()
	: LockableDatabase(),
	  method(HTTP_GET) { }


DatabaseHandler::DatabaseHandler(const Endpoints& endpoints_, int flags_, enum http_method method_, std::shared_ptr<std::unordered_set<std::string>> context_)
	: LockableDatabase(endpoints_, flags_),
	  method(method_),
	  context(std::move(context_)) { }


std::shared_ptr<Schema>
DatabaseHandler::get_schema(const MsgPack* obj)
{
	L_CALL("DatabaseHandler::get_schema(<obj>)");
	auto s = XapiandManager::schemas()->get(this, obj, (obj != nullptr) && ((flags & DB_WRITABLE) != 0));
	return std::make_shared<Schema>(std::move(std::get<0>(s)), std::move(std::get<1>(s)), std::move(std::get<2>(s)));
}


void
DatabaseHandler::reset(const Endpoints& endpoints_, int flags_, enum http_method method_, const std::shared_ptr<std::unordered_set<std::string>>& context_)
{
	L_CALL("DatabaseHandler::reset(%s, %x, <method>)", repr(endpoints_.to_string()), flags_);

	if (endpoints_.empty()) {
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
DatabaseHandler::repr_wal(uint32_t start_revision, uint32_t end_revision, bool unserialised)
{
	L_CALL("DatabaseHandler::repr_wal(%u, %u)", start_revision, end_revision);

	if (endpoints.size() != 1) {
		THROW(ClientError, "It is expected one single endpoint");
	}

	// WAL required on a local writable database, open it.
	DatabaseWAL wal(endpoints[0].path);
	return wal.repr(start_revision, end_revision, unserialised);
}
#endif


MsgPack
DatabaseHandler::check()
{
	L_CALL("DatabaseHandler::check()");

	if (endpoints.size() != 1) {
		THROW(ClientError, "It is expected one single endpoint");
	}

	try {
		return {
			{"errors", Xapian::Database::check(endpoints[0].path)},
		};
	} catch (const Xapian::Error &error) {
		return {
			{"error", error.get_description()},
		};
	} catch (...) {
		return {
			{"error", "Unknown error"},
		};
	}
}


Document
DatabaseHandler::get_document_term(const std::string& term_id)
{
	L_CALL("DatabaseHandler::get_document_term(%s)", repr(term_id));

	lock_database lk_db(this);
	auto did = database()->find_document(term_id);
	return Document(did, this);
}


Document
DatabaseHandler::get_document_term(std::string_view term_id)
{
	return get_document_term(std::string(term_id));
}


#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
std::mutex DatabaseHandler::documents_mtx;
std::unordered_map<std::string, std::shared_ptr<std::pair<std::string, const Data>>> DatabaseHandler::documents;


template<typename Processor>
std::unique_ptr<MsgPack>
DatabaseHandler::call_script(const MsgPack& object, std::string_view term_id, size_t script_hash, size_t body_hash, std::string_view script_body, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair)
{
	try {
		auto processor = Processor::compile(script_hash, body_hash, std::string(script_body));
		switch (method) {
			case HTTP_PUT: {
				auto mut_object = std::make_unique<MsgPack>(object);
				if (old_document_pair == nullptr && !term_id.empty()) {
					old_document_pair = get_document_change_seq(term_id);
				}
				if (old_document_pair != nullptr) {
					auto old_object = old_document_pair->second.get_obj();
					L_INDEX("Script: on_put(%s, %s)", mut_object->to_string(4), old_object.to_string(4));
					*mut_object = (*processor)["on_put"](*mut_object, old_object);
				} else {
					L_INDEX("Script: on_put(%s)", mut_object->to_string(4));
					*mut_object = (*processor)["on_put"](*mut_object, MsgPack(MsgPack::Type::MAP));
				}
				return mut_object;
			}

			case HTTP_PATCH:
			case HTTP_MERGE: {
				auto mut_object = std::make_unique<MsgPack>(object);
				if (old_document_pair == nullptr && !term_id.empty()) {
					old_document_pair = get_document_change_seq(term_id);
				}
				if (old_document_pair != nullptr) {
					auto old_object = old_document_pair->second.get_obj();
					L_INDEX("Script: on_patch(%s, %s)", mut_object->to_string(4), old_object.to_string(4));
					*mut_object = (*processor)["on_patch"](*mut_object, old_object);
				} else {
					L_INDEX("Script: on_patch(%s)", mut_object->to_string(4));
					*mut_object = (*processor)["on_patch"](*mut_object, MsgPack(MsgPack::Type::MAP));
				}
				return mut_object;
			}

			case HTTP_DELETE: {
				auto mut_object = std::make_unique<MsgPack>(object);
				if (old_document_pair == nullptr && !term_id.empty()) {
					old_document_pair = get_document_change_seq(term_id);
				}
				if (old_document_pair != nullptr) {
					auto old_object = old_document_pair->second.get_obj();
					L_INDEX("Script: on_delete(%s, %s)", mut_object->to_string(4), old_object.to_string(4));
					*mut_object = (*processor)["on_delete"](*mut_object, old_object);
				} else {
					L_INDEX("Script: on_delete(%s)", mut_object->to_string(4));
					*mut_object = (*processor)["on_delete"](*mut_object, MsgPack(MsgPack::Type::MAP));
				}
				return mut_object;
			}

			case HTTP_GET: {
				auto mut_object = std::make_unique<MsgPack>(object);
				L_INDEX("Script: on_get(%s)", mut_object->to_string(4));
				*mut_object = (*processor)["on_get"](*mut_object);
				return mut_object;
			}

			case HTTP_POST: {
				auto mut_object = std::make_unique<MsgPack>(object);
				L_INDEX("Script: on_post(%s)", mut_object->to_string(4));
				*mut_object = (*processor)["on_post"](*mut_object);
				return mut_object;
			}

			default:
				break;
		}
		return nullptr;
#if defined(XAPIAND_V8)
	} catch (const v8pp::ReferenceError&) {
		return nullptr;
	} catch (const v8pp::Error& exc) {
		THROW(ClientError, exc.what());
#endif
#if defined(XAPIAND_CHAISCRIPT)
	} catch (const chaipp::ReferenceError&) {
		return nullptr;
	} catch (const chaipp::Error& exc) {
		THROW(ClientError, exc.what());
#endif
	}
}


std::unique_ptr<MsgPack>
DatabaseHandler::run_script(const MsgPack& object, std::string_view term_id, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair, const MsgPack& data_script)
{
	L_CALL("DatabaseHandler::run_script(...)");

	if (data_script.is_map()) {
		const auto& type = data_script.at(RESERVED_TYPE);
		const auto& sep_type = required_spc_t::get_types(type.str_view());
		if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
			THROW(ClientError, "Missing Implementation for Foreign scripts");
		} else {
			auto it_s = data_script.find(RESERVED_CHAI);
			if (it_s == data_script.end()) {
#if defined(XAPIAND_V8)
				const auto& ecma = data_script.at(RESERVED_ECMA);
				return call_script<v8pp::Processor>(object, term_id, ecma.at(RESERVED_HASH).u64(), ecma.at(RESERVED_BODY_HASH).u64(), ecma.at(RESERVED_BODY).str_view(), old_document_pair);
#else
				THROW(ClientError, "Script type 'ecma' (ECMAScript or JavaScript) not available.");
#endif
			} else {
#if defined(XAPIAND_CHAISCRIPT)
				const auto& chai = it_s.value();
				return call_script<chaipp::Processor>(object, term_id, chai.at(RESERVED_HASH).u64(), chai.at(RESERVED_BODY_HASH).u64(), chai.at(RESERVED_BODY).str_view(), old_document_pair);
#else
				THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
			}
		}
	}

	return nullptr;
}
#endif


std::tuple<std::string, Xapian::Document, MsgPack>
DatabaseHandler::prepare(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair)
{
	L_CALL("DatabaseHandler::prepare(%s, %s, <data>)", repr(document_id.to_string()), repr(obj.to_string()));

	std::tuple<std::string, Xapian::Document, MsgPack> prepared;

#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	while (true) {
#endif
		auto schema_begins = std::chrono::system_clock::now();
		do {
			schema = get_schema(&obj);
			L_INDEX("Schema: %s", repr(schema->to_string()));
			prepared = schema->index(obj, document_id, old_document_pair, *this);
		} while (!update_schema(schema_begins));

		auto& term_id = std::get<0>(prepared);
		auto& doc = std::get<1>(prepared);
		auto& data_obj = std::get<2>(prepared);

		// Finish document: add data, ID term and ID value.
		data.set_obj(data_obj);
		data.flush();
		doc.set_data(data.serialise());

#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
		auto current_document_pair = std::make_shared<std::pair<std::string, const Data>>(std::make_pair(term_id, data));
		if (set_document_change_seq(current_document_pair, old_document_pair)) {
			break;
		}
	}
#endif

	return prepared;
}


std::tuple<std::string, Xapian::Document, MsgPack>
DatabaseHandler::prepare(const MsgPack& document_id, bool stored, const MsgPack& body, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::prepare(%s, %s, %s, %s/%s)", repr(document_id.to_string()), stored ? "true" : "false", repr(body.to_string()), ct_type.first, ct_type.second);

	if ((flags & DB_WRITABLE) != DB_WRITABLE) {
		THROW(Error, "Database is read-only");
	}

	std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
	try {
		Data data;
		switch (body.getType()) {
			case MsgPack::Type::STR:
				if (stored) {
					data.update(ct_type, -1, 0, 0, body.str_view());
				} else {
					auto blob = body.str_view();
					if (blob.size() > NON_STORED_SIZE_LIMIT) {
						THROW(ClientError, "Non-stored object has a size limit of %s", string::from_bytes(NON_STORED_SIZE_LIMIT));
					}
					data.update(ct_type, blob);
				}
				return prepare(document_id, MsgPack(MsgPack::Type::MAP), data, old_document_pair);
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED:
				data.erase(ct_type);
				return prepare(document_id, MsgPack(MsgPack::Type::MAP), data, old_document_pair);
			case MsgPack::Type::MAP:
				inject_data(data, body);
				return prepare(document_id, body, data, old_document_pair);
			default:
				THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is %s", body.getStrType());
		}
	} catch (...) {
		if (old_document_pair != nullptr) {
			dec_document_change_cnt(old_document_pair);
		}
		throw;
	}
}


DataType
DatabaseHandler::index(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair, bool commit)
{
	L_CALL("DatabaseHandler::index(%s, %s, <data>, %s)", repr(document_id.to_string()), repr(obj.to_string()), commit ? "true" : "false");

	auto prepared = prepare(document_id, obj, data, old_document_pair);
	auto& term_id = std::get<0>(prepared);
	auto& doc = std::get<1>(prepared);
	auto& data_obj = std::get<2>(prepared);

	lock_database lk_db(this);
	auto did = database()->replace_document_term(term_id, std::move(doc), commit);
	return std::make_pair(std::move(did), std::move(data_obj));
}


DataType
DatabaseHandler::index(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::index(%s, %s, %s, %s, %s/%s)", repr(document_id.to_string()), stored ? "true" : "false", repr(body.to_string()), commit ? "true" : "false", ct_type.first, ct_type.second);

	if ((flags & DB_WRITABLE) != DB_WRITABLE) {
		THROW(Error, "Database is read-only");
	}

	std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
	try {
		Data data;
		switch (body.getType()) {
			case MsgPack::Type::STR:
				if (stored) {
					data.update(ct_type, -1, 0, 0, body.str_view());
				} else {
					auto blob = body.str_view();
					if (blob.size() > NON_STORED_SIZE_LIMIT) {
						THROW(ClientError, "Non-stored object has a size limit of %s", string::from_bytes(NON_STORED_SIZE_LIMIT));
					}
					data.update(ct_type, blob);
				}
				return index(document_id, MsgPack(MsgPack::Type::MAP), data, old_document_pair, commit);
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED:
				data.erase(ct_type);
				return index(document_id, MsgPack(MsgPack::Type::MAP), data, old_document_pair, commit);
			case MsgPack::Type::MAP:
				inject_data(data, body);
				return index(document_id, body, data, old_document_pair, commit);
			default:
				THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is %s", body.getStrType());
		}
	} catch (...) {
		if (old_document_pair != nullptr) {
			dec_document_change_cnt(old_document_pair);
		}
		throw;
	}
}


DataType
DatabaseHandler::patch(const MsgPack& document_id, const MsgPack& patches, bool commit, const ct_type_t& /*ct_type*/)
{
	L_CALL("DatabaseHandler::patch(%s, <patches>, %s)", repr(document_id.to_string()), commit ? "true" : "false");

	if ((flags & DB_WRITABLE) != DB_WRITABLE) {
		THROW(Error, "database is read-only");
	}

	if (!document_id) {
		THROW(ClientError, "Document must have an 'id'");
	}

	if (!patches.is_map() && !patches.is_array()) {
		THROW(ClientError, "Patches must be a JSON or MsgPack");
	}

	const auto term_id = get_prefixed_term_id(document_id);

	std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
	try {
		Data data;
		if (old_document_pair == nullptr && !term_id.empty()) {
			old_document_pair = get_document_change_seq(term_id, true);
		}
		if (old_document_pair != nullptr) {
			data = old_document_pair->second;
		}
		auto obj = data.get_obj();

		apply_patch(patches, obj);

		return index(document_id, obj, data, old_document_pair, commit);
	} catch (...) {
		if (old_document_pair != nullptr) {
			dec_document_change_cnt(old_document_pair);
		}
		throw;
	}
}


DataType
DatabaseHandler::merge(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::merge(%s, %s, <body>, %s, %s/%s)", repr(document_id.to_string()), stored ? "true" : "false", commit ? "true" : "false", ct_type.first, ct_type.second);

	if ((flags & DB_WRITABLE) != DB_WRITABLE) {
		THROW(Error, "database is read-only");
	}

	if (!document_id) {
		THROW(ClientError, "Document must have an 'id'");
	}

	const auto term_id = get_prefixed_term_id(document_id);

	std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
	try {
		Data data;
		if (old_document_pair == nullptr && !term_id.empty()) {
			old_document_pair = get_document_change_seq(term_id, !stored);
		}
		if (old_document_pair != nullptr) {
			data = old_document_pair->second;
		}
		auto obj = data.get_obj();

		switch (body.getType()) {
			case MsgPack::Type::STR:
				if (stored) {
					data.update(ct_type, -1, 0, 0, body.str_view());
				} else {
					auto blob = body.str_view();
					if (blob.size() > NON_STORED_SIZE_LIMIT) {
						THROW(ClientError, "Non-stored object has a size limit of %s", string::from_bytes(NON_STORED_SIZE_LIMIT));
					}
					data.update(ct_type, blob);
				}
				return index(document_id, obj, data, old_document_pair, commit);
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED:
				data.erase(ct_type);
				return index(document_id, obj, data, old_document_pair, commit);
			case MsgPack::Type::MAP:
				if (stored) {
					THROW(ClientError, "Objects of this type cannot be put in storage");
				}
				if (obj.empty()) {
					inject_data(data, body);
					return index(document_id, body, data, old_document_pair, commit);
				} else {
					obj.update(body);
					inject_data(data, obj);
					return index(document_id, obj, data, old_document_pair, commit);
				}
			default:
				THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is %s", body.getStrType());
		}

		return index(document_id, obj, data, old_document_pair, commit);
	} catch (...) {
		if (old_document_pair != nullptr) {
			dec_document_change_cnt(old_document_pair);
		}
		throw;
	}
}


void
DatabaseHandler::write_schema(const MsgPack& obj, bool replace)
{
	L_CALL("DatabaseHandler::write_schema(%s)", repr(obj.to_string()));

	auto schema_begins = std::chrono::system_clock::now();
	bool was_foreign_obj;
	do {
		schema = get_schema();
		was_foreign_obj = schema->write(obj, replace);
		if (!was_foreign_obj && opts.foreign) {
			THROW(ForeignSchemaError, "Schema of %s must use a foreign schema", repr(endpoints.to_string()));
		}
		L_INDEX("Schema to write: %s %s", repr(schema->to_string()), was_foreign_obj ? "(foreign)" : "(local)");
	} while (!update_schema(schema_begins));

	if (was_foreign_obj) {
		MsgPack o = obj;
		o[RESERVED_TYPE] = "object";
		o.erase(RESERVED_ENDPOINT);
		do {
			schema = get_schema();
			was_foreign_obj = schema->write(o, replace);
			L_INDEX("Schema to write: %s (local)", repr(schema->to_string()));
		} while (!update_schema(schema_begins));
	}
}


void
DatabaseHandler::delete_schema()
{
	L_CALL("DatabaseHandler::delete_schema()");

	auto schema_begins = std::chrono::system_clock::now();
	bool done;
	do {
		schema = get_schema();
		auto old_schema = schema->get_const_schema();
		done = XapiandManager::schemas()->drop(this, old_schema);
		L_INDEX("Schema to delete: %s", repr(schema->to_string()));
	} while (!done);
	auto schema_ends = std::chrono::system_clock::now();
	(void)schema_begins;
	(void)schema_ends;
	// Stats::add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
}


Xapian::RSet
DatabaseHandler::get_rset(const Xapian::Query& query, Xapian::doccount maxitems)
{
	L_CALL("DatabaseHandler::get_rset(...)");

	lock_database lk_db(this);

	// Xapian::RSet only keeps a set of Xapian::docid internally,
	// so it's thread safe across database checkouts.

	Xapian::RSet rset;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			Xapian::Enquire enquire(*db());
			enquire.set_query(query);
			auto mset = enquire.get_mset(0, maxitems);
			for (const auto& did : mset) {
				rset.add_document(did);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		database()->reopen();
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

	XXH32_state_t* xxh_state = XXH32_createState();
	XXH32_reset(xxh_state, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_metadata_header);
	XXH32_update(xxh_state, dump_metadata_header.data(), dump_metadata_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(xxh_state, db_endpoints.data(), db_endpoints.size());

	database()->dump_metadata(fd, xxh_state);

	uint32_t current_hash = XXH32_digest(xxh_state);
	XXH32_freeState(xxh_state);

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

	XXH32_state_t* xxh_state = XXH32_createState();
	XXH32_reset(xxh_state, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_schema_header);
	XXH32_update(xxh_state, dump_schema_header.data(), dump_schema_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(xxh_state, db_endpoints.data(), db_endpoints.size());

	serialise_string(fd, saved_schema_ser);
	XXH32_update(xxh_state, saved_schema_ser.data(), saved_schema_ser.size());

	uint32_t current_hash = XXH32_digest(xxh_state);
	XXH32_freeState(xxh_state);

	serialise_length(fd, current_hash);
	L_INFO("Dump hash is 0x%08x", current_hash);
}


void
DatabaseHandler::dump_documents(int fd)
{
	L_CALL("DatabaseHandler::dump_documents()");

	lock_database lk_db(this);

	XXH32_state_t* xxh_state = XXH32_createState();
	XXH32_reset(xxh_state, 0);

	auto db_endpoints = endpoints.to_string();
	serialise_string(fd, dump_documents_header);
	XXH32_update(xxh_state, dump_documents_header.data(), dump_documents_header.size());

	serialise_string(fd, db_endpoints);
	XXH32_update(xxh_state, db_endpoints.data(), db_endpoints.size());

	database()->dump_documents(fd, xxh_state);

	uint32_t current_hash = XXH32_digest(xxh_state);
	XXH32_freeState(xxh_state);

	serialise_length(fd, current_hash);
	L_INFO("Dump hash is 0x%08x", current_hash);
}


void
DatabaseHandler::restore(int fd)
{
	L_CALL("DatabaseHandler::restore()");

	std::string buffer;
	std::size_t off = 0;
	std::size_t acc = 0;

	lock_database lk_db(this);

	XXH32_state_t* xxh_state = XXH32_createState();
	XXH32_reset(xxh_state, 0);

	auto header = unserialise_string(fd, buffer, off, acc);
	XXH32_update(xxh_state, header.data(), header.size());
	if (header != dump_documents_header && header != dump_schema_header && header != dump_metadata_header) {
		THROW(ClientError, "Invalid dump", RESERVED_TYPE);
	}

	auto db_endpoints = unserialise_string(fd, buffer, off, acc);
	XXH32_update(xxh_state, db_endpoints.data(), db_endpoints.size());

	// restore metadata (key, value)
	if (header == dump_metadata_header) {
		size_t i = 0;
		while (true) {
			++i;
			auto key = unserialise_string(fd, buffer, off, acc);
			XXH32_update(xxh_state, key.data(), key.size());
			auto value = unserialise_string(fd, buffer, off, acc);
			XXH32_update(xxh_state, value.data(), value.size());
			if (key.empty() && value.empty()) {
				break;
			}
			if (key.empty()) {
				L_WARNING("Metadata with no key ignored [%zu]", ID_FIELD_NAME, i);
				continue;
			}
			L_INFO_HOOK("DatabaseHandler::restore", "Restoring metadata %s = %s", key, value);
			database()->set_metadata(key, value, false, false);
		}
	}

	// restore schema
	if (header == dump_schema_header) {
		auto saved_schema_ser = unserialise_string(fd, buffer, off, acc);
		XXH32_update(xxh_state, saved_schema_ser.data(), saved_schema_ser.size());

		lk_db.unlock();
		if (!saved_schema_ser.empty()) {
			auto saved_schema = MsgPack::unserialise(saved_schema_ser);
			L_INFO_HOOK("DatabaseHandler::restore", "Restoring schema: %s", saved_schema.to_string(4));
			write_schema(saved_schema, true);
		}
		schema = get_schema();
		lk_db.lock();
	}

	// restore documents (document_id, object, blob)
	if (header == dump_documents_header) {
		lk_db.unlock();
		schema = get_schema();

		constexpr size_t limit_max = 16;
		constexpr size_t limit_signal = 8;
		LightweightSemaphore limit(limit_max);
		BlockingConcurrentQueue<std::tuple<std::string, Xapian::Document, MsgPack>> queue;
		std::atomic_bool ready = false;
		std::atomic_size_t accumulated = 0;
		std::atomic_size_t processed = 0;
		std::size_t total = 0;

		ThreadPool<> thread_pool("TP%02zu", 4 * std::thread::hardware_concurrency());

		// Index documents.
		auto indexer = thread_pool.async([&]{
			DatabaseHandler db_handler(endpoints, flags, method);
			lock_database lk_db(&db_handler);
			lk_db.unlock();
			bool _ready = false;
			while (true) {
				if (XapiandManager::manager()->is_detaching()) {
					thread_pool.finish();
					break;
				}
				std::tuple<std::string, Xapian::Document, MsgPack> prepared;
				queue.wait_dequeue(prepared);
				if (!_ready) {
					_ready = ready.load(std::memory_order_relaxed);
				}
				auto _processed = processed.fetch_add(1, std::memory_order_acquire) + 1;

				auto& term_id = std::get<0>(prepared);
				auto& doc = std::get<1>(prepared);

				if (!term_id.empty()) {
					lk_db.lock();
					try {
						db_handler.database()->replace_document_term(term_id, std::move(doc), false, false);
					} catch (...) {
						L_EXC("ERROR: Cannot replace document");
					}
					lk_db.unlock();
				}

				if (_ready) {
					if (_processed >= total) {
						queue.enqueue(std::make_tuple(std::string{}, Xapian::Document{}, MsgPack{}));
						break;
					}
					if (_processed % (1024 * 16) == 0) {
						L_INFO("%zu of %zu documents processed (%s)", _processed, total, string::from_bytes(accumulated.load(std::memory_order_relaxed)));
					}
				} else {
					if (_processed % (limit_signal * 32) == 0) {
						limit.signal(limit_signal);
					}
					if (_processed % (1024 * 16) == 0) {
						L_INFO("%zu documents processed (%s)", _processed, string::from_bytes(accumulated.load(std::memory_order_relaxed)));
					}
				}
			}
		});

		std::array<std::function<void()>, ConcurrentQueueDefaultTraits::BLOCK_SIZE> bulk;
		size_t bulk_cnt = 0;
		while (true) {
			if (XapiandManager::manager()->is_detaching()) {
				thread_pool.finish();
				break;
			}
			MsgPack obj(MsgPack::Type::MAP);
			Data data;
			try {
				bool eof = true;
				while (true) {
					auto blob = unserialise_string(fd, buffer, off, acc);
					XXH32_update(xxh_state, blob.data(), blob.size());
					if (blob.empty()) { break; }
					auto content_type = unserialise_string(fd, buffer, off, acc);
					XXH32_update(xxh_state, content_type.data(), content_type.size());
					auto type_ser = unserialise_char(fd, buffer, off, acc);
					XXH32_update(xxh_state, &type_ser, 1);
					if (content_type.empty()) {
						obj = MsgPack::unserialise(blob);
					} else {
						switch (static_cast<Locator::Type>(type_ser)) {
							case Locator::Type::inplace:
							case Locator::Type::compressed_inplace:
								data.update(content_type, blob);
								break;
							case Locator::Type::stored:
							case Locator::Type::compressed_stored:
								data.update(content_type, -1, 0, 0, blob);
								break;
						}
					}
					eof = false;
				}
				if (eof) { break; }
				accumulated.store(acc, std::memory_order_relaxed);
			} catch (...) {
				L_EXC("ERROR: Cannot replace document");
				break;
			}

			MsgPack document_id;

			auto f_it = obj.find(ID_FIELD_NAME);
			if (f_it != obj.end()) {
				const auto& field = f_it.value();
				if (field.is_map()) {
					auto f_it_end = field.end();
					auto fv_it = field.find(RESERVED_VALUE);
					if (fv_it != f_it_end) {
						document_id = fv_it.value();
					}
				} else {
					document_id = field;
				}
			}

			if (!document_id) {
				L_WARNING("Skipping document with no valid '%s'", ID_FIELD_NAME);
				continue;
			}

			++total;

			bulk[bulk_cnt++] = [
				&,
				document_id = std::move(document_id),
				obj = std::move(obj),
				data = std::move(data)
			]() mutable {
				try {
					DatabaseHandler db_handler(endpoints, flags, method);
					std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
					queue.enqueue(db_handler.prepare(document_id, obj, data, old_document_pair));
				} catch (...) {
					L_EXC("ERROR: Cannot prepare document");
					queue.enqueue(std::make_tuple(std::string{}, Xapian::Document{}, MsgPack{}));
				}
			};
			if (bulk_cnt == bulk.size()) {
				if (!thread_pool.enqueue_bulk(bulk.begin(), bulk_cnt)) {
					L_ERR("Ignored %zu documents: cannot enqueue tasks!", bulk_cnt);
				}
				bulk_cnt = 0;
				limit.wait();
			}
		}

		if (bulk_cnt != 0) {
			if (!thread_pool.enqueue_bulk(bulk.begin(), bulk_cnt)) {
				L_ERR("Ignored %zu documents: cannot enqueue tasks!", bulk_cnt);
			}
		}

		ready.store(true, std::memory_order_release);

		indexer.wait();

		L_INFO("%zu of %zu documents processed (%s)", processed.load(std::memory_order_relaxed), total, string::from_bytes(acc));

		lk_db.lock();
	}

	database()->commit(false);

	uint32_t saved_hash = unserialise_length(fd, buffer, off, acc);
	uint32_t current_hash = XXH32_digest(xxh_state);
	XXH32_freeState(xxh_state);

	if (saved_hash != current_hash) {
		L_WARNING("Invalid dump hash. Should be 0x%08x, but instead is 0x%08x", saved_hash, current_hash);
	}
}


MsgPack
DatabaseHandler::dump_documents()
{
	L_CALL("DatabaseHandler::dump_documents()");

	lock_database lk_db(this);

	return database()->dump_documents();
}



std::tuple<std::string, Xapian::Document, MsgPack>
DatabaseHandler::prepare_document(const MsgPack& obj)
{
	L_CALL("DatabaseHandler::prepare_document(<obj>)");

	MsgPack document_id;

	auto f_it = obj.find(ID_FIELD_NAME);
	if (f_it != obj.end()) {
		const auto& field = f_it.value();
		if (field.is_map()) {
			auto f_it_end = field.end();
			auto fv_it = field.find(RESERVED_VALUE);
			if (fv_it != f_it_end) {
				document_id = fv_it.value();
			}
		} else {
			document_id = field;
		}
	}

	Data data;
	inject_data(data, obj);

	std::shared_ptr<std::pair<std::string, const Data>> old_document_pair;
	return prepare(document_id, obj, data, old_document_pair);
}


MSet
DatabaseHandler::get_all_mset(Xapian::docid initial, size_t limit)
{
	L_CALL("DatabaseHandler::get_all_mset()");

	MSet mset{};

	lock_database lk_db(this);

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto it = db()->postlist_begin("");
			auto it_e = db()->postlist_end("");
			if (initial) {
				it.skip_to(initial);
			}
			for (; it != it_e && limit; ++it, --limit) {
				initial = *it;
				mset.push_back(initial);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const QueryParserError& exc) {
			THROW(ClientError, exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			THROW(ClientError, exc.get_description());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		} catch (const std::exception& exc) {
			THROW(ClientError, "The search was not performed: %s", exc.what());
		}
		database()->reopen();
	}

	return mset;
}


MSet
DatabaseHandler::get_mset(const query_field_t& query_field, const MsgPack* qdsl, AggregationMatchSpy* aggs)
{
	L_CALL("DatabaseHandler::get_mset(%s, %s)", repr(string::join(query_field.query, " & ")), qdsl ? repr(qdsl->to_string()) : "null");

	schema = get_schema();

	auto limit = query_field.limit;
	auto check_at_least = query_field.check_at_least;
	auto offset = query_field.offset;

	Xapian::Query query;
	std::unique_ptr<Multi_MultiValueKeyMaker> sorter;
	switch (method) {
		case HTTP_GET:
		case HTTP_POST: {
			QueryDSL query_object(schema);

			if (qdsl && qdsl->find(QUERYDSL_SORT) != qdsl->end()) {
				auto value = qdsl->at(QUERYDSL_SORT);
				query_object.get_sorter(sorter, value);
			}

			if (qdsl && qdsl->find(QUERYDSL_QUERY) != qdsl->end()) {
				query = query_object.get_query(qdsl->at(QUERYDSL_QUERY));
			} else {
				query = query_object.get_query(query_object.make_dsl_query(query_field));
			}

			if (qdsl && qdsl->find(QUERYDSL_OFFSET) != qdsl->end()) {
				auto value = qdsl->at(QUERYDSL_OFFSET);
				if (value.is_integer()) {
					offset = value.as_u64();
				} else {
					THROW(ClientError, "The %s must be a unsigned int", QUERYDSL_OFFSET);
				}
			}

			if (qdsl && qdsl->find(QUERYDSL_LIMIT) != qdsl->end()) {
				auto value = qdsl->at(QUERYDSL_LIMIT);
				if (value.is_integer()) {
					limit = value.as_u64();
				} else {
					THROW(ClientError, "The %s must be a unsigned int", QUERYDSL_LIMIT);
				}
			}

			if (qdsl && qdsl->find(QUERYDSL_CHECK_AT_LEAST) != qdsl->end()) {
				auto value = qdsl->at(QUERYDSL_CHECK_AT_LEAST);
				if (value.is_integer()) {
					check_at_least = value.as_u64();
				} else {
					THROW(ClientError, "The %s must be a unsigned int", QUERYDSL_CHECK_AT_LEAST);
				}
			}

			break;
		}

		default:
			break;
	}

	// L_DEBUG("query: %s", query.get_description());

	// Configure sorter.
	if (!sorter && !query_field.sort.empty()) {
		sorter = std::make_unique<Multi_MultiValueKeyMaker>();
		std::string field, value;
		for (const auto& sort : query_field.sort) {
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
					/* FALLTHROUGH */
				case '+':
					field.erase(field.begin());
					break;
			}
			const auto field_spc = schema->get_slot_field(field);
			if (field_spc.get_type() != FieldType::EMPTY) {
				sorter->add_value(field_spc, descending, value, query_field);
			}
		}
	}

	// Get the collapse key to use for queries.
	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (!query_field.collapse.empty()) {
		const auto field_spc = schema->get_slot_field(query_field.collapse);
		collapse_key = field_spc.slot;
	}

	// Configure nearest and fuzzy search:
	std::unique_ptr<Xapian::ExpandDecider> nearest_edecider;
	Xapian::RSet nearest_rset;
	if (query_field.is_nearest) {
		nearest_edecider = get_edecider(query_field.nearest);
		nearest_rset = get_rset(query, query_field.nearest.n_rset);
	}

	Xapian::RSet fuzzy_rset;
	std::unique_ptr<Xapian::ExpandDecider> fuzzy_edecider;
	if (query_field.is_fuzzy) {
		fuzzy_edecider = get_edecider(query_field.fuzzy);
		fuzzy_rset = get_rset(query, query_field.fuzzy.n_rset);
	}

	MSet mset{};

	lock_database lk_db(this);
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto final_query = query;
			Xapian::Enquire enquire(*db());
			if (collapse_key != Xapian::BAD_VALUENO) {
				enquire.set_collapse_key(collapse_key, query_field.collapse_max);
			}
			if (aggs != nullptr) {
				enquire.add_matchspy(aggs);
			}
			if (sorter) {
				enquire.set_sort_by_key_then_relevance(sorter.get(), false);
			}
			if (query_field.is_nearest) {
				auto eset = enquire.get_eset(query_field.nearest.n_eset, nearest_rset, nearest_edecider.get());
				final_query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), query_field.nearest.n_term);
			}
			if (query_field.is_fuzzy) {
				auto eset = enquire.get_eset(query_field.fuzzy.n_eset, fuzzy_rset, fuzzy_edecider.get());
				final_query = Xapian::Query(Xapian::Query::OP_OR, final_query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), query_field.fuzzy.n_term));
			}
			enquire.set_query(final_query);
			mset = enquire.get_mset(offset, limit, check_at_least);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const QueryParserError& exc) {
			THROW(ClientError, exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			THROW(ClientError, exc.get_description());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		} catch (const std::exception& exc) {
			THROW(ClientError, "The search was not performed: %s", exc.what());
		}
		database()->reopen();
	}

	return mset;
}


bool
DatabaseHandler::update_schema(std::chrono::time_point<std::chrono::system_clock> schema_begins)
{
	L_CALL("DatabaseHandler::update_schema()");
	bool done = true;
	bool updated = false;
	bool created = false;

	auto mod_schema = schema->get_modified_schema();
	if (mod_schema) {
		updated = true;
		auto old_schema = schema->get_const_schema();
		done = XapiandManager::schemas()->set(this, old_schema, mod_schema);
		if (done) {
			created = old_schema->at("schema").empty();
		}
	}

	if (done) {
		auto schema_ends = std::chrono::system_clock::now();
		(void)schema_begins;
		(void)schema_ends;
		if (updated) {
			L_DEBUG("Schema for %s %s", repr(endpoints.to_string()), created ? "created" : "updated");
			// Stats::add("schema_updates", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
		} else {
			// Stats::add("schema_reads", std::chrono::duration_cast<std::chrono::nanoseconds>(schema_ends - schema_begins).count());
		}
	}

	return done;
}


std::string
DatabaseHandler::get_prefixed_term_id(const MsgPack& document_id)
{
	L_CALL("DatabaseHandler::get_prefixed_term_id(%s)", repr(document_id.to_string()));

	schema = get_schema();

	std::string unprefixed_term_id;
	auto spc_id = schema->get_data_id();
	auto id_type = spc_id.get_type();
	if (id_type == FieldType::EMPTY) {
		// Search like namespace.
		const auto type_ser = Serialise::guess_serialise(document_id);
		id_type = type_ser.first;
		if (id_type == FieldType::TEXT || id_type == FieldType::STRING) {
			id_type = FieldType::KEYWORD;
		}
		spc_id.set_type(id_type);
		unprefixed_term_id = type_ser.second;
	} else {
		unprefixed_term_id = Serialise::serialise(spc_id, Cast::cast(id_type, document_id));
	}
	return prefixed(unprefixed_term_id, spc_id.prefix(), spc_id.get_ctype());
}


std::vector<std::string>
DatabaseHandler::get_metadata_keys()
{
	L_CALL("DatabaseHandler::get_metadata_keys()");

	lock_database lk_db(this);
	return database()->get_metadata_keys();
}


std::string
DatabaseHandler::get_metadata(const std::string& key)
{
	L_CALL("DatabaseHandler::get_metadata(%s)", repr(key));

	lock_database lk_db(this);
	return database()->get_metadata(key);
}


std::string
DatabaseHandler::get_metadata(std::string_view key)
{
	return get_metadata(std::string(key));
}


bool
DatabaseHandler::set_metadata(const std::string& key, const std::string& value, bool commit, bool overwrite)
{
	L_CALL("DatabaseHandler::set_metadata(%s, %s, %s, %s)", repr(key), repr(value), commit ? "true" : "false", overwrite ? "true" : "false");

	lock_database lk_db(this);
	if (!overwrite) {
		auto old_value = database()->get_metadata(key);
		if (!old_value.empty()) {
			return (old_value == value);
		}
	}
	database()->set_metadata(key, value, commit);
	return true;
}


bool
DatabaseHandler::set_metadata(std::string_view key, std::string_view value, bool commit, bool overwrite)
{
	return set_metadata(std::string(key), std::string(value), commit, overwrite);
}


Document
DatabaseHandler::get_document(Xapian::docid did)
{
	L_CALL("DatabaseHandler::get_document((Xapian::docid)%d)", did);

	lock_database lk_db(this);
	return Document(did, this);
}


Document
DatabaseHandler::get_document(std::string_view document_id)
{
	L_CALL("DatabaseHandler::get_document((std::string)%s)", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		return get_document(did);
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	did = database()->find_document(term_id);
	return Document(did, this);
}


Xapian::docid
DatabaseHandler::get_docid(std::string_view document_id)
{
	L_CALL("DatabaseHandler::get_docid(%s)", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		return did;
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	return database()->find_document(term_id);
}


void
DatabaseHandler::delete_document(std::string_view document_id, bool commit)
{
	L_CALL("DatabaseHandler::delete_document(%s)", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		database()->delete_document(did, commit);
		return;
	}

	const auto term_id = get_prefixed_term_id(document_id);

	lock_database lk_db(this);
	database()->delete_document(database()->find_document(term_id), commit);
}


Xapian::docid
DatabaseHandler::replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit)
{
	L_CALL("Database::replace_document(%d, <doc>)", did);

	lock_database lk_db(this);
	return database()->replace_document(did, std::move(doc), commit);
}


MsgPack
DatabaseHandler::get_document_info(std::string_view document_id, bool raw_data)
{
	L_CALL("DatabaseHandler::get_document_info(%s, %s)", repr(document_id), raw_data ? "true" : "false");

	auto document = get_document(document_id);
	const auto data = Data(document.get_data());

	MsgPack info;

	info[RESPONSE_DOCID] = document.get_docid();

	if (raw_data) {
		info[RESPONSE_RAW_DATA] = data.serialise();
	}

	auto& info_data = info[RESPONSE_DATA];
	if (!data.empty()) {
		for (auto& locator : data) {
			switch (locator.type) {
				case Locator::Type::inplace:
				case Locator::Type::compressed_inplace:
					if (locator.ct_type.empty()) {
						info_data.push_back(MsgPack({
							{ RESPONSE_CONTENT_TYPE, MSGPACK_CONTENT_TYPE },
							{ RESPONSE_TYPE, "inplace" },
						}));
					} else {
						info_data.push_back(MsgPack({
							{ RESPONSE_CONTENT_TYPE, locator.ct_type.to_string() },
							{ RESPONSE_TYPE, "inplace" },
						}));
					}
					break;
				case Locator::Type::stored:
				case Locator::Type::compressed_stored:
					info_data.push_back(MsgPack({
						{ RESPONSE_CONTENT_TYPE, locator.ct_type.to_string() },
						{ RESPONSE_TYPE, "stored" },
						{ RESPONSE_VOLUME, locator.volume },
						{ RESPONSE_OFFSET, locator.offset },
						{ RESPONSE_SIZE, locator.size },
					}));
					break;
			}
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
	auto doccount = db()->get_doccount();
	auto lastdocid = db()->get_lastdocid();
	MsgPack info;
	info[RESPONSE_UUID] = db()->get_uuid();
	info[RESPONSE_REVISION] = db()->get_revision();
	info[RESPONSE_DOC_COUNT] = doccount;
	info[RESPONSE_LAST_ID] = lastdocid;
	info[RESPONSE_DOC_DEL] = lastdocid - doccount;
	info[RESPONSE_AV_LENGTH] = db()->get_avlength();
	info[RESPONSE_DOC_LEN_LOWER] =  db()->get_doclength_lower_bound();
	info[RESPONSE_DOC_LEN_UPPER] = db()->get_doclength_upper_bound();
	info[RESPONSE_HAS_POSITIONS] = db()->has_positions();
	return info;
}


#ifdef XAPIAND_DATA_STORAGE
std::string
DatabaseHandler::storage_get_stored(const Locator& locator, Xapian::docid did)
{
	L_CALL("DatabaseHandler::storage_get_stored()");

	lock_database lk_db(this);
	return database()->storage_get_stored(locator, did);
}
#endif /* XAPIAND_DATA_STORAGE */


bool
DatabaseHandler::commit(bool wal)
{
	L_CALL("DatabaseHandler::commit(%s)", wal ? "true" : "false");

	lock_database lk_db(this);
	return database()->commit(wal);
}


bool
DatabaseHandler::reopen()
{
	L_CALL("DatabaseHandler::reopen()");

	lock_database lk_db(this);
	return database()->reopen();
}


#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
const std::shared_ptr<std::pair<std::string, const Data>>
DatabaseHandler::get_document_change_seq(std::string_view term_id, bool validate_exists)
{
	L_CALL("DatabaseHandler::get_document_change_seq(%s, %s)", endpoints.to_string(), repr(term_id));

	ASSERT(endpoints.size() == 1);
	auto key = endpoints[0].path + std::string(term_id);
	bool is_local = endpoints[0].is_local();

	std::unique_lock<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = is_local ? DatabaseHandler::documents.find(key) : DatabaseHandler::documents.end();

	std::shared_ptr<std::pair<std::string, const Data>> current_document_pair;
	if (it == DatabaseHandler::documents.end()) {
		lk.unlock();

		// Get document from database
		try {
			auto current_document = get_document_term(term_id);
			current_document_pair = std::make_shared<std::pair<std::string, const Data>>(std::make_pair(term_id, Data(current_document.get_data())));
		} catch (const NotFoundError&) {
			if (validate_exists) {
				throw;
			}
		}

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
DatabaseHandler::set_document_change_seq(const std::shared_ptr<std::pair<std::string, const Data>>& new_document_pair, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair)
{
	L_CALL("DatabaseHandler::set_document_change_seq(%s, %s)", repr(new_document_pair->first), old_document_pair ? repr(old_document_pair->first) : "nullptr");

	ASSERT(endpoints.size() == 1);
	auto key = endpoints[0].path + new_document_pair->first;
	bool is_local = endpoints[0].is_local();

	std::unique_lock<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = is_local ? DatabaseHandler::documents.find(key) : DatabaseHandler::documents.end();

	std::shared_ptr<std::pair<std::string, const Data>> current_document_pair;
	if (it == DatabaseHandler::documents.end()) {
		if (old_document_pair != nullptr) {
			lk.unlock();

			// Get document from database
			try {
				auto current_document = get_document_term(new_document_pair->first);
				current_document_pair = std::make_shared<std::pair<std::string, const Data>>(std::make_pair(new_document_pair->first, Data(current_document.get_data())));
			} catch (const NotFoundError&) { }

			lk.lock();

			if (is_local) {
				it = DatabaseHandler::documents.emplace(key, current_document_pair).first;
				current_document_pair = it->second;
			}
		}
	} else {
		current_document_pair = it->second;
	}

	bool accepted = (old_document_pair == nullptr || (current_document_pair && old_document_pair->second == current_document_pair->second));

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
DatabaseHandler::dec_document_change_cnt(std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair)
{
	L_CALL("DatabaseHandler::dec_document_change_cnt(%s)", endpoints.to_string(), repr(old_document_pair->first));

	ASSERT(endpoints.size() == 1);
	auto key = endpoints[0].path + old_document_pair->first;
	bool is_local = endpoints[0].is_local();

	std::lock_guard<std::mutex> lk(DatabaseHandler::documents_mtx);

	auto it = DatabaseHandler::documents.end();
	if (is_local) {
		it = DatabaseHandler::documents.find(key);
	}

	old_document_pair.reset();

	if (it != DatabaseHandler::documents.end()) {
		if (it->second.use_count() == 1) {
			DatabaseHandler::documents.erase(it);
		}
	}
}
#endif


//  ____             ___           _
// |  _ \  ___   ___|_ _|_ __   __| | _____  _____ _ __
// | | | |/ _ \ / __|| || '_ \ / _` |/ _ \ \/ / _ \ '__|
// | |_| | (_) | (__ | || | | | (_| |  __/>  <  __/ |
// |____/ \___/ \___|___|_| |_|\__,_|\___/_/\_\___|_|

void
DocPreparer::operator()()
{
	L_CALL("DocPreparer::operator()()");

	ASSERT(indexer);
	if (indexer->running) {
		try {
			DatabaseHandler db_handler(indexer->endpoints, indexer->flags, indexer->method);
			switch (obj.getType()) {
				case MsgPack::Type::MAP:
					indexer->ready_queue.enqueue(db_handler.prepare_document(obj));
					break;
				case MsgPack::Type::ARRAY:
					for (auto& o : obj) {
						indexer->ready_queue.enqueue(db_handler.prepare_document(o));
					}
					break;
				default:
					L_ERR("Indexing object has an unsupported type: %s", obj.getStrType());
					break;
			}
		} catch (...) {
			L_EXC("ERROR: Cannot prepare document");
			indexer->ready_queue.enqueue(std::make_tuple(std::string{}, Xapian::Document{}, MsgPack{}));
		}
	}
}


void
DocIndexer::operator()()
{
	L_CALL("DocIndexer::operator()()");

	DatabaseHandler db_handler(endpoints, flags, method);
	bool _ready = false;
	while (running) {
		std::tuple<std::string, Xapian::Document, MsgPack> prepared;
		ready_queue.wait_dequeue(prepared);
		if (!_ready) {
			_ready = ready.load(std::memory_order_relaxed);
		}
		auto _processed = processed.fetch_add(1, std::memory_order_acquire) + 1;

		auto& term_id = std::get<0>(prepared);
		auto& doc = std::get<1>(prepared);

		if (!term_id.empty()) {
			try {
				lock_database lk_db(&db_handler);
				db_handler.database()->replace_document_term(term_id, std::move(doc), false, false);
			} catch (...) {
				L_EXC("ERROR: Cannot replace document");
			}
		}

		if (_ready) {
			if (_processed >= total) {
				done.signal();
				break;
			}
		} else {
			if (_processed % (limit_signal * 32) == 0) {
				limit.signal(limit_signal);
			}
		}
	}
}


void
DocIndexer::prepare(MsgPack&& obj)
{
	L_CALL("DocIndexer::prepare(<obj>)");

	bulk[bulk_cnt++] = DocPreparer::make_unique(shared_from_this(), std::move(obj));
	if (bulk_cnt == bulk.size()) {
		total += bulk_cnt;
		if (!XapiandManager::doc_preparer_pool()->enqueue_bulk(bulk.begin(), bulk_cnt)) {
			total -= bulk_cnt;
			L_ERR("Ignored %zu documents: cannot enqueue tasks!", bulk_cnt);
		}
		if (total == bulk_cnt) {
			XapiandManager::doc_indexer_pool()->enqueue(shared_from_this());
		}
		bulk_cnt = 0;
		limit.wait();  // throttle the prepare
	}
}


bool
DocIndexer::wait(double timeout)
{
	L_CALL("DocIndexer::wait(<timeout>)");

	if (bulk_cnt != 0) {
		total += bulk_cnt;
		if (!XapiandManager::doc_preparer_pool()->enqueue_bulk(bulk.begin(), bulk_cnt)) {
			total -= bulk_cnt;
			L_ERR("Ignored %zu documents: cannot enqueue tasks!", bulk_cnt);
		}
		if (total == bulk_cnt) {
			XapiandManager::doc_indexer_pool()->enqueue(shared_from_this());
		}
		bulk_cnt = 0;
	}

	ready.store(true, std::memory_order_release);

	if (timeout) {
		if (timeout > 0.0) {
			return done.wait(timeout * 1e6);
		} else {
			done.wait();
		}
	}
	return processed >= total;
}


void
DocIndexer::finish()
{
	L_CALL("DocIndexer::finish()");

	running = false;
	ready_queue.enqueue(std::make_tuple(std::string{}, Xapian::Document{}, MsgPack{}));
}


//  ____                                        _
// |  _ \  ___   ___ _   _ _ __ ___   ___ _ __ | |_
// | | | |/ _ \ / __| | | | '_ ` _ \ / _ \ '_ \| __|
// | |_| | (_) | (__| |_| | | | | | |  __/ | | | |_
// |____/ \___/ \___|\__,_|_| |_| |_|\___|_| |_|\__|
//

Document::Document()
	: did(0),
	  db_handler(nullptr) { }


Document::Document(const Xapian::Document& doc_)
	: did(doc_.get_docid()),
	  db_handler(nullptr) { }


Document::Document(Xapian::docid did_, DatabaseHandler* db_handler_)
	: did(did_),
	  db_handler(db_handler_) { }


Xapian::Document
Document::_get_document()
{
	L_CALL("Document::_get_document()");

	Xapian::Document doc;
	if (db_handler != nullptr && db_handler->database()) {
		doc = db_handler->database()->get_document(did, true);
	}
	return doc;
}


Xapian::docid
Document::get_docid()
{
	return did;
}


std::string
Document::serialise(size_t retries)
{
	L_CALL("Document::serialise(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		auto doc = _get_document();
		return doc.serialise();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries != 0u) {
			return serialise(--retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}


std::string
Document::get_value(Xapian::valueno slot, size_t retries)
{
	L_CALL("Document::get_value(%u, %zu)", slot, retries);

	try {
		lock_database lk_db(db_handler);
		auto doc = _get_document();
		return doc.get_value(slot);
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries != 0u) {
			return get_value(slot, --retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}


std::string
Document::get_data(size_t retries)
{
	L_CALL("Document::get_data(%zu)", retries);

	try {
		lock_database lk_db(db_handler);
		auto doc = _get_document();
		return doc.get_data();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries != 0u) {
			return get_data(--retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}


MsgPack
Document::get_terms(size_t retries)
{
	L_CALL("get_terms(%zu)", retries);

	try {
		MsgPack terms;

		lock_database lk_db(db_handler);
		auto doc = _get_document();

		// doc.termlist_count() disassociates the database in doc.

		const auto it_e = doc.termlist_end();
		for (auto it = doc.termlist_begin(); it != it_e; ++it) {
			auto& term = terms[*it];
			term[RESPONSE_WDF] = it.get_wdf();  // The within-document-frequency of the current term in the current document.
			try {
				auto _term_freq = it.get_termfreq();  // The number of documents which this term indexes.
				term[RESPONSE_TERM_FREQ] = _term_freq;
			} catch (const Xapian::InvalidOperationError&) { }  // Iterator has moved, and does not support random access or doc is not associated with a database.
			if (it.positionlist_count() != 0u) {
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
		if (retries != 0u) {
			return get_terms(--retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}


MsgPack
Document::get_values(size_t retries)
{
	L_CALL("get_values(%zu)", retries);

	try {
		MsgPack values;

		lock_database lk_db(db_handler);
		auto doc = _get_document();

		values.reserve(doc.values_count());
		const auto iv_e = doc.values_end();
		for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
			values[std::to_string(iv.get_valueno())] = *iv;
		}
		return values;
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries != 0u) {
			return get_values(--retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}


MsgPack
Document::get_value(std::string_view slot_name)
{
	L_CALL("Document::get_value(%s)", repr(slot_name));

	if (db_handler != nullptr) {
		auto slot_field = db_handler->get_schema()->get_slot_field(slot_name);
		return Unserialise::MsgPack(slot_field.get_type(), get_value(slot_field.slot));
	}
	return MsgPack(MsgPack::Type::NIL);
}


MsgPack
Document::get_obj()
{
	L_CALL("Document::get_obj()");

	auto data = Data(get_data());
	return data.get_obj();
}


MsgPack
Document::get_field(std::string_view slot_name)
{
	L_CALL("Document::get_field(%s)", repr(slot_name));

	return get_field(slot_name, get_obj());
}


MsgPack
Document::get_field(std::string_view slot_name, const MsgPack& obj)
{
	L_CALL("Document::get_field(%s, <obj>)", repr(slot_name));

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

		auto doc = _get_document();

		uint64_t hash = 0;

		// Add hash of values
		const auto iv_e = doc.values_end();
		for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
			hash ^= xxh64::hash(*iv) * iv.get_valueno();
		}

		// Add hash of terms
		const auto it_e = doc.termlist_end();
		for (auto it = doc.termlist_begin(); it != it_e; ++it) {
			hash ^= xxh64::hash(*it) * it.get_wdf();
			const auto pit_e = it.positionlist_end();
			for (auto pit = it.positionlist_begin(); pit != pit_e; ++pit) {
				hash ^= *pit;
			}
		}

		// Add hash of data
		hash ^= xxh64::hash(doc.get_data());

		return hash;
	} catch (const Xapian::DatabaseModifiedError& exc) {
		if (retries != 0u) {
			return hash(--retries);
		}
		THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description());
	}
}
