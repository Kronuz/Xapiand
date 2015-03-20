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

#define DOCUMENT_ID_TERM_PREFIX 'Q'
#define DOCUMENT_CUSTOM_TERM_PREFIX = 'X'


Database::Database(Endpoints &endpoints_, bool writable_)
	: endpoints(endpoints_),
	  writable(writable_),
	  db(NULL)
{
	hash = endpoints.hash(writable);
	reopen();
}


void
Database::reopen()
{
	if (db) {
		// Try to reopen
		try {
			db->reopen();
			return;
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			db->close();
			delete db;
			db = NULL;
		}
	}

	// FIXME: Handle remote endpoints and figure out if the endpoint is a local database
	const Endpoint *e;
	if (writable) {
		db = new Xapian::WritableDatabase();
		if (endpoints.size() != 1) {
			LOG_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), endpoints.as_string().c_str());
		} else {
			e = &endpoints[0];
			if (e->protocol == "file") {
				db->add_database(Xapian::WritableDatabase(e->path, Xapian::DB_CREATE_OR_OPEN));
			} else {
				db->add_database(Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path));
			}
		}
	} else {
		db = new Xapian::Database();
		std::vector<Endpoint>::const_iterator i(endpoints.begin());
		for (; i != endpoints.end(); ++i) {
			e = &(*i);
			if (e->protocol == "file") {
				db->add_database(Xapian::Database(e->path, Xapian::DB_CREATE_OR_OPEN));
			} else {
				db->add_database(Xapian::Remote::open(e->host, e->port, 0, 10000, e->path));
			}
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
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);
}


DatabasePool::~DatabasePool()
{
	finish();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);
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

	LOG_DATABASE(this, "+ CHECKING OUT DB %lx %s(%s)...\n", (unsigned long)*database, writable ? "w" : "r", endpoints.as_string().c_str());

	pthread_mutex_lock(&qmtx);
	
	if (!finished && *database == NULL) {
		size_t hash = endpoints.hash(writable);
		DatabaseQueue &queue = databases[hash];
		
		if (!queue.pop(database_, 0)) {
			if (!writable || queue.count == 0) {
				queue.count++;
				pthread_mutex_unlock(&qmtx);
				database_ = new Database(endpoints, writable);
				pthread_mutex_lock(&qmtx);
			} else {
				// Lock until a database is available if it can't get one.
				pthread_mutex_unlock(&qmtx);
				int s = queue.pop(database_);
				pthread_mutex_lock(&qmtx);
				if (!s) {
					LOG_ERR(this, "ERROR: Database is not available. Writable: %d", writable);
				}
			}
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
	LOG_DATABASE(this, "- CHECKING IN DB %lx %s(%s)...\n", (unsigned long)*database, (*database)->writable ? "w" : "r", (*database)->endpoints.as_string().c_str());

	pthread_mutex_lock(&qmtx);
	
	DatabaseQueue &queue = databases[(*database)->hash];
	
	queue.push(*database);
	
	*database = NULL;
	
	pthread_mutex_unlock(&qmtx);

	LOG_DATABASE(this, "- CHECKIN DB %lx\n", (unsigned long)*database);
}


void
Database::drop(const char *document_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return;
	}

    document_id  = prefixed(document_id, DOCUMENT_ID_TERM_PREFIX);

	for (int t = 3; t >= 0; --t) {
		LOG(this, "Deleting: -%s- t:%d\n", document_id, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	    try {
	    	wdb->delete_document(document_id);
	    } catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
    	LOG(this, "Document deleted\n");
		if (commit) _commit();
		return;
	}

	LOG_ERR(this, "ERROR: Cannot delete document: %s!\n", document_id);
}

char*
Database::prefixed(const char *term, const char prefix)
{ 
    char *prefix_term = new char[strlen(term) + 3];
    int i = 0;

    prefix_term[0] = toupper(prefix);
    prefix_term[1] = ':';

    while (term[i] != '\0') { 
        prefix_term[i + 2] = term[i];
        i++;
    }
    prefix_term[i + 2] = '\0';
    return prefix_term;
}

char*
Database::prefixed_string(const char *term, const char *prefix)
{ 
    char *prefix_term = new char[strlen(term) + strlen(prefix) + 1];
    int i = 0, j = 0;
    
    while (prefix[i] != '\0') {
  	  	prefix_term[i] = toupper(prefix[i]);
  	  	i++;
    }
    while (term[j] != '\0') { 
        prefix_term[i] = term[j];
        i++;
        j++;
    }
    prefix_term[i] = '\0';
    return prefix_term;
}

void
Database::_commit()
{
	for (int t = 3; t >= 0; --t) {
		LOG(this, "Commit: t%d\n", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->commit();
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}

		LOG(this, "Commit made\n");
		return;
	}

	LOG_ERR(this, "ERROR: Cannot do commit!\n");
}

void
Database::index(const char *document, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return;
	}

	Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);

	cJSON *root = cJSON_Parse(document);
	if (!root) {
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
	}
	cJSON *doc_id = root ? cJSON_GetObjectItem(root, "id") : NULL;
    cJSON *document_data = root ? cJSON_GetObjectItem(root, "data") : NULL;
    cJSON *document_values = root ? cJSON_GetObjectItem(root, "values") : NULL;
    cJSON *document_terms = root ? cJSON_GetObjectItem(root, "terms") : NULL;
  	LOG(this, "Document_terms: %s\n", cJSON_Print(document_terms));

    Xapian::Document doc;
    
    LOG(this, "Start DATA");
    if (document_data) {
    	const char *doc_data = cJSON_Print(document_data);
    	LOG(this, "Document data: %s\n", doc_data);
		doc.set_data(doc_data); 	
    } 
    LOG(this, "End DATA");

    //Document Values
    
    LOG(this, "Start values");
    if (document_values) {
    	for (int i = 0; i < cJSON_GetArraySize(document_values); i++) {
    		cJSON *value = cJSON_GetArrayItem(document_values, i);
            if (value->type == 3) {
            	doc.add_value((i+1), Xapian::sortable_serialise(value->valuedouble));
            	LOG(this, "%s: %f\n", value->string, value->valuedouble);
            } else {
            	LOG_ERR(this, "ERROR: %s must be a double (%s)\n", value->string, cJSON_Print(value));
            }
  		}
    }
    LOG(this, "End values");

    //Make sure document_id is also a term (otherwise it doesn't replace an existing document)
    const char *document_id  = prefixed(doc_id->valuestring, DOCUMENT_ID_TERM_PREFIX);
    doc.add_value(0, doc_id->valuestring);
    doc.add_boolean_term(document_id);
    
    //Document Terms
    LOG(this, "Document terms: %s\n", cJSON_Print(document_terms));
    if (document_terms) {
    	LOG(this, "Terms..\n");
    	for (int i = 0; i < cJSON_GetArraySize(document_terms); i++) {
            cJSON *term = cJSON_GetArrayItem(document_terms, i);
            LOG(this, "Prefix: %s   Term: %s\n",term->string, term->valuestring);
            doc.add_term(prefixed_string(term->valuestring, term->string), 1);
            LOG(this, "Term: %s was added\n", prefixed_string(term->valuestring, term->string));
  		}
    }
   


    cJSON_Delete(root);
}