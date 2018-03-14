/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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
#include <string_view>                       // for std::string_view
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

		MSetItem(const Xapian::MSetIterator& it) {
			did = *it;
			rank = it.get_rank();
			weight = it.get_weight();
			percent = it.get_percent();
		}
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
	MSet() = default;
	MSet(const Xapian::MSet& mset) {
		auto it_end = mset.end();
		for (auto it = mset.begin(); it != it_end; ++it) {
			items.push_back(it);
		}
		matches_estimated = mset.get_matches_estimated();
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

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	static std::mutex documents_mtx;
	static std::unordered_map<size_t, std::shared_ptr<std::pair<size_t, const MsgPack>>> documents;

	template<typename ProcessorCompile>
	MsgPack& call_script(MsgPack& data, std::string_view term_id, size_t script_hash, size_t body_hash, std::string_view script_body, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair);
	MsgPack& run_script(MsgPack& data, std::string_view term_id, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair, const MsgPack& data_script);
#endif

	DataType index(std::string_view document_id, bool stored, std::string_view stored_locator, MsgPack& obj, std::string_view blob, bool commit_, const ct_type_t& ct_type);

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

	DataType index(std::string_view document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);
	DataType patch(std::string_view document_id, const MsgPack& patches, bool commit_, const ct_type_t& ct_type);
	DataType merge(std::string_view document_id, bool stored, const MsgPack& body, bool commit_, const ct_type_t& ct_type);

	void write_schema(const MsgPack& obj, bool replace);
	void delete_schema();

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs, std::vector<std::string>& suggestions);

	void dump_metadata(int fd);
	void dump_schema(int fd);
	void dump_documents(int fd);
	void restore(int fd);

	std::string get_prefixed_term_id(std::string_view document_id);

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

	MsgPack get_document_info(std::string_view document_id);
	MsgPack get_database_info();

	bool commit(bool _wal=true);
	bool reopen();
	long long get_mastery_level();

	static void init_ref(const Endpoint& endpoint);
	static void inc_ref(const Endpoint& endpoint);
	static void dec_ref(const Endpoint& endpoint);
	static int get_master_count();

#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
	void dec_document_change_cnt(std::string_view term_id);
	const std::shared_ptr<std::pair<size_t, const MsgPack>> get_document_change_seq(std::string_view term_id);
	bool set_document_change_seq(std::string_view term_id, const std::shared_ptr<std::pair<size_t, const MsgPack>>& new_document_pair, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair);
#endif
};


class Document {
	Xapian::docid did;

	DatabaseHandler* db_handler;

	Xapian::Document get_document();

public:
	Document();
	Document(const Xapian::Document& doc_);
	Document(DatabaseHandler* db_handler_, const Xapian::Document& doc_);

	Document(const Document& doc_);
	Document& operator=(const Document& doc_);

	Xapian::docid get_docid();

	std::string serialise(size_t retries=DB_RETRIES);
	std::string get_value(Xapian::valueno slot, size_t retries=DB_RETRIES);
	std::string get_data(size_t retries=DB_RETRIES);
	std::string get_blob(size_t retries=DB_RETRIES);
	MsgPack get_terms(size_t retries=DB_RETRIES);
	MsgPack get_values(size_t retries=DB_RETRIES);

	MsgPack get_value(std::string_view slot_name);
	std::pair<bool, std::string_view> get_store();
	MsgPack get_obj();
	MsgPack get_field(std::string_view slot_name);
	static MsgPack get_field(std::string_view slot_name, const MsgPack& obj);

	uint64_t hash(size_t retries=DB_RETRIES);
};
