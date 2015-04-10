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

#include "database.h"

//change prefix to Q only
#define DOCUMENT_ID_TERM_PREFIX "Q"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define FIND_FIELD_RE "(([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[^\"]+\"|[^\" ]+)"
#define FIND_TERMS_RE "(?:([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[-\\w. ]+\"|[-\\w.]+)"

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


pcre *Database::compiled_find_field_re = NULL;
pcre *Database::compiled_find_terms_re = NULL;

bool
Database::drop(const std::string &doc_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

	std::string document_id  = prefixed(doc_id, std::string(DOCUMENT_ID_TERM_PREFIX));

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
		if (commit) return _commit();
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
Database::index(const std::string &document, const std::string &_document_id, bool commit)
{
	if (!writable) {
		LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
		return false;
	}

	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	cJSON *root = cJSON_Parse(document.c_str());

	if (!root) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		return false;
	}
	cJSON *document_data = root ? cJSON_GetObjectItem(root, "data") : NULL;
	cJSON *document_values = root ? cJSON_GetObjectItem(root, "values") : NULL;
	cJSON *document_terms = root ? cJSON_GetObjectItem(root, "terms") : NULL;
	cJSON *document_texts = root ? cJSON_GetObjectItem(root, "texts") : NULL;

	Xapian::Document doc;

	std::string document_id;
	if (_document_id.c_str()) {
		//Make sure document_id is also a term (otherwise it doesn't replace an existing document)
		doc.add_value(get_slot(std::string("ID")), document_id);
		document_id = prefixed(_document_id, std::string(DOCUMENT_ID_TERM_PREFIX));
		doc.add_boolean_term(document_id);
	} else {
		LOG_ERR(this, "ERROR: Document must have an 'id'\n");
		return false;
	}

	if (document_data) {
		std::string doc_data = std::string(cJSON_Print(document_data));
		LOG_DATABASE_WRAP(this, "Document data: %s\n", doc_data.c_str());
		doc.set_data(doc_data);
	} else {
		LOG_ERR(this, "ERROR: You must provide 'data' to index\n");
		return false;
	}

	if (document_values) {
		LOG_DATABASE_WRAP(this, "Values..\n");
		for (int i = 0; i < cJSON_GetArraySize(document_values); i++) {
			cJSON *name = cJSON_GetArrayItem(document_values, i);
			std::string value = std::string(cJSON_Print(name));
			if (name->type == 4 || name->type == 5) {
				value = std::string(value, 1, (int) value.size() - 2);
			}
			LOG_DATABASE_WRAP(this, "Name: (%s) Value: (%s)\n", name->string, value.c_str());
			std::string val_serialized = serialise(std::string(name->string), value);
			if (val_serialized.c_str()) {
				unsigned int slot = get_slot(std::string(name->string));
				doc.add_value(slot, val_serialized);
				LOG_DATABASE_WRAP(this, "Slot: %X serialized: %s\n", slot, val_serialized.c_str());
			} else {
				LOG_ERR(this, "%s: %s not serialized", name->string, cJSON_Print(name));
				return false;
			}
		}
	}
	
	if (document_terms) {
		LOG_DATABASE_WRAP(this, "Terms..\n");
		for (int i = 0; i < cJSON_GetArraySize(document_terms); i++) {
			cJSON *term_data = cJSON_GetArrayItem(document_terms, i);
			cJSON *name = cJSON_GetObjectItem(term_data, "name");
			cJSON *term = cJSON_GetObjectItem(term_data, "term");
			cJSON *weight = cJSON_GetObjectItem(term_data, "weight");
			cJSON *position = cJSON_GetObjectItem(term_data, "position");
			std::string term_v = std::string(cJSON_Print(term));
			if (term->type == 4 || term->type == 5) {
				term_v = std::string(term_v, 1, term_v.size() - 2);
			}
			LOG_DATABASE_WRAP(this, "Term value: %s\n", term_v.c_str());
			if (name) {
				LOG_DATABASE_WRAP(this, "Name: %s\n", name->valuestring);
				term_v = serialise(std::string(name->valuestring), term_v);
			}
			if (term) {
				Xapian::termcount w;
				(weight && weight->type == 3) ? w = weight->valueint : w = 1;
				if (position) {
					if (name && name->valuestring[0] == 'g' && name->valuestring[1] == '_') {
						insert_terms_geo(term_v, &doc, std::string(name->valuestring), w, position->valueint);
					} else {
						std::string name_v;
						(name) ? name_v = get_prefix(std::string(name->valuestring), std::string(DOCUMENT_CUSTOM_TERM_PREFIX)) : name_v = std::string(DOCUMENT_CUSTOM_TERM_PREFIX);
						std::string nameterm(prefixed(term_v, name_v));
						doc.add_posting(nameterm, position->valueint, w);
						LOG_DATABASE_WRAP(this, "Posting: %s %d %d\n", nameterm.c_str(), position->valueint, w);
					}
				} else {
					if (name && name->valuestring[0] == 'g' && name->valuestring[1] == '_') {
						insert_terms_geo(term_v, &doc, std::string(name->valuestring), w, -1);
					} else {
						std::string name_v;
						(name) ? name_v = get_prefix(std::string(name->valuestring), std::string(DOCUMENT_CUSTOM_TERM_PREFIX)) : name_v = std::string(DOCUMENT_CUSTOM_TERM_PREFIX);
						std::string nameterm(prefixed(term_v, name_v));
						doc.add_term(nameterm, w);
						LOG_DATABASE_WRAP(this, "Term: %s %d\n", nameterm.c_str(), w);
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
				std::string lan;
				bool spelling_v;
				bool positions_v;
				std::string name_v;
				(weight && weight->type == 3) ? w = weight->valueint : w = 1;
				(language && language->type == 4) ? lan = std::string(language->valuestring) : lan = std::string("en");
				(spelling && (strcmp(cJSON_Print(spelling), "true") == 0)) ? spelling_v = true : spelling_v = false;
				(positions && (strcmp(cJSON_Print(positions), "true") == 0)) ? positions_v = true : positions_v = false;
                (name && name->type == 4) ? name_v = stringtoupper(get_prefix(std::string(name->valuestring), std::string(DOCUMENT_CUSTOM_TERM_PREFIX))) : name_v = std::string("");
				LOG_DATABASE_WRAP(this, "Language: %s  Weight: %d  Spelling: %s Positions: %s Name: %s\n", lan.c_str(), w, spelling_v ? "true" : "false", positions_v ? "true" : "false", name_v.c_str());
				Xapian::TermGenerator term_generator;
				term_generator.set_document(doc);
				term_generator.set_stemmer(Xapian::Stem(lan));
                
				if (spelling_v) {
					term_generator.set_database(*wdb);
					term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
				}
				
                try { (positions_v) ? term_generator.index_text(text->valuestring, w, name_v) : term_generator.index_text_without_positions     (text->valuestring, w, name_v);
                }
                catch ( Xapian::Error &e) {
                    LOG(this,"ERROR: %s\n",e.get_msg().c_str());
                }
                
                //LOG(this,"Aqui!!!!!!\n");
			} else {
				LOG_ERR(this, "ERROR: Text must be defined\n");
				return false;
			}
		}
	}

	cJSON_Delete(root);
	return replace(document_id, doc, commit);
}


bool
Database::replace(const std::string &document_id, const Xapian::Document doc, bool commit)
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Inserting: -%s- t:%d\n", document_id.c_str(), t);
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


std::string
Database::serialise(const std::string &field_name, const std::string &field_value)
{
	if (field_type(field_name) == 0) {
		return serialise_numeric(field_value);
	} else if (field_type(field_name) == 1) {
		return field_value;
	} else if (field_type(field_name) == 2) {
		return serialise_date(field_value);
	} else if (field_type(field_name) == 3) {
		return serialise_geo(field_value);
	} else if (field_type(field_name) == 4) {
		return serialise_bool(field_value);
	}
	return std::string("");
}


void
Database::insert_terms_geo(const std::string &g_serialise, Xapian::Document *doc, const std::string &name, 
	int w, int position)
{
	bool found;
	std::vector<std::string> terms;
	for (int i = 6; i > 1; i--) {
		for (int j = 0; j < g_serialise.size(); j += 6) {
			found = false;
			std::string s_coord = std::string(g_serialise, j, i);
			for (int k = 0; k < terms.size(); k++) {
				if (s_coord.compare(terms[k]) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				std::string name_v;
				(name.c_str()) ? name_v = get_prefix(name, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)) : name_v = std::string(DOCUMENT_CUSTOM_TERM_PREFIX);
				std::string nameterm(prefixed(s_coord, name_v));
				LOG(this, "Nameterm: %s   Prefix: %s   Term: ", nameterm.c_str(), name_v.c_str());
				print_hexstr(s_coord);

				if (position > 0) {
					doc->add_posting(nameterm, position, w);
					LOG_DATABASE_WRAP(this, "Posting: %s %d %d\n", nameterm.c_str(), position, w);
				} else {
					doc->add_term(nameterm, w);
					LOG_DATABASE_WRAP(this, "Term: %s %d\n", nameterm.c_str(), w);
				}
				terms.push_back(s_coord);
			}
		}
	}
}


bool
Database::isbooleanprefix(std::string field) {
    const char *c = field.c_str();
    while(*c) {
        if (isupper(*c)) return true;
        c++;
    }
    return false;
}

bool
Database::search(struct query_t e, std::string &results)
{
    Xapian::QueryParser queryparser;

    if (writable) {
        LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
        return false;
    }

	group *g = NULL;

	int offset = 0;
    if (e.query.size() != 0) {
        while ((pcre_search(e.query.c_str(), (int)e.query.size(), offset, 0, FIND_FIELD_RE, &compiled_find_field_re, &g)) != -1) {
			offset = g[0].end;
			LOG(this,"group[1] %s\n" , std::string(e.query.c_str() + g[1].start, g[1].end - g[1].start).c_str());
			LOG(this,"group[2] %s\n" , std::string(e.query.c_str() + g[2].start, g[2].end - g[2].start).c_str());
			LOG(this,"group[3] %s\n" , std::string(e.query.c_str() + g[3].start, g[3].end - g[3].start).c_str());
			
            std::string type (e.query.c_str()+ g[1].start, g[1].end - g[1].start);
            std::string field_ (e.query.c_str()+ g[2].start, g[2].end - g[2].start);
			std::string field = type + field_;
			
            //case add_boolean_prefix
            if (isbooleanprefix(field)) {
                field = stringtoupper(field);
                LOG(this,"boolean Field: %s\n",field.c_str());
            }
            //case add_prefix
            else {
                LOG(this,"not boolean Field: %s\n",field.c_str());
				//case Range
				if ((g[3].end - g[3].start) != 0) {
				//if (1) {
					switch (4) {
						case 0:{
							Xapian::NumberValueRangeProcessor vrp(get_slot(field), "");
							queryparser.add_valuerangeprocessor(&vrp);
							break; }
							
						case 1: {
							Xapian::StringValueRangeProcessor vrp(get_slot(field));
							queryparser.add_valuerangeprocessor(&vrp);
							break; }
							
						case 2: {
							//This is not a range search

							//LatLongDistanceFieldProcessor f(get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)), field);
							//queryparser.add_prefix(field, &f);
							break; }
							
						case 4: {
							LOG(this,"DateValueRangeProcessor\n");
							DateTimeValueRangeProcessor vrp(get_slot("d_date"), "d_date", get_prefix("d_date", std::string(DOCUMENT_CUSTOM_TERM_PREFIX)));
							queryparser.add_valuerangeprocessor(&vrp);
							break; }
							
						default: LOG_ERR(this, "This type of Data has no support for range search\n");
							return false;
					}
					
					
                 }else {
					switch (field_type(type)) {
						  case 0: {
							 NumericFieldProcessor f(get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)), field);
							 queryparser.add_prefix(field, &f);
							 break; }
							 
						 case 1:{
							 queryparser.add_prefix(field, get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)));
							 break; }
							 
						 case 2:{
							 LatLongFieldProcessor f(get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)), field);
							 queryparser.add_prefix(field, &f);
							 break; }
							 
						 case 3:{
							 BooleanFieldProcessor f(get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)), field);
							 queryparser.add_prefix(field, &f);
							 break; }
							 
						 case 4:{
							 //Not working...
							 LOG(this,"Before DateFieldProcessor\n");
							 DateFieldProcessor f(get_prefix(field, std::string(DOCUMENT_CUSTOM_TERM_PREFIX)), field);
							 queryparser.add_prefix(field, &f);
							 break; }
					 }
				}
            }
		}

		if (g) {
			free(g);
			g = NULL;
		}

		
		std::string querystring;
		bool first_time = true;

		offset = 0;
		while ((pcre_search(e.query.c_str(), (int)e.query.size(), offset, 0, FIND_TERMS_RE, &compiled_find_terms_re, &g)) != -1) {
			offset = g[0].end;
			if (!first_time) {
				querystring += " AND " + std::string(e.query.c_str() + g[0].start, g[0].end - g[0].start);
			} else {
				querystring =  std::string(e.query.c_str() + g[0].start, g[0].end - g[0].start);
				first_time = false;
			}
		}

		if (g) {
			free(g);
			g = NULL;
		}

		LOG(this,"querystring %s\n",querystring.c_str());
		Xapian::Query query = queryparser.parse_query(querystring);
		//Xapian::Query query = queryparser.parse_query("1997-07-16T19:20:30.451+05:00..");
		
		/*
		int flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_PURE_NOT;
		try {
			Xapian::Query query = queryparser.parse_query(querystring, flags);
			results = get_results(query, e);
		}
		catch ( Xapian::Error &er) {
			LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
			reopen();
			queryparser.set_database(*db);
			Xapian::Query query = queryparser.parse_query(querystring, flags);
			results = get_results(query, e);
		}
    }
    return true;
}

std::string
Database::search1(struct query_t e)
{
    Xapian::QueryParser queryparser;
    Xapian::Query query;
    queryparser.add_prefix("Kind", "XK");
    queryparser.add_prefix("Title", "S");
    
    query = queryparser.parse_query("Action");
    std::string content = get_results(query, e);
    //LOG(this, "RESPONSE------->%s\n",res.c_str());
    
    return content;
}

Xapian::Enquire
Database::get_enquire(Xapian::Query query, struct query_t e) {
    Xapian::Enquire enquire(*db);
    enquire.set_query(query);
    /*
     complement enquire ....
     */
    return enquire;
}

std::string
Database::get_results(Xapian::Query query, struct query_t e) {
    
    cJSON *root = cJSON_CreateObject();
    
    int rc = 0;
    int maxitems = db->get_doccount() - e.offset;
    maxitems = std::max(std::min(maxitems, e.limit), 0);
    
    Xapian::Enquire enquire(*db);
    enquire.set_query(query);
    
    Xapian::MSet mset = enquire.get_mset(e.offset, e.limit);
    LOG(this, "mset size:%d!!!!\n",mset.size());
    for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); ++m) {
        Xapian::docid did = *m;
        cJSON *response = cJSON_CreateObject();
        LOG(this, "loop %d docid:%d rank:%d data:%s\n",rc,did,m.get_rank(),std::string(m.get_document().get_data()).c_str());
        std::string name_resp = "response" + std::to_string(rc);
        cJSON_AddItemToObject(root, name_resp.c_str(), response);
        cJSON_AddNumberToObject(response, "docid", did);
        cJSON_AddNumberToObject(response, "rank", m.get_rank());
        const std::string data (m.get_document().get_data());
        cJSON_AddStringToObject(response, "data", data.c_str());
        rc ++;
    }
    
    std::string res =cJSON_PrintUnformatted(root);
    LOG(this, "RESPONSE------->%s\n",res.c_str());
    cJSON_Delete(root);
    return (res);
}

