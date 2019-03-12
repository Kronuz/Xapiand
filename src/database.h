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

#include "config.h"               // for XAPIAND_REMOTE_SERVERPORT

#include <atomic>                 // for std::atomic_bool
#include <chrono>                 // for system_clock, system_clock::time_point
#include <cstring>                // for size_t
#include <memory>                 // for std::shared_ptr
#include <string>                 // for std::string
#include <utility>                // for std::pair
#include <vector>                 // for std::vector

#include "endpoint.h"             // for Endpoints
#include "cuuid/uuid.h"           // for UUID, UUID_LENGTH
#include "database_flags.h"       // for DB_OPEN
#include "lz4/xxhash.h"           // for XXH32_state_t
#include "string.hh"              // for string::join
#include "xapian.h"               // for Xapian::docid, Xapian::termcount, Xapian::Document


class Locator;
class Logging;
class MsgPack;
class DataStorage;
class DatabaseEndpoint;

namespace moodycamel {
	struct ProducerToken;
}
using namespace moodycamel;


inline std::string readable_flags(int flags) {
	std::vector<std::string> values;
	if ((flags & DB_OPEN) == DB_OPEN) values.push_back("DB_OPEN");
	if ((flags & DB_WRITABLE) == DB_WRITABLE) values.push_back("DB_WRITABLE");
	if ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN) values.push_back("DB_CREATE_OR_OPEN");
	if ((flags & DB_NO_WAL) == DB_NO_WAL) values.push_back("DB_NO_WAL");
	if ((flags & DB_NOSTORAGE) == DB_NOSTORAGE) values.push_back("DB_NOSTORAGE");
	return string::join(values, "|");
}

//  ____        _        _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|
//
class Database {
public:
	enum class Transaction : uint8_t {
		none,
		flushed,
		unflushed,
	};

	////////////////////////////////////////////////////////////////////////////
	// Raw Database:
	// TODO: Split Shard Database and Raw Database into two classes
private:
#ifdef XAPIAND_DATA_STORAGE
	std::pair<std::string, std::string> storage_push_blobs(std::string&& doc_data);
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

	bool reopen_writable();
	bool reopen_readable();

public:
	DatabaseEndpoint* endpoint;
	int flags;

	std::atomic_bool busy;

	std::chrono::system_clock::time_point reopen_time;
	Xapian::rev reopen_revision;

	std::atomic_bool local;
	std::atomic_bool closed;
	std::atomic_bool modified;
	std::atomic_bool incomplete;

	bool is_local() const {
		return local.load(std::memory_order_relaxed);
	}

	bool is_closed() const {
		return closed.load(std::memory_order_relaxed);
	}

	bool is_modified() const {
		return modified.load(std::memory_order_relaxed);
	}

	bool is_incomplete() const {
		return incomplete.load(std::memory_order_relaxed);
	}

	bool is_writable() const {
		return (flags & DB_WRITABLE) == DB_WRITABLE;
	}

	bool is_wal_active() const {
		return is_writable() && is_local() && (flags & DB_NO_WAL) != DB_NO_WAL;
	}

	bool is_busy() const {
		return busy.load(std::memory_order_relaxed);
	}

	std::unique_ptr<Xapian::Database> _database;

#ifdef XAPIAND_DATA_STORAGE
	std::unique_ptr<DataStorage> writable_storage;
	std::unique_ptr<DataStorage> storage;
#endif /* XAPIAND_DATA_STORAGE */

#ifdef XAPIAND_DATABASE_WAL
	ProducerToken* producer_token;
#endif

	std::shared_ptr<Logging> log;

	Transaction transaction;

	Database(DatabaseEndpoint* endpoint_, int flags);
	~Database() noexcept;

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Locator& locator);
#endif /* XAPIAND_DATA_STORAGE */


	////////////////////////////////////////////////////////////////////////////
	// Shard Database:
	// TODO: Split Shard Database and Raw Database into two classes
public:
	std::vector<std::shared_ptr<Database>> _shards;
	Endpoints endpoints;

	Database(std::vector<std::shared_ptr<Database>>&& shards, const Endpoints& endpoints_, int flags_);

	////////////////////////////////////////////////////////////////////////////
	// Common methods:

	bool reopen();

	Xapian::Database* db();

	UUID get_uuid();
	std::string get_uuid_string();
	Xapian::rev get_revision();

	void reset() noexcept;

	void do_close(bool commit_, bool closed_, Transaction transaction_, bool throw_exceptions = true);
	void close();

	static void autocommit(const std::shared_ptr<Database>& database);
	bool commit(bool wal_ = true, bool send_update = true);

	void begin_transaction(bool flushed = true);
	void commit_transaction();
	void cancel_transaction();

	void delete_document(Xapian::docid did, bool commit_ = false, bool wal_ = true, bool version_ = true);
	void delete_document_term(const std::string& term, bool commit_ = false, bool wal_ = true, bool version_ = true);

	Xapian::docid add_document(Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);
	Xapian::docid replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);
	Xapian::docid replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true, bool version_ = true);

	void add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_ = false, bool wal_ = true);
	Xapian::termcount remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_ = false, bool wal_ = true);

	Xapian::docid find_document(const std::string& term);
	Xapian::Document get_document(Xapian::docid did, bool assume_valid_ = false);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_ = false, bool wal_ = true);

	void dump_metadata(int fd, XXH32_state_t* xxh_state);
	void dump_documents(int fd, XXH32_state_t* xxh_state);
	MsgPack dump_documents();

	std::string to_string() const;

	std::string __repr__() const;
};
