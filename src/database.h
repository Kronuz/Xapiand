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

#include "xapiand.h"

#include <atomic>               // for atomic_bool
#include <chrono>               // for system_clock, system_clock::time_point
#include <cstring>              // for size_t
#include <functional>           // for function
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
#include "cuuid/uuid.h"         // for UUID, UUID_LENGTH
#include "endpoint.h"           // for Endpoints, Endpoint
#include "lru.h"                // for LRU, DropAction, LRU<>::iterator, DropAc...
#include "lz4/xxhash.h"         // for XXH32
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
constexpr int RECOVER_DECREMENT_COUNT         = 0x08; // Decrement count queue


#define WAL_SLOTS ((STORAGE_BLOCK_SIZE - sizeof(WalHeader::StorageHeaderHead)) / sizeof(uint32_t))

struct DatabaseCount {
	size_t count;
	size_t queues;
	size_t enqueued;
};

#if XAPIAND_DATABASE_WAL
struct WalHeader {
	struct StorageHeaderHead {
		char magic[8];
		uint32_t offset;
		Xapian::rev revision;
		std::array<unsigned char, 16> uuid;
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
	class iterator;

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

	bool validate_uuid;

	MsgPack repr_document(std::string_view document, bool unserialised);
	MsgPack repr_metadata(std::string_view document, bool unserialised);
	MsgPack repr_line(std::string_view line, bool unserialised);
	uint32_t highest_valid_slot();

	inline bool open(std::string_view path, int flags, bool commit_eof=false) {
		return Storage<WalHeader, WalBinHeader, WalBinFooter>::open(path, flags, reinterpret_cast<void*>(commit_eof));
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

	mutable UUID _uuid;
	mutable UUID _uuid_le;
	Database* database;

	DatabaseWAL(std::string_view base_path_, Database* database_);
	~DatabaseWAL();

	iterator begin();
	iterator end();

	bool open_current(bool only_committed, bool unsafe = false);
	MsgPack repr(Xapian::rev start_revision, Xapian::rev end_revision, bool unserialised);

	const UUID& uuid() const;
	const UUID& uuid_le() const;

	bool init_database();
	bool execute(std::string_view line, bool wal_ = false, bool unsafe = false);
	void write_line(Type type, std::string_view data);
	void write_add_document(const Xapian::Document& doc);
	void write_cancel();
	void write_delete_document_term(std::string_view term);
	void write_commit();
	void write_replace_document(Xapian::docid did, const Xapian::Document& doc);
	void write_replace_document_term(std::string_view term, const Xapian::Document& doc);
	void write_delete_document(Xapian::docid did);
	void write_set_metadata(std::string_view key, std::string_view val);
	void write_add_spelling(std::string_view word, Xapian::termcount freqinc);
	void write_remove_spelling(std::string_view word, Xapian::termcount freqdec);
	std::pair<bool, unsigned long long> has_revision(Xapian::rev revision);
	iterator find(Xapian::rev revision);
	std::pair<Xapian::rev, std::string> get_current_line(uint32_t end_off);
};


class DatabaseWAL::iterator {
	friend DatabaseWAL;

	DatabaseWAL* wal;
	std::pair<Xapian::rev, std::string> item;
	uint32_t end_off;

public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = std::pair<Xapian::rev, std::string>;
	using difference_type = std::pair<Xapian::rev, std::string>;
	using pointer = std::pair<Xapian::rev, std::string>*;
	using reference = std::pair<Xapian::rev, std::string>&;

	iterator(DatabaseWAL* wal_, std::pair<Xapian::rev, std::string>&& item_, uint32_t end_off_)
		: wal(wal_),
		  item(item_),
		  end_off(end_off_) { }

	iterator& operator++() {
		item = wal->get_current_line(end_off);
		return *this;
	}

	iterator operator=(const iterator& other) {
		wal = other.wal;
		item = other.item;
		return *this;
	}

	std::pair<Xapian::rev, std::string>& operator*() {
		return item;
	}

	std::pair<Xapian::rev, std::string>* operator->() {
		return &operator*();
	}

	std::pair<Xapian::rev, std::string>& value() {
		return item;
	}

	bool operator==(const iterator& other) const {
		return this == &other || item == other.item;
	}

	bool operator!=(const iterator& other) const {
		return !operator==(other);
	}

};


inline DatabaseWAL::iterator DatabaseWAL::begin() {
	return find(0);
}


inline DatabaseWAL::iterator DatabaseWAL::end() {
	return iterator(this, std::make_pair(std::numeric_limits<Xapian::rev>::max() - 1, ""), 0);
}

#endif /* XAPIAND_DATABASE_WAL */


#ifdef XAPIAND_DATA_STORAGE
struct DataHeader {
	struct DataHeaderHead {
		uint32_t magic;
		uint32_t offset;  // required
		char uuid[UUID_LENGTH];
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
	int flags;

	uint32_t volume;

	DataStorage(std::string_view base_path_, void* param_, int flags);
	~DataStorage();

	bool open(std::string_view relative_path);
};
#endif /* XAPIAND_DATA_STORAGE */


class Database {
#ifdef XAPIAND_DATA_STORAGE
	void storage_pull_blobs(Xapian::Document& doc, const Xapian::docid& did) const;
	void storage_push_blobs(Xapian::Document& doc, const Xapian::docid& did) const;
	void storage_commit();
#endif /* XAPIAND_DATA_STORAGE */

	void reopen_writable();
	void reopen_readable();
public:
	std::weak_ptr<DatabaseQueue> weak_queue;
	std::weak_ptr<DatabaseQueue> weak_readable_queue;

	Endpoints endpoints;
	int flags;
	size_t hash;
	bool modified;
	std::chrono::system_clock::time_point reopen_time;
	Xapian::rev reopen_revision;
	bool incomplete;
	bool closed;
	std::set<std::size_t> fail_db;

	std::unique_ptr<Xapian::Database> db;
	std::vector<std::pair<Xapian::Database, bool>> dbs;

#if XAPIAND_DATABASE_WAL
	std::unique_ptr<DatabaseWAL> wal;
#endif /* XAPIAND_DATABASE_WAL */

#ifdef XAPIAND_DATA_STORAGE
	std::vector<std::unique_ptr<DataStorage>> writable_storages;
	std::vector<std::unique_ptr<DataStorage>> storages;
#endif /* XAPIAND_DATA_STORAGE */

	Database(std::shared_ptr<DatabaseQueue>& queue_, Endpoints  endpoints_, int flags_);
	~Database();

	bool reopen();

#ifdef XAPIAND_DATA_STORAGE
	std::string storage_get_stored(const Xapian::docid& did, const Data::Locator& locator) const;
#endif /* XAPIAND_DATA_STORAGE */

	UUID get_uuid() const;
	Xapian::rev get_revision() const;

	bool commit(bool wal_=true);
	void cancel(bool wal_=true);

	void close();

	void delete_document(Xapian::docid did, bool commit_=false, bool wal_=true);
	void delete_document_term(const std::string& term, bool commit_=false, bool wal_=true);
	Xapian::docid add_document(const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_=false, bool wal_=true);

	void add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_=false, bool wal_=true);
	void remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_=false, bool wal_=true);

	Xapian::docid find_document(const std::string& term_id);
	Xapian::Document get_document(const Xapian::docid& did, bool assume_valid_=false, bool pull_=false);

	std::vector<std::string> get_metadata_keys();
	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_=false, bool wal_=true);

	void dump_metadata(int fd, XXH32_state_t* xxh_state);
	void dump_documents(int fd, XXH32_state_t* xxh_state);
	MsgPack dump_documents();

	std::string to_string() const {
		return endpoints.to_string();
	}
};


class DatabaseQueue : public queue::Queue<std::shared_ptr<Database>>,
			  public std::enable_shared_from_this<DatabaseQueue> {
	// FIXME: Add queue creation time and delete databases when deleted queue

	friend class Database;
	friend class DatabasePool;
	friend class DatabasesLRU;

private:
	bool locked;
	std::atomic<Xapian::rev> local_revision;
	std::chrono::time_point<std::chrono::system_clock> renew_time;
	bool persistent;

	size_t count;

	std::condition_variable unlock_cond;
	std::condition_variable exclusive_cond;

	std::weak_ptr<DatabasePool> weak_database_pool;
	Endpoints endpoints;

protected:
	template <typename... Args>
	DatabaseQueue(const Endpoints& endpoints, Args&&... args);

public:
	DatabaseQueue(const DatabaseQueue&) = delete;
	DatabaseQueue(DatabaseQueue&&) = delete;
	DatabaseQueue& operator=(const DatabaseQueue&) = delete;
	DatabaseQueue& operator=(DatabaseQueue&&) = delete;
	~DatabaseQueue();

	bool inc_count(int max=-1);
	bool dec_count();

	template <typename... Args>
	static std::shared_ptr<DatabaseQueue> make_shared(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : DatabaseQueue {
			enable_make_shared(Args&&... args_) : DatabaseQueue(std::forward<Args>(args_)...) { }
		};

		return std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
	}
};


class DatabasesLRU : public lru::LRU<size_t, std::shared_ptr<DatabaseQueue>> {

	const std::shared_ptr<queue::QueueState> _queue_state;

public:
	DatabasesLRU(size_t dbpool_size, std::shared_ptr<queue::QueueState> queue_state);

	std::shared_ptr<DatabaseQueue> get(size_t hash);
	std::shared_ptr<DatabaseQueue> get(size_t hash, bool db_volatile, const Endpoints& endpoints);

	void cleanup(const std::chrono::time_point<std::chrono::system_clock>& now);

	void finish();
};


class DatabasePool {
	// FIXME: Add maximum number of databases available for the queue
	// FIXME: Add cleanup for removing old database queues
	friend class DatabaseQueue;

	std::mutex qmtx;
	std::atomic_bool finished;
	size_t locks;

	const std::shared_ptr<queue::QueueState> queue_state;

	std::unordered_map<size_t, std::unordered_set<std::shared_ptr<DatabaseQueue>>> queues;

	DatabasesLRU databases;
	DatabasesLRU writable_databases;

	std::chrono::time_point<std::chrono::system_clock> cleanup_readable_time;
	std::chrono::time_point<std::chrono::system_clock> cleanup_writable_time;

	std::condition_variable checkin_cond;

	void add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	void drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);

	void _cleanup(bool writable, bool readable);

public:
	void checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags);
	void checkin(std::shared_ptr<Database>& database);

#ifdef XAPIAND_CLUSTERING
	queue::QueueSet<Endpoint> updated_databases;
#endif

	DatabasePool(size_t dbpool_size, size_t max_databases);
	DatabasePool(const DatabasePool&) = delete;
	DatabasePool(DatabasePool&&) = delete;
	DatabasePool& operator=(const DatabasePool&) = delete;
	DatabasePool& operator=(DatabasePool&&) = delete;
	~DatabasePool();

	void finish();
	void switch_db(const std::string& tmp, const std::string& endpoint_path);
	void cleanup();

	DatabaseCount total_writable_databases();
	DatabaseCount total_readable_databases();
};
