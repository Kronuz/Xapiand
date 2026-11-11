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

#include "config.h"               // for XAPIAND_DATA_STORAGE

#include <exception>              // for std::exception_ptr
#include <memory>                 // for std::shared_ptr
#include <optional>               // for std::optional
#include <string>                 // for std::string
#include <vector>                 // for std::vector

#include "database/flags.h"       // for DB_*
#include "database/pool.h"        // for ShardEndpoint
#include "engine/document.h"      // for engine::Document
#include "engine/doc_view.h"      // for engine::DocView
#include "engine/error.h"         // for engine::Error
#include "engine/shard.h"         // for engine::Shard
#include "engine/types.h"         // for engine::WriteResult
#include "task.hh"                // for task::task


class Node;
class Locator;


//   ____  _                   _
//  / ___|| |__   __ _ _ __ __| |
//  \___ \| '_ \ / _` | '__/ _` |
//   ___) | | | | (_| | | | (_| |
//  |____/|_| |_|\__,_|_|  \__,_|
//
// One use of an index: its pin and the flags the user asked with. Cheap to make and to drop.
// Every operation that can wait is a task, so the callers decide where it runs; a write takes the
// index's turn and holds it for its apply, its revision update and its WAL line, nothing longer.
class Shard {
	IndexPin pin_;
	DatabasePool* pool_;
	std::string path_;
	bool stale_ = false;  // a failure asked for a fresh index: reopen() hands the pin back and pins again
	bool missing_ = false;  // its index was missing at the pin: a use with no pin is not_found, not `closed`

public:
	int flags;
	// The request's token: this use's turn waits (a write's, a read's refresh) end when it is requested. Empty for
	// work that is not cancelled, such as an admitted write (docs/design/phase7c.md D3).
	task::cancel_token cancel;

	// `pin` is the use's pin (lock_shard's, taken before any view, x-pool D4).
	Shard(IndexPin pin, int flags_, task::cancel_token cancel_ = {});

	// A use with no pin yet (its index was missing when it pinned): ready() pins it, or raises not_found again.
	Shard(DatabasePool& pool, std::string path, int flags_, task::cancel_token cancel_ = {});

	// The index's entry, under the pin; raises not_found while the index is missing, `closed` while a reopen left
	// the use with no pin.
	ShardEndpoint& endpoint() const;

	bool is_local() const {
		return true;
	}

	bool is_writable() const {
		return has_db_writable(flags);
	}

	bool is_restore() const {
		return has_db_restore(flags);
	}

	bool is_autocommit_active() const {
		return !has_db_disable_autocommit(flags);
	}

	bool is_synchronous_wal() const {
		return has_db_synchronous_wal(flags);
	}

	bool is_wal_active() const {
		return is_writable() && !has_db_disable_wal(flags);
	}

	bool is_write_active() const {
		return !has_db_disable_writes(flags);
	}

	std::shared_ptr<const Node> node() const;

	// The end of the use: a restore that finishes deletes the log it did not write.
	void release() noexcept;

	// Pins the index again when a reopen gave the pin back (a missing one is created when the flags say so).
	task::task<void> ready(bool use = true);   // `use` false: a re-pin within the request, not a use (CT-R31)

	// The engine of the pinned index; raises `closed` with no pin.
	engine::Shard& engine();

	// ready(), then a refresh if something was applied since the last one, so the caller reads it.
	task::task<engine::Shard*> engine_for_read();

	// Where a write places a document: the engine's written reads (the id holding a key term, 0 for none, and the
	// live documents) as the writes left them, in the index's turn, refreshing nothing (docs/mechanisms/placement.md).
	task::task<engine::docid> written_key(const std::string& term);
	task::task<engine::doccount> written_count();

	// The engine's integrity check of the index, in its turn (a missing index is opened, or created by the flags).
	task::task<engine::CheckResult> check();

	// Hands the pin back when a failure marked the use stale, so the catalog replaces the index, and pins again;
	// true when the use had no usable pin.
	task::task<bool> reopen();

	// Called from a catch block of a read ladder: marks the use stale as the failure demands and rethrows when
	// it is final or not a failure of the shard; otherwise the caller reopens.
	void retry_read(const engine::Error& error, int t);

	// A fan-out over shards: remember a failure of this one so the next can go on, and rethrow what is
	// final. A write does not treat a stale snapshot as retryable. Called from a catch block.
	void collect_failure(const engine::Error& error, std::exception_ptr& eptr, size_t& valid, bool writes);

	// What follows a write: its commit when it asked for one, else the debounced autocommit. A commit whose turn
	// times out leaves the write standing (applied and logged) for the autocommit; the caller is not failed.
	task::task<void> finish_write(bool commit_, bool wal_);

	// Commits what is applied; a commit that covers this caller's writes and is already in flight is joined.
	task::task<bool> commit(bool wal_ = true);

	// A delete with an expected version removes the document only while it has that version (0: it does not exist).
	task::task<void> delete_document(engine::docid shard_did, bool commit_ = false, bool wal_ = true, std::optional<engine::rev> expected_ = std::nullopt);
	task::task<void> delete_document_term(const std::string& term, bool commit_ = false, bool wal_ = true, std::optional<engine::rev> expected_ = std::nullopt);

	task::task<engine::WriteResult> add_document(engine::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);
	task::task<engine::WriteResult> replace_document(engine::docid shard_did, engine::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true, bool must_exist = false);
	task::task<engine::WriteResult> replace_document_term(const std::string& term, engine::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);

	task::task<void> add_spelling(const std::string& word, engine::termcount freqinc, bool commit_ = false, bool wal_ = true);
	task::task<engine::termcount> remove_spelling(const std::string& word, engine::termcount freqdec, bool commit_ = false, bool wal_ = true);

	task::task<std::unique_ptr<engine::DocView>> get_view(engine::docid shard_did);

	task::task<std::vector<std::string>> get_metadata_keys();
	task::task<std::string> get_metadata(const std::string& key);
	task::task<void> set_metadata(const std::string& key, const std::string& value, bool commit_ = false, bool wal_ = true);

#ifdef XAPIAND_DATA_STORAGE
	// A blob of the index's data storage: its read is a chore, on a reader of its own.
	task::task<std::string> storage_get_stored(const Locator& locator);

	// Begins a streamed upload of a blob into a volume of the index's data storage that nothing else writes.
	task::task<std::unique_ptr<StoredUpload>> upload_begin(engine::Chores& chores, std::string ct_type, std::uint64_t size);
#endif /* XAPIAND_DATA_STORAGE */

	std::string to_string() const;

	std::string __repr__() const;
};


// The debounced autocommit of an index: a commit in its own turn, then its merges settled in another. True when a
// merge is still pending, so the caller arms the autocommit again (x-pool D8).
task::task<bool> autocommit(DatabasePool& pool, Endpoint endpoint);
