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
	friend class SchemasLRU;

	Endpoints endpoints;
	int flags;
	enum http_method method;
	std::shared_ptr<Schema> schema;
	std::shared_ptr<Database> database;

	void recover_index();

	MsgPack get_document_obj(const std::string& term_id);

#ifdef XAPIAND_V8
	MsgPack run_script(MsgPack& data, const std::string& term_id);
#endif

	DataType index(const std::string& _document_id, bool stored, const std::string& storage, MsgPack& obj, const std::string& blob, bool commit_, const std::string& ct_type);

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET);

	~DatabaseHandler() = default;

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj=nullptr);

	void reset(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET);

	DataType index(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type);
	DataType patch(const std::string& _document_id, const MsgPack& patches, bool commit_, const std::string& ct_type);
	DataType merge(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type);

	void write_schema(const MsgPack& obj);

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& suggestions);

	std::pair<bool, bool> update_schema();

	std::string get_prefixed_term_id(const std::string& doc_id);

	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value);

	Document get_document(const Xapian::docid& did);
	Document get_document(const std::string& doc_id);
	Xapian::docid get_docid(const std::string& doc_id);

	void delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);

	MsgPack get_document_info(const std::string& document_id);
	MsgPack get_database_info();

	bool commit(bool _wal=true);

	long long get_mastery_level();

	static void init_ref(const Endpoint& endpoint);
	static void inc_ref(const Endpoint& endpoint);
	static void dec_ref(const Endpoint& endpoint);
	static int get_master_count();
};


class Document {
	Xapian::Document doc;
	DatabaseHandler* db_handler;
	std::shared_ptr<Database> database;

	void update();

public:
	Document();
	Document(const Xapian::Document& doc_);
	Document(DatabaseHandler* db_handler_, const Xapian::Document& doc_);

	Document(const Document& doc_);

	Document& operator=(const Document& doc_);

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
};
