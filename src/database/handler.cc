/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "database/handler.h"

#include <algorithm>                        // for min, move
#include <atomic>                           // for std::atomic
#include <array>                            // for std::array
#include <cassert>                          // for assert
#include <cctype>                           // for tolower
#include <cmath>                            // for std::floor
#include <exception>                        // for std::exception
#include <limits>                           // for std::numeric_limits
#include <optional>                         // for std::optional
#include <utility>                          // for std::move

#include "cast.h"                           // for Cast
#include "lua/exception.h"               // for lua::Error
#include "database/lock.h"                  // for lock_shard
#include "database/schema.h"                // for Schema, required_spc_t
#include "exception_engine.h"                     // for QueryParserError
#include "database/schemas_lru.h"           // for SchemasLRU
#include "database/shard.h"                 // for Shard
#include "database/utils.h"                 // for split_path_id, UNKNOWN_REVISION, make_msgpack_unpacker
#include "database/wal.h"                   // for DatabaseWAL
#include "xapiand_exception.h"                      // for ClientError
#include "sha256.h"                         // for SHA256
#include "io.hh"                            // for io::write (for MsgPack::serialise)
#include "length.h"                         // for serialise_string, unserialise_string
#include "log.h"                            // for L_CALL
#include "msgpack.h"                        // for MsgPack
#include "msgpack_patcher.h"                // for apply_patch
#include "multivalue/geospatialrange.h"     // for GeoSpatialRange
#include "multivalue/range.h"               // for MultipleValueRange, MultipleValueGE, MultipleValueLE
#include "multivalue/keymaker.h"            // for Multi_MultiValueKeyMaker
#include "opts.h"                           // for opts::
#include "engine/open.h"                    // for engine::check
#include "engine/search.h"                  // for engine::search, prepare
#include "query_dsl.h"                      // for QueryDSL
#include "random.hh"                        // for random_int
#include "rapidjson/document.h"             // for Document
#include "repr.hh"                          // for repr
#include "reserved/query_dsl.h"             // for RESERVED_QUERYDSL_*
#include "reserved/schema.h"                // for RESERVED_*
#include "response.h"                       // for RESPONSE_*
#include "script.h"                         // for Script
#include "xapiand_strings.hh"                       // for strings::from_bytes
#include "serialise.h"                      // for cast, serialise, type
#include "server/http_utils.h"              // for catch_http_errors

#ifdef XAPIAND_LUA
#include "lua/processor.h"                  // for lua namespace
#endif


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_INDEX
// #define L_INDEX L_CHOCOLATE


constexpr int SCHEMA_RETRIES   = 10;   // Number of tries for schema operations
constexpr int CONFLICT_RETRIES = 10;   // Number of tries for resolving version conflicts

constexpr size_t NON_STORED_SIZE_LIMIT = 1024 * 1024;

const std::string dump_documents_header("xapiand-dump-docs");


engine::docid
to_docid(std::string_view document_id)
{
	L_CALL("to_docid({})", document_id);

	if (document_id.size() > 1 && document_id[0] == '&') {
		document_id.remove_prefix(1);
		try {
			return static_cast<engine::docid>(strict_stoull(document_id));
		} catch (const std::invalid_argument& er) {
			THROW(ClientError, "Value {} cannot be cast to integer [{}]", repr(document_id), er.what());
		} catch (const std::out_of_range& er) {
			THROW(ClientError, "Value {} cannot be cast to integer [{}]", repr(document_id), er.what());
		}
	}
	return static_cast<engine::docid>(0);
}


bool
has_document_id(const MsgPack& document_id)
{
	switch (document_id.get_type()) {
		case MsgPack::Type::UNDEFINED:
		case MsgPack::Type::NIL:
			return false;
		case MsgPack::Type::STR:
			return !document_id.empty();
		case MsgPack::Type::BOOLEAN:
			return document_id.boolean();
		default:
			return true;
	}
}


engine::docid
numeric_docid(std::string_view key)
{
	if (key.size() < 2 || key[0] != 'N') {
		return 0;
	}
	const auto id = sortable_unserialise(std::string(key.substr(1)));
	// Below the largest docid: an id there would leave the shard nothing to number after it.
	constexpr auto top = static_cast<double>(std::numeric_limits<engine::docid>::max());
	if (!(id >= 1 && id < top) || id != std::floor(id)) {  // NaN fails the first test
		return 0;
	}
	return static_cast<engine::docid>(id);
}


// The version a write expects: none for UNKNOWN_REVISION.
static std::optional<engine::rev>
expected_of(engine::rev document_ver)
{
	return document_ver == UNKNOWN_REVISION ? std::nullopt : std::optional<engine::rev>(document_ver);
}


static void
inject_blob(Data& data, const MsgPack& obj)
{
	auto blob_it = obj.find(RESERVED_BLOB);
	if (blob_it == obj.end()) {
		THROW(ClientError, "Data inconsistency, objects in '{}' must contain '{}'", RESERVED_DATA, RESERVED_BLOB);
	}
	auto& blob_value = blob_it.value();
	if (!blob_value.is_string()) {
		THROW(ClientError, "Data inconsistency, '{}' must be a string", RESERVED_BLOB);
	}

	auto content_type_it = obj.find(RESERVED_CONTENT_TYPE);
	if (content_type_it == obj.end()) {
		THROW(ClientError, "Data inconsistency, objects in '{}' must contain '{}'", RESERVED_DATA, RESERVED_CONTENT_TYPE);
	}
	auto& content_type_value = content_type_it.value();
	auto ct_type = ct_type_t(content_type_value.is_string() ? content_type_value.str_view() : "");
	if (ct_type.empty()) {
		THROW(ClientError, "Data inconsistency, '{}' must be a valid content type string", RESERVED_CONTENT_TYPE);
	}

	std::string_view type;
	auto type_it = obj.find(RESERVED_TYPE);
	if (type_it == obj.end()) {
		type = "inplace";
	} else {
		auto& type_value = type_it.value();
		if (!type_value.is_string()) {
			THROW(ClientError, "Data inconsistency, '{}' must be either \"inplace\" or \"stored\"", RESERVED_TYPE);
		}
		type = type_value.str_view();
	}

	if (type == "inplace") {
		auto blob = blob_value.str_view();
		if (blob.size() > NON_STORED_SIZE_LIMIT) {
			THROW(ClientError, "Non-stored object has a size limit of {}", strings::from_bytes(NON_STORED_SIZE_LIMIT));
		}
		data.update(ct_type, blob);
	} else if (type == "stored") {
		data.update(ct_type, -1, 0, 0, blob_value.str_view());
	} else {
		THROW(ClientError, "Data inconsistency, '{}' must be either \"inplace\" or \"stored\"", RESERVED_TYPE);
	}
}


static void
inject_data(Data& data, const MsgPack& obj)
{
	auto data_it = obj.find(RESERVED_DATA);
	if (data_it != obj.end()) {
		auto& _data = data_it.value();
		switch (_data.get_type()) {
			case MsgPack::Type::STR: {
				auto blob = _data.str_view();
				if (blob.size() > NON_STORED_SIZE_LIMIT) {
					THROW(ClientError, "Non-stored object has a size limit of {}", strings::from_bytes(NON_STORED_SIZE_LIMIT));
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
				THROW(ClientError, "Data inconsistency, '{}' must be an array or an object", RESERVED_DATA);
		}
	}
}


/*
 *  ____        _        _                    _   _                 _ _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| | | | __ _ _ __   __| | | ___ _ __
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_| |/ _` | '_ \ / _` | |/ _ \ '__|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  _  | (_| | | | | (_| | |  __/ |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_| |_|\__,_|_| |_|\__,_|_|\___|_|
 *
 */


DatabaseHandler::DatabaseHandler()
	: flags(0)
{
}


DatabaseHandler::DatabaseHandler(const Endpoints& endpoints_, int flags_, std::shared_ptr<std::unordered_set<std::string>> context_)
	: flags(flags_),
	  endpoints(endpoints_),
	  context(std::move(context_)) { }


task::task<std::shared_ptr<Schema>>
DatabaseHandler::get_schema(const MsgPack* obj)
{
	L_CALL("DatabaseHandler::get_schema(<obj>)");

	auto manager = XapiandManager::manager();
	if (!manager) {
		co_return nullptr;
	}
	auto s = co_await manager->schemas->get(this, obj);
	co_return std::make_shared<Schema>(std::move(std::get<0>(s)), std::move(std::get<1>(s)), std::move(std::get<2>(s)));
}


void
DatabaseHandler::reset(const Endpoints& endpoints_, int flags_, const std::shared_ptr<std::unordered_set<std::string>>& context_)
{
	L_CALL("DatabaseHandler::reset({}, {:#x})", repr(endpoints_.to_string()), flags_);

	if (endpoints_.empty()) {
		THROW(ClientError, "It is expected at least one endpoint");
	}

	if (endpoints != endpoints_ || flags != flags_) {
		endpoints = endpoints_;
		flags = flags_;
	}

	context = context_;
}


#if XAPIAND_DATABASE_WAL
task::task<MsgPack>
DatabaseHandler::repr_wal(engine::rev start_revision, engine::rev end_revision, bool unserialised)
{
	L_CALL("DatabaseHandler::repr_wal({}, {})", start_revision, end_revision);

	if (endpoints.size() != 1) {
		THROW(ClientError, "This operation can only be executed on a single shard");
	}

	// The log is read in a query chore: the reactor thread never reads the device.
	IndexChores chores(XapiandManager::manager(true)->database_pool->work_pool());
	MsgPack result;
	const auto path = endpoints[0].path;
	co_await chores.run(engine::ChoreClass::query, [&] {
		DatabaseWAL wal(path);
		result = wal.to_string(start_revision, end_revision, unserialised);
	});
	co_return result;
}
#endif


task::task<MsgPack>
DatabaseHandler::check()
{
	L_CALL("DatabaseHandler::check()");

	MsgPack errors = MsgPack::MAP();
	for (auto& endpoint : endpoints) {
		try {
			// The check runs in the index's turn, on the engine the node has open.
			lock_shard lk_shard(endpoint, flags, turn_token());
			co_await lk_shard.pin();
			auto result = co_await lk_shard->check();
			if (result.failure.empty()) {
				errors[endpoint.path] = result.errors;
			} else {
				errors[endpoint.path] = result.failure;
			}
		} catch (...) {
			L_EXC("Check: Unknown error");
			errors[endpoint.path] = "Unknown error";
		}
	}
	co_return MsgPack{
		{"errors", errors},
	};
}


task::task<std::unique_ptr<MsgPack>>
DatabaseHandler::call_script(const MsgPack& object, const std::string& term_id, const Script& script, const Data& data)
{
#ifdef XAPIAND_LUA
	std::string method;  // TODO: Fill vriable "method" to pass to script

	auto doc = std::make_unique<MsgPack>(object);

	MsgPack old_doc;
	if (data.version.empty()) {
		Data current_data;
		try {
			auto current_document = co_await get_document_term(term_id);
			current_data = Data(co_await current_document.get_data());
			data.version = co_await current_document.get_value(DB_SLOT_VERSION);  // update version in data
		} catch (const engine::Error& error) {
			if (!engine::is_missing(error.kind)) { throw; }
		}
		old_doc = current_data.get_obj();
	} else {
		old_doc = data.get_obj();
	}

	L_INDEX("Script: call({}, {})", doc->to_string(4), old_doc.to_string(4));

	// Compiled and run without a wait between: the processor is this thread's own.
	auto processor = co_await lua::Processor::compile(script);
	co_await (*processor)(method, *doc, old_doc, script.get_params(), budget);
	co_return doc;
#else
	THROW(ClientError, "Script type 'lua' (Lua scripting) not available.");
	co_return nullptr;
#endif
}


task::task<std::tuple<std::string, engine::Document, MsgPack>>
DatabaseHandler::prepare(const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, size_t seq)
{
	L_CALL("DatabaseHandler::prepare({}, {}, <data>, <seq>)", repr(document_id.to_string()), obj.to_string());

	std::tuple<std::string, engine::Document, MsgPack> prepared;

	if (document_ver != UNKNOWN_REVISION && !data.version.empty()) {
		const auto current = static_cast<engine::rev>(sortable_unserialise(data.version));
		if (document_ver != current) {
			throw engine::version_conflict(current);  // early; the shard checks it again in the write's turn
		}
	}

	for (int t = SCHEMA_RETRIES; t >= 0; --t) {
		schema = co_await get_schema(&obj);
		L_INDEX("Prepare {}: {} using schema {}", repr(document_id.to_string()), obj.to_string(), schema->to_string());
		prepared = co_await schema->index(obj, document_id, *this, data, seq);
		if (co_await update_schema()) {
			break;
		}
		if (t == 0) {
			THROW(Error, "Cannot update schema while preparing: Too many retries");
		}
	}

	auto& doc = std::get<1>(prepared);
	auto& data_obj = std::get<2>(prepared);

	// Finish document: add data, ID term and ID value.
	// The following flush() **must** be after passing data to Schema::index() as
	// it uses it to get the old document during DatabaseHandler::call_script().
	// The record stays raw: finalise_write compresses it once, in the shard's turn.
	data.set_obj(data_obj, false);
	data.flush();
	auto serialised = data.serialise();
	if (!serialised.empty()) {
		doc.set_data(serialised);
	}

	// Request version
	if (document_ver != UNKNOWN_REVISION) {
		doc.add_value(DB_SLOT_VERSION, sortable_serialise(document_ver));
	} else if (!data.version.empty()) {
		doc.add_value(DB_SLOT_VERSION, data.version);
	}

	co_return prepared;
}


task::task<std::tuple<std::string, engine::Document, MsgPack>>
DatabaseHandler::prepare(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::prepare({}, {}, {}, {}/{})", repr(document_id.to_string()), stored, repr(body.to_string()), ct_type.first, ct_type.second);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	Data data;
	switch (body.get_type()) {
		case MsgPack::Type::STR:
			if (stored) {
				data.update(ct_type, -1, 0, 0, body.str_view());
			} else {
				auto blob = body.str_view();
				if (blob.size() > NON_STORED_SIZE_LIMIT) {
					THROW(ClientError, "Non-stored object has a size limit of {}", strings::from_bytes(NON_STORED_SIZE_LIMIT));
				}
				data.update(ct_type, blob);
			}
			co_return co_await prepare(document_id, document_ver, MsgPack::MAP(), data);
		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED:
			data.erase(ct_type);
			co_return co_await prepare(document_id, document_ver, MsgPack::MAP(), data);
		case MsgPack::Type::MAP:
			inject_data(data, body);
			co_return co_await prepare(document_id, document_ver, body, data);
		default:
			THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is {}", enum_name(body.get_type()));
	}
}


task::task<DocumentInfo>
DatabaseHandler::index(engine::docid did, const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, bool commit)
{
	L_CALL("DatabaseHandler::index({}, {}, {}, <data>, {})", repr(document_id.to_string()), document_ver, repr(obj.to_string()), commit);

	auto prepared = co_await prepare(document_id, document_ver, obj, data);
	auto& term_id = std::get<0>(prepared);
	auto& doc = std::get<1>(prepared);
	auto& data_obj = std::get<2>(prepared);

	engine::WriteResult info;
	bool placed = false;
	if (did) {
		assert(term_id != NEW_NUMERIC_KEY);
		try {
			info = co_await replace_document(did, std::move(doc), commit, true, true, true);
			placed = true;
		} catch (const engine::Error& error) {
			// Erased or moved since the lookup: placed afresh, not put back where it was found.
			if (error.kind != engine::errc::doc_not_found) { throw; }
		}
	}
	if (!placed) {
		info = co_await replace_document_term(term_id, std::move(doc), commit);
	}

	auto it = data_obj.find(ID_FIELD_NAME);
	if (it != data_obj.end() && term_id == NEW_NUMERIC_KEY) {
		data_obj.erase(it);
	}

	co_return std::make_pair(std::move(info), std::move(data_obj));
}


task::task<DocumentInfo>
DatabaseHandler::index(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::index({}, {}, {}, {}, {}/{})", repr(document_id.to_string()), stored, repr(body.to_string()), commit, ct_type.first, ct_type.second);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	const auto term_id = has_document_id(document_id) ? co_await get_prefixed_term_id(document_id) : std::string();

	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Data data;
			engine::docid did = 0;
			if (!term_id.empty()) {
				try {
					did = co_await written_docid_term(term_id);
				} catch (const engine::Error& error) {
					if (!engine::is_missing(error.kind)) { throw; }
				}
			}

			switch (body.get_type()) {
				case MsgPack::Type::STR:
					if (stored) {
						data.update(ct_type, -1, 0, 0, body.str_view());
					} else {
						auto blob = body.str_view();
						if (blob.size() > NON_STORED_SIZE_LIMIT) {
							THROW(ClientError, "Non-stored object has a size limit of {}", strings::from_bytes(NON_STORED_SIZE_LIMIT));
						}
						data.update(ct_type, blob);
					}
					co_return co_await index(did, document_id, document_ver, MsgPack::MAP(), data, commit);
				case MsgPack::Type::NIL:
				case MsgPack::Type::UNDEFINED:
					data.erase(ct_type);
					co_return co_await index(did, document_id, document_ver, MsgPack::MAP(), data, commit);
				case MsgPack::Type::MAP:
					inject_data(data, body);
					co_return co_await index(did, document_id, document_ver, body, data, commit);
				default:
					THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is {}", enum_name(body.get_type()));
			}
		} catch (const engine::Error& error) {
			if (error.kind != engine::errc::version_conflict || --t == 0 || document_ver != UNKNOWN_REVISION) { throw; }
		}
	}
}


task::task<DatabaseHandler::UploadedBlob>
DatabaseHandler::upload_stored(const MsgPack& document_id, const ct_type_t& ct_type, size_t size, engine::Chores& chores, BodyChunks& body)
{
	L_CALL("DatabaseHandler::upload_stored({}, {}/{}, {})", repr(document_id.to_string()), ct_type.first, ct_type.second, size);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}
	if (!has_document_id(document_id)) {
		THROW(ClientError, "A blob too big to buffer needs a document id in the URL");
	}
	if (size == 0) {
		THROW(ClientError, "A streamed blob has at least a byte");
	}

	// The blob goes to the data storage of the shard the document is indexed in, so that shard is known first: an id
	// that names a document id says it (numeric_docid), and any other id is placed by its hash.
	const auto term_id = co_await get_prefixed_term_id(document_id);
	const size_t n_shards = endpoints.size();
	size_t shard_num = 0;
	if (n_shards > 1) {
		const auto did = numeric_docid(term_id);
		shard_num = did != 0 ? (did - 1) % n_shards : key_placement_hash(term_id) % n_shards;
	}

	lock_shard lk_shard(endpoints[shard_num], flags, turn_token());
	co_await lk_shard.pin();
	auto upload = co_await lk_shard->upload_begin(chores, ct_type.to_string(), size);
	std::exception_ptr error;
	try {
		size_t received = 0;
		std::string chunk;
		while (co_await body.next(chunk)) {
			received += chunk.size();
			if (received > size) {
				THROW(ClientError, "The blob is longer than its Content-Length");
			}
			co_await upload->append(std::move(chunk));
			chunk = std::string();
		}
		if (received != size) {
			THROW(ClientError, "The blob is shorter than its Content-Length");
		}
		co_await upload->end();
	} catch (...) {
		error = std::current_exception();
	}
	if (error) {
		co_await upload->abort();
		std::rethrow_exception(error);
	}
	UploadedBlob blob;
	blob.volume = upload->volume();
	blob.offset = upload->offset();
	blob.size = size;
	blob.shard = shard_num;
	co_return blob;
}


task::task<DocumentInfo>
DatabaseHandler::index_uploaded(const MsgPack& document_id, engine::rev document_ver, const ct_type_t& ct_type, const UploadedBlob& blob, bool commit)
{
	L_CALL("DatabaseHandler::index_uploaded({}, {}, {}/{})", repr(document_id.to_string()), document_ver, ct_type.first, ct_type.second);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	const auto term_id = has_document_id(document_id) ? co_await get_prefixed_term_id(document_id) : std::string();

	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Data data;
			engine::docid did = 0;
			if (!term_id.empty()) {
				try {
					did = co_await written_docid_term(term_id);
				} catch (const engine::Error& error) {
					if (!engine::is_missing(error.kind)) { throw; }
				}
			}
			data.update(ct_type, blob.volume, blob.offset, blob.size);
			auto indexed = co_await index(did, document_id, document_ver, MsgPack::MAP(), data, commit);
			if (const auto n_shards = endpoints.size(); n_shards > 1 && (indexed.first.did - 1) % n_shards != blob.shard) {
				// The blob lies in another shard's storage than the document: the write is applied, so say so.
				THROW(Error, "The document landed in shard {} but its blob is in shard {}", (indexed.first.did - 1) % n_shards, blob.shard);
			}
			co_return indexed;
		} catch (const engine::Error& error) {
			if (error.kind != engine::errc::version_conflict || --t == 0 || document_ver != UNKNOWN_REVISION) { throw; }
		}
	}
}


task::task<DocumentInfo>
DatabaseHandler::patch(const MsgPack& document_id, engine::rev document_ver, bool create, const MsgPack& patches, bool commit)
{
	L_CALL("DatabaseHandler::patch({}, {}, {}, {})", repr(document_id.to_string()), document_ver, repr(patches.to_string()), commit);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	if (!patches.is_map() && !patches.is_array()) {
		THROW(ClientError, "Patches must be a JSON or MsgPack");
	}

	if (!has_document_id(document_id)) {
		THROW(ClientError, "Document must have an 'id'");
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);

	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Data data;
			engine::docid did = 0;
			try {
				auto current_document = co_await get_document_term(term_id);
				did = current_document.get_docid();
				data = Data(co_await current_document.get_data(), co_await current_document.get_value(DB_SLOT_VERSION));
			} catch (const engine::Error& error) {
				if (!engine::is_missing(error.kind)) { throw; }
				if (document_ver != UNKNOWN_REVISION && document_ver != 0) {
					throw engine::version_conflict(0);  // the version is checked before the document is missing
				}
				if (!create) {
					throw;
				}
			}
			auto obj = data.get_obj();
			obj.erase(VERSION_FIELD_NAME);  // discard any saved version

			apply_patch(patches, obj);
			auto it = obj.find(ID_FIELD_NAME);
			if (it != obj.end() && it + 1 != obj.end()) {
				auto id_field = it.value();
				obj.erase(it);
				obj[ID_FIELD_NAME] = std::move(id_field);
			}
			inject_data(data, obj);
			co_return co_await index(did, document_id, document_ver, obj, data, commit);
		} catch (const engine::Error& error) {
			if (error.kind != engine::errc::version_conflict || --t == 0 || document_ver != UNKNOWN_REVISION) { throw; }
		}
	}
}


task::task<DocumentInfo>
DatabaseHandler::update(const MsgPack& document_id, engine::rev document_ver, bool stored, bool create, const MsgPack& body, bool commit, const ct_type_t& ct_type)
{
	L_CALL("DatabaseHandler::update({}, {}, {}, <body:{}>, {}, {}/{})", repr(document_id.to_string()), document_ver, stored, enum_name(body.get_type()), commit, ct_type.first, ct_type.second);

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	if (!has_document_id(document_id)) {
		THROW(ClientError, "Document must have an 'id'");
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);

	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Data data;
			engine::docid did = 0;
			try {
				auto current_document = co_await get_document_term(term_id);
				did = current_document.get_docid();
				data = Data(co_await current_document.get_data(), co_await current_document.get_value(DB_SLOT_VERSION));
			} catch (const engine::Error& error) {
				if (!engine::is_missing(error.kind)) { throw; }
				if (document_ver != UNKNOWN_REVISION && document_ver != 0) {
					throw engine::version_conflict(0);  // the version is checked before the document is missing
				}
				if (!create) {
					throw;
				}
			}
			auto obj = data.get_obj();
			obj.erase(VERSION_FIELD_NAME);  // discard any saved version

			switch (body.get_type()) {
				case MsgPack::Type::STR:
					if (stored) {
						data.update(ct_type, -1, 0, 0, body.str_view());
					} else {
						auto blob = body.str_view();
						if (blob.size() > NON_STORED_SIZE_LIMIT) {
							THROW(ClientError, "Non-stored object has a size limit of {}", strings::from_bytes(NON_STORED_SIZE_LIMIT));
						}
						data.update(ct_type, blob);
					}
					co_return co_await index(did, document_id, document_ver, obj, data, commit);
				case MsgPack::Type::NIL:
				case MsgPack::Type::UNDEFINED:
					data.erase(ct_type);
					co_return co_await index(did, document_id, document_ver, obj, data, commit);
				case MsgPack::Type::MAP:
					if (stored) {
						THROW(ClientError, "Objects of this type cannot be put in storage");
					}
					if (obj.empty()) {
						inject_data(data, body);
						co_return co_await index(did, document_id, document_ver, body, data, commit);
					} else {
						obj.update(body);
						auto it = obj.find(ID_FIELD_NAME);
						if (it != obj.end() && it + 1 != obj.end()) {
							auto id_field = it.value();
							obj.erase(it);
							obj[ID_FIELD_NAME] = std::move(id_field);
						}
						inject_data(data, obj);
						co_return co_await index(did, document_id, document_ver, obj, data, commit);
					}
				default:
					THROW(ClientError, "Indexed object must be a JSON, a MsgPack or a blob, is {}", enum_name(body.get_type()));
			}

			co_return co_await index(did, document_id, document_ver, obj, data, commit);
		} catch (const engine::Error& error) {
			if (error.kind != engine::errc::version_conflict || --t == 0 || document_ver != UNKNOWN_REVISION) { throw; }
		}
	}
}


task::task<void>
DatabaseHandler::update_schema(const MsgPack& obj)
{
	L_CALL("DatabaseHandler::update_schema({}, {})", repr(obj.to_string()));

	bool was_foreign_obj;
	for (int t = SCHEMA_RETRIES; t >= 0; --t) {
		schema = co_await get_schema();
		was_foreign_obj = schema->update(obj);
		L_INDEX("Schema to update: {} {}", repr(schema->to_string()), was_foreign_obj ? "(foreign)" : "(local)");
		if (co_await update_schema()) {
			break;
		}
		if (t == 0) {
			THROW(Error, "Cannot update schema: Too many retries");
		}
	}

	if (was_foreign_obj) {
		MsgPack o = obj;
		o[RESERVED_TYPE] = "object";
		o.erase(RESERVED_FOREIGN);
		for (int t = SCHEMA_RETRIES; t >= 0; --t) {
			schema = co_await get_schema();
			was_foreign_obj = schema->update(o);
			L_INDEX("Schema to update: {} (local)", repr(schema->to_string()));
			if (co_await update_schema()) {
				break;
			}
			if (t == 0) {
				THROW(Error, "Cannot update foreign schema: Too many retries");
			}
		}
	}
}


task::task<void>
DatabaseHandler::write_schema(const MsgPack& obj)
{
	L_CALL("DatabaseHandler::write_schema({}, {})", repr(obj.to_string()));

	bool was_foreign_obj;
	for (int t = SCHEMA_RETRIES; t >= 0; --t) {
		schema = co_await get_schema();
		was_foreign_obj = schema->write(obj);
		L_INDEX("Schema to write: {} {}", repr(schema->to_string()), was_foreign_obj ? "(foreign)" : "(local)");
		if (co_await update_schema()) {
			break;
		}
		if (t == 0) {
			THROW(Error, "Cannot write schema: Too many retries");
		}
	}

	if (was_foreign_obj) {
		MsgPack o = obj;
		o[RESERVED_TYPE] = "object";
		o.erase(RESERVED_FOREIGN);
		for (int t = SCHEMA_RETRIES; t >= 0; --t) {
			schema = co_await get_schema();
			was_foreign_obj = schema->write(o);
			L_INDEX("Schema to write: {} (local)", repr(schema->to_string()));
			if (co_await update_schema()) {
				break;
			}
			if (t == 0) {
				THROW(Error, "Cannot write foreign schema: Too many retries");
			}
		}
	}
}


task::task<MsgPack>
DatabaseHandler::_dump_document(engine::docid did, const Data& data) {
	auto main_locator = data.get("");
	auto obj = main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack::MAP();
	for (auto& locator : data) {
		switch (locator.type) {
			case Locator::Type::inplace:
			case Locator::Type::lz4_compressed_inplace:
			case Locator::Type::zstd_compressed_inplace: {
				if (!locator.ct_type.empty()) {
					obj[RESERVED_DATA].push_back(MsgPack({
						{ "_content_type", locator.ct_type.to_string() },
						{ "_type", "inplace" },
						{ "_blob", locator.data() },
					}));
				}
				break;
			}
			case Locator::Type::stored:
			case Locator::Type::lz4_compressed_stored:
			case Locator::Type::zstd_compressed_stored: {
#ifdef XAPIAND_DATA_STORAGE
				auto stored = co_await storage_get_stored(locator, did);
				obj[RESERVED_DATA].push_back(MsgPack({
					{ "_content_type", unserialise_string_at(STORED_CONTENT_TYPE, stored) },
					{ "_type", "stored" },
					{ "_blob", unserialise_string_at(STORED_BLOB, stored) },
				}));
#endif
				break;
			}
		}
	}
	co_return obj;
}


task::task<MsgPack>
DatabaseHandler::dump_document(engine::docid did)
{
	L_CALL("DatabaseHandler::dump_document()");

	auto document = get_document(did);
	co_return co_await _dump_document(did, Data(co_await document.get_data()));
}


task::task<MsgPack>
DatabaseHandler::dump_document(std::string_view document_id)
{
	L_CALL("DatabaseHandler::dump_document()");

	auto did = co_await get_docid(document_id);
	co_return co_await dump_document(did);
}


namespace {

// How many documents a dump reads from a shard before it writes them out.
constexpr size_t DUMP_PAGE = 256;

// The sink of a dump's page throws it to end the scan at the page's size.
struct PageFull { };

// One page of a shard's documents, from `initial`: the shard's docid and data of each. `more` says the shard has
// documents beyond the page. A failure is retried the way a read is, from the page's start.
task::task<std::vector<std::pair<engine::docid, std::string>>>
dump_page(lock_shard& lk_shard, engine::docid initial, bool& more)
{
	std::vector<std::pair<engine::docid, std::string>> page;
	for (int t = DB_RETRIES; t >= 0; --t) {
		page.clear();
		more = false;
		try {
			auto* eng = co_await lk_shard->engine_for_read();
			co_await eng->scan(initial, [&](const engine::DocView& doc) {
				if (page.size() == DUMP_PAGE) {
					more = true;
					throw PageFull{};
				}
				page.emplace_back(doc.did(), doc.data());
			});
			break;
		} catch (const PageFull&) {
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}
	co_return page;
}

}  // namespace


task::task<MsgPack>
DatabaseHandler::dump_documents()
{
	L_CALL("DatabaseHandler::dump_documents()");

	L_DATABASE_WRAP_BEGIN("DatabaseHandler::dump_documents:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("DatabaseHandler::dump_documents:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto docs = MsgPack::ARRAY();

	size_t n_shards = endpoints.size();

	size_t shard_num = 0;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();

		engine::docid initial = 1;
		bool more = true;
		while (more) {
			auto page = co_await dump_page(lk_shard, initial, more);
			for (auto& [shard_did, data] : page) {
				engine::docid did = (shard_did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
				docs.push_back(co_await _dump_document(did, Data(std::move(data))));
			}
			if (!page.empty()) {
				initial = page.back().first + 1;
			}
		}

		++shard_num;
	}

	co_return docs;
}


task::task<std::string>
DatabaseHandler::dump_documents(int fd)
{
	L_CALL("DatabaseHandler::dump_documents(<fd>)");

	L_DATABASE_WRAP_BEGIN("DatabaseHandler::dump_documents:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("DatabaseHandler::dump_documents:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	SHA256 sha256;

	size_t n_shards = endpoints.size();

	size_t shard_num = 0;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();

		engine::docid initial = 1;
		bool more = true;
		while (more) {
			auto page = co_await dump_page(lk_shard, initial, more);
			for (auto& [shard_did, data] : page) {
				engine::docid did = (shard_did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
				auto obj = co_await _dump_document(did, Data(std::move(data)));
				std::string obj_ser = obj.serialise();
				ssize_t w = io::write(fd, obj_ser.data(), obj_ser.size());
				if (w < 0) THROW(Error, "Cannot write to file [{}]", fd);
				sha256.add(obj_ser.data(), obj_ser.size());
			}
			if (!page.empty()) {
				initial = page.back().first + 1;
			}
		}

		++shard_num;
	}

	co_return sha256.getHash();
}


task::task<std::string>
DatabaseHandler::restore_documents(const Endpoint& endpoint, int fd)
{
	L_CALL("DatabaseHandler::restore_documents({}, <fd>)", repr(endpoint.to_string()));

	SHA256 sha256;
	msgpack::unpacker unpacker = make_msgpack_unpacker();   // refuses a body nested deeper than MAX_JSON_DEPTH
	query_field_t query_field;
	query_field.commit = true;
	std::optional<DocIndexer> indexer;
	std::exception_ptr error;
	try {
		while (true) {
			auto manager = XapiandManager::manager();
			if (!manager || manager->is_detaching()) {
				break;
			}
			manager.reset();

			unpacker.reserve_buffer(1024);
			auto bytes = io::read(fd, unpacker.buffer(), unpacker.buffer_capacity());
			if (bytes < 0) THROW(Error, "Cannot read from file [{}]", fd);
			sha256.add(unpacker.buffer(), bytes);
			unpacker.buffer_consumed(bytes);

			std::vector<MsgPack> batch;
			msgpack::object_handle result;
			while (unpacker.next(result)) {
				batch.emplace_back(result.get());
			}
			if (!indexer && !batch.empty()) {
				// Resolved as RESTORE resolves it: writable, the first object as its settings, which saves the
				// index's settings and shard records in .xapiand/indexes before its documents are written.
				auto endpoints = co_await XapiandManager::resolve_index_endpoints(endpoint, true, &batch.front());
				if (endpoints.empty()) {
					throw engine::Error(engine::errc::network, "Endpoint node not available");
				}
				L_INFO("Restoring into: {}", repr(endpoints.to_string()));
				indexer.emplace(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, false, false, query_field);
			}
			if (indexer) {
				co_await indexer->index(batch);
			}

			if (!bytes) {
				break;
			}
		}
	} catch (...) {
		error = std::current_exception();
	}
	if (indexer) {
		co_await indexer->commit();   // what was indexed stays, whatever ended the stream
	}
	if (error) {
		std::rethrow_exception(error);
	}
	co_return sha256.getHash();
}


task::task<std::tuple<std::string, engine::Document, MsgPack>>
DatabaseHandler::prepare_document(MsgPack& body, size_t seq)
{
	L_CALL("DatabaseHandler::prepare_document(<body>, <seq>)");

	if (!has_db_writable(flags)) {
		THROW(Error, "Database is read-only");
	}

	if (!body.is_map()) {
		THROW(ClientError, "Object must be a JSON or MsgPack");
	}

	MsgPack document_id;

	auto f_it = body.find(ID_FIELD_NAME);
	if (f_it != body.end()) {
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

	std::string op_type = "index";
	f_it = body.find(RESERVED_OP_TYPE);
	if (f_it != body.end()) {
		op_type = f_it.value().as_str();
		body.erase(f_it);
	}

	if (op_type == "index") {
		Data data;
		inject_data(data, body);

		co_return co_await prepare(document_id, UNKNOWN_REVISION, body, data, seq);
	}

	if (op_type == "patch") {
		if (!has_document_id(document_id)) {
			THROW(ClientError, "Document must have an 'id'");
		}

		const auto term_id = co_await get_prefixed_term_id(document_id);

		Data data;
		try {
			auto current_document = co_await get_document_term(term_id);
			data = Data(co_await current_document.get_data(), co_await current_document.get_value(DB_SLOT_VERSION));
		} catch (const engine::Error& error) {
			if (!engine::is_missing(error.kind)) { throw; }
		}
		auto obj = data.get_obj();
		apply_patch(body, obj);

		co_return co_await prepare(document_id, UNKNOWN_REVISION, body, data, seq);
	}

	if (op_type == "update" || op_type == "merge") {
		if (!has_document_id(document_id)) {
			THROW(ClientError, "Document must have an 'id'");
		}

		const auto term_id = co_await get_prefixed_term_id(document_id);

		Data data;
		try {
			auto current_document = co_await get_document_term(term_id);
			data = Data(co_await current_document.get_data(), co_await current_document.get_value(DB_SLOT_VERSION));
		} catch (const engine::Error& error) {
			if (!engine::is_missing(error.kind)) { throw; }
		}
		auto obj = data.get_obj();

		if (obj.empty()) {
			inject_data(data, body);
			co_return co_await prepare(document_id, UNKNOWN_REVISION, body, data, seq);
		} else {
			obj.update(body);
			inject_data(data, obj);
			co_return co_await prepare(document_id, UNKNOWN_REVISION, obj, data, seq);
		}
	}

	THROW(ClientError, "Invalid operation type: {}", repr(op_type));
}


task::task<std::tuple<engine::SearchResult, MsgPack, engine::Query>>
DatabaseHandler::get_mset(const query_field_t& query_field, const MsgPack* qdsl, engine::MatchSpy* aggs)
{
	L_CALL("DatabaseHandler::get_mset({}, {})", repr(strings::join(query_field.query, " & ")), qdsl ? repr(qdsl->to_string()) : "null");

	schema = co_await get_schema();

	engine::doccount first = query_field.offset;
	engine::doccount maxitems = query_field.limit;
	engine::doccount check_at_least = query_field.check_at_least;

	QueryDSL query_object(schema);

	engine::Query query;
	std::unique_ptr<Multi_MultiValueKeyMaker> sorter;

	if (qdsl && qdsl->find(RESERVED_QUERYDSL_SORT) != qdsl->end()) {
		auto value = qdsl->at(RESERVED_QUERYDSL_SORT);
		sorter = query_object.get_sorter(value);
	}

	auto qdsl_query = (qdsl && qdsl->find(RESERVED_QUERYDSL_QUERY) != qdsl->end())
		? qdsl->at(RESERVED_QUERYDSL_QUERY)
		: query_object.make_dsl_query(query_field);
	L_DEBUG("qdsl: {}", qdsl_query.to_string());

	query = query_object.get_query(qdsl_query);
	L_DEBUG("query: {}", query.description());

	if (qdsl && qdsl->find(RESERVED_QUERYDSL_OFFSET) != qdsl->end()) {
		auto value = qdsl->at(RESERVED_QUERYDSL_OFFSET);
		if (value.is_integer()) {
			first = value.as_u64();
		} else {
			THROW(ClientError, "The {} must be a unsigned int", RESERVED_QUERYDSL_OFFSET);
		}
	}

	if (qdsl && qdsl->find(RESERVED_QUERYDSL_LIMIT) != qdsl->end()) {
		auto value = qdsl->at(RESERVED_QUERYDSL_LIMIT);
		if (value.is_integer()) {
			maxitems = value.as_u64();
		} else {
			THROW(ClientError, "The {} must be a unsigned int", RESERVED_QUERYDSL_LIMIT);
		}
	}

	if (qdsl && qdsl->find(RESERVED_QUERYDSL_CHECK_AT_LEAST) != qdsl->end()) {
		auto value = qdsl->at(RESERVED_QUERYDSL_CHECK_AT_LEAST);
		if (value.is_integer()) {
			check_at_least = value.as_u64();
		} else {
			THROW(ClientError, "The {} must be a unsigned int", RESERVED_QUERYDSL_CHECK_AT_LEAST);
		}
	}

	// Configure sorter.
	if (!query_field.sort.empty()) {
		if (!sorter) {
			sorter = std::make_unique<Multi_MultiValueKeyMaker>();
		}
		for (std::string_view sort : query_field.sort) {
			size_t pos = sort.find(':');
			if (pos != std::string_view::npos) {
				auto field = sort.substr(0, pos);
				auto value = sort.substr(pos);
				MsgPack sort_obj;
				if (!query_field.metric.empty()) {
					if (field[0] == '-') {
						field = field.substr(1, field.size());
						sort_obj = MsgPack({{ field, {{ RESERVED_VALUE, value }, { RESERVED_QUERYDSL_METRIC , query_field.metric}, { RESERVED_QUERYDSL_ORDER , QUERYDSL_DESC }} }});
					} else {
						sort_obj = MsgPack({{ field, {{ RESERVED_VALUE, value }, { RESERVED_QUERYDSL_METRIC , query_field.metric}} }});
					}
				} else {
					if (field[0] == '-') {
						field = field.substr(1, field.size());
						sort_obj = MsgPack({{ field, {{ RESERVED_VALUE, value }, { RESERVED_QUERYDSL_ORDER , QUERYDSL_DESC }} }});
					} else {
						sort_obj = MsgPack({{ field, {{ RESERVED_VALUE, value }} }});
					}
				}

				query_object.get_sorter(sorter, sort_obj);
			} else {
				query_object.get_sorter(sorter, sort);
			}
		}
	}

	// Get the collapse key to use for queries.
	engine::valueno collapse_key = engine::BAD_VALUENO;
	engine::doccount collapse_max = 0;
	if (!query_field.collapse.empty()) {
		const auto field_spc = schema->get_slot_field(query_field.collapse);
		collapse_key = field_spc.slot;
		collapse_max = query_field.collapse_max;
	}

	engine::DocidOrder order = engine::DocidOrder::ascending;

	if (aggs) {
		// Aggregations see only the documents the engine checks: every match on every shard,
		// whatever `check_at_least` asks (no value is larger), so they are never a sample.
		check_at_least = engine::CHECK_ALL;
	}

	auto mset = co_await get_mset(
		query,
		first,
		maxitems,
		check_at_least,
		sorter.get(),
		collapse_key,
		collapse_max,
		order,
		aggs);
	co_return std::make_tuple(std::move(mset), std::move(qdsl_query), std::move(query));
}




task::task<engine::SearchResult>
DatabaseHandler::get_mset(
	const engine::Query& query,
	engine::doccount first,
	engine::doccount maxitems,
	engine::doccount check_at_least,
	const engine::KeyMaker* sorter,
	engine::valueno collapse_key,
	engine::doccount collapse_max,
	engine::DocidOrder order,
	engine::MatchSpy* aggs
	)
{
	L_CALL("DatabaseHandler::get_mset({}, {}, {}, {})", query.description(), first, maxitems, check_at_least);

	// The engine reads the text of the query here, once, before any database is checked out.
	auto prepared = engine::prepare(query);
	co_return co_await match(*prepared, first, maxitems, check_at_least, sorter, collapse_key, collapse_max, order, aggs);
}


SplitHitId
split_hit_id(engine::hitid did, size_t shards)
{
	if (did == 0u || shards == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}
	const auto shard_did = (did - 1) / shards + 1;
	if (shard_did > std::numeric_limits<engine::docid>::max()) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}
	return {static_cast<size_t>((did - 1) % shards), static_cast<engine::docid>(shard_did)};
}


void
check_search_fanout(size_t shards, size_t max_search_indexes)
{
	if (shards > max_search_indexes) {
		throw engine::Error(engine::errc::invalid, strings::format("A search can read at most {} indexes (--max-search-indexes) and this one names {}; narrow the index pattern", max_search_indexes, shards));
	}
}


size_t
search_batch_indexes()
{
	// What the pool keeps open (soft_open): a batch never holds more (CT-R39).
	auto manager = XapiandManager::manager();
	const auto open = manager && manager->database_pool ? std::max<size_t>(1, manager->database_pool->soft_open())
		: static_cast<size_t>(std::max<ssize_t>(1, opts.database_pool_size));
	if (opts.concurrent_search_indexes > 0) {
		return std::min(static_cast<size_t>(opts.concurrent_search_indexes), open);
	}
	// A reactor's share of the open indexes the pool holds.
	const auto reactors = static_cast<size_t>(std::max<ssize_t>(1, opts.num_http_servers));
	return std::max<size_t>(1, open / reactors);
}


namespace {

// The indexes searches hold open right now, and the most there have been at once.
std::atomic<size_t> pinned_now{0};
std::atomic<size_t> pinned_most{0};

// Counts a batch's indexes while the batch holds them.
class PinCount {
	size_t n;

public:
	explicit PinCount(size_t n) : n(n) {
		const auto now = pinned_now.fetch_add(n, std::memory_order_relaxed) + n;
		auto most = pinned_most.load(std::memory_order_relaxed);
		while (most < now && !pinned_most.compare_exchange_weak(most, now, std::memory_order_relaxed)) { }
	}
	~PinCount() { pinned_now.fetch_sub(n, std::memory_order_relaxed); }
	PinCount(const PinCount&) = delete;
	PinCount& operator=(const PinCount&) = delete;
};

}  // namespace


size_t
search_pinned_indexes(bool most)
{
	return (most ? pinned_most : pinned_now).load(std::memory_order_relaxed);
}


// Pins the indexes [begin, end) of the search and searches them, reopening them on a stale read. This is
// the one place that decides how a batch's indexes are held (a use of each registry entry, each engine
// refreshed once), so the search reads what was applied before it. A path named twice is pinned once and searched at
// both positions: a second pin of a key would wait for itself while its index is replaced.
task::task<engine::BatchResult>
DatabaseHandler::search_batch(size_t begin, size_t end, const engine::SearchRequest& request)
{
	std::vector<lock_shard> shards;
	std::vector<size_t> shard_of;   // the shard of each position
	shards.reserve(end - begin);
	shard_of.reserve(end - begin);
	for (auto i = begin; i < end; ++i) {
		const auto it = std::find_if(shards.begin(), shards.end(), [&](const lock_shard& shard) { return shard.endpoint.path == endpoints[i].path; });
		shard_of.push_back(static_cast<size_t>(it - shards.begin()));
		if (it == shards.end()) {
			shards.emplace_back(endpoints[i], flags, turn_token());
			co_await shards.back().pin();
		}
	}
	PinCount pinned(shards.size());
	engine::BatchResult batch;
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			// A shard whose index doesn't exist is skipped (multi-index search over a missing one).
			std::vector<engine::Shard*> found(shards.size(), nullptr);   // each shard's engine; null when missing
			for (size_t s = 0; s < shards.size(); ++s) {
				try {
					found[s] = co_await shards[s]->engine_for_read();
				} catch (const engine::Error& error) {
					if (error.kind != engine::errc::not_found) { throw; }
				}
			}
			std::vector<engine::Shard*> engines;
			engines.reserve(shard_of.size());
			std::vector<std::uint32_t> indexes;   // the index of each shard found
			batch.positions.clear();
			for (size_t i = 0; i < shard_of.size(); ++i) {
				if (found[shard_of[i]] == nullptr) {
					continue;
				}
				engines.push_back(found[shard_of[i]]);
				batch.positions.push_back(begin + i);
				if (!request.index_of.empty()) {
					indexes.push_back(request.index_of[i]);
				}
			}
			if (engines.empty()) {
				break;  // nothing to search -> empty result
			}

			auto searched = request;
			searched.index_of = indexes;
			batch.result = co_await engine::search(engines, searched);
			break;
		} catch (const QueryParserError& exc) {
			THROW(ClientError, exc.what());
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const QueryDslError& exc) {
			THROW(ClientError, exc.what());
		} catch (const engine::Error& error) {
			switch (error.kind) {
				case engine::errc::reopen:
				case engine::errc::opening:
				case engine::errc::not_found:
				case engine::errc::not_available:
				case engine::errc::timeout:
				case engine::errc::network:
				case engine::errc::closed:
					if (t == 0) { throw; }
					break;
				case engine::errc::syntax:
					THROW(ClientError, error.description());
				default:
					throw;
			}
		}
		for (auto& shard : shards) {
			co_await shard->reopen();
		}
	}
	co_return batch;
}


task::task<engine::SearchResult>
DatabaseHandler::match(
	const engine::PreparedQuery& query,
	engine::doccount first,
	engine::doccount maxitems,
	engine::doccount check_at_least,
	const engine::KeyMaker* sorter,
	engine::valueno collapse_key,
	engine::doccount collapse_max,
	engine::DocidOrder order,
	engine::MatchSpy* aggs
	)
{
	L_CALL("DatabaseHandler::match({}, {}, {}, {})", query.description(), first, maxitems, check_at_least);

	if (query.empty()) {
		co_return engine::SearchResult{};
	}

	engine::SearchRequest request;
	request.query = &query;
	request.first = first;
	request.maxitems = maxitems;
	request.check_at_least = check_at_least;
	request.sorter = sorter;
	request.collapse_key = collapse_key;
	request.collapse_max = collapse_max;
	request.order = order;
	request.spy = aggs;
	request.budget = budget;

	// Only a runaway search is refused; a wide one is walked in batches, which hold at most
	// `search_batch_indexes()` indexes open at once.
	check_search_fanout(endpoints.size(), static_cast<size_t>(opts.max_search_indexes));

	// The engine feeds each batch into one search; the batches' results merge into the top of the whole.
	const engine::BatchRunner run = [this](size_t begin, size_t end, const engine::SearchRequest& batch_request) {
		return search_batch(begin, end, batch_request);
	};
	// The shards of one index weigh against that index's statistics together, and no other's.
	std::vector<std::uint32_t> index_of;
	index_of.reserve(endpoints.size());
	std::string_view last;
	for (const auto& endpoint : endpoints) {
		const auto index = unsharded_path(endpoint.path).first;
		if (index_of.empty() || index != last) {
			index_of.push_back(index_of.empty() ? 0 : index_of.back() + 1);
			last = index;
		} else {
			index_of.push_back(index_of.back());
		}
	}
	co_return co_await engine::search_batched(endpoints.size(), search_batch_indexes(), index_of, run, request);
}


task::task<bool>
DatabaseHandler::update_schema()
{
	L_CALL("DatabaseHandler::update_schema()");

	auto mod_schema = schema->get_modified_schema();
	if (mod_schema) {
		auto old_schema = schema->get_const_schema();
		auto manager = XapiandManager::manager();
		co_return manager && co_await manager->schemas->set(this, old_schema, mod_schema);
	}
	co_return true;
}


task::task<std::string>
DatabaseHandler::get_prefixed_term_id(const MsgPack& document_id)
{
	L_CALL("DatabaseHandler::get_prefixed_term_id({})", repr(document_id.to_string()));

	assert(has_document_id(document_id));

	schema = co_await get_schema();

	std::string unprefixed_term_id;
	auto spc_id = schema->get_data_id();
	auto id_type = spc_id.get_type();
	if (id_type == FieldType::empty) {
		// Search like namespace.
		const auto type_ser = Serialise::guess_serialise(document_id);
		id_type = type_ser.first;
		if (id_type == FieldType::text) {
			id_type = FieldType::keyword;
		}
		spc_id.set_type(id_type);
		spc_id.flags.bool_term = true;
		unprefixed_term_id = type_ser.second;
	} else {
		unprefixed_term_id = Serialise::serialise(spc_id, Cast::cast(id_type, document_id));
	}
	co_return document_key(unprefixed_term_id, spc_id.get_ctype());
}


task::task<std::vector<std::string>>
DatabaseHandler::get_metadata_keys()
{
	L_CALL("DatabaseHandler::get_metadata_keys()");

	assert(!endpoints.empty());
	std::vector<std::string> keys;
	auto valid = endpoints.size();
	std::exception_ptr eptr;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		try {
			keys = co_await lk_shard->get_metadata_keys();
			if (!keys.empty()) {
				break;
			}
		} catch (const engine::Error& error) {
			lk_shard->collect_failure(error, eptr, valid, false);
		}
	}
	if (eptr && !valid) {
		std::rethrow_exception(eptr);
	}
	co_return keys;
}


task::task<std::string>
DatabaseHandler::get_metadata(const std::string& key)
{
	L_CALL("DatabaseHandler::get_metadata({})", repr(key));

	assert(!endpoints.empty());
	assert(!key.empty());
	std::string value;
	auto valid = endpoints.size();
	std::exception_ptr eptr;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		try {
			value = co_await lk_shard->get_metadata(key);
			if (!value.empty()) {
				break;
			}
		} catch (const engine::Error& error) {
			lk_shard->collect_failure(error, eptr, valid, false);
		}
	}
	if (eptr && !valid) {
		std::rethrow_exception(eptr);
	}
	co_return value;
}


task::task<std::string>
DatabaseHandler::get_metadata(std::string_view key)
{
	assert(!key.empty());
	co_return co_await get_metadata(std::string(key));
}


task::task<void>
DatabaseHandler::set_metadata(const std::string& key, const std::string& value, bool commit, bool wal)
{
	L_CALL("DatabaseHandler::set_metadata({}, {}, {}, {})", repr(key), repr(value), commit, wal);

	assert(!endpoints.empty());
	assert(!key.empty());
	auto valid = endpoints.size();
	std::exception_ptr eptr;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		try {
			co_await lk_shard->set_metadata(key, value, commit, wal);
		} catch (const engine::Error& error) {
			lk_shard->collect_failure(error, eptr, valid, true);
		}
	}
	if (eptr && !valid) {
		std::rethrow_exception(eptr);
	}
}


task::task<void>
DatabaseHandler::set_metadata(std::string_view key, std::string_view value, bool commit, bool wal)
{
	assert(!key.empty());
	co_await set_metadata(std::string(key), std::string(value), commit, wal);
}


Document
DatabaseHandler::get_document(engine::hitid did)
{
	L_CALL("DatabaseHandler::get_document((engine::hitid){})", did);

	return Document(did, this);
}


task::task<Document>
DatabaseHandler::get_document(std::string_view document_id)
{
	L_CALL("DatabaseHandler::get_document((std::string){})", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		co_return get_document(did);
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);
	did = co_await get_docid_term(term_id);
	co_return Document(did, this);
}


task::task<Document>
DatabaseHandler::get_document_term(const std::string& term_id)
{
	L_CALL("DatabaseHandler::get_document_term({})", repr(term_id));

	auto did = co_await get_docid_term(term_id);
	co_return Document(did, this);
}


task::task<engine::docid>
DatabaseHandler::get_docid(std::string_view document_id)
{
	L_CALL("DatabaseHandler::get_docid({})", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		co_return did;
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);
	co_return co_await get_docid_term(term_id);
}


task::task<engine::docid>
DatabaseHandler::get_docid_term(const std::string& term)
{
	L_CALL("DatabaseHandler::get_docid_term({})", repr(term));

	co_return co_await find_docid_term(term, false);
}


task::task<engine::docid>
DatabaseHandler::written_docid_term(const std::string& term)
{
	L_CALL("DatabaseHandler::written_docid_term({})", repr(term));

	co_return co_await find_docid_term(term, true);
}


task::task<engine::docid>
DatabaseHandler::find_docid_term(const std::string& term, bool written)
{
	assert(!term.empty());
	assert(!endpoints.empty());

	engine::docid did = 0;

	size_t n_shards = endpoints.size();

	std::exception_ptr eptr;

	size_t shard_num = 0;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();

		for (int t = DB_RETRIES; t >= 0; --t) {
			try {
				engine::docid shard_did = 0;
				if (written) {
					shard_did = co_await lk_shard->written_key(term);
				} else {
					shard_did = co_await (co_await lk_shard->engine_for_read())->find_key(term);
				}
				if (shard_did != 0) {
					did = (shard_did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
					co_return did;
				}
				break;
			} catch (const engine::Error& error) {
				if (error.kind == engine::errc::not_available) {
					eptr = std::current_exception(); break;
				}
				lk_shard->retry_read(error, t);
			}
			try {
				co_await lk_shard->reopen();
			} catch (const engine::Error& error) {
				if (error.kind != engine::errc::not_available) { throw; }
				eptr = std::current_exception(); break;
			}
		}

		++shard_num;
	}

	if (eptr) {
		std::rethrow_exception(eptr);
	}

	throw engine::Error(engine::errc::doc_not_found, "Document not found");
}


task::task<void>
DatabaseHandler::delete_document(engine::docid did, engine::rev document_ver, bool commit, bool wal)
{
	L_CALL("DatabaseHandler::delete_document({}, {}, {}, {})", did, document_ver, commit, wal);

	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	size_t shard_num = (did - 1) % n_shards;  // docid in the multi-db to shard number
	engine::docid shard_did = (did - 1) / n_shards + 1;  // docid in the multi-db to the docid in the shard
	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	co_await lk_shard->delete_document(shard_did, commit, wal, expected_of(document_ver));
}


task::task<void>
DatabaseHandler::delete_document(std::string_view document_id, engine::rev document_ver, bool commit, bool wal)
{
	L_CALL("DatabaseHandler::delete_document({}, {}, {}, {})", repr(document_id), document_ver, commit, wal);

	auto did = to_docid(document_id);
	if (did != 0u) {
		co_await delete_document(did, document_ver, commit, wal);
		co_return;
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);
	co_await delete_document_term(term_id, document_ver, commit, wal);
}


task::task<void>
DatabaseHandler::delete_document_term(const std::string& term, engine::rev document_ver, bool commit, bool wal)
{
	L_CALL("DatabaseHandler::delete_document_term({})", repr(term));

	assert(!endpoints.empty());

	size_t n_shards = endpoints.size();

	// The found docid only picks the shard: the erase is by key in that shard's turn, which checks
	// the version of the document the key names then, not of whatever holds the docid.
	size_t shard_num = 0;
	if (n_shards > 1) {
		engine::docid did = 0;
		try {
			did = co_await written_docid_term(term);
		} catch (const engine::Error& error) {
			if (!engine::is_missing(error.kind)) { throw; }
		}
		if (did == 0u) {
			did = numeric_docid(term);
		}
		shard_num = did != 0u ? (did - 1) % n_shards : key_placement_hash(term) % n_shards;
	}

	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	co_await lk_shard->delete_document_term(term, commit, wal, expected_of(document_ver));
}


task::task<size_t>
DatabaseHandler::least_used_shard()
{
	size_t shard_num = 0;
	auto min_doccount = std::numeric_limits<engine::doccount>::max();
	for (size_t n = 0; n < endpoints.size(); ++n) {
		lock_shard lk_shard(endpoints[n], flags, turn_token());
		co_await lk_shard.pin();
		auto node = lk_shard->node();
		if (node && node->is_active()) {
			try {
				auto doccount = co_await lk_shard->written_count();
				if (min_doccount > doccount) {
					min_doccount = doccount;
					shard_num = n;
				}
			} catch (...) {}
		}
	}
	co_return shard_num;
}


task::task<engine::WriteResult>
DatabaseHandler::add_document(engine::Document&& doc, bool commit, bool wal, bool version)
{
	L_CALL("DatabaseHandler::add_document(<doc>, {}, {})", commit, wal);

	assert(!endpoints.empty());

	size_t n_shards = endpoints.size();

	const size_t shard_num = n_shards > 1 ? co_await least_used_shard() : 0;

	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	auto info = co_await lk_shard->add_document(std::move(doc), commit, wal, version);
	info.did = (info.did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
	co_return info;
}


task::task<engine::WriteResult>
DatabaseHandler::replace_document(engine::docid did, engine::Document&& doc, bool commit, bool wal, bool version, bool must_exist)
{
	L_CALL("DatabaseHandler::replace_document({}, <doc>, {}, {})", did, commit, wal);

	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	size_t shard_num = (did - 1) % n_shards;  // docid in the multi-db to shard number
	engine::docid shard_did = (did - 1) / n_shards + 1;  // docid in the multi-db to the docid in the shard
	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	auto info = co_await lk_shard->replace_document(shard_did, std::move(doc), commit, wal, version, must_exist);
	info.did = did;
	co_return info;
}


task::task<engine::WriteResult>
DatabaseHandler::replace_document_term(const std::string& term, engine::Document&& doc, bool commit, bool wal, bool version)
{
	L_CALL("DatabaseHandler::replace_document_term({}, <doc>, {}, {})", repr(term), commit, wal);

	assert(!endpoints.empty());

	size_t n_shards = endpoints.size();

	size_t shard_num = 0;
	if (n_shards > 1) {
		if (term == NEW_NUMERIC_KEY) {
			// A new numeric id goes to the least used of the active shards, which numbers it.
			shard_num = co_await least_used_shard();
		} else {
			try {
				// A document that exists (in any shard) is replaced where it is.
				auto did = co_await written_docid_term(term);
				if (did != 0u) {
					// Written in the shard's turn only while it is still there: else it is placed afresh.
					co_return co_await replace_document(did, std::move(doc), commit, wal, version, true);
				}
			} catch (const engine::Error& error) {
				if (!engine::is_missing(error.kind)) { throw; }
			}
			// A new one goes where its id says (numeric_docid), else where its key hashes.
			const auto did = numeric_docid(term);
			shard_num = did != 0 ? (did - 1) % n_shards : key_placement_hash(term) % n_shards;
		}
		doc.add_value(DB_SLOT_SHARDS, serialise_length(shard_num) + serialise_length(n_shards));
	}

	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	auto info = co_await lk_shard->replace_document_term(term, std::move(doc), commit, wal, version);
	info.did = (info.did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
	co_return info;
}


task::task<engine::WriteResult>
DatabaseHandler::replace_document(std::string_view document_id, engine::Document&& doc, bool commit, bool wal, bool version)
{
	L_CALL("DatabaseHandler::replace_document({}, <doc>)", repr(document_id));

	auto did = to_docid(document_id);
	if (did != 0u) {
		co_return co_await replace_document(did, std::move(doc), commit, wal, version);
	}

	const auto term_id = co_await get_prefixed_term_id(document_id);
	co_return co_await replace_document_term(term_id, std::move(doc), commit, wal, version);
}


task::task<MsgPack>
DatabaseHandler::get_document_info(std::string_view document_id, bool raw_data, bool human)
{
	L_CALL("DatabaseHandler::get_document_info({}, {}, {})", repr(document_id), raw_data, human);

	auto document = co_await get_document(document_id);
	const auto data = Data(co_await document.get_data());

	MsgPack info;

	auto did = document.get_docid();
	info[RESPONSE_DOCID] = did;

	auto version = co_await document.get_value(DB_SLOT_VERSION);
	if (!version.empty()) {
		info[RESPONSE_VERSION] = static_cast<engine::rev>(sortable_unserialise(version));
	}

	size_t n_shards = endpoints.size();
	if (n_shards != 1) {
		size_t shard_num = (did - 1) % n_shards;  // docid in the multi-db to shard number
		info[RESPONSE_SHARD] = shard_num + 1;
		info[RESPONSE_ENDPOINT] = endpoints[shard_num].to_string();
	}

	if (raw_data) {
		info[RESPONSE_RAW_DATA] = data.serialise();
	}

	auto& info_data = info[RESPONSE_DATA];
	if (!data.empty()) {
		for (auto& locator : data) {
			switch (locator.type) {
				case Locator::Type::inplace:
				case Locator::Type::lz4_compressed_inplace:
				case Locator::Type::zstd_compressed_inplace:
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
				case Locator::Type::lz4_compressed_stored:
				case Locator::Type::zstd_compressed_stored:
					MsgPack locator_info = {
						{ RESPONSE_CONTENT_TYPE, locator.ct_type.to_string() },
						{ RESPONSE_TYPE, "stored" },
						{ RESPONSE_VOLUME, locator.volume },
						{ RESPONSE_OFFSET, locator.offset },
					};
					if (human) {
						locator_info[RESPONSE_SIZE] = strings::from_bytes(locator.size);
					} else {
						locator_info[RESPONSE_SIZE] = locator.size;
					}
					info_data.push_back(std::move(locator_info));
					break;
			}
		}
	}

	info[RESPONSE_VALUES] = co_await document.get_values();

	co_return info;
}


task::task<MsgPack>
DatabaseHandler::get_database_info()
{
	L_CALL("DatabaseHandler::get_database_info()");

	assert(!endpoints.empty());
	if (endpoints.size() == 1) {
		auto& endpoint = endpoints[0];
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		auto stats = co_await (co_await lk_shard->engine_for_read())->stats();
		co_return MsgPack{
			{ RESPONSE_ENDPOINT, endpoint.path },
			{ RESPONSE_UUID, stats.uuid },
			{ RESPONSE_REVISION, stats.revision },
			{ RESPONSE_DOC_COUNT, stats.doc_count },
			{ RESPONSE_LAST_ID, stats.lastdocid },
			{ RESPONSE_DOC_DEL, stats.lastdocid - stats.doc_count },
			{ RESPONSE_AV_LENGTH, stats.avlength },
			{ RESPONSE_DOC_LEN_LOWER, stats.doclen_lower },
			{ RESPONSE_DOC_LEN_UPPER, stats.doclen_upper },
			{ RESPONSE_HAS_POSITIONS, stats.has_positions },
		};
	}

	MsgPack shards = MsgPack::ARRAY();
	for (auto& endpoint : endpoints) {
		shards.append(endpoint.path);
	}

	engine::doccount doc_count = 0;
	engine::docid lastdocid = 0;
	bool has_positions = false;
	std::uint64_t total_length = 0;
	engine::termcount doclength_lower_bound = 0;
	engine::termcount doclength_upper_bound = 0;

	size_t n_shards = endpoints.size();

	size_t shard_num = 0;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		auto stats = co_await (co_await lk_shard->engine_for_read())->stats();
		doc_count += stats.doc_count;
		engine::docid shard_lastdocid = stats.lastdocid;
		engine::docid did = (shard_lastdocid - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
		lastdocid = std::max(lastdocid, did);
		has_positions = has_positions || stats.has_positions;
		auto old_total_length = total_length;
		total_length += stats.total_length;
		if (total_length < old_total_length) {
			throw engine::Error(engine::errc::database, "Total document length overflowed!");
		}
		engine::termcount shard_doclength_lower_bound = -stats.doclen_lower;
		doclength_lower_bound = std::max(doclength_lower_bound, shard_doclength_lower_bound);
		doclength_upper_bound = std::max(doclength_upper_bound, stats.doclen_upper);
		++shard_num;
	}
    double average_length = doc_count == 0 ? 0.0 : total_length / double(doc_count);

	co_return MsgPack{
		{ RESPONSE_ENDPOINT , unsharded_path(endpoints[0].path).first },
		{ RESPONSE_DOC_COUNT, doc_count },
		{ RESPONSE_LAST_ID, lastdocid },
		{ RESPONSE_DOC_DEL, lastdocid - doc_count },
		{ RESPONSE_AV_LENGTH, average_length },
		{ RESPONSE_DOC_LEN_LOWER,  doclength_lower_bound },
		{ RESPONSE_DOC_LEN_UPPER, doclength_upper_bound },
		{ RESPONSE_HAS_POSITIONS, has_positions },
		{ "shards", shards },
	};
}


#ifdef XAPIAND_DATA_STORAGE
task::task<std::string>
DatabaseHandler::storage_get_stored(const Locator& locator, engine::docid did)
{
	L_CALL("DatabaseHandler::storage_get_stored()");

	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	size_t shard_num = (did - 1) % n_shards;  // docid in the multi-db to shard number
	auto& endpoint = endpoints[shard_num];
	lock_shard lk_shard(endpoint, flags, turn_token());
	co_await lk_shard.pin();
	co_return co_await lk_shard->storage_get_stored(locator);
}
#endif /* XAPIAND_DATA_STORAGE */


task::task<bool>
DatabaseHandler::commit(bool wal)
{
	L_CALL("DatabaseHandler::commit({})", wal);

	assert(!endpoints.empty());
	bool ret = true;
	auto valid = endpoints.size();
	std::exception_ptr eptr;
	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		try {
			ret = co_await lk_shard->commit(wal) || ret;
		} catch (const engine::Error& error) {
			lk_shard->collect_failure(error, eptr, valid, true);
		}
	}
	if (eptr && !valid) {
		std::rethrow_exception(eptr);
	}
	co_return ret;
}


task::task<void>
DatabaseHandler::reopen()
{
	L_CALL("DatabaseHandler::reopen()");

	for (auto& endpoint : endpoints) {
		lock_shard lk_shard(endpoint, flags, turn_token());
		co_await lk_shard.pin();
		co_await lk_shard->reopen();
	}
}


task::task<MsgPack>
DatabaseHandler::unserialise_term_id(std::string_view term_id)
{
	L_CALL("DatabaseHandler::unserialise_term_id({})", repr(term_id));

	assert(!term_id.empty());
	if (term_id.size() < 2) {
		THROW(SerialisationError, "Term cannot be unserialised");
	}
	auto type = term_id[0];
	term_id.remove_prefix(1);
	switch (type) {
		case 'U':
			co_return Unserialise::MsgPack(FieldType::uuid, term_id);
		case 'K':
			co_return Unserialise::MsgPack(FieldType::keyword, term_id);
		case 'S':
			co_return Unserialise::MsgPack(FieldType::text, term_id);
		case 'B':
			co_return Unserialise::MsgPack(FieldType::boolean, term_id);
		case 'G':
			co_return Unserialise::MsgPack(FieldType::geo, term_id);
		case 'N':
		case 'D':
		case 'T': {
			// These three types need further information, get such from the schema.
			auto field_type = (co_await get_schema())->get_slot_field(ID_FIELD_NAME).get_type();
			co_return Unserialise::MsgPack(field_type, term_id);
		}
		default:
			THROW(SerialisationError, "Term type: {} is an unknown type", type);
	}
}


/*
 *  ____             ___           _
 * |  _ \  ___   ___|_ _|_ __   __| | _____  _____ _ __
 * | | | |/ _ \ / __|| || '_ \ / _` |/ _ \ \/ / _ \ '__|
 * | |_| | (_) | (__ | || | | | (_| |  __/>  <  __/ |
 * |____/ \___/ \___|___|_| |_|\__,_|\___/_/\_\___|_|
 *
 */

DocIndexer::DocIndexer(const Endpoints& endpoints, int flags, bool echo, bool comments, const query_field_t& query_field, task::budget* budget) :
	endpoints(endpoints),
	flags(flags),
	echo(echo),
	comments(comments),
	first(query_field.offset),
	maxitems(query_field.limit),
	db_handler(endpoints, flags)
{
	db_handler.budget = budget;
}


namespace {

// catch_http_errors for a task: the status and message an exception of the awaited work maps to.
template <typename F>
task::task<http_errors_t>
co_catch_http_errors(F work)
{
	std::exception_ptr error;
	try {
		co_await work();
	} catch (...) {
		error = std::current_exception();
	}
	co_return catch_http_errors([&] {
		if (error) {
			std::rethrow_exception(error);
		}
		return 0;
	});
}

}  // namespace


task::task<void>
DocIndexer::index_one(MsgPack&& obj)
{
	L_CALL("DocIndexer::index_one(<obj>)");

	if (!obj.is_map()) {
		L_ERR("Indexing object has an unsupported type: {}", enum_name(obj.get_type()));
		co_return;
	}

	if (first) {
		--first;
		co_return;
	}

	if (!maxitems) {
		co_return;
	}
	--maxitems;

	const auto idx = _idx++;
	++_total;
	if (_results.size() <= idx) {
		_results.resize(idx + 1, MsgPack::MAP());
	}

	// Prepare: the document the engine will index, and where it goes.
	std::tuple<std::string, engine::Document, MsgPack> prepared;
	auto prepare_errors = co_await co_catch_http_errors([&]() -> task::task<void> {
		prepared = co_await db_handler.prepare_document(obj, idx + 1);
	});
	++_processed;
	MsgPack result;
	if (prepare_errors.error_code != HTTP_STATUS_OK) {
		result = MsgPack{
			{ RESPONSE_STATUS, static_cast<unsigned>(prepare_errors.error_code) },
			{ RESPONSE_TYPE, http_status_str(prepare_errors.error_code) },
			{ RESPONSE_MESSAGE, prepare_errors.error }
		};
		_results[idx] = std::move(result);
		co_return;
	}
	++_prepared;

	// Apply: the document goes to its shard, in the shard's turn.
	auto& term_id = std::get<0>(prepared);
	auto& doc = std::get<1>(prepared);
	auto& data_obj = std::get<2>(prepared);
	if (!term_id.empty()) {
		auto index_errors = co_await co_catch_http_errors([&]() -> task::task<void> {
			auto info = co_await db_handler.replace_document_term(term_id, std::move(doc), false, true, false);

			if (term_id == NEW_NUMERIC_KEY) {
				result[ID_FIELD_NAME] = co_await db_handler.unserialise_term_id(info.term);
			} else {
				auto it_id = data_obj.find(ID_FIELD_NAME);
				if (it_id == data_obj.end()) {
					result[ID_FIELD_NAME] = co_await db_handler.unserialise_term_id(info.term);
				} else {
					result[ID_FIELD_NAME] = it_id.value();
				}
			}

			if (echo) {
				result[VERSION_FIELD_NAME] = info.version;

				if (comments) {
					result[RESPONSE_xDOCID] = info.did;

					size_t n_shards = endpoints.size();
					size_t shard_num = (info.did - 1) % n_shards;
					result[RESPONSE_xSHARD] = shard_num + 1;
					// result[RESPONSE_xENDPOINT] = endpoints[shard_num].to_string();
				}
			}

			++_indexed;
		});
		if (index_errors.error_code != HTTP_STATUS_OK) {
			result[RESPONSE_STATUS] = static_cast<unsigned>(index_errors.error_code);
			result[RESPONSE_TYPE] = http_status_str(index_errors.error_code);
			result[RESPONSE_MESSAGE] = index_errors.error;
		}
	} else if (!data_obj.is_undefined()) {
		result = std::move(data_obj);
	}
	_results[idx] = std::move(result);
}


task::task<void>
DocIndexer::index(std::vector<MsgPack>& batch)
{
	L_CALL("DocIndexer::index(<{} objects>)", batch.size());

	for (auto& obj : batch) {
		if (obj.is_array()) {
			for (auto& o : obj) {
				co_await index_one(std::move(o));
			}
		} else {
			co_await index_one(std::move(obj));
		}
		// An object per slice at most: the request gives its reactor back when its slice is spent.
		if (db_handler.budget != nullptr) {
			co_await db_handler.budget->checkpoint();
		}
	}
}


task::task<void>
DocIndexer::commit()
{
	L_CALL("DocIndexer::commit()");

	if (_indexed != 0) {
		DatabaseHandler committer(endpoints, flags);
		co_await committer.commit();
	}
}


/*
 *  ____                                        _
 * |  _ \  ___   ___ _   _ _ __ ___   ___ _ __ | |_
 * | | | |/ _ \ / __| | | | '_ ` _ \ / _ \ '_ \| __|
 * | |_| | (_) | (__| |_| | | | | | |  __/ | | | |_
 * |____/ \___/ \___|\__,_|_| |_| |_|\___|_| |_|\__|
 *
 */

Document::Document()
	: did(0),
	  db_handler(nullptr) { }


Document::Document(engine::hitid did_, DatabaseHandler* db_handler_)
	: did(did_),
	  db_handler(db_handler_) { }


engine::docid
Document::get_docid()
{
	if (did > std::numeric_limits<engine::docid>::max()) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}
	return static_cast<engine::docid>(did);
}


task::task<std::string>
Document::serialise()
{
	L_CALL("Document::serialise()");

	std::string serialised;

	if (db_handler == nullptr) {
		co_return serialised;
	}

	if (did == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}

	int flags = db_handler->flags;
	auto& endpoints = db_handler->endpoints;
	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	const auto where = split_hit_id(did, n_shards);
	const size_t shard_num = where.shard;
	const engine::docid shard_did = where.did;
	auto& endpoint = endpoints[shard_num];

	lock_shard lk_shard(endpoint, flags, db_handler->turn_token());
	co_await lk_shard.pin();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto doc = co_await lk_shard->get_view(shard_did);
			serialised = doc->serialise();
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}

	co_return serialised;
}


task::task<std::string>
Document::get_value(engine::valueno slot)
{
	L_CALL("Document::get_value({})", slot);

	std::string value;

	if (db_handler == nullptr) {
		co_return value;
	}

	if (did == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}

	int flags = db_handler->flags;
	auto& endpoints = db_handler->endpoints;
	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	const auto where = split_hit_id(did, n_shards);
	const size_t shard_num = where.shard;
	const engine::docid shard_did = where.did;
	auto& endpoint = endpoints[shard_num];

	lock_shard lk_shard(endpoint, flags, db_handler->turn_token());
	co_await lk_shard.pin();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto doc = co_await lk_shard->get_view(shard_did);
			value = doc->value(slot);
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}

	co_return value;
}


task::task<std::string>
Document::get_data()
{
	L_CALL("Document::get_data()");

	std::string data;

	if (db_handler == nullptr) {
		co_return data;
	}

	if (did == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}

	int flags = db_handler->flags;
	auto& endpoints = db_handler->endpoints;
	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	const auto where = split_hit_id(did, n_shards);
	const size_t shard_num = where.shard;
	const engine::docid shard_did = where.did;
	auto& endpoint = endpoints[shard_num];

	lock_shard lk_shard(endpoint, flags, db_handler->turn_token());
	co_await lk_shard.pin();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto doc = co_await lk_shard->get_view(shard_did);
			data = doc->data();
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}

	co_return data;
}


task::task<bool>
Document::validate()
{
	L_CALL("Document::validate()");

	if (db_handler == nullptr) {
		co_return false;
	}

	if (did == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}

	int flags = db_handler->flags;
	auto& endpoints = db_handler->endpoints;
	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	const auto where = split_hit_id(did, n_shards);
	const size_t shard_num = where.shard;
	const engine::docid shard_did = where.did;
	auto& endpoint = endpoints[shard_num];

	lock_shard lk_shard(endpoint, flags, db_handler->turn_token());
	co_await lk_shard.pin();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			co_await lk_shard->get_view(shard_did);
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}

	co_return true;
}


task::task<MsgPack>
Document::get_values()
{
	L_CALL("get_values()");

	MsgPack values;

	if (db_handler == nullptr) {
		co_return values;
	}

	if (did == 0u) {
		throw engine::Error(engine::errc::doc_not_found, "Document not found");
	}

	int flags = db_handler->flags;
	auto& endpoints = db_handler->endpoints;
	assert(!endpoints.empty());
	size_t n_shards = endpoints.size();
	const auto where = split_hit_id(did, n_shards);
	const size_t shard_num = where.shard;
	const engine::docid shard_did = where.did;
	auto& endpoint = endpoints[shard_num];

	lock_shard lk_shard(endpoint, flags, db_handler->turn_token());
	co_await lk_shard.pin();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto doc = co_await lk_shard->get_view(shard_did);
			doc->values([&](engine::valueno slot, std::string_view value) {
				values[std::to_string(slot)] = value;
			});
			break;
		} catch (const engine::Error& error) {
			lk_shard->retry_read(error, t);
		}
		co_await lk_shard->reopen();
	}

	co_return values;
}


task::task<MsgPack>
Document::get_value(std::string_view slot_name)
{
	L_CALL("Document::get_value({})", repr(slot_name));

	if (db_handler != nullptr) {
		auto slot_field = (co_await db_handler->get_schema())->get_slot_field(slot_name);
		co_return Unserialise::MsgPack(slot_field.get_type(), co_await get_value(slot_field.slot));
	}
	co_return MsgPack::NIL();
}


task::task<MsgPack>
Document::get_obj()
{
	L_CALL("Document::get_obj()");

	auto data = Data(co_await get_data());
	co_return data.get_obj();
}


task::task<MsgPack>
Document::get_field(std::string_view slot_name)
{
	L_CALL("Document::get_field({})", repr(slot_name));

	co_return get_field(slot_name, co_await get_obj());
}


MsgPack
Document::get_field(std::string_view slot_name, const MsgPack& obj)
{
	L_CALL("Document::get_field({}, <obj>)", repr(slot_name));

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

	return MsgPack::NIL();
}


// The autocommit, armed again while a merge of the index is pending (x-pool D8): the busy index's merges then publish
// and sync with no write to take its turn.
static task::task<void>
autocommit_and_rearm(DatabasePool& pool, Endpoint endpoint)
{
	if (co_await autocommit(pool, endpoint)) {
		XapiandManager::schedule_autocommit(endpoint);
	}
}


void
committer_commit(Endpoint endpoint) {
	// The debouncer's thread only starts the commit: it is a task of the index, and takes its turn.
	auto manager = XapiandManager::manager();
	if (!manager || !manager->database_pool) {
		return;
	}
	manager->database_pool->run_detached(autocommit_and_rearm(*manager->database_pool, endpoint));
}
