/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "xapiand.h"

#include <memory>                            // for shared_ptr, make_shared
#include <stddef.h>                          // for size_t
#include <string>                            // for string
#include <unordered_map>                     // for unordered_map
#include <vector>                            // for vector
#include <xapian.h>                          // for Document, docid, MSet

#include "database_utils.h"                  // for query_field_...
#include "endpoint.h"                        // for Endpoints
#include "http_parser.h"                     // for http_method
#include "manager.h"                         // for XapiandManager, XapiandM...
#include "msgpack.h"                         // for MsgPack


class AggregationMatchSpy;
class Database;
class DatabaseHandler;
class Document;
class Multi_MultiValueKeyMaker;
class Schema;
class SchemasLRU;


Xapian::docid to_docid(const std::string& document_id);


class MSet : public Xapian::MSet {
public:
	MSet() = default;
	MSet(const MSet& o) : Xapian::MSet(o) { }
	MSet(const Xapian::MSet& o) : Xapian::MSet(o) { }

	// The following either use enquire or db (db could have changed)
	Xapian::doccount get_termfreq(const std::string& term) const = delete;
	double get_termweight(const std::string& term) const = delete;
	std::string snippet(const std::string& text,
			size_t length = 500,
			const Xapian::Stem& stemmer = Xapian::Stem(),
			unsigned flags = SNIPPET_BACKGROUND_MODEL | SNIPPET_EXHAUSTIVE,
			const std::string& hi_start = "<b>",
			const std::string& hi_end = "</b>",
			const std::string& omit = "...") const = delete;
	void fetch(const Xapian::MSetIterator& begin, const Xapian::MSetIterator& end) const = delete;
	void fetch(const Xapian::MSetIterator& item) const = delete;
	void fetch() const = delete;
};


class lock_database {
	DatabaseHandler* db_handler;

	lock_database(const lock_database&) = delete;
	lock_database& operator=(const lock_database&) = delete;

public:
	template<typename F, typename... Args>
	lock_database(DatabaseHandler* db_handler_, F&& f, Args&&... args);
	lock_database(DatabaseHandler* db_handler_);
	~lock_database();

	template<typename F, typename... Args>
	void lock(F&& f, Args&&... args);
	void lock();

	void unlock();
};


using DataType = std::pair<Xapian::docid, MsgPack>;


class DatabaseHandler {
	friend class Document;
	friend class lock_database;
	friend class Schema;
	friend class SchemasLRU;

	Endpoints endpoints;
	int flags;
	enum http_method method;
	std::shared_ptr<Schema> schema;
	std::shared_ptr<Database> database;

	std::shared_ptr<std::unordered_set<size_t>> context;

	void recover_index();
	void delete_schema();


#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	static std::mutex documents_mtx;
	static std::unordered_map<size_t, std::shared_ptr<std::pair<size_t, const MsgPack>>> documents;

	template<typename ProcessorCompile>
	MsgPack& call_script(MsgPack& data, const std::string& term_id, size_t script_hash, size_t body_hash, const std::string& script_body, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair);
	MsgPack& run_script(MsgPack& data, const std::string& term_id, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair, const MsgPack& data_script);
#endif

	DataType index(const std::string& document_id, bool stored, const std::string& stored_locator, MsgPack& obj, const std::string& blob, bool commit_, const ct_type_t& ct_type);

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

	bool update_schema(std::chrono::time_point<std::chrono::system_clock> schema_begins);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, const std::shared_ptr<std::unordered_set<size_t>>& context_=nullptr);

	~DatabaseHandler() = default;

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj=nullptr);

	void reset(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, const std::shared_ptr<std::unordered_set<size_t>>& context_=nullptr);

#if XAPIAND_DATABASE_WAL
	MsgPack repr_wal(uint32_t start_revision, uint32_t end_revision);
#endif

	DataType index(const std::string& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);
	DataType patch(const std::string& document_id, const MsgPack& patches, bool commit_, const ct_type_t& ct_type);
	DataType merge(const std::string& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);

	void write_schema(const MsgPack& obj);

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& suggestions);

	void dump_metadata(int fd);
	void dump_schema(int fd);
	void dump_documents(int fd);
	void restore(int fd);

	std::string get_prefixed_term_id(const std::string& document_id);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	bool set_metadata(const std::string& key, const std::string& value, bool overwrite=true);

	Document get_document(const Xapian::docid& did);
	Document get_document(const std::string& document_id);
	Document get_document_term(const std::string& term_id);
	Xapian::docid get_docid(const std::string& document_id);

	void delete_document(const std::string& document_id, bool commit_=false, bool wal_=true);

	MsgPack get_document_info(const std::string& document_id);
	MsgPack get_database_info();

	bool commit(bool _wal=true);
	bool reopen();
	long long get_mastery_level();

	static void init_ref(const Endpoint& endpoint);
	static void inc_ref(const Endpoint& endpoint);
	static void dec_ref(const Endpoint& endpoint);
	static int get_master_count();

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	void dec_document_change_cnt(const std::string& term_id);
	const std::shared_ptr<std::pair<size_t, const MsgPack>> get_document_change_seq(const std::string& term_id);
	bool set_document_change_seq(const std::string& term_id, const std::shared_ptr<std::pair<size_t, const MsgPack>>& new_document_pair, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair);
#endif
};


class Document {
	Xapian::Document doc;
	mutable uint64_t _hash;

	DatabaseHandler* db_handler;
	std::shared_ptr<Database> database;

	void update();

public:
	Document();
	Document(const Xapian::Document& doc_, uint64_t hash_=0);
	Document(DatabaseHandler* db_handler_, const Xapian::Document& doc_, uint64_t hash_=0);

	Document(const Document& doc_);
	Document& operator=(const Document& doc_);

	Xapian::docid get_docid();

	std::string serialise(size_t retries=DB_RETRIES);
	std::string get_value(Xapian::valueno slot, size_t retries=DB_RETRIES);
	std::string get_data(size_t retries=DB_RETRIES);
	std::string get_blob(size_t retries=DB_RETRIES);
	MsgPack get_terms(size_t retries=DB_RETRIES);
	MsgPack get_values(size_t retries=DB_RETRIES);

	MsgPack get_value(const std::string& slot_name);
	std::pair<bool, std::string> get_store();
	MsgPack get_obj();
	MsgPack get_field(const std::string& slot_name);
	static MsgPack get_field(const std::string& slot_name, const MsgPack& obj);

	uint64_t hash(size_t retries=DB_RETRIES);
};
