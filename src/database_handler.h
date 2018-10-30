/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#pragma once

#include "config.h"

#include <memory>                            // for shared_ptr, make_shared
#include <stddef.h>                          // for size_t
#include <string>                            // for string
#include "string_view.hh"                    // for std::string_view
#include <unordered_map>                     // for unordered_map
#include <utility>                           // for std::pair
#include <vector>                            // for vector
#include <xapian.h>                          // for Document, docid, MSet

#include "database_flags.h"                  // for DB_*
#include "http_parser.h"                     // for http_method
#include "lock_database.h"                   // for LockableDatabase


class AggregationMatchSpy;
class Data;
class Database;
class DatabaseHandler;
class Document;
class Endpoints;
class MsgPack;
class Multi_MultiValueKeyMaker;
class Schema;
class SchemasLRU;
struct ct_type_t;
struct query_field_t;
struct similar_field_t;


Xapian::docid to_docid(std::string_view document_id);


// MSet is a thin wrapper, as Xapian::MSet keeps enquire->db references; this
// only keeps a set of Xapian::docid internally (mostly) so it's thread safe
// across database checkouts.
class MSet {
	struct MSetItem {
		Xapian::docid did;
		Xapian::doccount rank;
		double weight;
		int percent;

		MSetItem(const Xapian::MSetIterator& it) :
			did{*it},
			rank{it.get_rank()},
			weight{it.get_weight()},
			percent{it.get_percent()} { }

		MSetItem(const Xapian::docid& did) :
			did{did},
			rank{0},
			weight{0},
			percent{0} { }
	};

	using items_t = std::vector<MSetItem>;

	class MSetIterator {
		items_t::const_iterator it;

	public:
		MSetIterator(items_t::const_iterator&& it) : it{std::move(it)} { }

		auto operator*() const {
			return it->did;
		}

		auto operator++() {
			return ++it;
		}

		auto operator!=(const MSetIterator& mit) {
			return it != mit.it;
		}

		auto get_rank() const {
			return it->rank;
		}

		auto get_weight() const {
			return it->weight;
		}

		auto get_percent() const {
			return it->percent;
		}
	};

	items_t items;
	Xapian::doccount matches_estimated;

public:
	MSet() :
		matches_estimated{0}
	{ }

	MSet(const Xapian::MSet& mset) :
		matches_estimated{mset.get_matches_estimated()}
	{
		auto it_end = mset.end();
		for (auto it = mset.begin(); it != it_end; ++it) {
			items.push_back(it);
		}
	}

	MSet(const Xapian::docid& did) :
		matches_estimated{1}
	{
		items.push_back(did);
	}

	std::size_t size() const {
		return items.size();
	}

	Xapian::doccount get_matches_estimated() const {
		return matches_estimated;
	}

	auto begin() const {
		return MSetIterator(items.begin());
	}

	auto end() const {
		return MSetIterator(items.end());
	}

	void push_back(const Xapian::docid& did) {
		items.push_back(did);
		++matches_estimated;
	}
};

using DataType = std::pair<Xapian::docid, MsgPack>;

class DatabaseHandler : protected LockableDatabase {
	friend Document;
	friend Schema;
	friend SchemasLRU;

	enum http_method method;
	std::shared_ptr<Schema> schema;

	std::shared_ptr<std::unordered_set<size_t>> context;

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	static std::mutex documents_mtx;
	static std::unordered_map<size_t, std::shared_ptr<std::pair<std::string, const Data>>> documents;

	template<typename ProcessorCompile>
	std::unique_ptr<MsgPack> call_script(const MsgPack& object, std::string_view term_id, size_t script_hash, size_t body_hash, std::string_view script_body, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair);
	std::unique_ptr<MsgPack> run_script(const MsgPack& object, std::string_view term_id, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair, const MsgPack& data_script);
#endif

	std::tuple<std::string, Xapian::Document, MsgPack> prepare(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair);

	DataType index(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair, bool commit_);

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

	bool update_schema(std::chrono::time_point<std::chrono::system_clock> schema_begins);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, std::shared_ptr<std::unordered_set<size_t>> context_=nullptr);

	~DatabaseHandler() = default;

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj=nullptr);

	void reset(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, const std::shared_ptr<std::unordered_set<size_t>>& context_=nullptr);

#if XAPIAND_DATABASE_WAL
	MsgPack repr_wal(uint32_t start_revision, uint32_t end_revision, bool unserialised);
#endif

	MsgPack check();

	DataType index(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);
	DataType patch(const MsgPack& document_id, const MsgPack& patches, bool commit_, const ct_type_t& ct_type);
	DataType merge(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);

	void write_schema(const MsgPack& obj, bool replace);
	void delete_schema();

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	MSet get_all_mset(Xapian::docid initial=0, size_t limit=-1);
	MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& suggestions);

	void dump_metadata(int fd);
	void dump_schema(int fd);
	void dump_documents(int fd);
	void restore(int fd);

	MsgPack dump_documents();

	std::tuple<std::string, Xapian::Document, MsgPack> prepare_document(const MsgPack& obj);
	void restore_documents(const MsgPack& docs);

	std::string get_prefixed_term_id(const MsgPack& document_id);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	std::string get_metadata(std::string_view key);
	bool set_metadata(const std::string& key, const std::string& value, bool overwrite=true);
	bool set_metadata(std::string_view key, std::string_view value, bool overwrite=true);

	Document get_document(const Xapian::docid& did);
	Document get_document(std::string_view document_id);
	Document get_document_term(const std::string& term_id);
	Document get_document_term(std::string_view term_id);
	Xapian::docid get_docid(std::string_view document_id);

	void delete_document(std::string_view document_id, bool commit_=false, bool wal_=true);

	MsgPack get_document_info(std::string_view document_id, bool raw_data);
	MsgPack get_database_info();

	bool commit(bool _wal=true);
	bool reopen();

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	const std::shared_ptr<std::pair<std::string, const Data>> get_document_change_seq(std::string_view term_id, bool validate_exists = false);
	bool set_document_change_seq(const std::shared_ptr<std::pair<std::string, const Data>>& new_document_pair, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair);
	void dec_document_change_cnt(std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair);
#endif
};


class Document {
	Xapian::docid did;

	DatabaseHandler* db_handler;

	Xapian::Document _get_document();

public:
	Document();
	Document(const Xapian::Document& doc_);
	Document(const Xapian::docid& did_, DatabaseHandler* db_handler_);

	Document(const Document& doc_) = default;
	Document& operator=(const Document& doc_) = default;

	Xapian::docid get_docid();

	std::string serialise(size_t retries=DB_RETRIES);
	std::string get_value(Xapian::valueno slot, size_t retries=DB_RETRIES);
	std::string get_data(size_t retries=DB_RETRIES);
	std::string get_blob(const ct_type_t& ct_type, size_t retries=DB_RETRIES);
	MsgPack get_terms(size_t retries=DB_RETRIES);
	MsgPack get_values(size_t retries=DB_RETRIES);

	MsgPack get_value(std::string_view slot_name);
	MsgPack get_obj();
	MsgPack get_field(std::string_view slot_name);
	static MsgPack get_field(std::string_view slot_name, const MsgPack& obj);

	uint64_t hash(size_t retries=DB_RETRIES);
};
