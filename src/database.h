/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <atomic>               // for atomic_bool
#include <chrono>               // for system_clock, system_clock::time_point
#include <cstring>              // for size_t
#include <list>                 // for __list_iterator, operator!=
#include <memory>               // for shared_ptr, enable_shared_from_this, mak...
#include <mutex>                // for mutex, condition_variable, unique_lock
#include <stdexcept>            // for range_error
#include <string>               // for string, operator!=
#include <sys/types.h>          // for uint32_t, uint8_t, ssize_t
#include <type_traits>          // for forward
#include <unordered_map>        // for unordered_map
#include <unordered_set>        // for unordered_set
#include <utility>              // for pair, make_pair
#include <vector>               // for vector
#include <xapian.h>             // for docid, termcount, Document, ExpandDecider

#include "atomic_shared_ptr.h"  // for atomic_shared_ptr
#include "database_utils.h"     // for DB_WRITABLE
#include "endpoint.h"           // for Endpoints, Endpoint
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...
#include "queue.h"              // for Queue, QueueSet
#include "storage.h"            // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "threadpool.h"         // for TaskQueue


class Database;
class DatabasePool;
class DatabaseQueue;
class DatabasesLRU;
class MsgPack;
struct WalHeader;

constexpr int RECOVER_REMOVE_WRITABLE         = 0x01; // Remove endpoint from writable database
constexpr int RECOVER_REMOVE_DATABASE         = 0x02; // Remove endpoint from database
constexpr int RECOVER_REMOVE_ALL              = 0x04; // Remove endpoint from writable database and database
constexpr int RECOVER_DECREMENT_COUT          = 0x08; // Decrement count queue


#define WAL_SLOTS ((STORAGE_BLOCK_SIZE - sizeof(WalHeader::StorageHeaderHead)) / sizeof(uint32_t))


#if XAPIAND_DATABASE_WAL
struct WalHeader {
	struct StorageHeaderHead {
		uint32_t magic;
		uint32_t offset;
		char uuid[36];
		uint32_t revision;
	} head;

	uint32_t slot[WAL_SLOTS];

	void init(void* param, void* args);
	void validate(void* param, void* args);
};


#pragma pack(push, 1)
struct WalBinHeader {
	uint8_t magic;
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void*, void*, uint32_t size_, uint8_t flags_) {
		magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void*, void*) {
		if (magic != STORAGE_BIN_HEADER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad line header magic number");
		}
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Line deleted");
		}
	}
};


struct WalBinFooter {
	uint32_t checksum;
	uint8_t magic;

	inline void init(void*, void*, uint32_t checksum_) {
		magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(void*, void*, uint32_t checksum_) {
		if (magic != STORAGE_BIN_FOOTER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad line footer magic number");
		}
		if (checksum != checksum_) {
			THROW(StorageCorruptVolume, "Bad line checksum");
		}
	}
};
#pragma pack(pop)


class DatabaseWAL : Storage<WalHeader, WalBinHeader, WalBinFooter> {
	friend WalHeader;

	static constexpr const char* const names[] = {
		"ADD_DOCUMENT",
		"CANCEL",
		"DELETE_DOCUMENT_TERM",
		"COMMIT",
		"REPLACE_DOCUMENT",
		"REPLACE_DOCUMENT_TERM",
		"DELETE_DOCUMENT",
		"SET_METADATA",
		"ADD_SPELLING",
		"REMOVE_SPELLING",
		"MAX",
	};

	bool modified;
	bool validate_uuid;

	bool execute(const std::string& line);
	uint32_t highest_valid_slot();

	inline void open(const std::string& path, int flags, bool commit_eof=false) {
		Storage<WalHeader, WalBinHeader, WalBinFooter>::open(path, flags, reinterpret_cast<void*>(commit_eof));
	}

public:
	enum class Type : uint8_t {
		ADD_DOCUMENT,
		CANCEL,
		DELETE_DOCUMENT_TERM,
		COMMIT,
		REPLACE_DOCUMENT,
		REPLACE_DOCUMENT_TERM,
		DELETE_DOCUMENT,
		SET_METADATA,
		ADD_SPELLING,
		REMOVE_SPELLING,
		MAX,
	};

	Database* database;

	DatabaseWAL(const std::string& base_path_, Database* database_);
	~DatabaseWAL();

	bool open_current(bool current);

	bool init_database();
	void write_line(Type type, const std::string& data, bool commit=false);
	void write_add_document(const Xapian::Document& doc);
	void write_cancel();
	void write_delete_document_term(const std::string& term);
	void write_commit();
	void write_replace_document(Xapian::docid did, const Xapian::Document& doc);
	void write_replace_document_term(const std::string& term, const Xapian::Document& doc);
	void write_delete_document(Xapian::docid did);
	void write_set_metadata(const std::string& key, const std::string& val);
	void write_add_spelling(const std::string& word, Xapian::termcount freqinc);
	void write_remove_spelling(const std::string& word, Xapian::termcount freqdec);
};
#endif /* XAPIAND_DATABASE_WAL */


#ifdef XAPIAND_DATA_STORAGE
struct DataHeader {
	struct DataHeaderHead {
		uint32_t magic;
		uint32_t offset;  // required
		char uuid[36];
	} head;

	char padding[(STORAGE_BLOCK_SIZE - sizeof(DataHeader::DataHeaderHead)) / sizeof(char)];

	void init(void* param, void* args);
	void validate(void* param, void* args);
};


#pragma pack(push, 1)
struct DataBinHeader {
	uint8_t magic;
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void*, void*, uint32_t size_, uint8_t flags_) {
		magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void*, void*) {
		if (magic != STORAGE_BIN_HEADER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad document header magic number");
		}
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Data Storage document deleted");
		}
	}
};


struct DataBinFooter {
	uint32_t checksum;
	uint8_t magic;

	inline void init(void*, void*, uint32_t checksum_) {
		magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(void*, void*, uint32_t checksum_) {
		if (magic != STORAGE_BIN_FOOTER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad document footer magic number");
		}
		if (checksum != checksum_) {
			THROW(StorageCorruptVolume, "Bad document checksum");
		}
	}
};
#pragma pack(pop)


class DataStorage : public Storage<DataHeader, DataBinHeader, DataBinFooter> {
public:
	uint32_t volume;

	DataStorage(const std::string& base_path_, void* param_);
	~DataStorage();

	uint32_t highest_volume();
};
#endif /* XAPIAND_DATA_STORAGE */


class Database {
#ifdef XAPIAND_V8
	std::unordered_map<std::string, std::pair<short, short>> documents;
#endif

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get(const std::unique_ptr<DataStorage>& storage, const std::string& store);
	void storage_pull_blob(Xapian::Document& doc);
	void storage_push_blob(Xapian::Document& doc);
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

public:
	std::weak_ptr<DatabaseQueue> weak_queue;
	Endpoints endpoints;
	int flags;
	size_t hash;
	std::chrono::system_clock::time_point access_time;
	bool modified;
	long long mastery_level;
	uint32_t checkout_revision;

	std::unique_ptr<Xapian::Database> db;

#if XAPIAND_DATABASE_WAL
	std::unique_ptr<DatabaseWAL> wal;
#endif /* XAPIAND_DATABASE_WAL */

#ifdef XAPIAND_DATA_STORAGE
	std::vector<std::unique_ptr<DataStorage>> writable_storages;
	std::vector<std::unique_ptr<DataStorage>> storages;
#endif /* XAPIAND_DATA_STORAGE */

	Database(std::shared_ptr<DatabaseQueue>& queue_, const Endpoints& endpoints, int flags);
	~Database();

	long long read_mastery(const Endpoint& endpoint);

	bool reopen();

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_blob(const Xapian::Document& doc);
#endif /* XAPIAND_DATA_STORAGE */

	std::string get_uuid() const;
	uint32_t get_revision() const;
	std::string get_revision_str() const;

	bool commit(bool wal_=true);
	void cancel(bool wal_=true);

	void delete_document(Xapian::docid did, bool commit_=false, bool wal_=true);
	void delete_document_term(const std::string& term, bool commit_=false, bool wal_=true);
	Xapian::docid add_document(const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_=false, bool wal_=true);

	void add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_=false, bool wal_=true);
	void remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_=false, bool wal_=true);

	Xapian::docid find_document(const std::string& term_id);
	Xapian::Document get_document(const Xapian::docid& did, bool assume_valid_=false, bool pull_=false);
	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_=false, bool wal_=true);

	std::string to_string() const {
		return endpoints.to_string();
	}

#ifdef XAPIAND_V8
	void dec_count_document(const std::string& term_id);
	short get_revision_document(const std::string& term_id);
	bool set_revision_document(const std::string& term_id, short old_revision);
#endif
};


class DatabaseQueue : public queue::Queue<std::shared_ptr<Database>>,
			  public std::enable_shared_from_this<DatabaseQueue> {
	// FIXME: Add queue creation time and delete databases when deleted queue

	friend class Database;
	friend class DatabasePool;
	friend class DatabasesLRU;

private:
	enum class replica_state : uint8_t {
		REPLICA_FREE,
		REPLICA_LOCK,
		REPLICA_SWITCH,
	};

	replica_state state;
	bool persistent;
	bool _volatile;

	size_t count;

	std::condition_variable switch_cond;

	std::weak_ptr<DatabasePool> weak_database_pool;
	Endpoints endpoints;

	TaskQueue<> checkin_callbacks;

public:
	DatabaseQueue(bool volatile_=false);
	DatabaseQueue(DatabaseQueue&&);
	DatabaseQueue(const DatabaseQueue&) = delete;
	~DatabaseQueue();

	bool inc_count(int max=-1);
	bool dec_count();
};


class DatabasesLRU : public lru::LRU<size_t, std::shared_ptr<DatabaseQueue>> {
public:
	DatabasesLRU(ssize_t max_size);

	std::shared_ptr<DatabaseQueue>& operator[] (std::pair<bool, size_t> key);

	void finish();
};


class MsgPackLRU: public lru::LRU<size_t, atomic_shared_ptr<const MsgPack>> {
public:
	MsgPackLRU(ssize_t max_size=-1);
};


class DatabasePool : public std::enable_shared_from_this<DatabasePool> {
	// FIXME: Add maximum number of databases available for the queue
	// FIXME: Add cleanup for removing old database queues
	friend class DatabaseQueue;

private:
	std::mutex qmtx;
	std::mutex smtx;
	std::atomic_bool finished;

	std::unordered_map<size_t, std::unordered_set<std::shared_ptr<DatabaseQueue>>> queues;

	DatabasesLRU databases;
	DatabasesLRU writable_databases;
	MsgPackLRU schemas;

	std::condition_variable checkin_cond;

	void init_ref(const Endpoint& endpoint);
	void inc_ref(const Endpoint& endpoint);
	void dec_ref(const Endpoint& endpoint);
	int get_master_count();

	void add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	void drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	bool _switch_db(const Endpoint& endpoint);
	MsgPack get_shared_schema(const Endpoint& endpoint, const std::string& id, int flags=-1);
	std::pair<bool, atomic_shared_ptr<const MsgPack>*> get_local_schema(const Endpoint& endpoint, int flags=-1, const MsgPack* obj=nullptr);

public:
	queue::QueueSet<Endpoint> updated_databases;

	DatabasePool(size_t max_size);
	~DatabasePool();

	long long get_mastery_level(const std::string& dir);

	void finish();

	template<typename F, typename... Args>
	bool checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, F&& f, Args&&... args);
	bool checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags);

	void checkin(std::shared_ptr<Database>& database);
	bool switch_db(const Endpoint& endpoint);
	void recover_database(const Endpoints& endpoints, int flags);

	std::shared_ptr<const MsgPack> get_schema(const Endpoint& endpoint, int flags=-1, const MsgPack* obj=nullptr);
	bool set_schema(const Endpoint& endpoint, int flags, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema);
};
