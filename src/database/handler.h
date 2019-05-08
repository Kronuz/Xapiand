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

#include <cassert>                           // for assert
#include <chrono>                            // for std::chrono
#include <condition_variable>                // for std::condition_variable
#include <memory>                            // for std::shared_ptr, std::make_shared
#include <stddef.h>                          // for size_t
#include <string>                            // for std::string
#include <string_view>                       // for std::string_view
#include <unordered_map>                     // for std::unordered_map
#include <utility>                           // for std::pair
#include <vector>                            // for std::vector

#include "blocking_concurrent_queue.h"       // for BlockingConcurrentQueue
#include "database/flags.h"                  // for DB_*
#include "debouncer.h"                       // for make_debouncer
#include "endpoint.h"                        // for Endpoints
#include "msgpack.h"                         // for MsgPack
#include "opts.h"                            // for opts::*
#include "thread.hh"                         // for ThreadPolicyType::*
#include "xapian.h"                          // for Document, docid, MSet


class AggregationMatchSpy;
class Data;
class Shard;
class Script;
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

using DocumentInfo = std::pair<Xapian::DocumentInfo, MsgPack>;

class DatabaseHandler {
	friend class Document;
	friend class Schema;
	friend class SchemasLRU;

	int flags;
	Endpoints endpoints;

	std::shared_ptr<Schema> schema;

	std::shared_ptr<std::unordered_set<std::string>> context;

#ifdef XAPIAND_CHAISCRIPT
	static std::mutex documents_mtx;
	static std::unordered_map<std::string, std::shared_ptr<std::pair<std::string, const Data>>> documents;

	std::unique_ptr<MsgPack> call_script(const MsgPack& object, const std::string& term_id, const Script& script, const Data& data);
#endif

	std::tuple<std::string, Xapian::Document, MsgPack> prepare(const MsgPack& document_id, Xapian::rev document_ver, const MsgPack& obj, Data& data);

	DocumentInfo index(Xapian::docid did, const MsgPack& document_id, Xapian::rev document_ver, const MsgPack& obj, Data& data, bool commit);

	std::unique_ptr<Xapian::ExpandDecider> get_edecider(const similar_field_t& similar);

	bool update_schema();

	MsgPack _dump_document(Xapian::docid did, const Data& data);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_ = 0, std::shared_ptr<std::unordered_set<std::string>> context_ = nullptr);

	std::shared_ptr<Database> get_database() const noexcept;
	std::shared_ptr<Schema> get_schema(const MsgPack* obj = nullptr);

	void reset(const Endpoints& endpoints_, int flags_ = 0, const std::shared_ptr<std::unordered_set<std::string>>& context_ = nullptr);

#if XAPIAND_DATABASE_WAL
	MsgPack repr_wal(Xapian::rev start_revision, Xapian::rev end_revision, bool unserialised);
#endif

	MsgPack check();

	std::tuple<std::string, Xapian::Document, MsgPack> prepare(const MsgPack& document_id, Xapian::rev document_ver, bool stored, const MsgPack& body, const ct_type_t& ct_type);

	DocumentInfo index(const MsgPack& document_id, Xapian::rev document_ver, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);
	DocumentInfo patch(const MsgPack& document_id, Xapian::rev document_ver, const MsgPack& patches, bool commit);
	DocumentInfo update(const MsgPack& document_id, Xapian::rev document_ver, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);

	void write_schema(const MsgPack& obj, bool replace);

	Xapian::RSet get_rset(const Xapian::Query& query, Xapian::doccount maxitems);
	Xapian::MSet get_mset(const query_field_t& e, const MsgPack* qdsl, AggregationMatchSpy* aggs);
	Xapian::MSet get_mset(
		const Xapian::Query& query = Xapian::Query(std::string()),
		Xapian::doccount first = 0,
		Xapian::doccount maxitems = 10,
		Xapian::doccount check_at_least = 0,
		Xapian::KeyMaker* sorter = nullptr,
		Xapian::valueno collapse_key = Xapian::BAD_VALUENO,
		Xapian::doccount collapse_max = 0,
		double percent_threshold = 0,
		double weight_threshold = 0,
		Xapian::Enquire::docid_order order = Xapian::Enquire::ASCENDING,
		AggregationMatchSpy* aggs = nullptr,
		const similar_field_t* fuzzy = nullptr,
		const similar_field_t* nearest = nullptr);

	MsgPack dump_document(Xapian::docid did);
	MsgPack dump_document(std::string_view document_id);
	MsgPack dump_documents();
	std::string dump_documents(int fd);
	std::string restore_documents(int fd);

	std::tuple<std::string, Xapian::Document, MsgPack> prepare_document(MsgPack& obj);

	std::string get_prefixed_term_id(const MsgPack& document_id);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	std::string get_metadata(std::string_view key);
	void set_metadata(const std::string& key, const std::string& value, bool commit = false, bool wal = true);
	void set_metadata(std::string_view key, std::string_view value, bool commit = false, bool wal = true);

	Document get_document(Xapian::docid did);
	Document get_document(std::string_view document_id);
	Document get_document_term(const std::string& term);
	Xapian::docid get_docid(std::string_view document_id);
	Xapian::docid get_docid_term(const std::string& term);

	void delete_document(Xapian::docid did, bool commit = false, bool wal = true, bool version = true);
	void delete_document(std::string_view document_id, bool commit = false, bool wal = true, bool version = true);
	void delete_document_term(const std::string& term, bool commit = false, bool wal = true, bool version = true);

	Xapian::DocumentInfo add_document(Xapian::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	Xapian::DocumentInfo replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	Xapian::DocumentInfo replace_document(std::string_view document_id, Xapian::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	Xapian::DocumentInfo replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit = false, bool wal = true, bool version = true);

	MsgPack get_document_info(std::string_view document_id, bool raw_data, bool human);
	MsgPack get_database_info();

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Locator& locator, Xapian::docid did);
#endif /* XAPIAND_DATA_STORAGE */

	bool commit(bool wal = true);
	void reopen();

	void do_close(bool commit_ = true);
};


class DocMatcher {
	friend DatabaseHandler;

	using dispatch_func = void (DocMatcher::*)();
	dispatch_func dispatcher;

	Xapian::doccount doccount;
	Xapian::rev revision;
	Xapian::Enquire enquire;

	std::atomic_size_t& pending;
	std::condition_variable& ready;
	size_t shard_num;
	const Endpoints& endpoints;
	int flags;
	Xapian::Query query;
	Xapian::doccount first;
	Xapian::doccount maxitems;
	Xapian::doccount check_at_least;
	std::unique_ptr<Xapian::KeyMaker> sorter;
	Xapian::valueno collapse_key;
	Xapian::doccount collapse_max;
	double percent_threshold;
	double weight_threshold;
	Xapian::Enquire::docid_order order;
	AggregationMatchSpy* aggs;
	const similar_field_t* nearest;
	Xapian::RSet nearest_rset;
	std::unique_ptr<Xapian::ExpandDecider> nearest_edecider;
	const similar_field_t* fuzzy;
	Xapian::RSet fuzzy_rset;
	std::unique_ptr<Xapian::ExpandDecider> fuzzy_edecider;
	const Xapian::Enquire& merger;

	void prepare_mset();
	void get_mset();

public:
	Xapian::MSet& mset;
	std::exception_ptr eptr;

	DocMatcher(
		std::atomic_size_t& pending,
		std::condition_variable& ready,
		size_t shard_num,
		const Endpoints& endpoints,
		int flags,
		const Xapian::Query query,
		Xapian::MSet& mset,
		Xapian::doccount first,
		Xapian::doccount maxitems,
		Xapian::doccount check_at_least,
		Xapian::KeyMaker* sorter,
		Xapian::valueno collapse_key,
		Xapian::doccount collapse_max,
		double percent_threshold,
		double weight_threshold,
		Xapian::Enquire::docid_order order,
		AggregationMatchSpy* aggs,
		const similar_field_t* nearest,
		const Xapian::RSet& nearest_rset,
		std::unique_ptr<Xapian::ExpandDecider>&& nearest_edecider,
		const similar_field_t* fuzzy,
		const Xapian::RSet& fuzzy_rset,
		std::unique_ptr<Xapian::ExpandDecider>&& fuzzy_edecider,
		const Xapian::Enquire& merger
	);

	void operator()();
};


class DocIndexer;


class DocPreparer {
	std::shared_ptr<DocIndexer> indexer;
	MsgPack obj;
	size_t idx;

	DocPreparer(const std::shared_ptr<DocIndexer>& indexer, MsgPack&& obj, size_t idx) :
		indexer{indexer},
		obj{std::move(obj)},
		idx{idx} { }

public:
	template <typename... Args>
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

	std::atomic_bool finished;
	std::atomic_bool running;
	std::atomic_bool ready;

	Endpoints endpoints;
	int flags;

	bool echo;
	bool comments;
	bool commit;

	std::atomic_size_t _processed;
	std::atomic_size_t _indexed;
	std::atomic_size_t _total;
	std::atomic_size_t _idx;
	std::condition_variable done;
	std::condition_variable limit;
	std::mutex limit_mtx;
	size_t limit_cnt;

	std::mutex _results_mtx;
	std::vector<MsgPack> _results;
	BlockingConcurrentQueue<std::tuple<std::string, Xapian::Document, MsgPack, size_t>> ready_queue;

	std::array<std::unique_ptr<DocPreparer>, ConcurrentQueueDefaultTraits::BLOCK_SIZE> bulk;
	size_t bulk_cnt;

	DocIndexer(const Endpoints& endpoints, int flags, bool echo, bool comments, bool commit) :
		finished{false},
		running{false},
		ready{false},
		endpoints{endpoints},
		flags{flags},
		echo{echo},
		comments{comments},
		commit{commit},
		_processed{0},
		_indexed{0},
		_total{0},
		_idx{0},
		limit_cnt{limit_max},
		bulk_cnt{0} { }

	void _prepare(MsgPack&& obj);

public:
	template <typename... Args>
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

	const std::vector<MsgPack>& results() {
		return _results;
	}
};


class Document {
	Xapian::docid did;

	DatabaseHandler* db_handler;

public:
	Document();
	Document(const Xapian::Document& doc_);
	Document(Xapian::docid did_, DatabaseHandler* db_handler_);

	Document(const Document& doc_) = default;
	Document& operator=(const Document& doc_) = default;

	Xapian::docid get_docid();
	bool validate();

	std::string serialise();
	std::string get_value(Xapian::valueno slot);
	std::string get_data();
	MsgPack get_terms();
	MsgPack get_values();

	MsgPack get_value(std::string_view slot_name);
	MsgPack get_obj();
	MsgPack get_field(std::string_view slot_name);
	static MsgPack get_field(std::string_view slot_name, const MsgPack& obj);

	uint64_t hash();
};


void committer_commit(std::weak_ptr<Shard> weak_shard);


inline auto& committer(bool create = true) {
	static auto committer = create ? make_unique_debouncer<Endpoint, ThreadPolicyType::committers>("AC--", "AC{:02}", opts.num_committers, committer_commit, std::chrono::milliseconds(opts.committer_throttle_time), std::chrono::milliseconds(opts.committer_debounce_timeout), std::chrono::milliseconds(opts.committer_debounce_busy_timeout), std::chrono::milliseconds(opts.committer_debounce_force_timeout)) : nullptr;
	assert(!create || committer);
	return committer;
}
