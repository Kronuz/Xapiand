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
#include "msgpack.h"                         // for MsgPack

class AggregationMatchSpy;
class Database;
class Schema;
class Document;
class Multi_MultiValueKeyMaker;


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


using DataType = std::pair<Xapian::docid, MsgPack>;


class DatabaseHandler {
	friend class Document;

	Endpoints endpoints;
	int flags;
	enum http_method method;
	std::shared_ptr<Schema> schema;
	std::shared_ptr<Database> database;

	void recover_index();

	Document get_document_term(const std::string& term_id);

#ifdef XAPIAND_V8
	MsgPack run_script(const MsgPack& data, const std::string& term_id);
#endif

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

public:
	class lock_database {
		DatabaseHandler* db_handler;

		lock_database(const lock_database&) = delete;
		lock_database& operator=(const lock_database&) = delete;

	public:
		lock_database(DatabaseHandler* db_handler_);
		~lock_database();

		void lock();
		void unlock() noexcept;
	};

	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_=0);

	~DatabaseHandler() = default;

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj=nullptr) const;

	void reset(const Endpoints& endpoints_, int flags_, enum http_method method_);

	DataType index(const std::string& _document_id, bool stored, const std::string& storage, const MsgPack& obj, const std::string& blob, bool commit_, const std::string& ct_type);
	DataType index(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type);
	DataType patch(const std::string& _document_id, const MsgPack& patches, bool commit_, const std::string& ct_type);
	DataType merge(const std::string& _document_id, bool stored, const MsgPack& body, bool commit_, const std::string& ct_type);

	void write_schema(const std::string& body);
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

	void get_document_info(MsgPack& info, const std::string& document_id);
	void get_database_info(MsgPack& info);
};


class Document : public Xapian::Document {
	DatabaseHandler* db_handler;
	std::shared_ptr<Database> database;

	void update();
	void update() const;

public:
	Document();
	Document(const Xapian::Document& doc);
	Document(DatabaseHandler* db_handler_, const Xapian::Document& doc);

	MsgPack get_field(const std::string& slot_name) const;
	std::string get_value(Xapian::valueno slot) const;
	MsgPack get_value(const std::string& slot_name) const;
	void add_value(Xapian::valueno slot, const std::string& value);
	void add_value(const std::string& slot_name, const MsgPack& value);
	void remove_value(Xapian::valueno slot);
	void remove_value(const std::string& slot_name);
	std::string get_data() const;
	void set_data(const std::string& data);
	void set_data(const std::string& obj, const std::string& blob, bool stored);
	std::pair<bool, std::string> get_store();
	std::string get_blob();
	void set_blob(const std::string& blob, bool stored);
	MsgPack get_obj() const;
	void set_obj(const MsgPack& obj);
	Xapian::termcount termlist_count() const;
	Xapian::TermIterator termlist_begin() const;
	Xapian::termcount values_count() const;
	Xapian::ValueIterator values_begin() const;
	std::string serialise() const;
};
