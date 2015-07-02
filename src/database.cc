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

#include "database.h"
#include "cJSON_Utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#define XAPIAN_LOCAL_DB_FALLBACK 1

//change prefix to Q only
#define DOCUMENT_ID_TERM_PREFIX "Q"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define FIND_FIELD_RE "(([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[^\"]+\"|[^\" ]+)"
#define FIND_TYPES_RE "(object/)?(array/)?(date|numeric|geospatial|boolean|string)|(object)"
#define MAX_DOCS 100
#define DATABASE_UPDATE_TIME 10


int read_mastery(const std::string &dir, bool force)
{
	LOG_DATABASE(NULL, "+ READING MASTERY OF INDEX '%s'...\n", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		LOG_DATABASE(NULL, "- NO MASTERY OF INDEX '%s'\n", dir.c_str());
		return -1;
	}

	int mastery_level = -1;
	unsigned char buf[512];

	int fd = open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if(force) {
			mastery_level = (int)time(0);
			fd = open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
			if (fd >= 0) {
				snprintf((char *)buf, sizeof(buf), "%d", mastery_level);
				write(fd, buf, strlen((char *)buf));
				close(fd);
			}
		}
	} else {
		mastery_level = 0;
		size_t length = read(fd, (char *)buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			mastery_level = atoi((const char *)buf);
		}
		close(fd);
		if (!mastery_level) {
			mastery_level = (int)time(0);
			fd = open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
			if (fd >= 0) {
				snprintf((char *)buf, sizeof(buf), "%d", mastery_level);
				write(fd, buf, strlen((char *)buf));
				close(fd);
			}
		}
	}

	LOG_DATABASE(NULL, "- MASTERY OF INDEX '%s' is %d\n", dir.c_str(), mastery_level);

	return mastery_level;
}

class ExpandDeciderFilterPrefixes : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

	public:
		ExpandDeciderFilterPrefixes(const std::vector<std::string> &prefixes_)
			: prefixes(prefixes_) { }

		virtual bool operator() (const std::string &term) const;
};


Database::Database(DatabaseQueue * queue_, const Endpoints &endpoints_, int flags_)
	: queue(queue_),
	  endpoints(endpoints_),
	  flags(flags_),
	  hash(endpoints.hash()),
	  access_time(time(0)),
	  mastery_level(-1),
	  db(NULL)
{
	reopen();
	queue->inc_count();
}


Database::~Database()
{
	queue->dec_count();
	delete db;
}


int
Database::read_mastery(const std::string &dir)
{
	if (!local) return -1;
	if (mastery_level != -1) return mastery_level;

	mastery_level = ::read_mastery(dir, true);

	return mastery_level;
}


void
Database::reopen()
{
	access_time = time(0);

	if (db) {
		// Try to reopen
		try {
			db->reopen();
			return;
		} catch (const Xapian::Error &err) {
			LOG_ERR(this, "ERROR: %s\n", err.get_msg().c_str());
			db->close();
			delete db;
			db = NULL;
		}
	}

	Xapian::Database rdb;
	Xapian::WritableDatabase wdb;
	Xapian::Database ldb;

	size_t endpoints_size = endpoints.size();

	const Endpoint *e;
	endpoints_set_t::const_iterator i(endpoints.begin());
	if (flags & DB_WRITABLE) {
		db = new Xapian::WritableDatabase();
		if (endpoints_size != 1) {
			LOG_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s\n", endpoints_size, endpoints.as_string().c_str());
		} else {
			e = &*i;
			if (e->is_local()) {
				local = true;
				wdb = Xapian::WritableDatabase(e->path, (flags & DB_SPAWN) ? Xapian::DB_CREATE_OR_OPEN : Xapian::DB_OPEN);
				if (endpoints_size == 1) read_mastery(e->path);
			} else {
				local = false;
#ifdef XAPIAN_LOCAL_DB_FALLBACK
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
				try {
					ldb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (ldb.get_uuid() == rdb.get_uuid()) {
						// Handle remote endpoints and figure out if the endpoint is a local database
						LOG_DATABASE(this, "Endpoint %s fallback to local database!\n", e->as_string().c_str());
						wdb = Xapian::WritableDatabase(e->path, Xapian::DB_OPEN);
						local = true;
						if (endpoints_size == 1) read_mastery(e->path);
					} else {
						wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
					}
				} catch (const Xapian::DatabaseOpeningError &err) {
					wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
				}
#else
				wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
#endif
			}
			db->add_database(wdb);
		}
	} else {
		db = new Xapian::Database();
		for (; i != endpoints.end(); ++i) {
			e = &*i;
			if (e->is_local()) {
				local = true;
				try {
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (endpoints_size == 1) read_mastery(e->path);
				} catch (const Xapian::DatabaseOpeningError &err) {
					if (!(flags & DB_SPAWN)) throw;
					Xapian::WritableDatabase wdb = Xapian::WritableDatabase(e->path, Xapian::DB_CREATE_OR_OPEN);
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (endpoints_size == 1) read_mastery(e->path);
				}
			} else {
				local = false;
#ifdef XAPIAN_LOCAL_DB_FALLBACK
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
				try {
					ldb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (ldb.get_uuid() == rdb.get_uuid()) {
						LOG_DATABASE(this, "Endpoint %s fallback to local database!\n", e->as_string().c_str());
						// Handle remote endpoints and figure out if the endpoint is a local database
						rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
						local = true;
						if (endpoints_size == 1) read_mastery(e->path);
					}
				} catch (const Xapian::DatabaseOpeningError &err) {}
#else
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
#endif
			}
			db->add_database(rdb);
		}
	}
}


DatabaseQueue::DatabaseQueue()
	: persistent(false),
	  is_switch_db(false),
	  count(0),
	  database_pool(NULL)
{
	pthread_cond_init(&switch_cond,0);
}


DatabaseQueue::~DatabaseQueue()
{
	assert(size() == count);

	pthread_cond_destroy(&switch_cond);

	endpoints_set_t::const_iterator it_e = endpoints.cbegin();
	for (; it_e != endpoints.cend(); it_e++) {
		const Endpoint &endpoint = *it_e;
		database_pool->drop_endpoint_queue(endpoint, this);
	}

	while (!empty()) {
		Database *database;
		if (pop(database)) {
			delete database;
		}
	}
}

void
DatabaseQueue::setup_endpoints(DatabasePool *database_pool_, const Endpoints &endpoints_)
{
	if (database_pool == NULL) {
		database_pool = database_pool_;
		endpoints = endpoints_;
		endpoints_set_t::const_iterator it_e = endpoints.cbegin();
		for (; it_e != endpoints.cend(); it_e++) {
			const Endpoint &endpoint = *it_e;
			database_pool->add_endpoint_queue(endpoint, this);
		}
	}
}

bool
DatabaseQueue::inc_count(int max)
{
	pthread_mutex_lock(&_mtx);

	if (max == -1 || count < max) {
		count++;

		pthread_mutex_unlock(&_mtx);
		return true;
	}

	pthread_mutex_unlock(&_mtx);
	return false;
}


bool
DatabaseQueue::dec_count()
{
	pthread_mutex_lock(&_mtx);

	assert(count > 0);

	if (count > 0) {
		count--;

		pthread_mutex_unlock(&_mtx);
		return true;
	}

	pthread_mutex_unlock(&_mtx);

	return false;
}


DatabasePool::DatabasePool(size_t max_size)
	: finished(false),
	  databases(max_size),
	  writable_databases(max_size),
	  ref_database(NULL)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_cond_init(&checkin_cond,0);

	prefix_rf_node = get_prefix("node", DOCUMENT_CUSTOM_TERM_PREFIX,'s');

	Endpoints ref_endpoints;
	Endpoint ref_endpoint(".refs");
	ref_endpoints.insert(ref_endpoint);

	if(!checkout(&ref_database, ref_endpoints, DB_WRITABLE|DB_PERSISTENT)) {
		INFO(this, "Ref database doesn't exist. Generating database...\n");
		if (!checkout(&ref_database, ref_endpoints,DB_WRITABLE|DB_SPAWN|DB_PERSISTENT)) {
			LOG_ERR(this,"Database refs it could not be checkout\n");
			assert(false);
		}
	}
}


DatabasePool::~DatabasePool()
{
	checkin(&ref_database);

	finish();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);
	pthread_cond_destroy(&checkin_cond);
}


void DatabasePool::finish() {
	pthread_mutex_lock(&qmtx);

	finished = true;

	pthread_mutex_unlock(&qmtx);
}


void DatabasePool::add_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue)
{
	pthread_mutex_lock(&qmtx);

	size_t hash = endpoint.hash();
	std::unordered_set<DatabaseQueue *> &queues_set = queues[hash];
	queues_set.insert(queue);

	pthread_mutex_unlock(&qmtx);
}


void DatabasePool::drop_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue)
{
	pthread_mutex_lock(&qmtx);

	size_t hash = endpoint.hash();
	std::unordered_set<DatabaseQueue *> &queues_set = queues[hash];
	queues_set.erase(queue);

	if (queues_set.empty()) {
		queues.erase(hash);
	}

	pthread_mutex_unlock(&qmtx);
}


int
DatabasePool::get_mastery_level(const std::string &dir)
{
	int mastery_level;

	Database *database_ = NULL;

	Endpoints endpoints;
	endpoints.insert(Endpoint(dir));

	pthread_mutex_lock(&qmtx);
	if (checkout(&database_, endpoints, 0)) {
		mastery_level = database_->mastery_level;
		checkin(&database_);
		pthread_mutex_unlock(&qmtx);
		return mastery_level;
	}
	pthread_mutex_unlock(&qmtx);

	mastery_level = read_mastery(dir, false);

	return mastery_level;
}


bool
DatabasePool::checkout(Database **database, const Endpoints &endpoints, int flags)
{
	bool writable = flags & DB_WRITABLE;
	bool persistent = flags & DB_PERSISTENT;
	bool initref = flags & DB_INIT_REF;

	LOG_DATABASE(this, "++ CHECKING OUT DB %s(%s) [%lx]...\n", writable ? "w" : "r", endpoints.as_string().c_str(), (unsigned long)*database);

	time_t now = time(0);
	Database *database_ = NULL;

	pthread_mutex_lock(&qmtx);

	if (!finished && *database == NULL) {
		DatabaseQueue *queue = NULL;
		size_t hash = endpoints.hash();

		if (writable) {
			queue = &writable_databases[hash];
		} else {
			queue = &databases[hash];
		}
		queue->persistent = persistent;
		queue->setup_endpoints(this, endpoints);

		if (queue->is_switch_db) {
			pthread_cond_wait(&queue->switch_cond, &qmtx);
		}

		if (!queue->pop(database_, 0)) {
			// Increment so other threads don't delete the queue
			if (queue->inc_count(writable ? 1 : -1)) {
				pthread_mutex_unlock(&qmtx);
				try {
					database_ = new Database(queue, endpoints, flags);

					if (writable && initref && endpoints.size() == 1) {
						init_ref(endpoints);
					}

				} catch (const Xapian::DatabaseOpeningError &err) {
				} catch (const Xapian::Error &err) {
					LOG_ERR(this, "ERROR: %s\n", err.get_msg().c_str());
				}
				pthread_mutex_lock(&qmtx);
				queue->dec_count();  // Decrement, count should have been already incremented if Database was created
			} else {
				// Lock until a database is available if it can't get one.
				pthread_mutex_unlock(&qmtx);
				int s = queue->pop(database_);
				pthread_mutex_lock(&qmtx);
				if (!s) {
					LOG_ERR(this, "ERROR: Database is not available. Writable: %d", writable);
				}
			}
		}
		if (database_) {
			*database = database_;
		} else {
			if (queue->count == 0) {
				// There was an error and the queue ended up being empty, remove it!
				databases.erase(hash);
			}
		}
	}

	pthread_mutex_unlock(&qmtx);

	if (!database_) {
		LOG_DATABASE(this, "!! FAILED CHECKOUT DB (%s)!\n", endpoints.as_string().c_str());
		return false;
	}

	if ((now - database_->access_time) >= DATABASE_UPDATE_TIME && !writable) {
		database_->reopen();
		LOG_DATABASE(this, "== REOPEN DB %s(%s) [%lx]\n", (database_->flags & DB_WRITABLE) ? "w" : "r", database_->endpoints.as_string().c_str(), (unsigned long)database_);
	}
	database_->checkout_revision = database_->db->get_revision_info();

	LOG_DATABASE(this, "++ CHECKED OUT DB %s(%s), %s at rev:%s %lx\n", writable ? "w" : "r", endpoints.as_string().c_str(), database_->local ? "local" : "remote", repr(database_->checkout_revision, false).c_str(), (unsigned long)database_);

	return true;
}


void
DatabasePool::checkin(Database **database)
{
	Database *database_ = *database;

	LOG_DATABASE(this, "-- CHECKING IN DB %s(%s) [%lx]...\n", (database_->flags & DB_WRITABLE) ? "w" : "r", database_->endpoints.as_string().c_str(), (unsigned long)database_);

	pthread_mutex_lock(&qmtx);

	DatabaseQueue *queue;

	if (database_->flags & DB_WRITABLE) {
		queue = &writable_databases[database_->hash];
		if (database_->local && database_->mastery_level != -1) {
			std::string new_revision = database_->db->get_revision_info();
			if (new_revision != database_->checkout_revision) {
				Endpoint endpoint = *database_->endpoints.begin();
				endpoint.mastery_level = database_->mastery_level;
				updated_databases.push(endpoint);
			}
		}
	} else {
		queue = &databases[database_->hash];
	}

	assert(database_->queue == queue);

	int flags = database_->flags;
	Endpoints endpoints = database_->endpoints;

	if (database_->flags & DB_VOLATILE) {
		delete database_;
	} else {
		queue->push(database_);
	}

	if (queue->is_switch_db) {
		Endpoints::const_iterator it_edp;
		for(endpoints.cbegin(); it_edp != endpoints.cend(); it_edp++) {
			const Endpoint &endpoint = *it_edp;
			switch_db(endpoint);
		}
	}

	assert(queue->count >= queue->size());

	*database = NULL;

	pthread_mutex_unlock(&qmtx);

	pthread_cond_broadcast(&checkin_cond);

	LOG_DATABASE(this, "-- CHECKED IN DB %s(%s) [%lx]\n", (flags & DB_WRITABLE) ? "w" : "r", endpoints.as_string().c_str(), (unsigned long)database_);
}


void DatabasePool::init_ref(Endpoints endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it = endpoints.begin();

	if (ref_database) {
		for (; endp_it != endpoints.end(); endp_it++) {
			std::string unique_id = prefixed(get_slot_hex(endp_it->path),DOCUMENT_ID_TERM_PREFIX);

			Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
			if (p == ref_database->db->postlist_end(unique_id)) {
				doc.add_boolean_term(unique_id);
				doc.add_term(prefixed(endp_it->node_name,prefix_rf_node));
				doc.add_value(0, std::to_string(0));
				ref_database->replace(unique_id, doc, true);
			} else {
				LOG(this,"The document already exists nothing to do\n");
			}
		}
	}
}


void DatabasePool::inc_ref(Endpoints endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it = endpoints.begin();

	for (; endp_it != endpoints.end(); endp_it++) {
		std::string unique_id = prefixed(get_slot_hex(endp_it->path),DOCUMENT_ID_TERM_PREFIX);

		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);

		if (p == ref_database->db->postlist_end(unique_id)) {
			//QUESTION: Document not found - should add?
			//QUESTION: This case could happen?
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name,prefix_rf_node));
			doc.add_value(0, std::to_string(0));
			ref_database->replace(unique_id, doc, true);
		} else {
			//Document found - reference increased
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name,prefix_rf_node));
			int nref = strtoint(doc.get_value(0));
			doc.add_value(0, std::to_string(nref+1));
			ref_database->replace(unique_id, doc, true);
		}
	}
}


void DatabasePool::dec_ref(Endpoints endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it = endpoints.begin();

	for (; endp_it != endpoints.end(); endp_it++) {

		std::string unique_id = prefixed(get_slot_hex(endp_it->path),DOCUMENT_ID_TERM_PREFIX);
		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);

		if (p != ref_database->db->postlist_end(unique_id)) {
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name,prefix_rf_node));
			int nref = strtoint(doc.get_value(0))-1;
			doc.add_value(0, std::to_string(nref));
			ref_database->replace(unique_id, doc, true);

			if (nref == 0) {
				//qmtx need a lock
				pthread_cond_wait(&checkin_cond,&qmtx);
				delete_files(endp_it->path);
			}
		}
	}
}


bool DatabasePool::switch_db(const Endpoint &endpoint)
{
	Database *database;

	pthread_mutex_lock(&qmtx);

	size_t hash = endpoint.hash();
	DatabaseQueue *queue;

	std::unordered_set<DatabaseQueue *> &queues_set = queues[hash];
	std::unordered_set<DatabaseQueue *>::const_iterator it_qs;

	bool switched = true;
	for(it_qs=queues_set.cbegin(); it_qs != queues_set.cend(); it_qs++) {
		queue = *it_qs;
		queue->is_switch_db = true;
		if (queue->count != queue->size()) {
			switched = false;
			break;
		}
	}

	if (switched) {
		for(it_qs=queues_set.cbegin(); it_qs != queues_set.cend(); it_qs++) {
			queue = *it_qs;
			while (!queue->empty()) {
				if (queue->pop(database)) {
					delete database;
				}
			}
		}

		move_files(endpoint.path + "/.tmp", endpoint.path);

		for(it_qs=queues_set.cbegin(); it_qs != queues_set.cend(); it_qs++) {
			queue = *it_qs;
			queue->is_switch_db = false;

			pthread_cond_broadcast(&queue->switch_cond);
		}
	} else {
		LOG(this,"Inside switch_db not queue->count == queue->size()\n");
	}

	pthread_mutex_unlock(&qmtx);

	return switched;
}


pcre *Database::compiled_find_field_re = NULL;
pcre *Database::compiled_find_types_re = NULL;


bool
Database::drop(const std::string &doc_id, bool commit)
{
	if (!(flags & DB_WRITABLE)) {
		LOG_ERR(this, "ERROR: database is read-only\n");
		return false;
	}

	std::string document_id  = prefixed(doc_id, DOCUMENT_ID_TERM_PREFIX);

	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Deleting: -%s- t:%d\n", document_id.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->delete_document(document_id);
		}catch (const Xapian::DatabaseError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			return false;
		}
		catch (const Xapian::DatabaseCorruptError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			return false;
		} catch (const Xapian::Error &e) {
			LOG(this, "Inside catch drop\n");
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		LOG_DATABASE_WRAP(this, "Document deleted\n");
		return (commit) ? _commit() : true;
	}

	LOG_ERR(this, "ERROR: Cannot delete document: %s!\n", document_id.c_str());
	return false;
}


bool
Database::_commit()
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Commit: t%d\n", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->commit();
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		LOG_DATABASE_WRAP(this, "Commit made\n");
		return true;
	}

	LOG_ERR(this, "ERROR: Cannot do commit!\n");
	return false;
}


bool
Database::patch(cJSON *patches, const std::string &_document_id, bool commit)
{
	if (!(flags & DB_WRITABLE)) {
		LOG_ERR(this, "ERROR: database is read-only\n");
		return false;
	}

	Xapian::Document document;
	Xapian::QueryParser queryparser;
	queryparser.add_prefix("id", "Q");
	Xapian::Query query = queryparser.parse_query(std::string("id:" + _document_id));
	Xapian::Enquire enquire(*db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	Xapian::MSetIterator m = mset.begin();
	int t = 3;
	for (; t >= 0; --t) {
		try {
			document = db->get_document(*m);
			break;
		} catch (Xapian::InvalidArgumentError &err) {
			return false;
		} catch (Xapian::DocNotFoundError &err) {
			return false;
		} catch (const Xapian::Error &err) {
			reopen();
			m = mset.begin();
		}
	}

	cJSON *data_json = cJSON_Parse(document.get_data().c_str());
	if (!data_json) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		return false;
	}

	if (cJSONUtils_ApplyPatches(data_json, patches) == 0) {
		//Object patched
		return index(data_json, _document_id, commit);
	}

	//Object no patched
	return false;
}


bool
Database::is_reserved(const std::string &word)
{
	if (word.compare(RESERVED_WEIGHT)      != 0 &&
		word.compare(RESERVED_POSITION)    != 0 &&
		word.compare(RESERVED_LANGUAGE)    != 0 &&
		word.compare(RESERVED_SPELLING)    != 0 &&
		word.compare(RESERVED_POSITIONS)   != 0 &&
		word.compare(RESERVED_TEXTS)       != 0 &&
		word.compare(RESERVED_VALUES)      != 0 &&
		word.compare(RESERVED_TERMS)       != 0 &&
		word.compare(RESERVED_DATA)        != 0 &&
		word.compare(RESERVED_ACCURACY)    != 0 &&
		word.compare(RESERVED_ACC_PREFIX)  != 0 &&
		word.compare(RESERVED_STORE)       != 0 &&
		word.compare(RESERVED_TYPE)        != 0 &&
		word.compare(RESERVED_ANALYZER)    != 0 &&
		word.compare(RESERVED_DYNAMIC)     != 0 &&
		word.compare(RESERVED_D_DETECTION) != 0 &&
		word.compare(RESERVED_N_DETECTION) != 0 &&
		word.compare(RESERVED_G_DETECTION) != 0 &&
		word.compare(RESERVED_B_DETECTION) != 0 &&
		word.compare(RESERVED_S_DETECTION) != 0 &&
		word.compare(RESERVED_VALUE)       != 0 &&
		word.compare(RESERVED_NAME)        != 0 &&
		word.compare(RESERVED_SLOT)        != 0 &&
		word.compare(RESERVED_INDEX)       != 0 &&
		word.compare(RESERVED_PREFIX)      != 0 &&
		word.compare(RESERVED_ID)          != 0) {
		return false;
	}
	return true;
}


void
Database::index_fields(cJSON *item, const std::string &item_name, specifications_t &spc_now, Xapian::Document &doc, cJSON *properties, bool is_value, bool find)
{
	std::string subitem_name;
	specifications_t spc_bef = spc_now;
	if (item->type == cJSON_Object) {
		update_specifications(item, spc_now, properties);
		int offspring = 0;
		int elements = cJSON_GetArraySize(item);
		for (int i = 0; i < elements; i++) {
			cJSON *subitem = cJSON_GetArrayItem(item, i);
			cJSON *subproperties;
			if (!is_reserved(subitem->string)) {
				bool find = true;
				subproperties = cJSON_GetObjectItem(properties, subitem->string);
				if (!subproperties) {
					find = false;
					subproperties = cJSON_CreateObject();
					cJSON_AddItemToObject(properties, subitem->string, subproperties);
				}
				subitem_name = (item_name.size() != 0) ? item_name + OFFSPRING_UNION + subitem->string : subitem->string;
				if (subitem_name.at(subitem_name.size() - 3) == OFFSPRING_UNION[0]) {
					std::string language(subitem_name, subitem_name.size() - 2, subitem_name.size());
					spc_now.language = is_language(language) ? language : spc_now.language;
				}
				index_fields(subitem, subitem_name, spc_now, doc, subproperties, is_value, find);
				offspring++;
			} else if (strcmp(subitem->string, RESERVED_VALUE) == 0 && subitem->type != cJSON_Object) {
				if (is_value) {
					index_values(doc, subitem, spc_now, item_name, properties, find);
				} else {
					char type = spc_now.sep_types[2];
					if (type == STRING_TYPE && subitem->type != cJSON_Array && ((strlen(subitem->valuestring) > 30 && std::string(subitem->valuestring).find(" ") != -1) || std::string(subitem->valuestring).find("\n") != -1)) {
						index_texts(doc, subitem, spc_now, item_name, properties, find);
					} else {
						index_values(doc, subitem, spc_now, item_name, properties, find);
						index_terms(doc, subitem, spc_now, item_name, properties, true);
					}
				}
			}
		}
		if (offspring != 0) {
			cJSON *_type = cJSON_GetObjectItem(properties, RESERVED_TYPE);
			if (_type) {
				if (std::string(_type->valuestring).find("object") == -1) {
					spc_now.sep_types[0] = OBJECT_TYPE;
					std::string s_type("object/" + std::string(_type->valuestring));
					cJSON_ReplaceItemInObject(properties, RESERVED_TYPE, cJSON_CreateString(s_type.c_str()));
				}
			} else {
				spc_now.sep_types[0] = OBJECT_TYPE;
				cJSON_AddStringToObject(properties, RESERVED_TYPE, "object");
			}
		}
	} else {
		update_specifications(item, spc_now, properties);
		if (is_value) {
			index_values(doc, item, spc_now, item_name, properties, find);
		} else {
			char type = spc_now.sep_types[2];
			if (type == STRING_TYPE && item->type != cJSON_Array && ((strlen(item->valuestring) > 30 && std::string(item->valuestring).find(" ") != -1) || std::string(item->valuestring).find("\n") != -1)) {
				index_texts(doc, item, spc_now, item_name, properties, find);
			} else {
				index_terms(doc, item, spc_now, item_name, properties, find);
				index_values(doc, item, spc_now, item_name, properties, true);
			}
		}
	}
	spc_now = spc_bef;
}


void
Database::index_texts(Xapian::Document &doc, cJSON *text, specifications_t &spc, const std::string &name, cJSON *scheme, bool find)
{
	//LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!spc.store) return;
	if (!spc.dynamic && !find) throw MSG_Error("This object is not dynamic");

	if (text->type != cJSON_String || (spc.sep_types[2] != STRING_TYPE && spc.sep_types[2] != NO_TYPE)) throw MSG_Error("Data inconsistency should be string");

	std::string prefix;
	if (!name.empty()) {
		if (!find) {
			if (spc.sep_types[2] == NO_TYPE) cJSON_AddStringToObject(scheme, RESERVED_TYPE, "string");
			cJSON_AddStringToObject(scheme, "_analyzer", spc.analyzer.c_str());
			cJSON_AddStringToObject(scheme, "_index" , "analyzed");
		}
		cJSON *_prefix = cJSON_GetObjectItem(scheme, RESERVED_PREFIX);
		if (!_prefix) {
			prefix = get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, STRING_TYPE);
			cJSON_AddStringToObject(scheme, RESERVED_PREFIX , prefix.c_str());
		} else {
			prefix = _prefix->valuestring;
		}
	}

	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);

	Xapian::TermGenerator term_generator;
	term_generator.set_document(doc);
	term_generator.set_stemmer(Xapian::Stem(spc.language));
	if (spc.spelling) {
		term_generator.set_database(*wdb);
		term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		if (spc.analyzer.compare("STEM_SOME") == 0) {
			term_generator.set_stemming_strategy(term_generator.STEM_SOME);
		} else if (spc.analyzer.compare("STEM_NONE") == 0) {
			term_generator.set_stemming_strategy(term_generator.STEM_NONE);
		} else if (spc.analyzer.compare("STEM_ALL") == 0) {
			term_generator.set_stemming_strategy(term_generator.STEM_ALL);
		} else if (spc.analyzer.compare("STEM_ALL_Z") == 0) {
			term_generator.set_stemming_strategy(term_generator.STEM_ALL_Z);
		} else {
			term_generator.set_stemming_strategy(term_generator.STEM_SOME);
		}
	}

	if (spc.positions) {
		(prefix.empty()) ? term_generator.index_text_without_positions(text->valuestring, spc.weight) : term_generator.index_text_without_positions(text->valuestring, spc.weight, prefix);
		LOG_DATABASE_WRAP(this, "Text to Index with positions = %s: %s\n", prefix.c_str(), text->valuestring);
	} else {
		(prefix.empty()) ? term_generator.index_text(text->valuestring, spc.weight) : term_generator.index_text(text->valuestring, spc.weight, prefix);
		LOG_DATABASE_WRAP(this, "Text to Index = %s: %s\n", prefix.c_str(), text->valuestring);
	}
}


void
Database::index_terms(Xapian::Document &doc, cJSON *terms, specifications_t &spc, const std::string &name, cJSON *scheme, bool find)
{
	//LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!spc.store) return;
	if (!spc.dynamic && !find) throw MSG_Error("This object is not dynamic");

	char type = (spc.sep_types[2] == NO_TYPE) ? get_type(terms, spc): spc.sep_types[2];
	if (type == NO_TYPE) throw MSG_Error("The field's value %s is ambiguous", name.c_str());

	std::string prefix;
	if (!name.empty()) {
		if (!find) {
			if (spc.sep_types[2] == NO_TYPE) cJSON_AddStringToObject(scheme, RESERVED_TYPE, str_type(type).c_str());
			cJSON_AddStringToObject(scheme, RESERVED_INDEX, "not analyzed");
		}
		cJSON *_prefix = cJSON_GetObjectItem(scheme, RESERVED_PREFIX);
		if (!_prefix) {
			prefix = get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, type);
			cJSON_AddStringToObject(scheme, RESERVED_PREFIX , prefix.c_str());
		} else {
			prefix = _prefix->valuestring;
		}
	} else {
		prefix = DOCUMENT_CUSTOM_TERM_PREFIX;
	}

	int elements = 1;

	// If the type in scheme is not array, scheme is updated.
	if (terms->type == cJSON_Array) {
		if (type == GEO_TYPE) throw MSG_Error("An array can not serialized as a Geo Spatial");

		elements = cJSON_GetArraySize(terms);
		cJSON *term = cJSON_GetArrayItem(terms, 0);

		if (term->type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");

		cJSON *_type = cJSON_GetObjectItem(scheme, RESERVED_TYPE);
		if (_type) {
			if (std::string(_type->valuestring).find("array") == -1) {
				std::string s_type;
				if (spc.sep_types[0] == OBJECT_TYPE) {
					s_type = "object/array/" + str_type(type);
				} else {
					s_type = "array/" + str_type(type);
				}
				cJSON_ReplaceItemInObject(scheme, RESERVED_TYPE, cJSON_CreateString(s_type.c_str()));
			}
		} else {
			cJSON_AddStringToObject(scheme, RESERVED_TYPE, std::string("array/" + str_type(type)).c_str());
		}
	}

	for (int j = 0; j < elements; j++) {
		cJSON *term = (terms->type == cJSON_Array) ? cJSON_GetArrayItem(terms, j) : terms;

		std::string term_v(cJSON_Print(term));
		if (term->type == cJSON_String) {
			term_v.assign(term_v, 1, term_v.size() - 2);

			// If it is not a boolean prefix and it's a string type the value is passed to lowercase.
			if (type == STRING_TYPE && !strhasupper(name)) {
				term_v = stringtolower(term_v);
			}
		} else if (term->type == cJSON_Number) {
			term_v = std::to_string(term->valuedouble);
		}
		LOG_DATABASE_WRAP(this, "Term -> %s: %s\n", prefix.c_str(), term_v.c_str());

		if (type == GEO_TYPE) {
			bool partials = DE_PARTIALS;
			double error = DE_ERROR;
			if (!spc.accuracy.empty()) {
				partials = (serialise_bool(spc.accuracy.at(0)).compare("f") == 0) ? false : true;
				error = (spc.accuracy.size() >= 2) ? strtodouble(spc.accuracy.at(1)) : DE_ERROR;
			}
			std::vector<std::string> terms = serialise_geo(term_v, partials, error);
			std::vector<std::string>::const_iterator it(terms.begin());
			for ( ; it != terms.end(); it++) {
				if (spc.position >= 0) {
					std::string nameterm(prefixed(*it, prefix));
					doc.add_posting(nameterm, spc.position, spc.weight);
					LOG_DATABASE_WRAP(this, "Posting: %s\n", repr(nameterm).c_str());
				} else {
					std::string nameterm(prefixed(*it, prefix));
					doc.add_term(nameterm, spc.weight);
					LOG_DATABASE_WRAP(this, "Term: %s\n", repr(nameterm).c_str());
				}
			}
		} else {
			term_v = serialise(type, term_v);
			if (term_v.size() == 0) throw MSG_Error("%s: %s can not be serialized", name.c_str(), term_v.c_str());

			if (spc.position >= 0) {
				std::string nameterm(prefixed(term_v, prefix));
				doc.add_posting(nameterm, spc.position, spc.weight);
				LOG_DATABASE_WRAP(this, "Posting: %s\n", repr(nameterm).c_str());
			} else {
				std::string nameterm(prefixed(term_v, prefix));
				doc.add_term(nameterm, spc.weight);
				LOG_DATABASE_WRAP(this, "Term: %s\n", repr(nameterm).c_str());
			}
		}
	}
}


void
Database::index_values(Xapian::Document &doc, cJSON *values, specifications_t &spc, const std::string &name, cJSON *scheme, bool find)
{
	//LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!spc.store) return;
	if (!spc.dynamic && !find) throw MSG_Error("This object is not dynamic");

	char type = spc.sep_types[2];
	if (type == NO_TYPE) {
		type = get_type(values, spc);
		spc.type = type;
	}

	if (type == NO_TYPE) throw MSG_Error("The field's value %s is ambiguous", name.c_str());

	if (type == GEO_TYPE) {
		// Geo is looking for space regions, which are specified by terms so only
		// terms are indexed there is no point looking for values.
		index_terms(doc, values, spc, name, scheme, find);
		return;
	}

	if (!find) {
		if (spc.sep_types[2] == NO_TYPE) {
			cJSON_AddStringToObject(scheme, RESERVED_TYPE, str_type(type).c_str());
		}
		cJSON_AddStringToObject(scheme, RESERVED_INDEX, "not analyzed");
	}

	unsigned int slot;
	cJSON *_slot = cJSON_GetObjectItem(scheme, RESERVED_SLOT);
	if (!_slot) {
		slot = get_slot(name);
		cJSON_AddNumberToObject(scheme, RESERVED_SLOT, slot);
	} else {
		slot = _slot->valueint;
	}

	int elements = 1;

	// If the type in scheme is not array, scheme is updated.
	if (values->type == cJSON_Array) {
		if (type == GEO_TYPE) throw MSG_Error("An array can not serialized as a Geo Spatial");

		elements = cJSON_GetArraySize(values);
		cJSON *value = cJSON_GetArrayItem(values, 0);

		if (value->type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");

		cJSON *_type = cJSON_GetObjectItem(scheme, RESERVED_TYPE);
		if (_type) {
			if (std::string(_type->valuestring).find("array") == -1) {
				std::string s_type;
				if (spc.sep_types[0] == OBJECT_TYPE) {
					s_type = "object/array/" + str_type(type);
				} else {
					s_type = "array/" + str_type(type);
				}
				cJSON_ReplaceItemInObject(scheme, RESERVED_TYPE, cJSON_CreateString(s_type.c_str()));
			}
		} else {
			cJSON_AddStringToObject(scheme, RESERVED_TYPE, std::string("array/" + str_type(type)).c_str());
		}
	}

	StringList s;

	for (int j = 0; j < elements; j++) {
		cJSON *value = (values->type == cJSON_Array) ? cJSON_GetArrayItem(values, j) : values;

		std::string value_v = cJSON_Print(value);
		if (value->type == cJSON_String) {
			value_v.assign(value_v, 1, value_v.size() - 2);
		} else if (value->type == cJSON_Number) {
			value_v = std::to_string(value->valuedouble);
		}
		LOG_DATABASE_WRAP(this, "Name: (%s) Value: (%s)\n", name.c_str(), value_v.c_str());

		std::string value_s = serialise(type, value_v);
		if (value_s.empty()) {
			throw MSG_Error("%s: %s can not serialized", name.c_str(), value_v.c_str());
		}
		s.push_back(value_s);

		//terms generated by accuracy.
		if (!spc.accuracy.empty()) {
			std::vector<std::string>::const_iterator it = spc.accuracy.begin();
			switch (type) {
				case NUMERIC_TYPE: {
					int num_pre = 0;
					cJSON *_prefix_accuracy = cJSON_GetObjectItem(scheme, RESERVED_ACC_PREFIX);
					cJSON *_new_prefix_acc = (_prefix_accuracy) ? NULL : cJSON_CreateArray();
					for ( ; it != spc.accuracy.end(); it++, num_pre++) {
						long long int  _v = strtolonglong(value_v);
						long long int acc = strtolonglong(*it);
						if (acc >= 1) {
							std::string term_v = std::to_string(_v - _v % acc);
							std::string prefix;
							if (_prefix_accuracy) {
								prefix = cJSON_GetArrayItem(_prefix_accuracy, num_pre)->valuestring;
							} else {
								prefix = get_prefix(name + std::to_string(num_pre), DOCUMENT_CUSTOM_TERM_PREFIX, type);
								cJSON_AddItemToArray(_new_prefix_acc, cJSON_CreateString(prefix.c_str()));
							}
							term_v = serialise(type, term_v);
							if (spc.position >= 0) {
								std::string nameterm(prefixed(term_v, prefix));
								doc.add_posting(nameterm, spc.position, spc.weight);
								LOG_DATABASE_WRAP(this, "Posting by accuracy: %s\n", repr(nameterm).c_str());
							} else {
								std::string nameterm(prefixed(term_v, prefix));
								doc.add_term(nameterm, spc.weight);
								LOG_DATABASE_WRAP(this, "Term by accuracy: %s\n", repr(nameterm).c_str());
							}
						}
					}
					if (_new_prefix_acc) {
						cJSON_AddItemToObject(scheme, RESERVED_ACC_PREFIX, _new_prefix_acc);
					}
				}
				case DATE_TYPE: {
					for ( ; it != spc.accuracy.end(); it++) {
						std::string acc(value_v), _v(stringtolower(*it));
						if (isNumeric(acc)) {
							char date[25];
							double epoch = strtodouble(timestamp_date(acc));
							time_t timestamp = (time_t) epoch;
							std::string milliseconds = std::to_string(epoch);
							milliseconds.assign(milliseconds.c_str() + milliseconds.find("."), 4);
							struct tm *timeinfo = gmtime(&timestamp);
							sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d%s", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds.c_str());
							acc = value_v = date;
						}
						if (_v.compare("year") == 0) {
							acc = acc + "||//y";
						} else if (_v.compare("month") == 0) {
							acc = acc + "||//M";
						} else if (_v.compare("day") == 0) {
							acc = acc + "||//d";
						} else if (_v.compare("hour") == 0) {
							acc = acc + "||//h";
						} else if (_v.compare("minute") == 0) {
							acc = acc + "||//m";
						} else if (_v.compare("second") == 0) {
							acc = acc + "||//s";
						}
						if (acc.compare(value_v) != 0) {
							acc = timestamp_date(acc);
							cJSON *new_term = cJSON_CreateString(acc.c_str());
							index_terms(doc, new_term, spc, name, scheme, find);
							cJSON_Delete(new_term);
						}
					}
				}
			}
		}
	}

	doc.add_value(slot, s.serialise());
	LOG_DATABASE_WRAP(this, "Slot: %u serialized: %s\n", slot, repr(s.serialise()).c_str());
}


void
Database::update_specifications(cJSON *item, specifications_t &spc_now, cJSON *scheme)
{
	//clean specifications locals
	spc_now.type = "";
	spc_now.sep_types[0]  = spc_now.sep_types[1] = spc_now.sep_types[2] = NO_TYPE;
	spc_now.accuracy.clear();

	cJSON *spc = cJSON_GetObjectItem(item, RESERVED_POSITION);
	if (cJSON *position = cJSON_GetObjectItem(scheme, RESERVED_POSITION)) {
		if (spc) {
			if (spc->type == cJSON_Number) {
				spc_now.position = spc->valueint;
			} else {
				throw MSG_Error("Data inconsistency %s should be integer", RESERVED_POSITION);
			}
		} else {
			spc_now.position = position->valueint;
		}
	} else if (spc) {
		if (spc->type == cJSON_Number) {
			spc_now.position = spc->valueint;
			cJSON_AddNumberToObject(scheme, RESERVED_POSITION, spc->valueint);
		} else {
			throw MSG_Error("Data inconsistency %s should be integer", RESERVED_POSITION);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT);
	if (cJSON *weight = cJSON_GetObjectItem(scheme, RESERVED_WEIGHT)) {
		if (spc) {
			if (spc->type == cJSON_Number) {
				spc_now.weight = spc->valueint;
			} else {
				throw MSG_Error("Data inconsistency %s should be integer", RESERVED_WEIGHT);
			}
		} else {
			spc_now.weight = weight->valueint;
		}
	} else if (spc) {
		if (spc->type == cJSON_Number) {
			spc_now.weight = spc->valueint;
			cJSON_AddNumberToObject(scheme, RESERVED_WEIGHT, spc->valueint);
		} else {
			throw MSG_Error("Data inconsistency %s should be integer", RESERVED_WEIGHT);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE);
	if (cJSON *language = cJSON_GetObjectItem(scheme, RESERVED_LANGUAGE)) {
		if (spc) {
			if (spc->type == cJSON_String) {
				spc_now.language = is_language(spc->valuestring) ? spc->valuestring : spc_now.language;
			} else {
				throw MSG_Error("Data inconsistency %s should be string", RESERVED_LANGUAGE);
			}
		} else {
			spc_now.language = language->valuestring;
		}
	} else if (spc) {
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : spc_now.language;
			cJSON_AddStringToObject(scheme, RESERVED_LANGUAGE, lan.c_str());
			spc_now.language = lan;
		} else {
			throw MSG_Error("Data inconsistency %s should be string", RESERVED_LANGUAGE);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_SPELLING);
	if (cJSON *spelling = cJSON_GetObjectItem(scheme, RESERVED_SPELLING)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.spelling = false;
			} else if (spc->type == cJSON_True) {
				spc_now.spelling = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_SPELLING);
			}
		} else {
			spc_now.spelling = (spelling->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_SPELLING);
			spc_now.spelling = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_SPELLING);
			spc_now.spelling = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_SPELLING);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS);
	if (cJSON *positions = cJSON_GetObjectItem(scheme, RESERVED_POSITIONS)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.positions = false;
			} else if (spc->type == cJSON_True) {
				spc_now.positions = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_POSITIONS);
			}
		} else {
			spc_now.positions = (positions->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_POSITIONS);
			spc_now.positions = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_POSITIONS);
			spc_now.positions = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_POSITIONS);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_ACCURACY);
	if (cJSON *accuracy = cJSON_GetObjectItem(scheme, RESERVED_ACCURACY)) {
		if (spc) {
			LOG(this, "Accuracy will not be taken into account because it was previously defined; if you want to define a new accuracy, you need to change the schema.\n");
		}
		spc_now.accuracy.clear();
		if (accuracy->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(accuracy);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(accuracy, i);
				if (acc->type == cJSON_String) {
					spc_now.accuracy.push_back(acc->valuestring);
				} else if (acc->type == cJSON_Number) {
					spc_now.accuracy.push_back(std::to_string(acc->valuedouble));
				}
			}
		} else if (accuracy->type == cJSON_String) {
			spc_now.accuracy.push_back(accuracy->valuestring);
		} else if (accuracy->type == cJSON_Number) {
			spc_now.accuracy.push_back(std::to_string(accuracy->valuedouble));
		}
	} else if (spc) {
		cJSON *acc_s = cJSON_CreateArray();
		if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(spc, i);
				if (acc->type == cJSON_String) {
					spc_now.accuracy.push_back(acc->valuestring);
					cJSON_AddItemToArray(acc_s, cJSON_CreateString(acc->valuestring));
				} else if (acc->type == cJSON_Number) {
					std::string str = std::to_string(acc->valuedouble);
					spc_now.accuracy.push_back(str);
					cJSON_AddItemToArray(acc_s, cJSON_CreateString(str.c_str()));
				} else {
					throw MSG_Error("Data inconsistency %s should be an array of strings or numerics", RESERVED_ACCURACY);
				}
			}
		} else if (spc->type == cJSON_String) {
			spc_now.accuracy.push_back(spc->valuestring);
			cJSON_AddItemToArray(acc_s, cJSON_CreateString(spc->valuestring));
		} else if (spc->type == cJSON_Number) {
			std::string str = std::to_string(spc->valuedouble);
			spc_now.accuracy.push_back(str);
			cJSON_AddItemToArray(acc_s, cJSON_CreateString(str.c_str()));
		} else {
			throw MSG_Error("Data inconsistency %s should be string or numeric", RESERVED_ACCURACY);
		}
		cJSON_AddItemToObject(scheme, RESERVED_ACCURACY, acc_s);
	}

	spc = cJSON_GetObjectItem(item, RESERVED_TYPE);
	if (cJSON *type = cJSON_GetObjectItem(scheme, RESERVED_TYPE)) {
		if (spc) {
			if (spc->type == cJSON_String) {
				std::string _type = stringtolower(spc->valuestring);
				if (set_types(_type, spc_now.sep_types)) {
					if (std::string(type->valuestring).find(str_type(spc_now.sep_types[2])) == -1) throw MSG_Error("Type inconsistency, it's %s not %s", type->valuestring, _type.c_str());
					spc_now.type = _type;
				} else {
					throw MSG_Error("%s is invalid type", spc->valuestring);
				}
			} else {
				throw MSG_Error("Data inconsistency %s should be string", RESERVED_TYPE);
			}
		} else {
			spc_now.type = type->valuestring;
			set_types(spc_now.type, spc_now.sep_types);
		}
	} else if (spc) {
		if (spc->type == cJSON_String) {
			if (set_types(spc->valuestring, spc_now.sep_types)) {
				spc_now.type = stringtolower(spc->valuestring);
				cJSON_AddStringToObject(scheme, RESERVED_TYPE, spc_now.type.c_str());
			} else {
				throw MSG_Error("%s is invalid type", spc->valuestring);
			}
		} else {
			throw MSG_Error("Data inconsistency %s should be string", RESERVED_TYPE);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_STORE);
	if (cJSON *store = cJSON_GetObjectItem(scheme, RESERVED_STORE)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.store = false;
			} else if (spc->type == cJSON_True) {
				spc_now.store = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_STORE);
			}
		} else {
			spc_now.store = (store->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_STORE);
			spc_now.store = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_STORE);
			spc_now.store = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_STORE);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER);
	if (cJSON *analyzer = cJSON_GetObjectItem(scheme, RESERVED_ANALYZER)) {
		if (spc) {
			if (spc->type == cJSON_String) {
				spc_now.analyzer = stringtoupper(spc->valuestring);
			} else {
				throw MSG_Error("Data inconsistency %s should be string", RESERVED_ANALYZER);
			}
		} else {
			spc_now.analyzer = analyzer->valuestring;
		}
	} else if (spc) {
		if (spc->type == cJSON_String) {
			std::string _analyzer = stringtoupper(spc->valuestring);
			cJSON_AddStringToObject(scheme, RESERVED_ANALYZER, _analyzer.c_str());
			spc_now.analyzer = _analyzer;
		} else {
			throw MSG_Error("Data inconsistency %s should be string", RESERVED_ANALYZER);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_DYNAMIC);
	if (cJSON *dynamic = cJSON_GetObjectItem(scheme, RESERVED_DYNAMIC)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.dynamic = false;
			} else if (spc->type == cJSON_True) {
				spc_now.dynamic = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_DYNAMIC);
			}
		} else {
			spc_now.dynamic = (dynamic->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_DYNAMIC);
			spc_now.dynamic = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_DYNAMIC);
			spc_now.dynamic = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_DYNAMIC);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_D_DETECTION);
	if (cJSON *date_detection = cJSON_GetObjectItem(scheme, RESERVED_D_DETECTION)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.date_detection = false;
			} else if (spc->type == cJSON_True) {
				spc_now.date_detection = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_D_DETECTION);
			}
		} else {
			spc_now.date_detection = (date_detection->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_D_DETECTION);
			spc_now.date_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_D_DETECTION);
			spc_now.date_detection = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_D_DETECTION);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_N_DETECTION);
	if (cJSON *numeric_detection = cJSON_GetObjectItem(scheme, RESERVED_N_DETECTION)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.numeric_detection = false;
			} else if (spc->type == cJSON_True) {
				spc_now.numeric_detection = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_N_DETECTION);
			}
		} else {
			spc_now.numeric_detection = (numeric_detection->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_N_DETECTION);
			spc_now.numeric_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_N_DETECTION);
			spc_now.numeric_detection = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_N_DETECTION);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_G_DETECTION);
	if (cJSON *geo_detection = cJSON_GetObjectItem(scheme, RESERVED_G_DETECTION)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.geo_detection = false;
			} else if (spc->type == cJSON_True) {
				spc_now.geo_detection = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_G_DETECTION);
			}
		} else {
			spc_now.geo_detection = (geo_detection->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_G_DETECTION);
			spc_now.geo_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_G_DETECTION);
			spc_now.geo_detection = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_G_DETECTION);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_B_DETECTION);
	if (cJSON *bool_detection = cJSON_GetObjectItem(scheme, RESERVED_B_DETECTION)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.bool_detection = false;
			} else if (spc->type == cJSON_True) {
				spc_now.bool_detection = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_B_DETECTION);
			}
		} else {
			spc_now.bool_detection = (bool_detection->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_B_DETECTION);
			spc_now.bool_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_B_DETECTION);
			spc_now.bool_detection = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_B_DETECTION);
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_S_DETECTION);
	if (cJSON *string_detection = cJSON_GetObjectItem(scheme, RESERVED_S_DETECTION)) {
		if (spc) {
			if (spc->type == cJSON_False) {
				spc_now.string_detection = false;
			} else if (spc->type == cJSON_True) {
				spc_now.string_detection = true;
			} else {
				throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_S_DETECTION);
			}
		} else {
			spc_now.string_detection = (string_detection->type == cJSON_True) ? true : false;
		}
	} else if (spc) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(scheme, RESERVED_S_DETECTION);
			spc_now.string_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(scheme, RESERVED_S_DETECTION);
			spc_now.string_detection = true;
		} else {
			throw MSG_Error("Data inconsistency %s should be boolean", RESERVED_S_DETECTION);
		}
	}
}


std::string
Database::specificationstostr(specifications_t &spc)
{
	std::stringstream str;
	str << "\n{\n";
	str << "\tposition: " << spc.position << "\n";
	str << "\tweight: "   << spc.weight   << "\n";
	str << "\tlanguage: " << spc.language << "\n";

	str << "\taccuracy: [ ";
	std::vector<std::string>::const_iterator it(spc.accuracy.begin());
	for (; it != spc.accuracy.end(); it++) {
		str << *it << " ";
	}
	str << "]\n";

	str << "\ttype: " << spc.type << "\n";
	str << "\tanalyzer: " << spc.analyzer << "\n";

	str << "\tspelling: "          << ((spc.spelling)          ? "true" : "false") << "\n";
	str << "\tpositions: "         << ((spc.positions)         ? "true" : "false") << "\n";
	str << "\tstore: "             << ((spc.store)             ? "true" : "false") << "\n";
	str << "\tdynamic: "           << ((spc.dynamic)           ? "true" : "false") << "\n";
	str << "\tdate_detection: "    << ((spc.date_detection)    ? "true" : "false") << "\n";
	str << "\tnumeric_detection: " << ((spc.numeric_detection) ? "true" : "false") << "\n";
	str << "\tgeo_detection: "     << ((spc.geo_detection)     ? "true" : "false") << "\n";
	str << "\tbool_detection: "    << ((spc.bool_detection)    ? "true" : "false") << "\n";
	str << "\tstring_detection: "  << ((spc.string_detection)  ? "true" : "false") << "\n}\n";

	return str.str();
}


bool
Database::is_language(const std::string &language)
{
	if (language.find(" ") != -1) {
		return false;
	}
	return (std::string(LANGUAGES).find(language) != -1) ? true : false;
}


bool
Database::index(cJSON *document, const std::string &_document_id, bool commit)
{
	if (!(flags & DB_WRITABLE)) {
		LOG_ERR(this, "ERROR: database is read-only\n");
		return false;
	}

	Xapian::Document doc;

	std::string doc_data(cJSON_Print(document));
	LOG_DATABASE_WRAP(this, "Document data: %s\n", doc_data.c_str());
	doc.set_data(doc_data);

	cJSON *document_terms = cJSON_GetObjectItem(document, RESERVED_TERMS);
	cJSON *document_texts = cJSON_GetObjectItem(document, RESERVED_TEXTS);

	std::string s_scheme = db->get_metadata("scheme");
	cJSON *scheme;
	cJSON *properties;
	const char *uuid = db->get_uuid().c_str();
	if (s_scheme.empty()) {
		scheme = cJSON_CreateObject();
		properties = cJSON_CreateObject();
		cJSON_AddItemToObject(scheme, uuid, properties);
	} else {
		scheme = cJSON_Parse(s_scheme.c_str());
		if (!scheme) {
			LOG_ERR(this, "ERROR: Scheme is corrupt, you need provide a new one. JSON Before: [%s]\n", cJSON_GetErrorPtr());
			return false;
		}
		properties = cJSON_GetObjectItem(scheme, uuid);
	}

	std::string document_id;
	if (_document_id.c_str()) {
		//Make sure document_id is also a term (otherwise it doesn't replace an existing document)
		doc.add_value(0, _document_id);
		document_id = prefixed(_document_id, DOCUMENT_ID_TERM_PREFIX);
		LOG_DATABASE_WRAP(this, "Slot: 0 id: %s  term: %s\n", _document_id.c_str(), document_id.c_str());
		doc.add_boolean_term(document_id);
	} else {
		LOG_ERR(this, "ERROR: Document must have an 'id'\n");
		return false;
	}

	try {
		//Default specifications
		int position = -1;
		int weight = 1;
		std::string language = "en";
		bool spelling = false;
		bool positions = false;
		std::vector<std::string> accuracy;
		bool store = true;
		std::string type = "";
		std::string analyzer = "STEM_SOME";
		bool dynamic = true;
		bool date_detection = true;
		bool numeric_detection = true;
		bool geo_detection = true;
		bool bool_detection = true;
		bool string_detection = true;
		specifications_t spc_now = {position, weight, language, spelling, positions, accuracy, store, type, {NO_TYPE, NO_TYPE, NO_TYPE}, analyzer, dynamic,
									date_detection, numeric_detection, geo_detection, bool_detection, string_detection};

		update_specifications(document, spc_now, properties);
		specifications_t spc_bef = spc_now;

		cJSON *subproperties = NULL;
		if (document_texts) {
			for (int i = 0; i < cJSON_GetArraySize(document_texts); i++) {
				cJSON *texts = cJSON_GetArrayItem(document_texts, i);
				cJSON *name = cJSON_GetObjectItem(texts, RESERVED_NAME);
				cJSON *text = cJSON_GetObjectItem(texts, RESERVED_VALUE);
				if (text) {
					bool find = true;
					std::string name_s = (name && name->type == cJSON_String) ? name->valuestring : std::string();
					if (!name_s.empty()) {
						subproperties = cJSON_GetObjectItem(properties, name_s.c_str());
						if (!subproperties) {
							find = false;
							subproperties = cJSON_CreateObject();
							cJSON_AddItemToObject(properties, name_s.c_str(), subproperties);
						}
						update_specifications(texts, spc_now, subproperties);
						if (name_s.at(name_s.size() - 3) == OFFSPRING_UNION[0]) {
							std::string language(name_s, name_s.size() - 2, name_s.size());
							spc_now.language = is_language(language) ? language : spc_now.language;
							if (cJSON* lan = cJSON_GetObjectItem(subproperties, RESERVED_LANGUAGE)) {
								lan = cJSON_CreateString(language.c_str());
							}
						}
					} else {
						cJSON *t = cJSON_CreateObject();
						update_specifications(texts, spc_now, t);
						cJSON_Delete(t);
					}
					index_texts(doc, text, spc_now, name_s, subproperties, find);
					spc_now = spc_bef;
				} else {
					LOG_DATABASE_WRAP(this, "ERROR: Text's value must be defined\n");
					return false;
				}
			}
		}

		if (document_terms) {
			for (int i = 0; i < cJSON_GetArraySize(document_terms); i++) {
				cJSON *data_terms = cJSON_GetArrayItem(document_terms, i);
				cJSON *name = cJSON_GetObjectItem(data_terms, RESERVED_NAME);
				cJSON *terms = cJSON_GetObjectItem(data_terms, RESERVED_VALUE);
				if (terms) {
					bool find = true;
					std::string name_s = (name && name->type == cJSON_String) ? name->valuestring : "";
					if (!name_s.empty()) {
						subproperties = cJSON_GetObjectItem(properties, name_s.c_str());
						if (!subproperties) {
							find = false;
							subproperties = cJSON_CreateObject();
							cJSON_AddItemToObject(properties, name_s.c_str(), subproperties);
						}
						update_specifications(data_terms, spc_now, subproperties);
					} else {
						cJSON *t = cJSON_CreateObject();
						update_specifications(data_terms, spc_now, t);
						cJSON_Delete(t);
					}
					index_terms(doc, terms, spc_now, name_s, subproperties, find);
					spc_now = spc_bef;
				} else {
					LOG_DATABASE_WRAP(this, "ERROR: Term must be defined\n");
					return false;
				}
			}
		}

		int elements = cJSON_GetArraySize(document);
		for (int i = 0; i < elements; i++) {
			cJSON *item = cJSON_GetArrayItem(document, i);
			bool find = true;
			if (!is_reserved(item->string)) {
				subproperties = cJSON_GetObjectItem(properties, item->string);
				if (!subproperties) {
					find = false;
					subproperties = cJSON_CreateObject();
					cJSON_AddItemToObject(properties, item->string, subproperties);
				}
				index_fields(item, item->string, spc_now, doc, subproperties, false, find);
			} else if (strcmp(item->string, RESERVED_VALUES) == 0) {
				index_fields(item, "", spc_now, doc, properties, true, find);
			}
		}

	} catch (const std::exception &err) {
		LOG_DATABASE_WRAP(this, "ERROR: %s\n", err.what());
		return false;
	}

	Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	LOG_DATABASE_WRAP(this, "Scheme: %s\n", cJSON_Print(scheme));
	wdb->set_metadata("scheme", cJSON_Print(scheme));
	return replace(document_id, doc, commit);
}


bool
Database::replace(const std::string &document_id, const Xapian::Document &doc, bool commit)
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Inserting: -%s- t:%d\n", document_id.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			LOG_DATABASE_WRAP(this, "Doing replace_document.\n");
			wdb->replace_document(document_id, doc);
			LOG_DATABASE_WRAP(this, "Replace_document was done.\n");
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		LOG_DATABASE_WRAP(this, "Document inserted\n");
		return (commit) ? _commit() : true;
	}

	return false;
}


std::vector<std::string>
Database::split_fields(const std::string &field_name)
{
	std::vector<std::string> fields;
	std::string aux(field_name.c_str());
	std::string::size_type pos = 0;
	while (aux.at(pos) == OFFSPRING_UNION[0]) {
		pos++;
	}
	std::string::size_type start = pos;
	while ((pos = aux.substr(start, aux.size()).find(OFFSPRING_UNION)) != -1) {
		std::string token = aux.substr(0, start + pos);
		fields.push_back(token);
		aux.assign(aux, start + pos + strlen(OFFSPRING_UNION), aux.size());
		pos = 0;
		while (aux.at(pos) == OFFSPRING_UNION[0]) {
			pos++;
		}
		start = pos;
	}
	fields.push_back(aux);
	return fields;
}


data_field_t
Database::get_data_field(const std::string &field_name)
{
	data_field_t res = {0xffffffff, "", NO_TYPE, std::vector<std::string>(), std::vector<std::string>()};

	if (field_name.empty()) {
		return res;
	}

	std::string json = db->get_metadata("scheme");
	if (json.empty()) return res;

	const char *uuid = db->get_uuid().c_str();
	cJSON *scheme = cJSON_Parse(json.c_str());
	cJSON *properties = cJSON_GetObjectItem(scheme, uuid);

	std::vector<std::string> fields = split_fields(field_name);
	std::vector<std::string>::const_iterator it= fields.begin();
	for ( ; it != fields.end(); it++) {
		properties = cJSON_GetObjectItem(properties, (*it).c_str());
		if (!properties) break;
	}

	if (properties) {
		cJSON *_aux = cJSON_GetObjectItem(properties, RESERVED_SLOT);
		res.slot = (_aux) ? _aux->valueint : get_slot(field_name);
		_aux = cJSON_GetObjectItem(properties, RESERVED_TYPE);
		if (_aux) {
			char sep_types[3];
			set_types(_aux->valuestring, sep_types);
			res.type = sep_types[2];
		} else {
			res.type = NO_TYPE;
		}
		_aux = cJSON_GetObjectItem(properties, RESERVED_PREFIX);
		res.prefix = (_aux) ? _aux->valuestring : get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, res.type);
		_aux = cJSON_GetObjectItem(properties, RESERVED_ACCURACY);
		if (_aux) {
			int elements = cJSON_GetArraySize(_aux);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(_aux, i);
				if (acc->type == cJSON_String) {
					res.accuracy.push_back(acc->valuestring);
				} else if (acc->type == cJSON_Number) {
					res.accuracy.push_back(std::to_string(acc->valuedouble));
				}
			}
		}
		_aux = cJSON_GetObjectItem(properties, RESERVED_ACC_PREFIX);
		if (_aux) {
			int elements = cJSON_GetArraySize(_aux);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(_aux, i);
				if (acc->type == cJSON_String) {
					res.acc_prefix.push_back(acc->valuestring);
				} else if (acc->type == cJSON_Number) {
					res.acc_prefix.push_back(std::to_string(acc->valuedouble));
				}
			}
		}
	}

	cJSON_Delete(scheme);
	return res;
}


char
Database::get_type(cJSON *field, specifications_t &spc)
{
	int type = field->type;
	cJSON *aux = field;
	if (type == cJSON_Array) {
		int num_ele = cJSON_GetArraySize(field);
		aux = cJSON_GetArrayItem(field, 0);
		type = aux->type;
		if (type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");
		for (int i = 1; i < num_ele; i++) {
			aux = cJSON_GetArrayItem(field, i);
			if (aux->type != type && (aux->type > 1 || type > 1)) {
				throw MSG_Error("Different types of data");
			}
		}
	}

	switch (type) {
		case cJSON_Number: if (spc.numeric_detection) return NUMERIC_TYPE; break;
		case cJSON_False: if (spc.bool_detection) return BOOLEAN_TYPE; break;
		case cJSON_True: if (spc.bool_detection) return BOOLEAN_TYPE; break;
		case cJSON_String:
			if (spc.bool_detection && serialise_bool(aux->valuestring).size() != 0) {
				return BOOLEAN_TYPE;
			} else if (spc.date_detection && timestamp_date(aux->valuestring).size() != 0) {
				return DATE_TYPE;
			} else if(spc.geo_detection && field->type != cJSON_Array && is_like_EWKT(aux->valuestring)) {
				// For WKT format, it is not necessary to use arrays.
				return GEO_TYPE;
			} else if (spc.string_detection) {
				return STRING_TYPE;
			}
			break;
	}

	return NO_TYPE;
}


std::string
Database::str_type(char type)
{
	switch (type) {
		case STRING_TYPE: return "string";
		case NUMERIC_TYPE: return "numeric";
		case BOOLEAN_TYPE: return "boolean";
		case GEO_TYPE: return "geospatial";
		case DATE_TYPE: return "date";
		case OBJECT_TYPE: return "object";
		case ARRAY_TYPE: return "array";
	}
	return "";
}


bool
Database::set_types(const std::string &type, char sep_types[])
{
	group_t *gr = NULL;
	int len = (int)type.size();
	int ret = pcre_search(type.c_str(), len, 0, 0, FIND_TYPES_RE, &compiled_find_types_re , &gr);
	if (ret != -1 && len == gr[0].end - gr[0].start) {
		if (gr[4].end - gr[4].start != 0) {
			sep_types[0] = OBJECT_TYPE;
			sep_types[1] = NO_TYPE;
			sep_types[2] = NO_TYPE;
		} else {
			if (gr[1].end - gr[1].start != 0) {
				sep_types[0] = OBJECT_TYPE;
			}
			if (gr[2].end - gr[2].start != 0) {
				sep_types[1] = ARRAY_TYPE;
			}
			sep_types[2] = std::string(type.c_str(), gr[3].start, gr[3].end - gr[3].start).at(0);
		}

		if (gr) {
			free(gr);
			gr = NULL;
		}

		return true;
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	return false;
}


void
Database::clean_reserved(cJSON *root)
{
	int elements = cJSON_GetArraySize(root);
	for (int i = 0; i < elements; ) {
		cJSON *item = cJSON_GetArrayItem(root, i);
		if (is_reserved(item->string)) {
			cJSON_DeleteItemFromObject(root, item->string);
		} else {
			clean_reserved(root, item);
		}
		if (elements > cJSON_GetArraySize(root)) {
			elements = cJSON_GetArraySize(root);
		} else {
			i++;
		}
	}
}


void
Database::clean_reserved(cJSON *root, cJSON *item)
{
	if (is_reserved(item->string) && strcmp(item->string, RESERVED_VALUE) != 0) {
		cJSON_DeleteItemFromObject(root, item->string);
		return;
	}

	if (item->type == cJSON_Object) {
		int elements = cJSON_GetArraySize(item);
		for (int i = 0; i < elements; ) {
			cJSON *subitem = cJSON_GetArrayItem(item, i);
			clean_reserved(item, subitem);
			if (elements > cJSON_GetArraySize(item)) {
				elements = cJSON_GetArraySize(item);
			} else {
				i++;
			}
		}
	}
}


void
Database::insert_terms_geo(const std::string &g_serialise, Xapian::Document *doc, const std::string &prefix,
	int w, int position)
{
	bool found;
	int size = (int)g_serialise.size();
	std::vector<std::string> terms;
	for (int i = 6; i > 1; i--) {
		for (int j = 0; j < size; j += 6) {
			found = false;
			std::string s_coord(g_serialise, j, i);

			std::vector<std::string>::const_iterator it(terms.begin());
			for (; it != terms.end(); it++) {
				if (s_coord.compare(*it) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				std::string nameterm(prefixed(s_coord, prefix));
				LOG(this, "Nameterm: %s   Prefix: %s   Term: %s\n",  repr(nameterm).c_str(), prefix.c_str(), repr(s_coord).c_str());

				if (position > 0) {
					doc->add_posting(nameterm, position, w);
					LOG_DATABASE_WRAP(this, "Posting: %s %d %d\n", repr(nameterm).c_str(), position, w);
				} else {
					doc->add_term(nameterm, w);
					LOG_DATABASE_WRAP(this, "Term: %s %d\n", repr(nameterm).c_str(), w);
				}
				terms.push_back(s_coord);
			}
		}
	}
}


search_t
Database::search(query_t e)
{
	search_t srch_resul;

	Xapian::Query queryQ;
	Xapian::Query queryP;
	Xapian::Query queryT;
	Xapian::Query queryF;
	std::vector<std::string> sug_query;
	search_t srch;
	bool first = true;

	try {
		LOG(this, "e.query size: %d  Spelling: %d Synonyms: %d\n", e.query.size(), e.spelling, e.synonyms);
		std::vector<std::string>::const_iterator qit(e.query.begin());
		std::vector<std::string>::const_iterator lit(e.language.begin());
		std::string lan;
		unsigned int flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_PURE_NOT;
		if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
		if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
		for (; qit != e.query.end(); qit++) {
			if (lit != e.language.end()) {
				lan = *lit;
				lit++;
			}
			srch = _search(*qit, flags, true, lan, e.unique_doc);
			if (first) {
				queryQ = srch.query;
				first = false;
			} else {
				queryQ =  Xapian::Query(Xapian::Query::OP_AND, queryQ, srch.query);
			}
			sug_query.push_back(srch.suggested_query.back());
		}
		LOG(this, "e.query: %s\n", queryQ.get_description().c_str());


		LOG(this, "e.partial size: %d\n", e.partial.size());
		std::vector<std::string>::const_iterator pit(e.partial.begin());
		flags = Xapian::QueryParser::FLAG_PARTIAL;
		if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
		if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
		first = true;
		for (; pit != e.partial.end(); pit++) {
			srch = _search(*pit, flags, false, "", e.unique_doc);
			if (first) {
				queryP = srch.query;
				first = false;
			} else {
				queryP = Xapian::Query(Xapian::Query::OP_AND_MAYBE , queryP, srch.query);
			}
			sug_query.push_back(srch.suggested_query.back());
		}
		LOG(this, "e.partial: %s\n", queryP.get_description().c_str());


		LOG(this, "e.terms size: %d\n", e.terms.size());
		std::vector<std::string>::const_iterator tit(e.terms.begin());
		flags = Xapian::QueryParser::FLAG_BOOLEAN | Xapian::QueryParser::FLAG_PURE_NOT;
		if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
		if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
		first = true;
		for (; tit != e.terms.end(); tit++) {
			srch =  _search(*tit, flags, false, "", e.unique_doc);
			if (first) {
				queryT = srch.query;
				first = false;
			} else {
				queryT = Xapian::Query(Xapian::Query::OP_AND, queryT, srch.query);
			}
			sug_query.push_back(srch.suggested_query.back());
		}
		LOG(this, "e.terms: %s\n", queryT.get_description().c_str());

		first = true;
		if (e.query.size() != 0) {
			queryF = queryQ;
			first = false;
		}
		if (e.partial.size() != 0) {
			if (first) {
				queryF = queryP;
				first = false;
			} else {
				queryF = Xapian::Query(Xapian::Query::OP_AND, queryF, queryP);
			}
		}
		if (e.terms.size() != 0) {
			if (first) {
				queryF = queryT;
				first = false;
			} else {
				queryF = Xapian::Query(Xapian::Query::OP_AND, queryF, queryT);
			}
		}
		srch_resul.query = queryF;
		srch_resul.suggested_query = sug_query;
		return srch_resul;
	} catch (const Xapian::Error &error) {
		LOG_ERR(this, "ERROR: In search: %s\n", error.get_msg().c_str());
	}

	return srch_resul;
}


search_t
Database::_search(const std::string &query, unsigned int flags, bool text, const std::string &lan, bool unique_doc)
{
	search_t srch;

	if (query == "*") {
		srch.query = Xapian::Query("");
		srch.suggested_query.push_back("");
		return srch;
	}

	int len = (int)query.size(), offset = 0;
	group_t *g = NULL;
	bool first_time = true;
	std::string querystring;
	Xapian::QueryParser queryparser;
	queryparser.set_database(*db);

	if (text) {
		queryparser.set_stemming_strategy(queryparser.STEM_SOME);
		if (lan.size() != 0) {
			LOG(this, "User-defined language: %s\n", lan.c_str());
			queryparser.set_stemmer(Xapian::Stem(lan));
		} else {
			LOG(this, "Default language: en\n");
			queryparser.set_stemmer(Xapian::Stem("en"));
		}
	}

	NumericFieldProcessor *nfp;
	DateFieldProcessor *dfp;
	BooleanFieldProcessor *bfp;
	Xapian::NumberValueRangeProcessor *nvrp;
	Xapian::StringValueRangeProcessor *svrp;
	DateTimeValueRangeProcessor *dvrp;
	unsigned int slot;
	std::string prefix;

	while ((pcre_search(query.c_str(), len, offset, 0, FIND_FIELD_RE, &compiled_find_field_re, &g)) != -1) {
		offset = g[0].end;
		std::string field(query.c_str() + g[0].start, g[0].end - g[0].start);
		std::string field_name_dot(query.c_str() + g[1].start, g[1].end - g[1].start);
		std::string field_name(query.c_str() + g[2].start, g[2].end - g[2].start);
		std::string field_value(query.c_str() + g[3].start, g[3].end - g[3].start);
		data_field_t field_t = get_data_field(field_name);

		// Geo type variables
		std::vector<std::string> trixels, prefixes;
		std::vector<std::string>::const_iterator it;
		bool partials = DE_PARTIALS;
		double error = DE_ERROR;
		std::string filter_term;

		if (isRange(field_value)) {
			switch (field_t.type) {
				case NUMERIC_TYPE:
					slot = field_t.slot;
					nvrp = new Xapian::NumberValueRangeProcessor(slot, field_name_dot, true);
					LOG(this, "Numeric Slot: %u Field_name_dot: %s\n", slot, field_name_dot.c_str());
					nvrps.push_back(std::unique_ptr<Xapian::NumberValueRangeProcessor>(nvrp));
					queryparser.add_valuerangeprocessor(nvrp);
					break;
				case STRING_TYPE:
					if(!unique_doc) {
						slot = field_t.slot;
					} else {
						slot = 0;
					}
					svrp = new Xapian::StringValueRangeProcessor(slot, field_name_dot, true);
					svrps.push_back(std::unique_ptr<Xapian::StringValueRangeProcessor>(svrp));
					LOG(this, "String Slot: %u Field_name_dot: %s\n", slot, field_name_dot.c_str());
					queryparser.add_valuerangeprocessor(svrp);
					field_value = field_name_dot + field_value;
					break;
				case DATE_TYPE:
					slot = field_t.slot;
					field_name_dot = std::string("");
					dvrp = new DateTimeValueRangeProcessor(slot, field_name_dot);
					dvrps.push_back(std::unique_ptr<DateTimeValueRangeProcessor>(dvrp));
					LOG(this, "Date Slot: %u Field_name: %s\n", slot, field_name.c_str());
					queryparser.add_valuerangeprocessor(dvrp);
					field_value = field_name_dot + field_value;
					break;
				default:
					if (g) {
						free(g);
						g = NULL;
					}
					throw Xapian::QueryParserError("This type of Data has no support for range search.\n");
			}
		} else {
			switch (field_t.type) {
				case NUMERIC_TYPE:
					prefix = field_t.prefix;
					if (isupper(field_value.at(0))) {
						prefix = prefix + ":";
					}
					nfp = new NumericFieldProcessor(prefix);
					nfps.push_back(std::unique_ptr<NumericFieldProcessor>(nfp));
					if (strhasupper(field_name)) {
						LOG(this, "Boolean Prefix\n");
						queryparser.add_boolean_prefix(field_name, nfp);
					} else {
						LOG(this, "Prefix\n");
						queryparser.add_prefix(field_name, nfp);
					}
					break;
				case STRING_TYPE:
					if (field_name.size() != 0) {
						if(!unique_doc) {
							prefix = field_t.prefix;
						} else {
							prefix = "Q";
						}

						LOG(this, "prefix calculated: %s\n", prefix.c_str());
						if (strhasupper(field_name)) {
							LOG(this, "Boolean Prefix\n");
							if (isupper(field_value.at(0))) {
								prefix = prefix + ":";
							}
							queryparser.add_boolean_prefix(field_name, prefix);
						} else {
							LOG(this, "Prefix\n");
							queryparser.add_prefix(field_name, prefix);
						}
					}
					field_value = field_name_dot + field_value;
					break;
				case DATE_TYPE:
					prefix = field_t.prefix;
					field_value = timestamp_date(field_value);
					if (field_value.size() == 0) {
						if (g) {
							free(g);
							g = NULL;
						}
						throw Xapian::QueryParserError("Didn't understand date field name's specification: '" + field_name + "'");
					}
					dfp = new DateFieldProcessor(prefix);
					dfps.push_back(std::unique_ptr<DateFieldProcessor>(dfp));
					if (strhasupper(field_name)) {
						LOG(this, "Boolean Prefix\n");
						queryparser.add_boolean_prefix(field_name, dfp);
					} else {
						LOG(this, "Prefix\n");
						queryparser.add_prefix(field_name, dfp);
					}
					field_value = field_name_dot + field_value;
					break;
				case GEO_TYPE:
					flags |= Xapian::QueryParser::FLAG_WILDCARD;
					prefix = field_t.prefix;
					field_value.assign(field_value, 1, field_value.size() - 2);
					if (field_t.accuracy.size() > 0) {
						partials = (serialise_bool(field_t.accuracy.at(0)).compare("f") == 0) ? false : true;
						error = (field_t.accuracy.size() >= 2) ? strtodouble(field_t.accuracy.at(1)) : DE_ERROR;
					}
					LOG(this, "Partials: %d  Error: %f\n", partials, error);
					trixels = serialise_geo(field_value, true, 0.5);
					it = trixels.begin();
					field_value = "";
					for ( ; it != trixels.end(); it++) {
						field_value += field_name_dot + (*it) + (((*it).size() == (HTM_MAX_LEVEL + 2)) ? " " : "* ");
						for (int i = (int)(*it).size() - 1; i > 1; i--) {
							field_value += field_name_dot + std::string(*it, 0, i) + " ";
						}
					}
					if (strhasupper(field_name)) {
						LOG(this, "Boolean Prefix\n");
						queryparser.add_boolean_prefix(field_name, prefix);
					} else {
						LOG(this, "Prefix\n");
						queryparser.add_prefix(field_name, prefix);
					}
					break;
				case BOOLEAN_TYPE:
					prefix = field_t.prefix;
					bfp = new BooleanFieldProcessor(prefix);
					bfps.push_back(std::unique_ptr<BooleanFieldProcessor>(bfp));
					if (strhasupper(field_name)) {
						LOG(this, "Boolean Prefix\n");
						queryparser.add_boolean_prefix(field_name, bfp);
					} else {
						LOG(this, "Prefix\n");
						queryparser.add_prefix(field_name, bfp);
					}
					break;
			}
		}

		if (first_time) {
			querystring = field_value;
			first_time = false;
		} else {
			querystring += " " + field_value;
		}
	}

	if (g) {
		free(g);
		g = NULL;
	}

	if (offset != len) {
		throw Xapian::QueryParserError("Query '" + query + "' contains errors.\n" );
	}

	LOG_DATABASE_WRAP(this, "Query processed: (%s)\n", querystring.c_str());

	try {
		srch.query = queryparser.parse_query(querystring, flags);
		srch.suggested_query.push_back(queryparser.get_corrected_query_string());
	} catch (const Xapian::Error &er) {
		LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
		reopen();
		queryparser.set_database(*db);
		srch.query = queryparser.parse_query(querystring, flags);
		srch.suggested_query.push_back(queryparser.get_corrected_query_string());
	}

	return srch;
}


void
Database::get_similar(bool is_fuzzy, Xapian::Enquire &enquire, Xapian::Query &query, similar_t *similar)
{
	Xapian::RSet rset;
	std::vector<std::string>::const_iterator it;

	for (int t = 3; t >= 0; --t) {
		try{
			Xapian::Enquire renquire = get_enquire(query, NULL, NULL, NULL, NULL, NULL);
			Xapian::MSet mset = renquire.get_mset(0, similar->n_rset);
			for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); m++) {
				rset.add_document(*m);
			}
		}catch (const Xapian::Error &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		std::vector<std::string>prefixes;
		for(it = similar->type.begin(); it != similar->type.end(); it++) {
			prefixes.push_back(DOCUMENT_CUSTOM_TERM_PREFIX + to_type(*it));
		}
		for(it = similar->field.begin(); it != similar->field.end(); it++) {
			data_field_t field_t = get_data_field(*it);
			prefixes.push_back(field_t.prefix);
		}
		ExpandDeciderFilterPrefixes efp(prefixes);
		Xapian::ESet eset = enquire.get_eset(similar->n_eset, rset, &efp);

		if (is_fuzzy) {
			query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end()));
		} else {
			query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end());
		}
		return;
	}
}


Xapian::Enquire
Database::get_enquire(Xapian::Query &query, Xapian::MultiValueKeyMaker *sorter, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> * spies, similar_t *nearest, similar_t *fuzzy, std::vector<std::string> * facets)
{
	std::string field;
	MultiValueCountMatchSpy *spy;
	Xapian::Enquire enquire(*db);

	if(nearest) {
		get_similar(false, enquire, query, nearest);
	}

	if(fuzzy) {
		get_similar(true, enquire, query, fuzzy);
	}

	enquire.set_query(query);

	if (sorter) {
		enquire.set_sort_by_key(sorter, false);
	}

	if (spies) {
		if (!facets->empty()) {
			std::vector<std::string>::const_iterator fit(facets->begin());
			for (; fit != facets->end(); fit++) {
				spy = new MultiValueCountMatchSpy(get_slot(*fit));
				spies->push_back(std::make_pair (*fit, std::unique_ptr<MultiValueCountMatchSpy>(spy)));
				enquire.add_matchspy(spy);
				LOG_ERR(this, "added spy de -%s-\n", (*fit).c_str());
			}
		}
	}

	enquire.set_collapse_key(0);
	return enquire;
}


int
Database::get_mset(query_t &e, Xapian::MSet &mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, std::vector<std::string> &suggestions, int offset)
{
	Xapian::MultiValueKeyMaker *sorter = NULL;
	bool decreasing;
	std::string field;
	int doccount = db->get_doccount();
	int check_at_least = std::max(std::min(doccount, e.check_at_least), 0);

	if (!e.order.empty()) {
		sorter = new Xapian::MultiValueKeyMaker();
		std::vector<std::string>::const_iterator oit(e.order.begin());
		for ( ; oit != e.order.end(); oit++) {
			if (StartsWith(*oit, "-")) {
				decreasing = true;
				field.assign(*oit, 1, (*oit).size() - 1);
				sorter->add_value(get_slot(field), decreasing);
			} else if (StartsWith(*oit, "+")) {
				decreasing = false;
				field.assign(*oit, 1, (*oit).size() - 1);
				sorter->add_value(get_slot(field), decreasing);
			} else {
				decreasing = false;
				sorter->add_value(get_slot(*oit), decreasing);
			}
		}
	}

	for (int t = 3; t >= 0; --t) {
		try {
			search_t srch = search(e);
			if (srch.query.serialise().size() == 0) {
				delete sorter;
				return 1;
			}
			Xapian::Enquire enquire = get_enquire(srch.query, sorter, &spies, e.is_nearest ? &e.nearest : NULL, e.is_fuzzy ? &e.fuzzy : NULL, &e.facets);
			suggestions = srch.suggested_query;
			mset = enquire.get_mset(e.offset + offset, e.limit - offset, check_at_least);

			// The range field processors are released.
			nvrps.clear();
			svrps.clear();
			dvrps.clear();
			nfps.clear();
			dfps.clear();
			bfps.clear();
		} catch (const Xapian::DatabaseModifiedError &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			if (t) reopen();
			continue;
		} catch (const Xapian::NetworkError &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			if (t) reopen();
			continue;
		} catch (const Xapian::Error &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			return 2;
		}
		delete sorter;
		return 0;
	}
	LOG_ERR(this, "ERROR: Cannot search!\n");
	delete sorter;
	return 2;
}


bool
Database::get_metadata(const std::string &key, std::string &value)
{
	for (int t = 3; t >= 0; --t) {
		try {
			value = db->get_metadata(key);
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		return true;
	}
	return false;
}


bool
Database::set_metadata(const std::string &key, const std::string &value, bool commit)
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Set metadata: t%d\n", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->set_metadata(key, value);
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		LOG_DATABASE_WRAP(this, "Metadata set\n");
		return (commit) ? _commit() : true;
	}

	LOG_ERR(this, "ERROR: Cannot do set_metadata!\n");
	return false;
}


bool
Database::get_document(Xapian::docid did, Xapian::Document &doc)
{
	for (int t = 3; t >= 0; --t) {
		try {
			doc = db->get_document(did);
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		return true;
	}

	return false;
}


cJSON*
Database::get_stats_database()
{
	cJSON *database = cJSON_CreateObject();
	unsigned int doccount = db->get_doccount();
	unsigned int lastdocid = db->get_lastdocid();
	cJSON_AddStringToObject(database, "uuid", db->get_uuid().c_str());
	cJSON_AddNumberToObject(database, "doc_count", doccount);
	cJSON_AddNumberToObject(database, "last_id", lastdocid);
	cJSON_AddNumberToObject(database, "doc_del", lastdocid - doccount);
	cJSON_AddNumberToObject(database, "av_length", db->get_avlength());
	cJSON_AddNumberToObject(database, "doc_len_lower", db->get_doclength_lower_bound());
	cJSON_AddNumberToObject(database, "doc_len_upper", db->get_doclength_upper_bound());
	(db->has_positions()) ? cJSON_AddTrueToObject(database, "has_positions") : cJSON_AddFalseToObject(database, "has_positions");
	return database;
}


cJSON*
Database::get_stats_docs(int id_doc)
{
	cJSON *document = cJSON_CreateObject();

	try {
		if (id_doc == 0) {
			cJSON_AddStringToObject(document, "id", "all");
			cJSON_AddNumberToObject(document, "allterms", std::distance(db->allterms_begin(), db->allterms_end()));
			cJSON_AddNumberToObject(document, "allspellings", std::distance(db->spellings_begin(), db->spellings_end()));
		} else {
			Xapian::Document doc;
			Xapian::QueryParser queryparser;
			queryparser.add_prefix("id", "Q");
			Xapian::Query query = queryparser.parse_query(std::string("id:" + std::to_string(id_doc)));
			Xapian::Enquire enquire(*db);
			enquire.set_query(query);
			Xapian::MSet mset = enquire.get_mset(0, 1);
			Xapian::MSetIterator m = mset.begin();
			int t = 3;
			for (; t >= 0; --t) {
				try {
					doc = db->get_document(*m);
					break;
				} catch (Xapian::InvalidArgumentError &err) {
					cJSON_AddNumberToObject(document, "id", id_doc);
					cJSON_AddStringToObject(document, "error",  "Document not found");
					return document;
				} catch (Xapian::DocNotFoundError &err) {
					cJSON_AddNumberToObject(document, "id", id_doc);
					cJSON_AddStringToObject(document, "error",  "Document not found");
					return document;
				} catch (const Xapian::Error &err) {
					reopen();
					m = mset.begin();
				}
			}

			cJSON_AddStringToObject(document, "id", ("Q" + doc.get_value(0)).c_str());
			cJSON_AddStringToObject(document, "data", doc.get_data().c_str());
			cJSON_AddNumberToObject(document, "count_terms", doc.termlist_count());
			Xapian::TermIterator it(doc.termlist_begin());
			std::string terms;
			for ( ; it != doc.termlist_end(); it++) {
				terms = terms + repr(*it) + " ";
			}
			cJSON_AddStringToObject(document, "terms", terms.c_str());
			cJSON_AddNumberToObject(document, "count_values", doc.values_count());
			Xapian::ValueIterator iv(doc.values_begin());
			std::string values;
			for ( ; iv != doc.values_end(); iv++) {
				values = values + std::to_string(iv.get_valueno()) + ":" + repr(*iv) + " ";
			}
			cJSON_AddStringToObject(document, "values", values.c_str());
		}
	} catch (const Xapian::Error &err) {
		cJSON_AddNumberToObject(document, "id", id_doc);
		cJSON_AddStringToObject(document, "error",  "Document not found");
	}
	return document;
}


bool
ExpandDeciderFilterPrefixes::operator()(const std::string &term) const
{
	std::vector<std::string>::const_iterator i(prefixes.cbegin());
	for (; i != prefixes.cend(); i++) {
		if (StartsWith(term, *i)) {
			return true;
		}
	}
	return prefixes.empty();
}