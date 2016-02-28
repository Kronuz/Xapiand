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


#define DB_WRITABLE     1 // Opens as writable
#define DB_SPAWN        2 // Automatically creates the database if it doesn't exist
#define DB_PERSISTENT   4 // Always try keeping the database in the database pool
#define DB_INIT_REF     8 // Initializes the writable index in the database .refs
#define DB_VOLATILE    16 // Always drop the database from the database pool as soon as possible
#define DB_REPLICATION 32 // Use conditional pop in the queue, only pop when replication is done
#define DB_NOWAL       64 // Disable open wal file

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

	void init(const void* storage);
	void validate(const void* storage);
};

#pragma pack(push, 1)
struct WalBinHeader {
	char magic;
	uint32_t size;  // required

	inline void init(const void*, uint32_t size_) {
		magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
	}

	inline void validate(const void*) {
		if (magic != STORAGE_BIN_HEADER_MAGIC) {
			throw MSG_StorageCorruptVolume("Bad Bin Header Magic Number");
		}
	}
};

struct WalBinFooter {
	 uint32_t checksum;
	 char magic;

	inline void init(const void*, uint32_t checksum_) {
		magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(const void*, uint32_t checksum_) {
		if (magic != STORAGE_BIN_FOOTER_MAGIC) {
			throw MSG_StorageCorruptVolume("Bad Bin Footer Magic Number");
		}
		if (checksum != checksum_) {
			throw MSG_StorageCorruptVolume("Bad Bin Checksum");
		}
	}
};
#pragma pack(pop)


class DatabaseWAL : Storage<WalHeader, WalBinHeader, WalBinFooter> {
	static const constexpr char* const names[] = {
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

	bool execute(const std::string& line);
	uint32_t highest_valid_slot();
	uint64_t fget_revision(const std::string& filename);

	std::string read_checked(uint16_t& off_readed);
	inline void open(const std::string& path, bool writable) {
		Storage<WalHeader, WalBinHeader, WalBinFooter>::open(path, writable);
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

	DatabaseWAL (Database* _database)
	: Storage(), database(_database) { }

	~DatabaseWAL() {}

	void open_current(const std::string& path, bool current);

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
#if XAPIAND_DATABASE_WAL
	DatabaseWAL wal;
#endif

	Schema schema;

	std::weak_ptr<DatabaseQueue> weak_queue;
	Endpoints endpoints;
	int flags;
	bool local;
	size_t hash;
	system_clock::time_point access_time;
	bool modified;
	long long mastery_level;
	std::string checkout_revision;

	std::unique_ptr<Xapian::Database> db;

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

	long long read_mastery(const std::string& dir);
	void reopen();

	bool commit(bool wal_=true);
	bool cancel(bool wal_=true);

	std::string get_uuid() const;
	std::string get_revision_info() const;

	bool delete_document(Xapian::docid did, bool commit_=false, bool wal_=true);
	bool delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);
	bool delete_document_term(const std::string& term, bool commit_=false, bool wal_=true);
	Xapian::docid index(const std::string& body, const std::string& document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid patch(const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid add_document(const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document(const std::string& doc_id, const Xapian::Document& doc, bool commit_=false, bool wal_=true);
	Xapian::docid replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_=false, bool wal_=true);

	bool add_spelling(const std::string & word, Xapian::termcount freqinc, bool commit_=false, bool wal_=true);
	bool remove_spelling(const std::string & word, Xapian::termcount freqdec, bool commit_=false, bool wal_=true);

	data_field_t get_data_field(const std::string& field_name);
	data_field_t get_slot_field(const std::string& field_name);

	void get_mset(const query_field_t& e, Xapian::MSet& mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>& spies,
			std::vector<std::string>& suggestions, int offset=0);
	bool get_metadata(const std::string& key, std::string& value);
	bool set_metadata(const std::string& key, const std::string& value, bool commit_=false, bool wal_=true);
	bool get_document(const Xapian::docid& did, Xapian::Document& doc);

	void get_stats_database(MsgPack&& stats);
	void get_stats_doc(MsgPack&& stats, const std::string& document_id);

private:
	void index_required_data(Xapian::Document& doc, std::string& unique_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length) const;
	void index_object(Xapian::Document& doc, const std::string& str_key, const MsgPack& item_val, MsgPack&& properties, bool is_value=true);

	void index_texts(Xapian::Document& doc, const std::string& name, const MsgPack& texts, MsgPack& properties);
	void index_text(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;

	void index_terms(Xapian::Document& doc, const std::string& name, const MsgPack& terms, MsgPack& properties);
	void index_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;

	void index_values(Xapian::Document& doc, const std::string& name, const MsgPack& values, MsgPack& properties, bool is_term=false);
	void index_value(Xapian::Document& doc, const MsgPack& value, StringList& s, size_t& pos, bool is_term) const;

	void _index(Xapian::Document& doc, const MsgPack& obj);

	search_t _search(const std::string& query, unsigned flags, bool text, const std::string& lan);
	search_t search(const query_field_t& e);

	void get_similar(bool is_fuzzy, Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar);
	Xapian::Enquire get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t *e, Multi_MultiValueKeyMaker *sorter,
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies);
	bool _get_document(const Xapian::MSet& mset, Xapian::Document& doc);
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

	void init_ref(const Endpoints& endpoints);
	void inc_ref(const Endpoints& endpoints);
	void dec_ref(const Endpoints& endpoints);
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
	bool checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, F&& f, Args&&... args)
	{
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
