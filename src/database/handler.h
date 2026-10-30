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
#include "engine/callbacks.h"                // for engine::KeyMaker, engine::MatchSpy
#include "engine/document.h"                 // for engine::Document
#include "engine/query.h"                    // for engine::Query
#include "engine/search.h"                   // for engine::SearchRequest, SearchResult
#include "engine/types.h"                    // for engine::WriteResult
#include "msgpack.h"                         // for MsgPack
#include "opts.h"                            // for opts::*
#include "task.hh"                           // for task::task
#include "thread.hh"                         // for ThreadPolicyType::*
#include "engine/types.h"                  // for engine::rev, docid


class Data;
class Shard;
class Script;
class Locator;
class Database;
class DatabaseHandler;
class Document;
class Schema;
class SchemasLRU;
struct ct_type_t;
struct query_field_t;


engine::docid to_docid(std::string_view document_id);

using DocumentInfo = std::pair<engine::WriteResult, MsgPack>;


// Refuses a search over more shards than `max_indexes_per_search`: a search keeps every index it reads open for
// its whole run, and the pool holds no more open than its file-descriptor budget. Throws errc::invalid (a 400)
// naming the limit; the request is the client's to narrow, so retrying it cannot help. 0 is no limit (a tool
// that never ran main()'s sizing).
void check_search_fanout(size_t shards, size_t max_indexes_per_search);

class DatabaseHandler {
	friend class Document;
	friend class Schema;
	friend class SchemasLRU;

	int flags;
	Endpoints endpoints;

	std::shared_ptr<Schema> schema;

	std::shared_ptr<std::unordered_set<std::string>> context;

public:
	// The time slice a search of this handler runs in (a request's budget); null runs a search to its end
	// where it is awaited. Not owned.
	task::budget* budget = nullptr;

private:
#ifdef XAPIAND_LUA
	static std::mutex documents_mtx;
	static std::unordered_map<std::string, std::shared_ptr<std::pair<std::string, const Data>>> documents;
#endif

	task::task<std::unique_ptr<MsgPack>> call_script(const MsgPack& object, const std::string& term_id, const Script& script, const Data& data);

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare(const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, size_t idx = 0);

	task::task<DocumentInfo> index(engine::docid did, const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, bool commit);

	// The match proper, on a query the engine already read.
	task::task<engine::SearchResult> match(
		const engine::PreparedQuery& query,
		engine::doccount first,
		engine::doccount maxitems,
		engine::doccount check_at_least,
		const engine::KeyMaker* sorter,
		engine::valueno collapse_key,
		engine::doccount collapse_max,
		engine::DocidOrder order,
		engine::MatchSpy* aggs);

	task::task<bool> update_schema();

	task::task<MsgPack> _dump_document(engine::docid did, const Data& data);

public:
	DatabaseHandler();
	DatabaseHandler(const Endpoints& endpoints_, int flags_ = 0, std::shared_ptr<std::unordered_set<std::string>> context_ = nullptr);

	task::task<std::shared_ptr<Schema>> get_schema(const MsgPack* obj = nullptr);

	void reset(const Endpoints& endpoints_, int flags_ = 0, const std::shared_ptr<std::unordered_set<std::string>>& context_ = nullptr);

#if XAPIAND_DATABASE_WAL
	MsgPack repr_wal(engine::rev start_revision, engine::rev end_revision, bool unserialised);
#endif

	task::task<MsgPack> check();

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, const ct_type_t& ct_type);

	task::task<DocumentInfo> index(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);
	task::task<DocumentInfo> patch(const MsgPack& document_id, engine::rev document_ver, bool create, const MsgPack& patches, bool commit);
	task::task<DocumentInfo> update(const MsgPack& document_id, engine::rev document_ver, bool stored, bool create, const MsgPack& body, bool commit, const ct_type_t& ct_type);

	task::task<void> update_schema(const MsgPack& obj);
	task::task<void> write_schema(const MsgPack& obj);


	task::task<std::tuple<engine::SearchResult, MsgPack, engine::Query>> get_mset(const query_field_t& e, const MsgPack* qdsl, engine::MatchSpy* aggs);
	task::task<engine::SearchResult> get_mset(
		const engine::Query& query = engine::Query::match_all(),
		engine::doccount first = 0,
		engine::doccount maxitems = 10,
		engine::doccount check_at_least = 0,
		const engine::KeyMaker* sorter = nullptr,
		engine::valueno collapse_key = engine::BAD_VALUENO,
		engine::doccount collapse_max = 0,
		engine::DocidOrder order = engine::DocidOrder::ascending,
		engine::MatchSpy* aggs = nullptr);

	task::task<MsgPack> dump_document(engine::docid did);
	task::task<MsgPack> dump_document(std::string_view document_id);
	task::task<MsgPack> dump_documents();
	task::task<std::string> dump_documents(int fd);
	// Reads a dump from the descriptor, blocking: the CLI's main thread runs it.
	std::string restore_documents(int fd);

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare_document(MsgPack& obj, size_t idx);

	task::task<std::string> get_prefixed_term_id(const MsgPack& document_id);

	task::task<std::vector<std::string>> get_metadata_keys();
	task::task<std::string> get_metadata(const std::string& key);
	task::task<std::string> get_metadata(std::string_view key);
	task::task<void> set_metadata(const std::string& key, const std::string& value, bool commit = false, bool wal = true);
	task::task<void> set_metadata(std::string_view key, std::string_view value, bool commit = false, bool wal = true);

	// By id the document is a handle: it reads the index when its data is asked for.
	Document get_document(engine::docid did);
	task::task<Document> get_document(std::string_view document_id);
	task::task<Document> get_document_term(const std::string& term);
	task::task<engine::docid> get_docid(std::string_view document_id);
	task::task<engine::docid> get_docid_term(const std::string& term);

	task::task<void> delete_document(engine::docid did, bool commit = false, bool wal = true, bool version = true);
	task::task<void> delete_document(std::string_view document_id, bool commit = false, bool wal = true, bool version = true);
	task::task<void> delete_document_term(const std::string& term, bool commit = false, bool wal = true, bool version = true);

	task::task<engine::WriteResult> add_document(engine::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	task::task<engine::WriteResult> replace_document(engine::docid did, engine::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	task::task<engine::WriteResult> replace_document(std::string_view document_id, engine::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	task::task<engine::WriteResult> replace_document_term(const std::string& term, engine::Document&& doc, bool commit = false, bool wal = true, bool version = true);

	task::task<MsgPack> get_document_info(std::string_view document_id, bool raw_data, bool human);
	task::task<MsgPack> get_database_info();

#ifdef XAPIAND_DATA_STORAGE
	task::task<std::string> storage_get_stored(const Locator& locator, engine::docid did);
#endif /* XAPIAND_DATA_STORAGE */

	task::task<bool> commit(bool wal = true);
	task::task<void> reopen();

	task::task<MsgPack> unserialise_term_id(std::string_view term_id);
};


class DocIndexer;


class DocPreparer {
	std::shared_ptr<DocIndexer> indexer;
	MsgPack obj;
	size_t idx;

	DocPreparer(const std::shared_ptr<DocIndexer>& indexer, MsgPack&& obj, size_t idx) :
		indexer(indexer),
		obj(std::move(obj)),
		idx(idx) { }

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

	int indexers;
	std::atomic_int started;
	std::atomic_int running;
	std::atomic_bool finished;
	std::atomic_bool ready;

	Endpoints endpoints;
	int flags;

	bool echo;
	bool comments;
	engine::doccount first;
	engine::doccount maxitems;

	std::atomic_size_t _prepared;
	std::atomic_size_t _processed;
	std::atomic_size_t _indexed;
	std::atomic_size_t _total;
	std::atomic_size_t _idx;
	std::condition_variable done;
	std::condition_variable all_started;

	std::mutex _results_mtx;
	std::vector<MsgPack> _results;
	std::vector<std::unique_ptr<BlockingConcurrentQueue<std::tuple<std::string, engine::Document, MsgPack, size_t>>>> ready_queues;

	std::array<std::unique_ptr<DocPreparer>, ConcurrentQueueDefaultTraits::block_size> bulk;
	size_t bulk_cnt;

	DocIndexer(const Endpoints& endpoints, int flags, bool echo, bool comments, const query_field_t& query_field);

	void _prepare(MsgPack&& obj);

public:
	~DocIndexer();

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

	size_t prepared() {
		return _prepared.load(std::memory_order_relaxed);
	}

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
	engine::docid did;

	DatabaseHandler* db_handler;

public:
	Document();
	Document(engine::docid did_, DatabaseHandler* db_handler_);

	Document(const Document& doc_) = default;
	Document& operator=(const Document& doc_) = default;

	engine::docid get_docid();
	task::task<bool> validate();

	task::task<std::string> serialise();
	task::task<std::string> get_value(engine::valueno slot);
	task::task<std::string> get_data();
	task::task<MsgPack> get_values();

	task::task<MsgPack> get_value(std::string_view slot_name);
	task::task<MsgPack> get_obj();
	task::task<MsgPack> get_field(std::string_view slot_name);
	static MsgPack get_field(std::string_view slot_name, const MsgPack& obj);
};


void committer_commit(Endpoint endpoint);


inline auto& committer(bool create = true) {
	static auto committer = create ? make_unique_debouncer<Endpoint, ThreadPolicyType::committers>("AC--", "AC{:02}", opts.num_committers, committer_commit, std::chrono::milliseconds(opts.committer_throttle_time), std::chrono::milliseconds(opts.committer_debounce_timeout), std::chrono::milliseconds(opts.committer_debounce_busy_timeout), std::chrono::milliseconds(opts.committer_debounce_min_force_timeout), std::chrono::milliseconds(opts.committer_debounce_max_force_timeout)) : nullptr;
	assert(!create || committer);
	return committer;
}
