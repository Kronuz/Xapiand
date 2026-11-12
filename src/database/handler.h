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

#include <atomic>                            // for std::atomic
#include <cassert>                           // for assert
#include <chrono>                            // for std::chrono
#include <functional>                        // for std::function
#include <memory>                            // for std::shared_ptr, std::make_shared
#include <stddef.h>                          // for size_t
#include <string>                            // for std::string
#include <string_view>                       // for std::string_view
#include <unordered_map>                     // for std::unordered_map
#include <utility>                           // for std::pair
#include <vector>                            // for std::vector

#include "database/flags.h"                  // for DB_*
#include "endpoint.h"                        // for Endpoints
#include "engine/callbacks.h"                // for engine::KeyMaker, engine::MatchSpy
#include "engine/chores.h"                   // for engine::Chores
#include "engine/document.h"                 // for engine::Document
#include "engine/query.h"                    // for engine::Query
#include "engine/search.h"                   // for engine::SearchRequest, SearchResult
#include "engine/types.h"                    // for engine::WriteResult
#include "msgpack.h"                         // for MsgPack
#include "opts.h"                            // for opts::*
#include "task.hh"                           // for task::task
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

// Whether a document id is given: a missing or null id, an empty string or false is none; a number is one, 0 included.
bool has_document_id(const MsgPack& document_id);

// The key of a new document whose numeric id the shard numbers: a numeric key always has a value after its 'N'.
inline constexpr std::string_view NEW_NUMERIC_KEY = "N";

// The document id a numeric key names: the id when it is a whole number from 1 to below the largest docid, else 0
// (an id such as 0, -1, 1.5, 2^32-1 or 2^63 is placed by its key alone).
engine::docid numeric_docid(std::string_view key);

using DocumentInfo = std::pair<engine::WriteResult, MsgPack>;


// Where a document id across `shards` shards lives: the shard's position and the document's id in that shard,
// which is the id one shard numbers. (hit id = (docid - 1) * shards + shard + 1.) A hit id is 64 bits wide, a
// shard's docid 32: an id with no such docid, or 0, raises errc::doc_not_found.
struct SplitHitId {
	size_t shard;
	engine::docid did;
};
SplitHitId split_hit_id(engine::hitid did, size_t shards);

// Refuses a search over more shards than `max_search_indexes`, the runaway cap: a wider search is walked in
// batches, but the walk itself has a cost nobody asked for. Throws errc::invalid (a 400) naming the limit; the
// request is the client's to narrow, so retrying it cannot help. 0 is no limit (a tool that never ran main()).
void check_search_fanout(size_t shards, size_t max_search_indexes);

// The most indexes one search holds open at once: `--concurrent-search-indexes`, else a reactor's share of the
// indexes the pool holds open (the pool size over the reactors, at least 1).
size_t search_batch_indexes();

// The indexes searches hold open now, or the most they have held at once since the process started.
size_t search_pinned_indexes(bool most);

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

	// The budget's token, which the index uses this handler makes pass to their turn waits (empty without one).
	task::cancel_token turn_token() const { return budget != nullptr ? budget->token() : task::cancel_token{}; }

private:
#ifdef XAPIAND_LUA
	static std::mutex documents_mtx;
	static std::unordered_map<std::string, std::shared_ptr<std::pair<std::string, const Data>>> documents;
#endif

	task::task<std::unique_ptr<MsgPack>> call_script(const MsgPack& object, const std::string& term_id, const Script& script, const Data& data);

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare(const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, size_t idx = 0);

	task::task<DocumentInfo> index(engine::docid did, const MsgPack& document_id, engine::rev document_ver, const MsgPack& obj, Data& data, bool commit);

	// The docid holding the term in any shard, read from the published views or (written) as the writes left them.
	task::task<engine::docid> find_docid_term(const std::string& term, bool written);

	// The active shard with the fewest documents as the writes left them, where a new numeric id goes.
	task::task<size_t> least_used_shard();

	// Pins the indexes [begin, end) of the endpoints and searches them: one batch of a wide search.
	task::task<engine::BatchResult> search_batch(size_t begin, size_t end, const engine::SearchRequest& request);

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
	task::task<MsgPack> repr_wal(engine::rev start_revision, engine::rev end_revision, bool unserialised);
#endif

	task::task<MsgPack> check();

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, const ct_type_t& ct_type);

	task::task<DocumentInfo> index(const MsgPack& document_id, engine::rev document_ver, bool stored, const MsgPack& body, bool commit, const ct_type_t& ct_type);

	// The chunks of a body being uploaded, read one at a time.
	class BodyChunks {
	public:
		virtual ~BodyChunks() = default;
		// The next chunk into `chunk`; false at the end of the body.
		virtual task::task<bool> next(std::string& chunk) = 0;
	};

	// Where a streamed blob lies in the data storage of its shard.
	struct UploadedBlob {
		ssize_t volume = -1;
		size_t offset = 0;
		size_t size = 0;
		size_t shard = 0;   // the shard whose storage holds it
	};

	// Streams a blob of `size` bytes from `body` into the data storage of the shard that document `document_id` goes
	// to, in chores of `chores`. The document is indexed afterwards, with index_uploaded(), in a turn of its own. An
	// upload that fails, or whose body ends short, leaves nothing behind. Raises ClientError (a 4xx) when the blob
	// cannot be stored: a size past the limit, or an id that does not name its shard before it is indexed.
	task::task<UploadedBlob> upload_stored(const MsgPack& document_id, const ct_type_t& ct_type, size_t size, engine::Chores& chores, BodyChunks& body);

	// index() of a `stored` document whose blob upload_stored() wrote.
	task::task<DocumentInfo> index_uploaded(const MsgPack& document_id, engine::rev document_ver, const ct_type_t& ct_type, const UploadedBlob& blob, bool commit);
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
	// Restores a dump read from the descriptor into the index `endpoint`, blocking: the CLI's main thread runs it.
	// The index resolves as RESTORE resolves it, from the dump's first object.
	static task::task<std::string> restore_documents(const Endpoint& endpoint, int fd);

	task::task<std::tuple<std::string, engine::Document, MsgPack>> prepare_document(MsgPack& obj, size_t idx);

	task::task<std::string> get_prefixed_term_id(const MsgPack& document_id);

	task::task<std::vector<std::string>> get_metadata_keys();
	task::task<std::string> get_metadata(const std::string& key);
	task::task<std::string> get_metadata(std::string_view key);
	task::task<void> set_metadata(const std::string& key, const std::string& value, bool commit = false, bool wal = true);
	task::task<void> set_metadata(std::string_view key, std::string_view value, bool commit = false, bool wal = true);

	// By id the document is a handle: it reads the index when its data is asked for.
	Document get_document(engine::hitid did);
	task::task<Document> get_document(std::string_view document_id);
	task::task<Document> get_document_term(const std::string& term);
	task::task<engine::docid> get_docid(std::string_view document_id);
	task::task<engine::docid> get_docid_term(const std::string& term);
	// get_docid_term() as a write places a document: each shard as its writes left it, in its turn, flushing none.
	task::task<engine::docid> written_docid_term(const std::string& term);

	// A delete with a version (not UNKNOWN_REVISION) removes the document only while it has that version, 0 for one
	// that does not exist; else it raises errc::version_conflict with the current version.
	task::task<void> delete_document(engine::docid did, engine::rev document_ver, bool commit = false, bool wal = true);
	task::task<void> delete_document(std::string_view document_id, engine::rev document_ver, bool commit = false, bool wal = true);
	task::task<void> delete_document_term(const std::string& term, engine::rev document_ver, bool commit = false, bool wal = true);

	task::task<engine::WriteResult> add_document(engine::Document&& doc, bool commit = false, bool wal = true, bool version = true);
	task::task<engine::WriteResult> replace_document(engine::docid did, engine::Document&& doc, bool commit = false, bool wal = true, bool version = true, bool must_exist = false);
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


// What a RESTORE's run met (tests assert their case occurred): the largest range applied at once, the writes of an id
// already written in the same sub-run, the most shards applying at once, the objects that took the serial path
// after a join (a new numeric id), and the writes deferred by a miss between placement and apply.
struct DocIndexerStats {
	std::atomic<std::size_t> largest_range{0};
	std::atomic<std::size_t> repeated_in_subrun{0};
	std::atomic<std::size_t> peak_children{0};
	std::atomic<std::size_t> serial_after_join{0};
	std::atomic<std::size_t> deferred{0};
	std::atomic<std::size_t> children{0};   // the cap of the latest parallel run

	void reset() noexcept {
		largest_range = 0;
		repeated_in_subrun = 0;
		peak_children = 0;
		serial_after_join = 0;
		deferred = 0;
		children = 0;
	}
};

// Test seams (tests/test_loop_yield.cc), inert unless set: the objects a prepare chore takes instead of its
// time slice, a step awaited on the loop with the RESTORE's handler between a range's placement and its apply,
// one apply slice that throws before its writes, every write on the serial path (the model of a run), and the writes
// an apply slice takes, with a step awaited on the loop between two slices (the turn released) given the term of the
// slice's last write and whether it missed.
struct DocIndexerSeams {
	std::atomic<std::size_t> objects_per_prepare{0};
	std::atomic<std::size_t> writes_per_slice{0};
	std::function<task::task<void>(DatabaseHandler&)> after_placement;
	std::function<task::task<void>(DatabaseHandler&, const std::string& term, bool missed)> after_slice;
	std::atomic<bool> throw_in_slice{false};
	std::atomic<bool> serial{false};
};


// The bulk indexer of a RESTORE: prepares the objects of a stream in chores of the RESTORE's group and applies
// each to its shard, in order, on the reactor that serves the request (docs/design/phase7c.md D12). It owns no
// thread and waits for none: it yields when its slice is spent, and the parse of the next batch runs beside it.
class DocIndexer {
	Endpoints endpoints;
	int flags;
	engine::Chores& chores;

	bool echo;
	bool comments;
	engine::doccount first;
	engine::doccount maxitems;

	DatabaseHandler db_handler;

	size_t _prepared = 0;
	size_t _processed = 0;
	size_t _indexed = 0;
	size_t _total = 0;
	size_t _idx = 0;
	std::vector<MsgPack> _results;

public:
	static inline DocIndexerSeams seams;
	static inline DocIndexerStats stats;   // every RESTORE's, summed; tests reset it
	// `chores` runs the prepares (the RESTORE's group); `budget` is the request's time slice (null: the work runs
	// without yielding).
	DocIndexer(const Endpoints& endpoints, int flags, bool echo, bool comments, const query_field_t& query_field, engine::Chores& chores, task::budget* budget = nullptr);

	DocIndexer(const DocIndexer&) = delete;
	DocIndexer& operator=(const DocIndexer&) = delete;

	// Takes the objects of a batch in order (an array is the objects it holds): skips the first `offset`
	// of the stream, stops after `limit`, prepares and applies the rest, and records a result for each.
	task::task<void> index(std::vector<MsgPack>& batch);

	// Commits what was indexed (the closing commit of a restore).
	task::task<void> commit();

	size_t prepared() const noexcept {
		return _prepared;
	}

	size_t processed() const noexcept {
		return _processed;
	}

	size_t indexed() const noexcept {
		return _indexed;
	}

	size_t total() const noexcept {
		return _total;
	}

	const std::vector<MsgPack>& results() const noexcept {
		return _results;
	}
};


class Document {
	engine::hitid did;   // across the shards of the handler's endpoints

	DatabaseHandler* db_handler;

public:
	Document();
	Document(engine::hitid did_, DatabaseHandler* db_handler_);

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


