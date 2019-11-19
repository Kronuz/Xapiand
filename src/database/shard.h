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

#include "config.h"               // for XAPIAND_DATA_STORAGE

#include <atomic>                 // for std::atomic_bool
#include <chrono>                 // for std::chrono
#include <memory>                 // for std::shared_ptr
#include <string>                 // for std::string
#include <utility>                // for std::pair
#include <vector>                 // for std::vector

#include "cuuid/uuid.h"           // for UUID, UUID_LENGTH
#include "database/flags.h"       // for DB_*
#include "xapian.h"               // for Xapian::docid, Xapian::termcount, Xapian::Document


class Node;
class Locator;
class Logging;
class DataStorage;
class ShardEndpoint;


//   ____  _                   _
//  / ___|| |__   __ _ _ __ __| |
//  \___ \| '_ \ / _` | '__/ _` |
//   ___) | | | | (_| | | | (_| |
//  |____/|_| |_|\__,_|_|  \__,_|
//
class Shard {
	friend class ShardEndpoint;
	friend class DatabasePool;
	friend class DatabaseWAL;
	friend class lock_shard;

public:
	enum class Transaction : uint8_t {
		none,
		flushed,
		unflushed,
	};

private:
	std::chrono::steady_clock::time_point reopen_time;
	Xapian::rev reopen_revision;

	std::atomic<bool> _busy;
	std::atomic<bool> _local;
	std::atomic<bool> _closed;
	std::atomic<bool> _modified;
	std::atomic<bool> _incomplete;
	std::atomic<Transaction> _transaction;

	std::unique_ptr<Xapian::Database> database;

#ifdef XAPIAND_DATA_STORAGE
	std::unique_ptr<DataStorage> writable_storage;
	std::unique_ptr<DataStorage> storage;
#endif /* XAPIAND_DATA_STORAGE */

	std::shared_ptr<Logging> log;

#ifdef XAPIAND_DATA_STORAGE
	std::pair<std::string, std::string> storage_push_blobs(std::string&& doc_data);
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

	bool reopen_writable();
	bool reopen_readable();

public:

	ShardEndpoint& endpoint;
	int flags;

	bool is_local() const {
		return _local.load(std::memory_order_relaxed);
	}

	bool is_closed() const {
		return _closed.load(std::memory_order_relaxed);
	}

	bool is_modified() const {
		return _modified.load(std::memory_order_relaxed);
	}

	bool is_incomplete() const {
		return _incomplete.load(std::memory_order_relaxed);
	}

	bool is_writable() const {
		return has_db_writable(flags);
	}

	bool is_replica() const {
		return has_db_replica(flags);
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
		return is_writable() && is_local() && !has_db_disable_wal(flags);
	}

	bool is_write_active() const {
		return !has_db_disable_writes(flags);
	}

	bool is_busy() const {
		return _busy.load(std::memory_order_relaxed);
	}

	Transaction transactional() const {
		return _transaction.load(std::memory_order_relaxed);
	}

	bool is_transactional() const {
		return transactional() != Transaction::none;
	}

	Shard(ShardEndpoint& endpoint_, int flags_, bool busy_);
	~Shard() noexcept;

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Locator& locator);
#endif /* XAPIAND_DATA_STORAGE */

	bool reopen();

	Xapian::Database* db();
	unsigned refs() const;

	std::shared_ptr<const Node> node() const;

	void reset() noexcept;

	void do_close(bool commit_, bool closed_, Transaction transaction_, bool throw_exceptions = true);
	void do_close(bool commit_ = true);
	void close();

	static void autocommit(const std::shared_ptr<Shard>& shard);
	bool commit(bool wal_ = true, bool send_update = true);

	void begin_transaction(bool flushed = true);
	void commit_transaction();
	void cancel_transaction();

	void delete_document(Xapian::docid shard_did, bool commit_ = false, bool wal_ = true, bool version_ = true);
	void delete_document_term(const std::string& term, bool commit_ = false, bool wal_ = true, bool version_ = true);

	Xapian::DocumentInfo add_document(Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);
	Xapian::DocumentInfo replace_document(Xapian::docid shard_did, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);
	Xapian::DocumentInfo replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);

	void add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_ = false, bool wal_ = true);
	Xapian::termcount remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_ = false, bool wal_ = true);

	Xapian::docid get_docid_term(const std::string& term);

	Xapian::Document get_document(Xapian::docid shard_did, unsigned doc_flags = 0);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_ = false, bool wal_ = true);

	std::string to_string() const;

	std::string __repr__() const;
};
