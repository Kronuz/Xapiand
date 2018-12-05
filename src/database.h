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

#include "config.h"               // for XAPIAND_BINARY_SERVERPORT, XAPIAND_BINARY_PROXY

#include <atomic>                 // for std::atomic_bool
#include <chrono>                 // for system_clock, system_clock::time_point
#include <cstring>                // for size_t
#include <memory>                 // for std::shared_ptr
#include <string>                 // for std::string
#include <utility>                // for std::pair
#include <vector>                 // for std::vector
#include <xapian.h>               // for Xapian::docid, Xapian::termcount, Xapian::Document

#include "cuuid/uuid.h"           // for UUID, UUID_LENGTH
#include "database_flags.h"       // for Data::Locator
#include "database_data.h"        // for Data::Locator


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

private:
#ifdef XAPIAND_DATA_STORAGE
	void storage_pull_blobs(Xapian::Document& doc, Xapian::docid did) const;
	void storage_push_blobs(Xapian::Document& doc, Xapian::docid did) const;
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

	void reopen_writable();
	void reopen_readable();

public:
	DatabaseEndpoint& endpoints;
	int flags;

	std::atomic_bool busy;

	std::chrono::system_clock::time_point reopen_time;
	Xapian::rev reopen_revision;
	bool modified;
	bool incomplete;

	bool is_writable;
	bool is_writable_and_local;
	bool is_writable_and_local_with_wal;

	bool closed;
	Transaction transaction;

	std::unique_ptr<Xapian::Database> _database;
	std::vector<std::pair<Xapian::Database, bool>> _databases;

#ifdef XAPIAND_DATA_STORAGE
	std::vector<std::unique_ptr<DataStorage>> writable_storages;
	std::vector<std::unique_ptr<DataStorage>> storages;
#endif /* XAPIAND_DATA_STORAGE */

#ifdef XAPIAND_DATABASE_WAL
	std::unique_ptr<ProducerToken> producer_token;
#endif

	std::shared_ptr<Logging> log;

	Database(DatabaseEndpoint& endpoints_, int flags);
	~Database();

	bool reopen();

	Xapian::Database* db();

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(Xapian::docid did, const Data::Locator& locator) const;
#endif /* XAPIAND_DATA_STORAGE */

	UUID get_uuid();
	std::string get_uuid_string();
	Xapian::rev get_revision();

	void reset();

	void do_close(bool commit_, bool closed_, Transaction transaction_);
	void close();

	static void autocommit(const std::shared_ptr<Database>& database);
	bool commit(bool wal_ = true, bool send_update = true);

	void begin_transaction(bool flushed = true);
	void commit_transaction();
	void cancel_transaction();

	void delete_document(Xapian::docid did, bool commit_ = false, bool wal_ = true);
	void delete_document_term(const std::string& term, bool commit_ = false, bool wal_ = true);
	Xapian::docid add_document(Xapian::Document&& doc, bool commit_ = false, bool wal_ = true);
	Xapian::docid replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true);
	Xapian::docid replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_ = false, bool wal_ = true);

	void add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_ = false, bool wal_ = true);
	Xapian::termcount remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_ = false, bool wal_ = true);

	Xapian::docid find_document(const std::string& term_id);
	Xapian::Document get_document(Xapian::docid did, bool assume_valid_ = false, bool pull_ = false);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_ = false, bool wal_ = true);

	void dump_metadata(int fd, XXH32_state_t* xxh_state);
	void dump_documents(int fd, XXH32_state_t* xxh_state);
	MsgPack dump_documents();

	std::string to_string() const;

	std::string __repr__() const;
};
