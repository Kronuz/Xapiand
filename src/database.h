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


struct Database {
	size_t hash;
	Endpoints endpoints;
	
	Xapian::Database *db;
	
	~Database();
};


class DatabaseQueue : public Queue<Database *> {
public:
	~DatabaseQueue();
};


class DatabasePool {
private:
	bool finished = false;
	std::unordered_map<size_t, DatabaseQueue> databases;
	pthread_mutex_t qmtx;
	
public:
	DatabasePool();
	~DatabasePool();
	
	bool checkout(Database **database, Endpoints &endpoints, bool writable);
	void checkin(Database **database);
};

#endif /**/
