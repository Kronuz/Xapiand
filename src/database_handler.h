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

#pragma once

#include "config.h"

#include <condition_variable>                // for std::condition_variable
#include <memory>                            // for std::shared_ptr, std::make_shared
#include <stddef.h>                          // for size_t
#include <string>                            // for std::string
#include "string_view.hh"                    // for std::string_view
#include <unordered_map>                     // for std::unordered_map
#include <utility>                           // for std::pair
#include <vector>                            // for std::vector

#include "blocking_concurrent_queue.h"       // for BlockingConcurrentQueue
#include "database_flags.h"                  // for DB_*
#include "debouncer.h"                       // for make_debouncer
#include "endpoint.h"                        // for Endpoints
#include "http_parser.h"                     // for http_method
#include "lightweight_semaphore.h"           // for LightweightSemaphore
#include "lock_database.h"                   // for LockableDatabase
#include "msgpack.h"                         // for MsgPack
#include "opts.h"                            // for opts::*
#include "thread.hh"                         // for ThreadPolicyType::*
#include "xapian.h"                          // for Document, docid, MSet


class AggregationMatchSpy;
class Data;
class Locator;
class Database;
class DatabaseHandler;
class Document;
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

		MSetItem(Xapian::docid did) :
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

	MSet(Xapian::docid did) :
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

	void push_back(Xapian::docid did) {
		items.push_back(did);
		++matches_estimated;
	}
};

using DataType = std::pair<Xapian::docid, MsgPack>;

class DatabaseHandler : public LockableDatabase {
	friend Document;
	friend Schema;
	friend SchemasLRU;

	enum http_method method;
	std::shared_ptr<Schema> schema;

	std::shared_ptr<std::unordered_set<std::string>> context;

#ifdef XAPIAND_CHAISCRIPT
	static std::mutex documents_mtx;
	static std::unordered_map<std::string, std::shared_ptr<std::pair<std::string, const Data>>> documents;

	template<typename ProcessorCompile>
	std::unique_ptr<MsgPack> call_script(const MsgPack& object, std::string_view term_id, size_t script_hash, size_t body_hash, std::string_view script_name, std::string_view script_body, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair, const MsgPack& params);
	std::unique_ptr<MsgPack> run_script(const MsgPack& object, std::string_view term_id, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair, const MsgPack& data_script);
#endif

	std::tuple<std::string, Xapian::Document, MsgPack> prepare(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair);

	DataType index(const MsgPack& document_id, const MsgPack& obj, Data& data, std::shared_ptr<std::pair<std::string, const Data>> old_document_pair, bool commit);

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

	bool update_schema(std::chrono::time_point<std::chrono::system_clock> schema_begins);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, std::shared_ptr<std::unordered_set<std::string>> context_=nullptr);

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj=nullptr);

	void reset(const Endpoints& endpoints_, int flags_=0, enum http_method method_=HTTP_GET, const std::shared_ptr<std::unordered_set<std::string>>& context_=nullptr);

#if XAPIAND_DATABASE_WAL
	MsgPack repr_wal(uint32_t start_revision, uint32_t end_revision, bool unserialised);
#endif

	MsgPack check();

	std::tuple<std::string, Xapian::Document, MsgPack> prepare(const MsgPack& document_id, bool stored, const MsgPack& body, const ct_type_t& ct_type);

	DataType index(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);
	DataType patch(const MsgPack& document_id, const MsgPack& patches, bool commit, const ct_type_t& ct_type);
	DataType merge(const MsgPack& document_id, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);

	void write_schema(const MsgPack& obj, bool replace);
	void delete_schema();

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	MSet get_all_mset(std::string_view term = "", Xapian::docid initial = 0, size_t limit = -1);
	MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs);
	MSet get_mset(const Xapian::Query& query, unsigned offset = 0, unsigned limit = 10, unsigned check_at_least = 0, Xapian::KeyMaker* sorter = nullptr, Xapian::MatchSpy* spy = nullptr);

	void dump_metadata(int fd);
	void dump_schema(int fd);
	void dump_documents(int fd);
	void restore(int fd);

	MsgPack dump_documents();

	std::tuple<std::string, Xapian::Document, MsgPack> prepare_document(const MsgPack& obj);

	std::string get_prefixed_term_id(const MsgPack& document_id);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	std::string get_metadata(std::string_view key);
	bool set_metadata(const std::string& key, const std::string& value, bool commit = false, bool overwrite = true);
	bool set_metadata(std::string_view key, std::string_view value, bool commit = false, bool overwrite = true);

	Document get_document(Xapian::docid did);
	Document get_document(std::string_view document_id);
	Document get_document_term(const std::string& term_id);
	Document get_document_term(std::string_view term_id);
	Xapian::docid get_docid(std::string_view document_id);

	void delete_document(std::string_view document_id, bool commit = false);

	Xapian::docid replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit = false);

	MsgPack get_document_info(std::string_view document_id, bool raw_data);
	MsgPack get_database_info();

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Locator& locator, Xapian::docid did);
#endif /* XAPIAND_DATA_STORAGE */

	bool commit(bool wal = true);
	bool reopen();

#ifdef XAPIAND_CHAISCRIPT
	const std::shared_ptr<std::pair<std::string, const Data>> get_document_change_seq(std::string_view term_id, bool validate_exists = false);
	bool set_document_change_seq(const std::shared_ptr<std::pair<std::string, const Data>>& new_document_pair, std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair);
	void dec_document_change_cnt(std::shared_ptr<std::pair<std::string, const Data>>& old_document_pair);
#endif
};


class DocIndexer;


class DocPreparer {
	std::shared_ptr<DocIndexer> indexer;

	MsgPack obj;

	DocPreparer(const std::shared_ptr<DocIndexer>& indexer, MsgPack&& obj) :
		indexer{indexer},
		obj{std::move(obj)} { }

public:
	template<typename... Args>
	static auto make_unique(Args&&... args) {
		/*
		 * std::make_unique only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_unique : DocPreparer {
			enable_make_unique(Args&&... _args) : DocPreparer(std::forward<Args>(_args)...) { }
		};
		return std::make_unique<enable_make_unique>(std::forward<Args>(args)...);
	}

	void operator()();
};


class DocIndexer : public std::enable_shared_from_this<DocIndexer> {
	friend DocPreparer;

	static constexpr size_t limit_max = 16;
	static constexpr size_t limit_signal = 8;

	std::atomic_bool running;
	std::atomic_bool ready;

	Endpoints endpoints;
	int flags;
	enum http_method method;

	std::atomic_size_t _processed;
	std::atomic_size_t _indexed;
	size_t _total;
	LightweightSemaphore limit;
	LightweightSemaphore done;

	BlockingConcurrentQueue<std::tuple<std::string, Xapian::Document, MsgPack>> ready_queue;

	std::array<std::unique_ptr<DocPreparer>, ConcurrentQueueDefaultTraits::BLOCK_SIZE> bulk;
	size_t bulk_cnt;

	DocIndexer(const Endpoints& endpoints, int flags, enum http_method method) :
		running{true},
		ready{false},
		endpoints{endpoints},
		flags{flags},
		method{method},
		_processed{0},
		_indexed{0},
		_total{0},
		limit{limit_max},
		bulk_cnt{0} { }

	void _prepare(MsgPack&& obj);

public:
	template<typename... Args>
	static auto make_shared(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : DocIndexer {
			enable_make_shared(Args&&... _args) : DocIndexer(std::forward<Args>(_args)...) { }
		};
		return std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
	}

	void operator()();

	void prepare(MsgPack&& obj);

	bool wait(double timeout = -1.0);

	void finish();

	size_t processed() {
		return _processed.load(std::memory_order_relaxed);
	}

	size_t indexed() {
		return _indexed.load(std::memory_order_relaxed);
	}

	size_t total() {
		return _total;
	}
};


class Document {
	Xapian::docid did;

	DatabaseHandler* db_handler;

	Xapian::Document _get_document();

public:
	Document();
	Document(const Xapian::Document& doc_);
	Document(Xapian::docid did_, DatabaseHandler* db_handler_);

	Document(const Document& doc_) = default;
	Document& operator=(const Document& doc_) = default;

	Xapian::docid get_docid();

	std::string serialise(size_t retries=DB_RETRIES);
	std::string get_value(Xapian::valueno slot, size_t retries=DB_RETRIES);
	std::string get_data(size_t retries=DB_RETRIES);
	MsgPack get_terms(size_t retries=DB_RETRIES);
	MsgPack get_values(size_t retries=DB_RETRIES);

	MsgPack get_value(std::string_view slot_name);
	MsgPack get_obj();
	MsgPack get_field(std::string_view slot_name);
	static MsgPack get_field(std::string_view slot_name, const MsgPack& obj);

	uint64_t hash(size_t retries=DB_RETRIES);
};


void committer_commit(std::weak_ptr<Database> weak_database);


inline auto& committer(bool create = true) {
	static auto committer = create ? make_unique_debouncer<Endpoints, 1000, 3000, 9000, ThreadPolicyType::committers>("AC--", "AC{:02}", opts.num_committers, committer_commit) : nullptr;
	ASSERT(!create || committer);
	return committer;
}
