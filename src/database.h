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

#ifndef XAPIAND_INCLUDED_DATABASE_H
#define XAPIAND_INCLUDED_DATABASE_H

#include "endpoint.h"
#include "queue.h"
#include "lru.h"

#include <xapian.h>

#include "cJSON.h"
#include "utils.h"
#include "fields.h"
#include "multivalue.h"

#include <pthread.h>
#include <algorithm>
#include <queue>
#include <memory>
#include <unordered_map>
#include <unordered_set>


#define RESERVED_WEIGHT "_weight"
#define RESERVED_POSITION "_position"
#define RESERVED_LANGUAGE "_language"
#define RESERVED_SPELLING "_spelling"
#define RESERVED_POSITIONS "_positions"
#define RESERVED_TEXTS "_texts"
#define RESERVED_VALUES "_values"
#define RESERVED_TERMS "_terms"
#define RESERVED_DATA "_data"
#define RESERVED_ACCURACY "_accuracy"
#define RESERVED_ACC_PREFIX "_acc_prefix"
#define RESERVED_STORE "_store"
#define RESERVED_TYPE "_type"
#define RESERVED_ANALYZER "_analyzer"
#define RESERVED_DYNAMIC "_dynamic"
#define RESERVED_D_DETECTION "_date_detection"
#define RESERVED_N_DETECTION "_numeric_detection"
#define RESERVED_G_DETECTION "_geo_detection"
#define RESERVED_B_DETECTION "_bool_detection"
#define RESERVED_S_DETECTION "_string_detection"
#define RESERVED_VALUE "_value"
#define RESERVED_NAME "_name"
#define RESERVED_SLOT "_slot"
#define RESERVED_INDEX "_index"
#define RESERVED_PREFIX "_prefix"
#define RESERVED_ID "_id"
#define OFFSPRING_UNION "__"
#define LANGUAGES "da nl en lovins porter fi fr de hu it nb nn no pt ro ru es sv tr"
#define SCHEMA "schema"

//change prefix to Q only
#define DOCUMENT_ID_TERM_PREFIX "Q"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define DB_WRITABLE 1    // Opens as writable
#define DB_SPAWN 2       // Automatically creates the database if it doesn't exist
#define DB_PERSISTENT 4  // Always try keeping the database in the database pool
#define DB_INIT_REF 8	 // Initializes the writable index in the database .ref
#define DB_VOLATILE 16   // Always drop the database from the database pool as soon as possible

// Default partials and error for indexing and searching geospatials.
#define DE_PARTIALS true
#define DE_ERROR 0.2

class DatabasePool;
class DatabasesLRU;
class DatabaseQueue;


class Database {
public:
	DatabaseQueue *queue;
	Endpoints endpoints;
	int flags;
	bool local;
	size_t hash;
	time_t access_time;
	int mastery_level;
	std::string checkout_revision;

	Xapian::Database *db;

	static pcre *compiled_find_field_re;
	static pcre *compiled_find_types_re;

	typedef struct specifications_s {
		int position;
		int weight;
		std::string language;
		bool spelling;
		bool positions;
		std::vector<std::string> accuracy;
		bool store;
		std::string type;
		char sep_types[3];
		std::string analyzer;
		bool dynamic;
		bool date_detection;
		bool numeric_detection;
		bool geo_detection;
		bool bool_detection;
		bool string_detection;
	} specifications_t;

	typedef struct search_s {
		Xapian::Query query;
		std::vector<std::string> suggested_query;
		std::vector<std::unique_ptr<Xapian::NumberValueRangeProcessor>> nvrps;
		std::vector<std::unique_ptr<Xapian::StringValueRangeProcessor>> svrps;
		std::vector<std::unique_ptr<DateTimeValueRangeProcessor>> dvrps;
		std::vector<std::unique_ptr<NumericFieldProcessor>> nfps;
		std::vector<std::unique_ptr<DateFieldProcessor>> dfps;
		std::vector<std::unique_ptr<BooleanFieldProcessor>> bfps;
	} search_t;

	Database(DatabaseQueue * queue_, const Endpoints &endpoints, int flags);
	~Database();

	int read_mastery(const std::string &dir);
	void reopen();
	bool drop(const std::string &document_id, bool commit);
	bool index(cJSON *document, const std::string &document_id, bool commit);
	bool patch(cJSON *patches, const std::string &_document_id, bool commit);
	bool replace(const std::string &document_id, const Xapian::Document &doc, bool commit);
	bool get_metadata(const std::string &key, std::string &value);
	bool set_metadata(const std::string &key, const std::string &value, bool commit);
	bool get_document(Xapian::docid did, Xapian::Document &doc);
	void insert_terms_geo(const std::string &g_serialise, Xapian::Document *doc, const std::string &prefix, int w, int position);
	int find_field(const std::string &str, int *g, int size_g, int len, int offset);
	Xapian::Enquire get_enquire(Xapian::Query &query, Xapian::MultiValueKeyMaker *sorter, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies, similar_t *nearest, similar_t *fuzzy, std::vector<std::string> *facets);
	search_t search(query_t e);
	search_t _search(const std::string &query, unsigned int flags, bool text, const std::string &lan, bool unique_doc);
	void get_similar(bool is_fuzzy, Xapian::Enquire &enquire, Xapian::Query &query, similar_t *similar);
	int get_mset(query_t &e, Xapian::MSet &mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, std::vector<std::string> &suggestions, int offset = 0);
	cJSON* get_stats_database();
	cJSON* get_stats_docs(int id_doc);
	data_field_t get_data_field(const std::string &field_name);
	std::vector<std::string> split_fields(const std::string &field_name);
	char get_type(cJSON *field, specifications_t &spc);
	std::string str_type(char type);
	bool set_types(const std::string &type, char sep_types[]);
	cJSON* get_stats_time(const std::string &time_req);
	bool is_reserved(const std::string &word);
	void index_fields(cJSON *item, const std::string &item_name, specifications_t &spc_now, Xapian::Document &doc, cJSON *schema, bool is_value, bool find);
	void update_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema);
	bool is_language(const std::string &language);
	void index_texts(Xapian::Document &doc, cJSON *text, specifications_t &spc, const std::string &name, cJSON *schema, bool find);
	void index_terms(Xapian::Document &doc, cJSON *terms, specifications_t &spc, const std::string &name, cJSON *schema, bool find);
	void index_values(Xapian::Document &doc, cJSON *values, specifications_t &spc, const std::string &name, cJSON *schema, bool find);
	void clean_reserved(cJSON *root);
	void clean_reserved(cJSON *root, cJSON *item);
	std::string specificationstostr(specifications_t &spc);

private:
	bool _commit();
};


class DatabaseQueue : public Queue<Database *> {
	// FIXME: Add queue creation time and delete databases when deleted queue

	friend class Database;
	friend class DatabasePool;
	friend class DatabasesLRU;

private:
	bool is_switch_db;
	bool persistent;
	size_t count;

	pthread_cond_t switch_cond;

	DatabasePool *database_pool;
	Endpoints endpoints;

	void setup_endpoints(DatabasePool *database_pool_, const Endpoints &endpoints_);

public:
	DatabaseQueue();
	~DatabaseQueue();

	bool inc_count(int max=-1);
	bool dec_count();
};


class DatabasesLRU : public lru_map<size_t, DatabaseQueue> {
private:
	dropping_action on_drop(DatabaseQueue & val) {
		return (val.persistent || val.size() < val.count || val.is_switch_db) ? renew : drop;
	}

public:
	DatabasesLRU(size_t max_size) :
		lru_map(max_size) {}
};


class DatabasePool {
	// FIXME: Add maximum number of databases available for the queue
	// FIXME: Add cleanup for removing old dtabase queues

private:
	bool finished;
	std::unordered_map<size_t, std::unordered_set<DatabaseQueue *> > queues;
	DatabasesLRU databases;
	DatabasesLRU writable_databases;
	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	pthread_cond_t checkin_cond;

	Database *ref_database;
	std::string prefix_rf_node;

	void init_ref(Endpoints endpoints);
	void inc_ref(Endpoints endpoints);
	void dec_ref(Endpoints endpoints);


public:
	DatabasePool(size_t max_size);
	~DatabasePool();

	int get_mastery_level(const std::string &dir);

	bool checkout(Database **database, const Endpoints &endpoints, int flags);
	void checkin(Database **database);
	void finish();
	bool switch_db(const Endpoint &endpoint);
	void add_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue);
	void drop_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue);

	QueueSet<Endpoint> updated_databases;
};

#endif /* XAPIAND_INCLUDED_DATABASE_H */