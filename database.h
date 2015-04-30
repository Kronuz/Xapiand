/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include <xapian.h>

#include "cJSON.h"
#include "utils.h"
#include "fields.h"
#include "multivalue.h"

#include <pthread.h>
#include <algorithm>
#include <queue>
#include <memory>

class DatabasePool;
class DatabaseQueue;

#ifdef HAVE_CXX11
#  include <unordered_map>
   typedef std::unordered_map<size_t, DatabaseQueue> pool_databases_map_t;
#else
#  include <map>
   typedef std::map<size_t, DatabaseQueue> pool_databases_map_t;
#endif


class Database {
public:
	size_t hash;
	bool writable;
	Endpoints endpoints;
	time_t access_time;

	Xapian::Database *db;

	static pcre *compiled_find_field_re;

	Database(Endpoints &endpoints, bool writable);
	~Database();

	void reopen();
	bool drop(const std::string &document_id, bool commit);
	bool index(const std::string &document, const std::string &document_id, bool commit);
	bool replace(const std::string &document_id, const Xapian::Document &doc, bool commit);
	bool get_document(Xapian::docid did, Xapian::Document &doc);
	void insert_terms_geo(const std::string &g_serialise, Xapian::Document *doc, const std::string &name, int w, int position);
	int find_field(const std::string &str, int *g, int size_g, int len, int offset);
	Xapian::Enquire get_enquire(Xapian::Query &query, Xapian::MultiValueKeyMaker *sorter,std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, query_t e);
	search_t search(query_t e);
	search_t _search(const std::string &query, unsigned int flags, bool text, const std::string &lan, bool unique_doc);
	void get_similar(bool is_fuzzy, Xapian::Enquire &enquire, Xapian::Query &query, similar_t *similar);
	int get_mset(query_t &e, Xapian::MSet &mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, std::vector<std::string> &suggestions, int offset = 0);
	cJSON* get_stats_database();
	cJSON* get_stats_docs(int id_doc);
	char field_type(const std::string &field_name);
	cJSON* get_stats_time(const std::string &time_req);

private:
	bool _commit();
};


class DatabaseQueue : public Queue<Database *> {
	friend class DatabasePool;
protected:
	// FIXME: Add queue creation time and delete databases when deleted queue
	size_t count;

public:
	DatabaseQueue();
	~DatabaseQueue();
};


class DatabasePool {
protected:
	// FIXME: Add maximum number of databases available for the queue

private:
	bool finished;
	pool_databases_map_t databases;
	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	// FIXME: Add cleanup for removing old dtabase queues

public:
	DatabasePool();
	~DatabasePool();

	bool checkout(Database **database, Endpoints &endpoints, bool writable);
	void checkin(Database **database);
	void finish();
};

#endif /* XAPIAND_INCLUDED_DATABASE_H */