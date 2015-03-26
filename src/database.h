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

#include <queue>
#include <unordered_map>

#include "endpoint.h"
#include "queue.h"

#include <xapian.h>
#include "cJSON.h"
#include <pthread.h>

#include "md5.h"
#include <sstream>
#include <pcre.h>


class DatabasePool;


class Database {
public:
	size_t hash;
	bool writable;
	Endpoints endpoints;
	
	Xapian::Database *db;

	typedef struct query_t {
	    int first;  //Get first item (OFFSET)
	    int max_items; //Get maximum number of items (LIMIT)

	    std::string search;    	//Get searchs
	    std::string sort_by; //Get wanted order by
	    std::string sort_type; 	//DESC or ASC
	    std::string facets;  	//Get wanted facets
	} query_t;

	typedef struct group{
		int start;
		int end;
	} group;

	pcre *compiled_terms;
	pcre *compiled_date;
	
	Database(Endpoints &endpoints, bool writable);
	~Database();
	
	void reopen();
	bool drop(const char *document_id, bool commit);
	char* prefixed(const char *term, const char *prefix);
	bool index(const char *document, const char *document_id, bool commit);
	unsigned int get_slot(const char *name);
	unsigned int hex2long(const char *input);
	char* _date(const char *iso_date);
	char* _time(const char *iso_date);
	char* _sTZD(const char *iso_date);
	char* get_prefix(const char *name, const char *prefix);
	char* get_slot_hex(const char *name);
	const char* print_type(int type);
	bool replace(const char *document_id, const Xapian::Document doc, bool commit);
	bool search(query_t query, bool get_matches, bool get_data, bool get_terms, bool get_size, bool dead, bool counting);
	bool find_terms(const char *str);
	
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
