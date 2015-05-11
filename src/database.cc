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

//change prefix to Q only
#define DOCUMENT_ID_TERM_PREFIX "Q"
#define DOCUMENT_OBJECT_TYPE_PREFIX "T"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define FIND_FIELD_RE "(([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[^\"]+\"|[^\" ]+)"
#define FIND_TERMS_RE "(?:([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[-\\w. ]+\"|[-\\w.]+)"
#define MAX_DOCS 100
#define DATABASE_UPDATE_TIME 10


class ExpandDeciderFilterPrefixes : public Xapian::ExpandDecider {
	std::vector<std::string> prefixes;

	public:
		ExpandDeciderFilterPrefixes(const std::vector<std::string> &prefixes_)
			: prefixes(prefixes_) { }

		virtual bool operator() (const std::string &term) const;
};


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

	const Endpoint *e;
	endpoints_set_t::const_iterator i(endpoints.begin());
	if (writable) {
		db = new Xapian::WritableDatabase();
		if (endpoints.size() != 1) {
			LOG_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), endpoints.as_string().c_str());
		} else {
			e = &*i;
			if (e->protocol == "file" || e->host == "localhost" || e->host == "127.0.0.1") {
				wdb = Xapian::WritableDatabase(e->path, Xapian::DB_CREATE_OR_OPEN);
			} else {
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
				try {
					ldb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (ldb.get_uuid() == rdb.get_uuid()) {
						// Handle remote endpoints and figure out if the endpoint is a local database
						LOG(this, "Endpoint %s fallback to local database!\n", e->as_string().c_str());
						wdb = Xapian::WritableDatabase(e->path, Xapian::DB_OPEN);
					} else {
						wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
					}
				} catch (const Xapian::DatabaseOpeningError &err) {
					wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
				}
			}
			db->add_database(wdb);
		}
	} else {
		db = new Xapian::Database();
		for (; i != endpoints.end(); ++i) {
			e = &*i;
			if (e->protocol == "file" || e->host == "localhost" || e->host == "127.0.0.1") {
				try {
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
				} catch (const Xapian::DatabaseOpeningError &err) {
					Xapian::WritableDatabase wdb = Xapian::WritableDatabase(e->path, Xapian::DB_CREATE_OR_OPEN);
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
				}
			} else {
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
				try {
					ldb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (ldb.get_uuid() == rdb.get_uuid()) {
						LOG(this, "Endpoint %s fallback to local database!\n", e->as_string().c_str());
						// Handle remote endpoints and figure out if the endpoint is a local database
						rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
					}
				} catch (const Xapian::DatabaseOpeningError &err) {}
			}
			db->add_database(rdb);
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
				try {
					database_ = new Database(endpoints, writable);
					database_->access_time = time(0);
				} catch (const Xapian::Error &err) {
					LOG_ERR(this, "ERROR: %s\n", err.get_msg().c_str());
					return false;
				}
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

	if ((time(0) - database_->access_time) >= DATABASE_UPDATE_TIME && !writable) {
			database_->reopen();
			LOG_DATABASE(this, "+ DB REOPEN %lx\n", (unsigned long)database_);
	}

	LOG_DATABASE(this, "+ CHECKOUT DB %lx\n", (unsigned long)database_);

	return true;
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


pcre *Database::compiled_find_field_re = NULL;


bool
Database::drop(const std::string &doc_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

	std::string document_id  = prefixed(doc_id, DOCUMENT_ID_TERM_PREFIX);

	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Deleting: -%s- t:%d\n", document_id.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->delete_document(document_id);
		} catch (const Xapian::Error &e) {
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
Database::patch(cJSON *patches, const std::string &_document_id, bool commit, const std::string &object_type)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
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
		}

		catch (Xapian::InvalidArgumentError) {
			return false;
		}

		catch (Xapian::DocNotFoundError &err) {
			return false;
		}
		catch (const Xapian::Error &err) {
			reopen();
			m = mset.begin();
		}
	}

	//LOG(this, "data <<%s>>\n",document.get_data().c_str() );

	cJSON *data_json = cJSON_Parse(document.get_data().c_str());
	if (!data_json) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		return false;
	}

	if (cJSONUtils_ApplyPatches(data_json, patches) == 0) {
		//Object patched
		return index(data_json, _document_id, commit, object_type);
	}

	//Object no patched
	return false;
}

void
Database::index_fields(cJSON *father, cJSON *item, const std::string &item_name, specifications_t &spc_now, Xapian::Document &doc, bool empty_data)
{
	std::string subitem_name;
	specifications_t spc_bef = spc_now;
	if (item->string[0] != OFFSPRING_UNION[0] || std::string(item->string).find(RESERVED_VALUES) == 0) {
		if (item->type == cJSON_Object) {
			update_specifications(item, spc_now);
			int elements = cJSON_GetArraySize(item);
			for (int i = 0; i < elements; ) {
				cJSON *subitem = cJSON_GetArrayItem(item, i);
				subitem_name = (item_name.size() != 0) ? item_name + OFFSPRING_UNION + subitem->string : subitem->string;
				if (subitem_name.at(subitem_name.size() - 3) == OFFSPRING_UNION[0]) {
					std::string language(subitem_name, subitem_name.size() - 2, subitem_name.size());
					spc_now.language = is_language(language) ? language : spc_now.language;
				}
				index_fields(item, subitem, subitem_name, spc_now, doc, empty_data);
				if (empty_data && elements > cJSON_GetArraySize(item)) {
					elements = cJSON_GetArraySize(item);
				} else {
					i++;
				}
			}
		} else {
			if (item_name.find(RESERVED_VALUES) == 0) {
				subitem_name.assign(item_name, strlen(RESERVED_VALUES) + strlen(OFFSPRING_UNION), item_name.size());
				printf("%s\n", subitem_name.c_str());
				index_values(doc, item, subitem_name);
			} else {
				index_values(doc, item, item_name);
				char type = get_type(item);
				if (type == TEXT_TYPE) {
					index_texts(doc, item, spc_now, item_name);
				} else {
					index_terms(doc, item, spc_now, item_name);
				}
			}
		}
	} else if(empty_data) {
		cJSON_DeleteItemFromObject(father, item->string);
	}
	spc_now = spc_bef;
}


void
Database::index_texts(Xapian::Document &doc, cJSON *text, specifications_t &spc, const std::string &name)
{
	LOG_DATABASE_WRAP(this, "Specifications = {%d %d %s %d %d}\n", spc.position, spc.weight, spc.language.c_str(), spc.spelling, spc.positions);
	std::string prefix;
	if (!name.empty()) prefix = get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, TEXT_TYPE);
	if (text->type != cJSON_String) throw "Data inconsistency should be string";
	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);

	Xapian::TermGenerator term_generator;
	term_generator.set_document(doc);
	term_generator.set_stemmer(Xapian::Stem(spc.language));
	if (spc.spelling) {
		term_generator.set_database(*wdb);
		term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		term_generator.set_stemming_strategy(term_generator.STEM_SOME);
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
Database::index_terms(Xapian::Document &doc, cJSON *terms, specifications_t &spc, const std::string &name)
{
	LOG_DATABASE_WRAP(this, "Specifications = {%d %d %s %d %d}\n", spc.position, spc.weight, spc.language.c_str(), spc.spelling, spc.positions);
	char type = get_type(terms);
	std::string prefix = (!name.empty()) ? get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, type) : DOCUMENT_CUSTOM_TERM_PREFIX;
	int elements = (terms->type == cJSON_Array) ? cJSON_GetArraySize(terms) : 1;

	for (int j = 0; j < elements; j++) {
		cJSON *term = (terms->type == cJSON_Array) ? cJSON_GetArrayItem(terms, j) : terms;
		if (type == GEO_TYPE && term->type != cJSON_Array) {
			term = terms;
			j += elements;
		}

		std::string term_v(cJSON_Print(term));
		if (term->type == cJSON_String || term->type == cJSON_Array) {
			term_v.assign(term_v, 1, term_v.size() - 2);
		} else if (term->type == cJSON_Number) {
			term_v = std::to_string(term->valuedouble);
		}
		LOG_DATABASE_WRAP(this, "Term -> %s: %s\n", prefix.c_str(), term_v.c_str());

		term_v = serialise(type, term_v);
		if (term_v.size() == 0) {
			throw "ERROR: " + name + ": " + term_v + " can not serialized";
		}

		if (spc.position >= 0) {
			if (!prefix.empty() && type == GEO_TYPE) {
				insert_terms_geo(term_v, &doc, prefix, spc.weight, spc.position);
			} else {
				std::string nameterm(prefixed(term_v, prefix));
				doc.add_posting(nameterm, spc.position, spc.weight);
				LOG_DATABASE_WRAP(this, "Posting: %s\n", repr(nameterm).c_str());
			}
		} else {
			if (!prefix.empty() && type == GEO_TYPE) {
				insert_terms_geo(term_v, &doc, prefix, spc.weight, -1);
			} else {
				std::string nameterm(prefixed(term_v, prefix));
				doc.add_term(nameterm, spc.weight);
				LOG_DATABASE_WRAP(this, "Term: %s\n", repr(nameterm).c_str());
			}
		}
	}
}


void
Database::index_values(Xapian::Document &doc, cJSON *values, const std::string &name)
{
	char type = get_type(values);
	StringList s;
	int elements = (values->type == cJSON_Array) ? cJSON_GetArraySize(values) : 1;
	for (int j = 0; j < elements; j++) {
		cJSON *value = (values->type == cJSON_Array) ? cJSON_GetArrayItem(values, j) : values;
		if (type == GEO_TYPE && value->type != cJSON_Array) {
			value = values;
			j += elements;
		}
		std::string value_v = cJSON_Print(value);
		if (value->type == cJSON_String || value->type == cJSON_Array) {
			value_v.assign(value_v, 1, value_v.size() - 2);
		} else if (value->type == cJSON_Number) {
			value_v = std::to_string(value->valuedouble);
		}
		LOG_DATABASE_WRAP(this, "Name: (%s) Value: (%s)\n", name.c_str(), value_v.c_str());
		value_v = serialise(type, value_v);
		if (value_v.empty()) {
			throw "ERROR: " + name + ": " + value_v + " can not serialized";
		}
		s.push_back(value_v);
	}
	unsigned int slot = get_slot(name);
	doc.add_value(slot, s.serialise());
	LOG_DATABASE_WRAP(this, "Slot: %u serialized: %s\n", slot, repr(s.serialise()).c_str());
}


void
Database::update_specifications(cJSON *item, specifications_t &spc_now)
{
	cJSON *spc = cJSON_GetObjectItem(item, RESERVED_POSITION);
	if (spc) {
		if (spc->type == cJSON_Number) {
			spc_now.position = spc->valueint;
		} else {
			throw "Data inconsistency " + std::string(RESERVED_POSITION) + " should be integer";
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT);
	if (spc) {
		if (spc->type == cJSON_Number) {
			spc_now.weight = spc->valueint;
		} else {
			throw "Data inconsistency " + std::string(RESERVED_WEIGHT) + " should be integer";
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE);
	if (spc) {
		if (spc->type == cJSON_String) {
			spc_now.language = is_language(spc->valuestring) ? spc->valuestring : spc_now.language;
		} else {
			throw "Data inconsistency " + std::string(RESERVED_LANGUAGE) + " should be string";
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_SPELLING);
	if (spc) {
		if (spc->type == cJSON_False) {
			spc_now.spelling = false;
		} else if (spc->type == cJSON_True) {
			spc_now.spelling = true;
		} else {
			throw "Data inconsistency " + std::string(RESERVED_SPELLING) + " should be boolean";
		}
	}

	spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS);
	if (spc) {
		if (spc->type == cJSON_False) {
			spc_now.positions = false;
		} else if (spc->type == cJSON_True) {
			spc_now.positions = true;
		} else {
			throw "Data inconsistency " + std::string(RESERVED_POSITIONS) + " should be boolean";
		}
	}
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
Database::index(cJSON *document, const std::string &_document_id, bool commit, const std::string &object_type)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

	cJSON *document_data = cJSON_GetObjectItem(document, "_data");
	cJSON *document_terms = cJSON_GetObjectItem(document, "_terms");
	cJSON *document_texts = cJSON_GetObjectItem(document, "_texts");

	Xapian::Document doc;

	std::string document_id;
	if (_document_id.c_str()) {
		//Make sure document_id is also a term (otherwise it doesn't replace an existing document)
		doc.add_value(0, _document_id);
		LOG_DATABASE_WRAP(this, "Slot: 0  Document id: %s\n", _document_id.c_str());
		document_id = prefixed(_document_id, DOCUMENT_ID_TERM_PREFIX);
		doc.add_boolean_term(document_id);
	} else {
		LOG_ERR(this, "ERROR: Document must have an 'id'\n");
		return false;
	}

	if (object_type.empty()) {
		LOG_ERR(this, "ERROR: Object type must be defined\n");
		return false;
	} else {
		//Make sure document_id is also a term (otherwise it doesn't replace an existing document)
		doc.add_value(1, object_type);
		LOG_DATABASE_WRAP(this, "Slot: 1  Object type: %s\n", object_type.c_str());
		doc.add_boolean_term(prefixed(object_type, DOCUMENT_OBJECT_TYPE_PREFIX));
	}

	try {
		//Default specifications
		specifications_t spc_now = {-1, 1, "en", false, false};
		update_specifications(document, spc_now);
		specifications_t spc_bef = spc_now;

		if (document_texts) {
			for (int i = 0; i < cJSON_GetArraySize(document_texts); i++) {
				cJSON *texts = cJSON_GetArrayItem(document_texts, i);
				cJSON *name = cJSON_GetObjectItem(texts, "name");
				cJSON *text = cJSON_GetObjectItem(texts, "text");
				if (text) {
					update_specifications(texts, spc_now);
					std::string name_s = (name && name->type == cJSON_String) ? name->valuestring : "";
					if (!name_s.empty() && name_s.at(name_s.size() - 3) == OFFSPRING_UNION[0]) {
						std::string language(name_s, name_s.size() - 2, name_s.size());
						spc_now.language = is_language(language) ? language : spc_now.language;
					}
					index_texts(doc, text, spc_now, name_s);
					spc_now = spc_bef;
				} else {
					LOG_DATABASE_WRAP(this, "ERROR: Text must be defined\n");
					return false;
				}
			}
		}

		if (document_terms) {
			for (int i = 0; i < cJSON_GetArraySize(document_terms); i++) {
				cJSON *data_terms = cJSON_GetArrayItem(document_terms, i);
				cJSON *name = cJSON_GetObjectItem(data_terms, "name");
				cJSON *terms = cJSON_GetObjectItem(data_terms, "term");
				if (terms) {
					update_specifications(data_terms, spc_now);
					std::string name_s = (name && name->type == cJSON_String) ? name->valuestring : "";
					index_terms(doc, terms, spc_now, name_s);
					spc_now = spc_bef;
				} else {
					LOG_DATABASE_WRAP(this, "ERROR: Term must be defined\n");
					return false;
				}
			}
		}

		int elements = cJSON_GetArraySize(document);
		bool empty_data = (!document_data) ? true : false;
		for (int i = 0; i < elements; ) {
			cJSON *item = cJSON_GetArrayItem(document, i);
			std::string name(item->string);
			index_fields(document, item, item->string, spc_now, doc, empty_data);
			if (empty_data) {
				if (std::string(item->string).find(RESERVED_VALUES) == 0) cJSON_DeleteItemFromObject(document, item->string);
				if (elements > cJSON_GetArraySize(document)) {
					elements = cJSON_GetArraySize(document);
				} else {
					i++;
				}
			} else {
				i++;
			}
		}

		if (document_data) {
			std::string doc_data(cJSON_Print(document_data));
			LOG_DATABASE_WRAP(this, "Document data: %s\n", doc_data.c_str());
			doc.set_data(doc_data);
		} else {
			std::string doc_data(cJSON_Print(document));
			LOG_DATABASE_WRAP(this, "Document data: %s\n", doc_data.c_str());
			doc.set_data(doc_data);
		}

	} catch (const std::string &err) {
		LOG_DATABASE_WRAP(this, "ERROR: %s\n", err.c_str());
		return false;
	} catch (const char *err) {
		LOG_DATABASE_WRAP(this, "ERROR: %s\n", err);
		return false;
	}

	cJSON_Delete(document);
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


char
Database::field_type(const std::string &field_name)
{
	return STRING_TYPE;
}

char
Database::get_type(cJSON *field)
{
	int type = field->type;
	cJSON* aux = field;
	if (type == cJSON_Array) {
		int num_ele = cJSON_GetArraySize(field);
		aux = cJSON_GetArrayItem(field, 0);
		type = aux->type;
		bool meet_range = true;
		if (type == cJSON_Array) return GEO_TYPE;
		for (int i = 0; i < num_ele; i++) {
			aux = cJSON_GetArrayItem(field, i);
			if (aux->type != type) {
				throw "Different types of data";
			}
			if (i % 2 == 0 && (aux->type != cJSON_Number || aux->valuedouble < -90.0 || aux->valuedouble > 90.00)) {
				meet_range = false;
			} else if (i % 2 == 1 && (aux->type != cJSON_Number || aux->valuedouble < -360.0 || aux->valuedouble > 360.00)) {
				meet_range = false;
			}
		}
		if (num_ele % 2 == 0 && meet_range) {
			return GEO_TYPE;
		}
	}
	switch (type) {
		case cJSON_Number: return NUMERIC_TYPE;
		case cJSON_False: return BOOLEAN_TYPE;
		case cJSON_True: return BOOLEAN_TYPE;
		case cJSON_String:
			if (serialise_bool(aux->valuestring).size() != 0) {
				return BOOLEAN_TYPE;
			} else if (timestamp_date(aux->valuestring).size() != 0) {
				return DATE_TYPE;
			} else if ((strlen(aux->valuestring) > 30 && std::string(aux->valuestring).find(" ") != -1) || std::string(aux->valuestring).find("\n") != -1) {
				return TEXT_TYPE;
			} else {
				return STRING_TYPE;
			}
	}
	return STRING_TYPE;
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
	if (writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return srch_resul;
	}

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
		LOG(this, "e.query: %s\n", repr(queryQ.serialise()).c_str());


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
		LOG(this, "e.partial: %s\n", repr(queryP.serialise()).c_str());


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
			} else {
				queryT = Xapian::Query(Xapian::Query::OP_AND, queryT, srch.query);
			}
			sug_query.push_back(srch.suggested_query.back());
		}
		LOG(this, "e.terms: %s\n", repr(queryT.serialise()).c_str());

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

	int len = (int) query.size(), offset = 0;
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

	std::vector<std::unique_ptr<NumericFieldProcessor>> nfps;
	std::vector<std::unique_ptr<DateFieldProcessor>> dfps;
	std::vector<std::unique_ptr<BooleanFieldProcessor>> bfps;
	std::vector<std::unique_ptr<LatLongFieldProcessor>> gfps;
	std::vector<std::unique_ptr<LatLongDistanceFieldProcessor>> gdfps;
	std::vector<std::unique_ptr<Xapian::NumberValueRangeProcessor>> nvrps;
	std::vector<std::unique_ptr<Xapian::StringValueRangeProcessor>> svrps;
	std::vector<std::unique_ptr<DateTimeValueRangeProcessor>> dvrps;
	NumericFieldProcessor *nfp;
	DateFieldProcessor *dfp;
	BooleanFieldProcessor *bfp;
	LatLongFieldProcessor *gfp;
	LatLongDistanceFieldProcessor *gdfp;
	Xapian::NumberValueRangeProcessor *nvrp;
	Xapian::StringValueRangeProcessor *svrp;
	DateTimeValueRangeProcessor *dvrp;
	unsigned int slot;
	std::string prefix;

	while ((pcre_search(query.c_str(), len, offset, 0, FIND_FIELD_RE, &compiled_find_field_re, &g)) != -1) {
		offset = g[0].end;
		std::string field_name_dot(query.c_str() + g[1].start, g[1].end - g[1].start);
		std::string field_name(query.c_str() + g[2].start, g[2].end - g[2].start);
		std::string field_value(query.c_str() + g[3].start, g[3].end - g[3].start);

		if (isRange(field_value)) {
			switch (field_type(field_name)) {
				case NUMERIC_TYPE:
					slot = get_slot(field_name);
					nvrp = new Xapian::NumberValueRangeProcessor(slot, field_name_dot, true);
					LOG(this, "Numeric Slot: %u Field_name_dot: %s\n", slot, field_name_dot.c_str());
					nvrps.push_back(std::unique_ptr<Xapian::NumberValueRangeProcessor>(nvrp));
					queryparser.add_valuerangeprocessor(nvrp);
					break;
				case STRING_TYPE:
					if(!unique_doc) {
						slot = get_slot(field_name);
					} else {
						slot = 0;
					}
					svrp = new Xapian::StringValueRangeProcessor(slot, field_name_dot, true);
					svrps.push_back(std::unique_ptr<Xapian::StringValueRangeProcessor>(svrp));
					LOG(this, "String Slot: %u Field_name_dot: %s\n", slot, field_name_dot.c_str());
					queryparser.add_valuerangeprocessor(svrp);
					break;
				case DATE_TYPE:
					slot = get_slot(field_name);
					field_name_dot = std::string("");
					dvrp = new DateTimeValueRangeProcessor(slot, field_name_dot);
					dvrps.push_back(std::unique_ptr<DateTimeValueRangeProcessor>(dvrp));
					LOG(this, "Date Slot: %u Field_name: %s\n", slot, field_name.c_str());
					queryparser.add_valuerangeprocessor(dvrp);
					break;
				default:
					throw Xapian::QueryParserError("This type of Data has no support for range search.\n");
			}
		} else {
			switch (field_type(field_name)) {
				case NUMERIC_TYPE:
					prefix = get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE);
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
				case TEXT_TYPE:
				case STRING_TYPE:
					if (field_name.size() != 0) {
						if(!unique_doc) {
							prefix = get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, field_type(field_name));
							if (isupper(field_value.at(0))) {
								prefix = prefix + ":";
							}
						} else {
							prefix = "Q";
						}

						LOG(this, "prefix calculated: %s\n", prefix.c_str());
						if (strhasupper(field_name)) {
							LOG(this, "Boolean Prefix\n");
							queryparser.add_boolean_prefix(field_name, prefix);
						} else {
							LOG(this, "Prefix\n");
							queryparser.add_prefix(field_name, prefix);
						}
					}
					break;
				case DATE_TYPE:
					prefix = get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE);
					field_value = timestamp_date(field_value);
					if (field_value.size() == 0) {
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
					break;
				case GEO_TYPE:
					prefix = get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE);
					if (isLatLongDistance(field_value)) {
						gdfp = new LatLongDistanceFieldProcessor(prefix, field_name);
						gdfps.push_back(std::unique_ptr<LatLongDistanceFieldProcessor>(gdfp));
						if (strhasupper(field_name)) {
							LOG(this, "Boolean Prefix\n");
							queryparser.add_boolean_prefix(field_name, gdfp);
						} else {
							LOG(this, "Prefix\n");
							queryparser.add_prefix(field_name, gdfp);
						}
					} else {
						gfp = new LatLongFieldProcessor(prefix);
						gfps.push_back(std::unique_ptr<LatLongFieldProcessor>(gfp));
						if (strhasupper(field_name)) {
							LOG(this, "Boolean Prefix\n");
							queryparser.add_boolean_prefix(field_name, gfp);
						} else {
							LOG(this, "Prefix\n");
							queryparser.add_prefix(field_name, gfp);
						}
					}
					break;
				case BOOLEAN_TYPE:
					prefix = get_prefix(field_name, DOCUMENT_CUSTOM_TERM_PREFIX, BOOLEAN_TYPE);
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
			querystring =  field_name_dot + field_value;
			first_time = false;
		} else {
			querystring += " " + field_name_dot + field_value;
		}
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

	if (g) {
		free(g);
		g = NULL;
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
			prefixes.push_back(get_prefix(*it, DOCUMENT_CUSTOM_TERM_PREFIX, field_type(*it)));
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
		for (; oit != e.order.end(); oit++) {
			if (StartsWith(*oit, "-")) {
				decreasing = true;
				field.assign(*oit,1,(*oit).size() - 1);
				sorter->add_value(get_slot(field), decreasing);
			} else if (StartsWith(*oit, "+")) {
				decreasing = false;
				field.assign(*oit,1,(*oit).size()-1);
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
		} catch (const Xapian::Error &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		delete sorter;
		return 0;
	}
	LOG_ERR(this, "ERROR: Cannot search!\n");
	delete sorter;
	return 2;
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
	if (writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return database;
	}

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
	if (writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return document;
	}
	try {
		if (id_doc == 0) {
			cJSON_AddStringToObject(document, "id", "all");
			cJSON_AddNumberToObject(document, "allterms", std::distance(db->allterms_begin(), db->allterms_end()));
			cJSON_AddNumberToObject(document, "allspellings", std::distance(db->spellings_begin(), db->spellings_end()));
		} else {
			Xapian::Document doc = db->get_document(id_doc);
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