/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "log.h"
#include "database_utils.h"
#include "endpoint.h"
#include "fields.h"
#include "lru.h"
#include "multivaluekeymaker.h"
#include "multivalue.h"
#include "queue.h"
#include "threadpool.h"
#include "schema.h"
#include "storage.h"

#include <xapian/matchspy.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <regex>
#include <unordered_map>
#include <unordered_set>

constexpr int DB_OPEN         = 0x00; // Opens a database
constexpr int DB_WRITABLE     = 0x01; // Opens as writable
constexpr int DB_SPAWN        = 0x02; // Automatically creates the database if it doesn't exist
constexpr int DB_PERSISTENT   = 0x04; // Always try keeping the database in the database pool
constexpr int DB_INIT_REF     = 0x08; // Initializes the writable index in the database .refs
constexpr int DB_VOLATILE     = 0x10; // Always drop the database from the database pool as soon as possible
constexpr int DB_REPLICATION  = 0x20; // Use conditional pop in the queue, only pop when replication is done
constexpr int DB_NOWAL        = 0x40; // Disable open wal file
constexpr int DB_DATA_STORAGE = 0x80; // Enable separate data storage file for the database

#define DB_MASTER "M"
#define DB_SLAVE  "S"

#define DB_SLOT_ID     0 // Slot ID document
#define DB_SLOT_OFFSET 1 // Slot offset for data
#define DB_SLOT_TYPE   2 // Slot type data
#define DB_SLOT_LENGTH 3 // Slot length data
#define DB_SLOT_CREF   4 // Slot that saves the references counter

#define DB_SLOT_RESERVED 10 // Reserved slots by special data

#define DB_RETRIES 3 // Number of tries to do an operation on a Xapian::Database

#define WAL_SLOTS ((STORAGE_BLOCK_SIZE - sizeof(WalHeader::StorageHeaderHead)) / sizeof(uint32_t))

constexpr size_t START_POS = SIZE_BITS_ID - 4;


using namespace std::chrono;

struct WalHeader;

class DatabasePool;
class DatabasesLRU;
class DatabaseQueue;

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
			throw MSG_StorageCorruptVolume("Bad line header magic number");
		}
		if (flags & STORAGE_FLAG_DELETED) {
			throw MSG_StorageNotFound("Line deleted");
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
			throw MSG_StorageCorruptVolume("Bad line footer magic number");
		}
		if (checksum != checksum_) {
			throw MSG_StorageCorruptVolume("Bad line checksum");
		}
	}
};
#pragma pack(pop)


class DatabaseWAL : Storage<WalHeader, WalBinHeader, WalBinFooter> {
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
	};
	bool modified;

	bool execute(const std::string& line);
	uint32_t highest_valid_slot();

	inline void open(const std::string& path, int flags, bool commit_eof=false) {
		Storage<WalHeader, WalBinHeader, WalBinFooter>::open(path, flags, reinterpret_cast<void*>(commit_eof));
	}

public:
	enum class Type {
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
		MAX
	};

	Database* database;

	DatabaseWAL(Database* database_)
		: Storage<WalHeader, WalBinHeader, WalBinFooter>(this),
		  modified(false),
		  database(database_) {
		L_OBJ(this, "CREATED DATABASE WAL!");
	}

	~DatabaseWAL() {
		L_OBJ(this, "DELETED DATABASE WAL!");
	}

	bool open_current(const std::string& path, bool current);

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
#endif


class Database {
public:
	Schema schema;

	std::weak_ptr<DatabaseQueue> weak_queue;
	Endpoints endpoints;
	int flags;
	size_t hash;
	system_clock::time_point access_time;
	bool modified;
	long long mastery_level;
	std::string checkout_revision;

	std::unique_ptr<Xapian::Database> db;

#if XAPIAND_DATABASE_WAL
	std::unique_ptr<DatabaseWAL> wal;
#endif

	struct search_t {
		Xapian::Query query;
		std::vector<std::string> suggested_query;
		std::vector<std::unique_ptr<NumericFieldProcessor>> nfps;
		std::vector<std::unique_ptr<DateFieldProcessor>> dfps;
		std::vector<std::unique_ptr<GeoFieldProcessor>> gfps;
		std::vector<std::unique_ptr<BooleanFieldProcessor>> bfps;
	};

	Database(std::shared_ptr<DatabaseQueue>& queue_, const Endpoints& endpoints, int flags);
	~Database();

	long long read_mastery(const Endpoint& endpoint);

	data_field_t get_data_field(const std::string& field_name);
	data_field_t get_slot_field(const std::string& field_name);

	void get_stats_database(MsgPack&& stats);
	void get_stats_doc(MsgPack&& stats, const std::string& document_id);

	bool reopen();

	Xapian::docid index(const std::string& body, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid index(const MsgPack& obj, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid patch(const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);

	void get_mset(const query_field_t& e, Xapian::MSet& mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>& spies,
			std::vector<std::string>& suggestions, int offset=0);

	std::string get_uuid() const;
	std::string get_revision_info() const;

	bool commit(bool wal_=true);
	void cancel(bool wal_=true);

	void delete_document(Xapian::docid did, bool commit_=false, bool wal_=true);
	void delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);
	void delete_document_term(const std::string& term, bool commit_=false, bool wal_=true);
	Xapian::docid add_document(const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(const std::string& doc_id, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_=false, bool wal_=true);

	void add_spelling(const std::string & word, Xapian::termcount freqinc, bool commit_=false, bool wal_=true);
	void remove_spelling(const std::string & word, Xapian::termcount freqdec, bool commit_=false, bool wal_=true);

	std::string get_metadata(const std::string& key);
	void set_metadata(const std::string& key, const std::string& value, bool commit_=false, bool wal_=true);

	Xapian::Document get_document(const std::string& doc_id);
	Xapian::Document get_document(const Xapian::docid& did);

	std::string get_value(const Xapian::Document& document, Xapian::valueno slot);
	MsgPack get_value(const Xapian::Document& document, const std::string& slot_name);

private:
	void _index(Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length);

	search_t _search(const std::string& query, unsigned flags, bool text, const std::string& lan);
	search_t search(const query_field_t& e);

	void get_similar(bool is_fuzzy, Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar);
	Xapian::Enquire get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t *e, Multi_MultiValueKeyMaker *sorter,
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies);
};


class DatabaseQueue : public queue::Queue<std::shared_ptr<Database>>,
			  public std::enable_shared_from_this<DatabaseQueue> {
	// FIXME: Add queue creation time and delete databases when deleted queue

	friend class Database;
	friend class DatabasePool;
	friend class DatabasesLRU;

private:
	enum class replica_state {
		REPLICA_FREE,
		REPLICA_LOCK,
		REPLICA_SWITCH,
	};

	replica_state state;
	bool persistent;

	size_t count;

	std::condition_variable switch_cond;

	std::weak_ptr<DatabasePool> weak_database_pool;
	Endpoints endpoints;

	TaskQueue<> checkin_callbacks;

public:
	DatabaseQueue();
	DatabaseQueue(DatabaseQueue&&);
	~DatabaseQueue();

	bool inc_count(int max=-1);
	bool dec_count();
};


class DatabasesLRU : public lru::LRU<size_t, std::shared_ptr<DatabaseQueue>> {
public:
	DatabasesLRU(ssize_t max_size) : LRU(max_size) { }

	std::shared_ptr<DatabaseQueue>& operator[] (size_t key) {
		try {
			return at(key);
		} catch (std::range_error) {
			return insert_and([](std::shared_ptr<DatabaseQueue> & val) {
				if (val->persistent || val->size() < val->count || val->state != DatabaseQueue::replica_state::REPLICA_FREE) {
					return lru::DropAction::renew;
				} else {
					return lru::DropAction::drop;
				}
			}, std::make_pair(key, std::make_shared<DatabaseQueue>()));
		}
	}

	void finish() {
		for (iterator it = begin(); it != end(); ++it) {
			it->second->finish();
		}
	}
};


class DatabasePool : public std::enable_shared_from_this<DatabasePool> {
	// FIXME: Add maximum number of databases available for the queue
	// FIXME: Add cleanup for removing old database queues
	friend class DatabaseQueue;

private:
	std::mutex qmtx;
	std::atomic_bool finished;

	std::unordered_map<size_t, std::unordered_set<std::shared_ptr<DatabaseQueue>>> queues;

	DatabasesLRU databases;
	DatabasesLRU writable_databases;

	std::condition_variable checkin_cond;

	void init_ref(const Endpoint& endpoint);
	void inc_ref(const Endpoint& endpoint);
	void dec_ref(const Endpoint& endpoint);
	int get_master_count();

	void add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	void drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	bool _switch_db(const Endpoint& endpoint);

public:
	DatabasePool(size_t max_size);
	~DatabasePool();

	long long get_mastery_level(const std::string& dir);

	void finish();

	template<typename F, typename... Args>
	bool checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, F&& f, Args&&... args) {
		bool ret = checkout(database, endpoints, flags);
		if (!ret) {
			std::unique_lock<std::mutex> lk(qmtx);

			size_t hash = endpoints.hash();

			std::shared_ptr<DatabaseQueue> queue;
			if (flags & DB_WRITABLE) {
				queue = writable_databases[hash];
			} else {
				queue = databases[hash];
			}

			queue->checkin_callbacks.clear();
			queue->checkin_callbacks.enqueue(std::forward<F>(f), std::forward<Args>(args)...);
		}
		return ret;
	}

	bool checkout(std::shared_ptr<Database>& database, const Endpoints &endpoints, int flags);
	void checkin(std::shared_ptr<Database>& database);
	bool switch_db(const Endpoint &endpoint);

	queue::QueueSet<Endpoint> updated_databases;
};


class ExpandDeciderFilterPrefixes : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

public:
	ExpandDeciderFilterPrefixes(const std::vector<std::string>& prefixes_)
		: prefixes(prefixes_) { }

	virtual bool operator() (const std::string& term) const override;
};
