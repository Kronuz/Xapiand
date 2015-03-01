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

#include "utils.h"

#include "database.h"
#include <xapian/dbfactory.h>


Database::Database(Endpoints &endpoints_, bool writable_)
	: endpoints(endpoints_),
	  writable(writable_)
{
	hash = endpoints.hash(writable);
	reopen();
}


void
Database::reopen()
{
	// FIXME: Handle remote endpoints and figure out if the endpoint is a local database
	if (writable) {
		db = new Xapian::WritableDatabase();
		if (endpoints[0].protocol == "file") {
			db->add_database(Xapian::WritableDatabase(endpoints[0].path, Xapian::DB_CREATE_OR_OPEN));
		} else {
			db->add_database(Xapian::Remote::open_writable(endpoints[0].host, endpoints[0].port, 0, 10000, endpoints[0].path));
		}
	} else {
		db = new Xapian::Database();
		if (!writable) {
			std::vector<Endpoint>::const_iterator i(endpoints.begin());
			for (; i != endpoints.end(); ++i) {
				if (i->protocol == "file") {
					db->add_database(Xapian::Database((*i).path, Xapian::DB_CREATE_OR_OPEN));
				} else {
					db->add_database(Xapian::Remote::open(i->host, i->port, 0, 10000, i->path));
				}
			}
		} else if (endpoints.size() != 1) {
			LOG_ERR(this, "ERROR: Expecting exactly one database.");
		}
	}
}


Database::~Database()
{
	delete db;
}

DatabaseQueue::DatabaseQueue()
	: count(0)
{
}

DatabaseQueue::~DatabaseQueue()
{
	while (!empty()) {
		Database *database;
		if (pop(database)) {
			delete database;
		}
	}
}


DatabasePool::DatabasePool()
	: finished(false)
{
	pthread_mutex_init(&qmtx, 0);
}


DatabasePool::~DatabasePool()
{
	finish();
	pthread_mutex_destroy(&qmtx);
}


void DatabasePool::finish() {
	pthread_mutex_lock(&qmtx);
	
	finished = true;
	
	pthread_mutex_unlock(&qmtx);
}


bool
DatabasePool::checkout(Database **database, Endpoints &endpoints, bool writable)
{
	Database *database_ = NULL;

	pthread_mutex_lock(&qmtx);
	
	if (!finished && *database == NULL) {
		size_t hash = endpoints.hash(writable);
		DatabaseQueue &queue = databases[hash];
		
		if (!queue.pop(database_, 0)) {
			if (!writable || queue.count == 0) {
				database_ = new Database(endpoints, writable);
				queue.count++;
			}
			// FIXME: lock until a database is available if it can't get one
		}
		*database = database_;
	}
	
	pthread_mutex_unlock(&qmtx);

	LOG_DATABASE(this, "+ CHECKOUT DB %lx\n", (unsigned long)*database);

	return database_ != NULL;
}


void
DatabasePool::checkin(Database **database)
{
	LOG_DATABASE(this, "- CHECKIN DB %lx\n", (unsigned long)*database);

	pthread_mutex_lock(&qmtx);
	
	DatabaseQueue &queue = databases[(*database)->hash];
	
	queue.push(*database);
	
	*database = NULL;
	
	pthread_mutex_unlock(&qmtx);
}
