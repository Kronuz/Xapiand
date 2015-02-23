//
//  database.cpp
//  Xapiand
//
//  Created by Germán M. Bravo on 2/23/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#include "database.h"


Database::~Database()
{
	delete db;
}


DatabaseQueue::~DatabaseQueue()
{
	//		std::queue<Database *>::const_iterator i(queue.begin());
	//		for (; i != databases.end(); ++i) {
	//			(*i).second.finish();
	//		}
}


DatabasePool::DatabasePool()
{
	pthread_mutex_init(&qmtx, 0);
}


DatabasePool::~DatabasePool()
{
	pthread_mutex_lock(&qmtx);
	
	finished = true;
	
	pthread_mutex_lock(&qmtx);
	
	pthread_mutex_destroy(&qmtx);
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
			database_ = new Database();
			database_->endpoints = endpoints;
			database_->hash = hash;
			if (writable) {
				database_->db = new Xapian::WritableDatabase(endpoints[0].path, Xapian::DB_CREATE_OR_OPEN);
			} else {
				database_->db = new Xapian::Database(endpoints[0].path, Xapian::DB_CREATE_OR_OPEN);
				if (!writable) {
					std::vector<Endpoint>::const_iterator i(endpoints.begin());
					for (++i; i != endpoints.end(); ++i) {
						database_->db->add_database(Xapian::Database((*i).path));
					}
				} else if (endpoints.size() != 1) {
					printf("ERROR: Expecting exactly one database.");
				}
			}
		}
		*database = database_;
	}
	
	pthread_mutex_unlock(&qmtx);
	
	return database_ != NULL;
}


void
DatabasePool::checkin(Database **database)
{
	pthread_mutex_lock(&qmtx);
	
	DatabaseQueue &queue = databases[(*database)->hash];
	
	queue.push(*database);
	
	*database = NULL;
	
	pthread_mutex_unlock(&qmtx);
}
