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

#include "endpoint.h"
#include "queue.h"
#include "lru.h"

#include "database_utils.h"
#include "fields.h"
#include "multivaluekeymaker.h"

#include <xapian/matchspy.h>

#include <mutex>
#include <regex>
#include <atomic>
#include <condition_variable>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#define DB_WRITABLE 1    // Opens as writable
#define DB_SPAWN 2       // Automatically creates the database if it doesn't exist
#define DB_PERSISTENT 4  // Always try keeping the database in the database pool
#define DB_INIT_REF 8	 // Initializes the writable index in the database .refs
#define DB_VOLATILE 16   // Always drop the database from the database pool as soon as possible

#define DB_MASTER "M"
#define DB_SLAVE  "S"

#define SLOT_CREF 1	// Slot that saves the references counter

constexpr size_t START_POS = SIZE_BITS_ID - 4;


class DatabasePool;
class DatabasesLRU;
class DatabaseQueue;


class Database {
public:
	std::weak_ptr<DatabaseQueue> weak_queue;
	Endpoints endpoints;
	int flags;
	bool local;
	size_t hash;
	time_t access_time;
	long long mastery_level;
	std::string checkout_revision;

	std::unique_ptr<Xapian::Database> db;

	static std::regex find_field_re;

	struct search_t {
		Xapian::Query query;
		std::vector<std::string> suggested_query;
		std::vector<std::unique_ptr<NumericFieldProcessor>> nfps;
		std::vector<std::unique_ptr<DateFieldProcessor>> dfps;
		std::vector<std::unique_ptr<GeoFieldProcessor>> gfps;
		std::vector<std::unique_ptr<BooleanFieldProcessor>> bfps;
	};

	Database(std::shared_ptr<DatabaseQueue> &queue_, const Endpoints &endpoints, int flags);
	~Database();

	long long read_mastery(const std::string &dir);
	void reopen();
	bool drop(const std::string &document_id, bool commit);
	Xapian::docid index(const std::string &body, const std::string &document_id, bool commit, const std::string &ct_type, const std::string &ct_length);
	Xapian::docid patch(cJSON *patches, const std::string &_document_id, bool commit, const std::string &ct_type, const std::string &ct_length);
	Xapian::docid replace(const std::string &document_id, const Xapian::Document &doc, bool commit);
	Xapian::docid replace(const Xapian::docid &did, const Xapian::Document &doc, bool commit);
	bool get_metadata(const std::string &key, std::string &value);
	bool set_metadata(const std::string &key, const std::string &value, bool commit);
	bool get_document(const Xapian::docid &did, Xapian::Document &doc);
	Xapian::Enquire get_enquire(Xapian::Query &query, const Xapian::valueno &collapse_key, const Xapian::valueno &collapse_max,
					Multi_MultiValueKeyMaker *sorter, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies,
					const similar_field *nearest, const similar_field *fuzzy, const std::vector<std::string> *facets);
	search_t search(const query_field &e);
	search_t _search(const std::string &query, unsigned int flags, bool text, const std::string &lan);
	void get_similar(bool is_fuzzy, Xapian::Enquire &enquire, Xapian::Query &query, const similar_field *similar);
	int get_mset(const query_field &e, Xapian::MSet &mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, std::vector<std::string> &suggestions, int offset = 0);
	unique_cJSON get_stats_database();
	unique_cJSON get_stats_docs(const std::string &document_id);
	data_field_t get_data_field(const std::string &field_name);
	data_field_t get_slot_field(const std::string &field_name);
	void index_fields(cJSON *item, const std::string &item_name, specifications_t &spc_now, Xapian::Document &doc, cJSON *schema, bool find, bool is_value = true);
	void index_texts(Xapian::Document &doc, cJSON *texts, specifications_t &spc, const std::string &name, cJSON *schema, bool find = true);
	void index_terms(Xapian::Document &doc, cJSON *terms, specifications_t &spc, const std::string &name, cJSON *schema, bool find = true);
	void index_values(Xapian::Document &doc, cJSON *values, specifications_t &spc, const std::string &name, cJSON *schema, bool find = true);

private:
	bool _commit();
};


class DatabaseQueue : public queue::Queue<std::shared_ptr<Database>>,
		      public std::enable_shared_from_this<DatabaseQueue>
{
	// FIXME: Add queue creation time and delete databases when deleted queue

	friend class Database;
	friend class DatabasePool;
	friend class DatabasesLRU;

private:
	bool is_switch_db;
	bool persistent;
	size_t count;

	std::condition_variable switch_cond;

	std::weak_ptr<DatabasePool> weak_database_pool;
	Endpoints endpoints;

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
				if (val->persistent || val->size() < val->count || val->is_switch_db) {
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

	void init_ref(const Endpoints &endpoints);
	void inc_ref(const Endpoints &endpoints);
	void dec_ref(const Endpoints &endpoints);
	int get_master_count();

	void add_endpoint_queue(const Endpoint &endpoint, const std::shared_ptr<DatabaseQueue>& queue);
	void drop_endpoint_queue(const Endpoint &endpoint, const std::shared_ptr<DatabaseQueue>& queue);

public:
	DatabasePool(size_t max_size);
	~DatabasePool();

	long long get_mastery_level(const std::string &dir);

	void finish();
	bool checkout(std::shared_ptr<Database>& database, const Endpoints &endpoints, int flags);
	void checkin(std::shared_ptr<Database>& database);
	bool switch_db(const Endpoint &endpoint);

	queue::QueueSet<Endpoint> updated_databases;
};


class ExpandDeciderFilterPrefixes : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

	public:
		ExpandDeciderFilterPrefixes(const std::vector<std::string> &prefixes_)
			: prefixes(prefixes_) { }

		virtual bool operator() (const std::string &term) const;
};
