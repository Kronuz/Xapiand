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

#include <pthread.h>
#include <algorithm>
#include <queue>
#include <unordered_map>

class DatabasePool;


class Database {
public:
	size_t hash;
	bool writable;
	Endpoints endpoints;
	
	Xapian::Database *db;

	static pcre *compiled_find_field_re;
	
	Database(Endpoints &endpoints, bool writable);
	~Database();

	void reopen();
	bool drop(const std::string &document_id, bool commit);
	bool index(const std::string &document, const std::string &document_id, bool commit);
	bool replace(const std::string &document_id, const Xapian::Document doc, bool commit);
	std::string serialise(const std::string &name, const std::string &value);
	void insert_terms_geo(const std::string &g_serialise, Xapian::Document *doc, const std::string &name, int w, int position);
	int find_field(const std::string &str, int *g, int size_g, int len, int offset);
	Xapian::Enquire get_enquire(Xapian::Query query, struct query_t e);
	std::string get_results(Xapian::Query query, struct query_t e);
	bool search(struct query_t e);
	Xapian::Query _search(const std::string &query, unsigned int flags, bool text, const std::string &lan);
	
	
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
	std::unordered_map<size_t, DatabaseQueue> databases;
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
