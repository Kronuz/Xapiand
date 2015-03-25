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

#define DOCUMENT_ID_TERM_PREFIX "Q:"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"


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


bool
Database::drop(const char *document_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

    document_id  = prefixed(document_id, DOCUMENT_ID_TERM_PREFIX);

	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Deleting: -%s- t:%d\n", document_id, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	    try {
	    	wdb->delete_document(document_id);
	    } catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
    	LOG_DATABASE_WRAP(this, "Document deleted\n");
		if (commit) return _commit();
	}

	LOG_ERR(this, "ERROR: Cannot delete document: %s!\n", document_id);
	return false;
}

char*
Database::prefixed(const char *term, const char *prefix)
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
Database::index(const char *document, const char *document_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);

	cJSON *root = cJSON_Parse(document);
	
	if (!root) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n",cJSON_GetErrorPtr());
		return false;
	}
	
	cJSON *doc_id = root ? cJSON_GetObjectItem(root, "id") : NULL;
    cJSON *document_data = root ? cJSON_GetObjectItem(root, "data") : NULL;
    cJSON *document_values = root ? cJSON_GetObjectItem(root, "values") : NULL;
    cJSON *document_terms = root ? cJSON_GetObjectItem(root, "terms") : NULL;
    cJSON *document_texts = root ? cJSON_GetObjectItem(root, "texts") : NULL;

    Xapian::Document doc;

    const char *document_id;
    if (doc_id) {
	    //Make sure document_id is also a term (otherwise it doesn't replace an existing document)
	    doc.add_value(get_slot("ID"), doc_id->valuestring);
	    document_id  = prefixed(doc_id->valuestring, DOCUMENT_ID_TERM_PREFIX);
	    doc.add_boolean_term(document_id);	
    } else {
    	LOG_ERR(this, "ERROR: Document must have an 'id'\n");
    	return false;
    }
    
    if (document_data) {
    	const char *doc_data = cJSON_Print(document_data);
    	LOG_DATABASE_WRAP(this, "Document data: %s\n", doc_data);
		doc.set_data(doc_data); 	
    } else {
    	LOG_ERR(this, "ERROR: You must provide 'data' to index\n");
    	return false;
    }

    if (document_values) {
    	for (int i = 0; i < cJSON_GetArraySize(document_values); i++) {
    		cJSON *name = cJSON_GetArrayItem(document_values, i);
    		cJSON *value = cJSON_GetObjectItem(name, "value");
 			cJSON *type = cJSON_GetObjectItem(name, "type");
            if (type) {
	            if (strcmp(type->valuestring, "int") == 0 && value->type == 3) {
	            	doc.add_value(get_slot(name->string), Xapian::sortable_serialise(value->valueint));
	            	LOG_DATABASE_WRAP(this, "%s: (int) %i\n", name->string, value->valueint);
	            } else if (strcmp(type->valuestring, "float") == 0 && value->type == 3) {
	            	doc.add_value(get_slot(name->string), Xapian::sortable_serialise(value->valuedouble));
	            	LOG_DATABASE_WRAP(this, "%s: (float) %f\n", name->string, value->valuedouble);
	            } else if (strcmp(type->valuestring, "str") == 0 && value->type == 4) {
	            	doc.add_value(get_slot(name->string), value->valuestring);
	            	LOG_DATABASE_WRAP(this, "%s: (str) %s\n", name->string, value->valuestring);
	            } else if (strcmp(type->valuestring, "date") == 0 && value->type == 4) {
	            	const char *date;
					const char *time;
					const char *sTZD;
					if ((date = _date(value->valuestring)) == NULL || 
						(time = _time(value->valuestring)) == NULL ||
						(sTZD = _sTZD(value->valuestring)) == NULL) {
						LOG_ERR(this, "ERROR: Format date (%s) must be ISO 8601: YYYY-MM-DDThh:mm:ss.sTZD (eg 1997-07-16T19:20:30.45+01:00)\n", cJSON_Print(value));
	            		return false;
	            	} else {
	            		doc.add_value(get_slot(name->string), date);
	            		LOG_DATABASE_WRAP(this, "%s: (date) %s\n", name->string, date);
	            		doc.add_value(get_slot("time"), time);
	            		LOG_DATABASE_WRAP(this, "%s: (time) %s\n", name->string, time);
					}
	            } else if (strcmp(type->valuestring, "geo") == 0 && value->type == 5) {
	            	cJSON *latitude = cJSON_GetArrayItem(value, 0);
	            	cJSON *longitude = cJSON_GetArrayItem(value, 1);

	            	if (latitude && longitude && latitude->type == 3 && longitude->type == 3){
	            		double lat = latitude->valuedouble;
	            		double lon = longitude->valuedouble; 
	            		std::stringstream geo;
						geo << lat << "," << lon;
						std::string s = geo.str();
	            		doc.add_value(get_slot(name->string), s.c_str());
	            		LOG_DATABASE_WRAP(this, "%s: (geo) - [%s]\n", name->string, s.c_str());	
	            	} else {
	            		LOG_ERR(this, "ERROR: %s must be an array of doubles [latitude, longitude] (%s)\n", value->string, cJSON_Print(value));
	            		return false;
	            	}
	            } else {
		            LOG_ERR(this, "ERROR: Types inconsistency, %s %s was defined as a (%s) but is (%s)\n", name->string, cJSON_Print(value), type->valuestring, print_type(value->type));
		        	return false;
		        }
            } else {
            	if (value->type == 3) {
            		doc.add_value(get_slot(name->string), Xapian::sortable_serialise(value->valuedouble));
	            	LOG_DATABASE_WRAP(this, "%s: (double default) %f\n", name->string, value->valuedouble);
            	} else if (value->type == 4) {
            		doc.add_value(get_slot(name->string), value->valuestring);
	            	LOG_DATABASE_WRAP(this, "%s: (str default) %s\n", name->string, value->valuestring);
            	} else {
            		LOG_ERR(this, "ERROR: You should define type, or use a string o double value\n", value->string, cJSON_Print(value));
            		return false;
            	}
            }
  		}
    }

    if (document_terms) {
    	LOG_DATABASE_WRAP(this, "Terms..\n");
    	for (int i = 0; i < cJSON_GetArraySize(document_terms); i++) {
            cJSON *term_data = cJSON_GetArrayItem(document_terms, i);
            cJSON *name = cJSON_GetObjectItem(term_data, "name");
            cJSON *term = cJSON_GetObjectItem(term_data, "term");
            cJSON *type = cJSON_GetObjectItem(term_data, "type");
            cJSON *weight = cJSON_GetObjectItem(term_data, "weight");
            cJSON *position = cJSON_GetObjectItem(term_data, "position");
            if (term) {
                Xapian::termcount w;
            	(weight && weight->type == 3) ? w = weight->valueint : w = 1;
            	LOG_DATABASE_WRAP(this, "Weight: %d\n", w);
            	const char *name_v;
            	(name) ? name_v = get_prefix(name->valuestring, DOCUMENT_CUSTOM_TERM_PREFIX) : name_v = "";
	            if (type) {
		            if (strcmp(type->valuestring, "int") == 0 && term->type == 3) {
		            	std::stringstream int2str;
						int2str << term->valueint;
						std::string s = int2str.str();
						const std::string nameterm(prefixed(s.c_str(), name_v));
            			if (position) {
            				doc.add_posting(nameterm, position->valueint, w);
            				LOG_DATABASE_WRAP(this, "Posting int: %s %d %d\n", nameterm.c_str(), position->valueint, w);
		            	} else {
		            		doc.add_term(nameterm, w);
		            		LOG_DATABASE_WRAP(this, "Term int: %s %d\n", nameterm.c_str(), w);
		            	}
		            } else if (strcmp(type->valuestring, "float") == 0 && term->type == 3) {
		            	std::stringstream dou2str;
						dou2str << term->valuedouble;
						std::string s = dou2str.str();
						const std::string nameterm(prefixed(s.c_str(), name_v));
            			if (position) {
	            			doc.add_posting(nameterm, position->valueint, w);
	            			LOG_DATABASE_WRAP(this, "Posting float: %s %d %d\n", nameterm.c_str(), position->valueint, w);
	            		} else {
		            		doc.add_term(nameterm, w);
		            		LOG_DATABASE_WRAP(this, "Term float: %s %d\n", nameterm.c_str(), w);
		            	}
		            } else if (strcmp(type->valuestring, "str") == 0 && term->type == 4) {
		            	const std::string nameterm(prefixed(term->valuestring, name_v));
		            	if (position) {
	            			doc.add_posting(nameterm, position->valueint, w);
	            			LOG_DATABASE_WRAP(this, "Posting str: %s %d %d\n", nameterm.c_str(), position->valueint, w);
		            	} else {
		            		doc.add_term(nameterm, w);
		            		LOG_DATABASE_WRAP(this, "Term str: %s %d\n", nameterm.c_str(), w);
		            	}
		            } else if (strcmp(type->valuestring, "date") == 0 && term->type == 4) {
		            	const char *date;
						const char *time;
						const char *sTZD;
						if ((date = _date(term->valuestring)) == NULL || 
							(time = _time(term->valuestring)) == NULL ||
							(sTZD = _sTZD(term->valuestring)) == NULL) {
							LOG_ERR(this, "ERROR: Format date (%s) must be ISO 8601: YYYY-MM-DDThh:mm:ss.sTZD (eg 1997-07-16T19:20:30.45+01:00)\n", cJSON_Print(term));
		            		return false;
		            	} else {
		            		const std::string nameterm(prefixed(date, name_v));
	            			const std::string nameterm_t(prefixed(time, name_v));
	            			if (position) {
		            			doc.add_posting(nameterm, position->valueint, w);
		            			LOG_DATABASE_WRAP(this, "Posting date (date): %s %d %d\n", nameterm.c_str(), position->valueint, w);		            		
			            		doc.add_posting(nameterm_t, position->valueint, w);
		            			LOG_DATABASE_WRAP(this, "Posting date (time): %s %d %d\n", nameterm_t.c_str(), position->valueint, w);
		            		} else {
			            		doc.add_term(nameterm, w);
			            		LOG_DATABASE_WRAP(this, "Term date (date): %s %d\n", nameterm.c_str(), w);
			            		doc.add_term(nameterm_t, w);
			            		LOG_DATABASE_WRAP(this, "Term date (time): %s %d\n", nameterm_t.c_str(), w);
		            		}
						}
		            } else if (strcmp(type->valuestring, "geo") == 0 && term->type == 5) {
		            	cJSON *latitude = cJSON_GetArrayItem(term, 0);
		            	cJSON *longitude = cJSON_GetArrayItem(term, 1);
		            	if (latitude && longitude && latitude->type == 3 && longitude->type == 3){
		            		double lat = latitude->valuedouble;
		            		double lon = longitude->valuedouble; 
		            		std::stringstream geo;
							geo << lat << "," << lon;
							std::string s = geo.str();
							const std::string nameterm(prefixed(s.c_str(), name_v));
		            		if (position) {
		            			doc.add_posting(nameterm, position->valueint, w);
		            			LOG_DATABASE_WRAP(this, "Posting geo: %s %d %d\n", nameterm.c_str(), position->valueint, w);
		            		} else {
			            		doc.add_term(nameterm, w);
			            		LOG_DATABASE_WRAP(this, "Term geo: %s %d\n", nameterm.c_str(), w);
			            	}
		            	} else {
		            		LOG_ERR(this, "ERROR: %s must be an array of doubles [latitude, longitude] (%s)\n", term->string, cJSON_Print(term));
		            		return false;
		            	}
		            } else {
		            	LOG_ERR(this, "ERROR: Types inconsistency, %s %s was defined as a (%s) but is (%s)\n", term->string, cJSON_Print(term), type->valuestring, print_type(term->type));
		            	return false;
		            }
	            } else {
	            	LOG_DATABASE_WRAP(this, "Type: %d\n", term->type);
	            	if (term->type == 3) {
	            		std::stringstream dou2str;
						dou2str << term->valuedouble;
						std::string s = dou2str.str();
						const std::string nameterm(prefixed(s.c_str(), name_v));
            			if (position) {
	            			doc.add_posting(nameterm, position->valueint, w);
	            			LOG_DATABASE_WRAP(this, "Posting int (default): %s %d %d\n", nameterm.c_str(), position->valueint, w);
	            		} else {
		            		doc.add_term(nameterm, w);
		            		LOG_DATABASE_WRAP(this, "Term int (default): %s %d\n", nameterm.c_str(), w);
		            	}
	            	} else if (term->type == 4) {
	            		const std::string nameterm(prefixed(term->valuestring, name_v));
            			if (position) {
	            			doc.add_posting(nameterm, position->valueint, w);
	            			LOG_DATABASE_WRAP(this, "Posting str (default): %s %d %d\n", nameterm.c_str(), position->valueint, w);
	            		} else {
		            		doc.add_term(nameterm, w);
		            		LOG_DATABASE_WRAP(this, "Term str (default): %s %d\n", nameterm.c_str(), w);
		            	}
	            	} else {
	            		LOG_ERR(this, "ERROR: You should define type, or use a string o double term\n", term->string, cJSON_Print(term));
	            		return false;
	            	}
	            }
            } else {
            	LOG_ERR(this, "ERROR: Term must be defined\n");
            	return false;
            }
  		}
    }
  	
  	if (document_texts) {
    	LOG_DATABASE_WRAP(this, "Texts..\n");
    	for (int i = 0; i < cJSON_GetArraySize(document_texts); i++) {
    		cJSON *row_text = cJSON_GetArrayItem(document_texts, i);
            cJSON *name = cJSON_GetObjectItem(row_text, "name");
            cJSON *text = cJSON_GetObjectItem(row_text, "text");
            cJSON *language = cJSON_GetObjectItem(row_text, "language");
            cJSON *weight = cJSON_GetObjectItem(row_text, "weight");
            cJSON *spelling = cJSON_GetObjectItem(row_text, "spelling");
            cJSON *positions = cJSON_GetObjectItem(row_text, "positions");
            if (text) {
                Xapian::termcount w;
            	const char *lan;
            	bool spelling_v;
            	bool positions_v;
            	const char *name_v;
            	(weight && weight->type == 3) ? w = weight->valueint : w = 1;
            	(language && language->type == 4) ? lan = language->valuestring : lan = "en";
            	(spelling && (strcmp(cJSON_Print(spelling), "true") == 0)) ? spelling_v = true : spelling_v = false;
            	(positions && (strcmp(cJSON_Print(positions), "true") == 0)) ? positions_v = true : positions_v = false;
            	(name && name->type == 4) ? name_v = get_prefix(name->valuestring, DOCUMENT_CUSTOM_TERM_PREFIX) : name_v = "";
            	LOG_DATABASE_WRAP(this, "Language: %s  Weight: %d  Spelling: %s Positions: %s Name: %s\n", lan, w, spelling_v ? "true" : "false", positions_v ? "true" : "false", name_v);
            	Xapian::TermGenerator term_generator;
            	term_generator.set_document(doc);
    			term_generator.set_stemmer(Xapian::Stem(lan));
    			if (spelling_v) {
    				term_generator.set_database(*wdb);
	                term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
    			}
    			(positions_v) ? term_generator.index_text(text->valuestring, w, name_v) : term_generator.index_text_without_positions(text->valuestring, w, name_v);
    		} else {
            	LOG_ERR(this, "ERROR: Text must be defined\n");
            	return false;
            }
    	}
    }
    cJSON_Delete(root);
    return replace(document_id, doc, commit);
}

unsigned int
Database::get_slot(const char *name)
{
	char *cad = new char[strlen(name) + 1];
    int i = 0;
    for(;name[i] != '\0'; i++) {
    	cad[i] = tolower(name[i]);
    }
    cad[i] = '\0';
    std::string _md5(md5(cad), 24, 32);
    const char *md5_cad = _md5.c_str();
    unsigned int slot = hex2long(md5_cad);
    if (slot == 0xffffffff) {
    	slot = 0xfffffffe;
    }
    return slot;
}


unsigned int
Database::hex2long(const char *input) 
{
    unsigned n;
    std::stringstream ss;
    ss << std::hex << input;
    ss >> n;
    return n;
}

char*
Database::_date(const char *iso_date)
{
	char *date = new char[9];
	int j = 0;
	for (int i = 0; i < 10; i++) {
		if (iso_date[i] >= '0' && iso_date[i] <= '9' && i != 7 && i != 4) {
			date[j] = iso_date[i];
			j++;
		} else if (iso_date[i] != '-' || (i != 4 && i != 7)) {
			return NULL;
		}
	}
	date[j] = '\0';
	return date;
}

char*
Database::_time(const char *iso_date)
{
	char *time = new char[9];
	int j = 0;
	if (iso_date[10] != 'T') {
		return NULL;
	}
	for (int i = 11; i < 19; i++) {
		if (iso_date[i] >= '0' && iso_date[i] <= '9' && i != 13 && i != 16) {
			time[j] = iso_date[i];
			j++;
		} else if (iso_date[i] == ':' && (i == 13 || i == 16)) {
			time[j] = iso_date[i];
			j++;
		} else {
			return NULL;
		}
	}
	time[j] = '\0';
	return time;
} 

char*
Database::_sTZD(const char *iso_date)
{
	size_t size_iso = strlen(iso_date);
	char *sTZD = new char[9];
	size_t j = 0, pos1 = size_iso - 6, pos2 = size_iso - 3;
	if (iso_date[19] != '.') {
		return NULL;
	}
	for (int i = 20; i < size_iso; i++) {
		if (iso_date[i] >= '0' && iso_date[i] <= '9') {
			sTZD[j] = iso_date[i];
			j++;
		} else if (iso_date[i] == '-' && i == pos1) {
			sTZD[j] = iso_date[i];
			j++;
		} else if(iso_date[i] == ':'  && i == pos2) {
			sTZD[j] = iso_date[i];
			j++;
		} else {
			return NULL;
		}
	}
	sTZD[j] = '\0';
	return sTZD;
}

char *
Database::get_prefix(const char *name, const char *prefix)
{
	const char *slot = get_slot_hex(name);
	return prefixed(slot, prefix);
}


char*
Database::get_slot_hex(const char *name)
{
	char *cad = new char[strlen(name) + 1];
    int i = 0;
    for(;name[i] != '\0'; i++) {
    	cad[i] = tolower(name[i]);
    }
    cad[i] = '\0';
    std::string _md5(md5(cad), 24, 32);
    const char *md5_cad = _md5.c_str();
    char *md5_upper = new char[strlen(md5_cad) + 1];
    for(i = 0; md5_cad[i] != '\0'; i++) {
    	md5_upper[i] = toupper(md5_cad[i]);
    }
    md5_upper[i] = '\0';
    return md5_upper;
}

const char*
Database::print_type(int type)
{
	switch (type) {
		case 3:
			return "Numeric";
		case 4:
			return "String";
		case 5:
			return "Array";
		case 6:
			return "Object";
		default:
		 	return "Undefined";
	}
}

bool
Database::replace(const char *document_id, const Xapian::Document doc, bool commit)
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Inserting: -%s- t:%d\n", document_id, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	    try {
	    	LOG_DATABASE_WRAP(this, "Doing replace_document.\n");
	    	wdb->replace_document (document_id, doc);
	    	LOG_DATABASE_WRAP(this, "Replace_document was done.\n");
	    } catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
    	LOG_DATABASE_WRAP(this, "Document inserted\n");
		if (commit) return _commit();
	}
	return false;
}

void
Database::search(query_t query, bool get_matches, bool get_data, bool get_terms, bool get_size, bool dead, bool counting)
{
	
}