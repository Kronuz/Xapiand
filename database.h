//
//  database.h
//  Xapiand
//
//  Created by Germán M. Bravo on 2/23/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#ifndef Xapiand_database_h
#define Xapiand_database_h

#include <queue>
#include <unordered_map>

#include "endpoint.h"
#include "queue.h"

#include "xapian.h"
#include <pthread.h>


class DatabasePool;


class Database {
public:
	size_t hash;
	bool writable;
	Endpoints endpoints;
	
	Xapian::Database *db;
	
	Database(Endpoints &endpoints, bool writable);
	~Database();
	
	void reopen();
};


class DatabaseQueue : public Queue<Database *> {
	friend class DatabasePool;
protected:
	// FIXME: Add queue creation time and delete databases when deleted queue
	size_t instances_count = 0;

public:
	~DatabaseQueue();
};


class DatabasePool {
protected:
	// FIXME: Add maximum number of databases available for the queue
	size_t databases_count = 0;

private:
	bool finished = false;
	std::unordered_map<size_t, DatabaseQueue> databases;
	pthread_mutex_t qmtx;
	
	// FIXME: Add cleanup for removing old dtabase queues
	
public:
	DatabasePool();
	~DatabasePool();
	
	bool checkout(Database **database, Endpoints &endpoints, bool writable);
	void checkin(Database **database);
	void finish();
};

#endif /**/
