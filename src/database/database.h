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
#include <memory>                 // for std::shared_ptr
#include <string>                 // for std::string
#include <utility>                // for std::pair
#include <vector>                 // for std::vector

#include "endpoint.h"             // for Endpoints
#include "cuuid/uuid.h"           // for UUID
#include "lz4/xxhash.h"           // for XXH32_state_t
#include "xapian.h"               // for Xapian::docid, Xapian::termcount, Xapian::Document


class Shard;
class Locator;
class Logging;
class MsgPack;


//  ____        _        _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|
//
class Database {
	friend class DatabasePool;

#ifdef XAPIAND_DATA_STORAGE
	std::pair<std::string, std::string> storage_push_blobs(std::string&& doc_data);
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

	bool reopen_writable();
	bool reopen_readable();

	std::atomic_bool closed;

	std::vector<std::shared_ptr<Shard>> shards;
	std::unique_ptr<Xapian::Database> database;

	std::shared_ptr<Logging> log;

public:
	bool is_closed() const {
		return closed.load(std::memory_order_relaxed);
	}

	~Database() noexcept;

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Locator& locator, Xapian::docid did);
#endif /* XAPIAND_DATA_STORAGE */

	Endpoints endpoints;
	int flags;

	Database(const std::vector<std::shared_ptr<Shard>>& shards, const Endpoints& endpoints_, int flags_);

	bool reopen();

	Xapian::Database* db();

	UUID get_uuid();
	std::string get_uuid_string();
	Xapian::rev get_revision();

	void reset() noexcept;

	void do_close(bool commit_, bool closed_, bool throw_exceptions = true);
	void close();

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
