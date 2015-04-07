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

//change prefix to Q only
#define DOCUMENT_ID_TERM_PREFIX "Q:"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define FIND_FIELD_RE "\\b([ngsbd]_)?([_a-zA-Z][_a-zA-Z0-9]*):([^ ]*\\.\\.)?"
#define PREFIX_RE "(?:([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[-\\w. ]+\"|[-\\w.]+)"
#define TERM_SPLIT_RE "[^-\\w.]"
#define DATE_RE "(([1-9][0-9]{3})-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])(T([01][0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])(\\.([0-9]{3}))?)?(([+-])([01][0-9]|2[0-3])(:([0-5][0-9]))?)?)?)"
#define COORDS_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d+|\\d+)"

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

pcre *Database::compiled_terms = NULL;
pcre *Database::compiled_date_re = NULL;
pcre *Database::compiled_coords_re = NULL;
pcre *Database::compiled_find_field_re = NULL;

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

std::string
Database::stringtoupper(const std::string &str) 
{
	std::string tmp = str; 
	for (unsigned int i = 0; i < tmp.size(); i++)  {
		tmp.at(i) = toupper(tmp.at(i));
	}
	return tmp; 
}

std::string
Database::upper_stringtoupper(const std::string &str) 
{ 
	bool h_upper = false;
	std::string tmp = str; 
	for (unsigned int i = 0; i < tmp.size(); i++) {
		tmp.at(i) = toupper(tmp.at(i)); 
		if (tmp.at(i) == str.at(i) && tmp.at(i) != '_') {
			h_upper = true;
		}
	} 
	if (h_upper) {
		return tmp;
	} 
	return str;
} 

std::string
Database::stringtolower(const std::string &str) 
{ 
	std::string tmp = str; 
	for (unsigned int i = 0; i < tmp.size(); i++) {
		tmp.at(i) = tolower(tmp.at(i));
	}
	return tmp; 
}

std::string
Database::prefixed(const std::string &term, const std::string &prefix) {
	return stringtoupper(prefix) + stringtolower(term);
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
			LOG(this,"Ok\n");
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
				(name && name->type == 4) ? name_v = stringtoupper(get_prefix(std::string(name->valuestring), std::string(DOCUMENT_CUSTOM_TERM_PREFIX))) : name_v = "";
				LOG_DATABASE_WRAP(this, "Language: %s  Weight: %d  Spelling: %s Positions: %s Name: %s\n", lan.c_str(), w, spelling_v ? "true" : "false", positions_v ? "true" : "false", name_v.c_str());
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
Database::get_slot(const std::string &name)
{
	std::string standard_name = upper_stringtoupper(name);
	std::string _md5 = std::string(md5(standard_name), 24, 8);
	unsigned int slot = hex2int(_md5);
	if (slot == 0xffffffff) {
		slot = 0xfffffffe;
	}
	return slot;
}

unsigned int
Database::hex2int(const std::string &input) 
{
	unsigned int n;
	std::stringstream ss;
	ss << std::hex << input;
	ss >> n;
	ss.flush();
	return n;
}

int
Database::strtoint(const std::string &str)
{
	int number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


double
Database::strtodouble(const std::string &str)
{
	double number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}

double
Database::timestamp_date(const std::string &str)
{
	int len = (int) str.size();
	char sign;
	const char *errptr;
	int erroffset, ret, n[9]; 
	int grv[51]; // 17 * 3
	double  timestamp;
	
	if (compiled_date_re == NULL) {
		LOG(this, "Compiled date is NULL, we will compile\n");
		compiled_date_re = pcre_compile(DATE_RE, 0, &errptr, &erroffset, 0);
		if (compiled_date_re == NULL) {
			LOG_ERR(this, "ERROR: Could not compile '%s': %s\n", DATE_RE, errptr);
			return -1;
		}
	}

	ret = pcre_exec(compiled_date_re, 0, str.c_str(), len, 0, 0,  grv, sizeof(grv) / sizeof(int));
	group *gr = (group *) grv;
		
	if (ret && len == (gr[0].end - gr[0].start)) {
		std::string parse = std::string(str, gr[2].start, gr[2].end - gr[2].start);
		n[0] = strtoint(parse);
		parse = std::string(str, gr[3].start, gr[3].end - gr[3].start);
		n[1] = strtoint(parse);
		parse = std::string(str, gr[4].start, gr[4].end - gr[4].start);
		n[2] = strtoint(parse);

		if (gr[5].end - gr[5].start > 0) {
			parse = std::string(str, gr[6].start, gr[6].end - gr[6].start);
			n[3] = strtoint(parse);
			parse = std::string(str, gr[7].start, gr[7].end - gr[7].start);
			n[4] = strtoint(parse);
			if (gr[8].end - gr[8].start > 0) {
				parse = std::string(str, gr[9].start, gr[9].end - gr[9].start);
				n[5] = strtoint(parse);
				if (gr[10].end - gr[10].start > 0) {
					parse = std::string(str, gr[11].start, gr[11].end - gr[11].start);
					n[6] = strtoint(parse);
				} else {
					n[6] = 0;
				}
			} else {
				n[5] =  n[6] = 0;
			}
			if (gr[12].end - gr[12].start > 0) {
				sign = std::string(str, gr[13].start, gr[13].end - gr[13].start).at(0);
				parse = std::string(str, gr[14].start, gr[14].end - gr[14].start);
				n[7] = strtoint(parse);
				if (gr[15].end - gr[15].start > 0) {   
					parse = std::string(str, gr[16].start, gr[16].end - gr[16].start);
					n[8] = strtoint(parse); 
				} else {
					n[8] = 0;
				}
			} else {
				n[7] = 0;
				n[8] = 0;
				sign = '+';
			}
		} else {
			n[3] = n[4] = n[5] = n[6] = n[7] = n[8] = 0;
		}
		LOG(this, "Fecha Reconstruida: %04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d:%02d\n", n[0], n[1], n[2], n[3], n[4], n[5], n[6], sign, n[7], n[8]);
		if (n[1] == 2 && !((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 28) {
			LOG_ERR(this, "ERROR: Incorrect Date, This month only has 28 days\n");
			return -1;
		} else if(n[1] == 2 && ((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 29) {
		   LOG_ERR(this, "ERROR: Incorrect Date, This month only has 29 days\n");
		   return -1;
		} else if((n[1] == 4 || n[1] == 6 || n[1] == 9 || n[1] == 11) && n[2] > 30) {
			LOG_ERR(this, "ERROR: Incorrect Date, This month only has 30 days\n");
			return -1;
		}
	} else {
		return -1;
	}

	time_t tt = 0;
	struct tm *timeinfo = gmtime(&tt);
	timeinfo->tm_year   = n[0] - 1900;
	timeinfo->tm_mon    = n[1] - 1;
	timeinfo->tm_mday   = n[2]; 
	if (sign == '-') {
		timeinfo->tm_hour  = n[3] + n[7];
		timeinfo->tm_min   = n[4] + n[8];   
	} else {
		timeinfo->tm_hour  = n[3] - n[7];
		timeinfo->tm_min   = n[4] - n[8];
	}
	timeinfo->tm_sec    = n[5];
	const time_t dateGMT = timegm(timeinfo);
	timestamp = (double) dateGMT;
	timestamp += n[6]/1000.0;
	return timestamp;
}


std::string
Database::get_prefix(const std::string &name, const std::string &prefix)
{
	std::string slot = get_slot_hex(name);
	return prefix + slot;
}


std::string
Database::get_slot_hex(const std::string &name)
{
	std::string standard_name = upper_stringtoupper(name);
	std::string _md5 = std::string(md5(standard_name), 24, 8);
	return _md5;
}

std::string
Database::print_type(int type)
{
	switch (type) {
		case 3:
			return std::string("Numeric");
		case 4:
			return std::string("String");
		case 5:
			return std::string("Array");
		case 6:
			return std::string("Object");
		default:
			return std::string("Undefined");
	}
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
Database::serialise(const std::string &name, const std::string &value)
{
	if (name.at(0) == 'n' && name.at(1) == '_') {
		double val;
		val = strtodouble(value);
		return Xapian::sortable_serialise(val);
	} else if (name.at(0) == 's' && name.at(1) == '_') {
		return std::string(value);
	} else if (name.at(0) == 'd' && name.at(1) == '_') {
		double timestamp = timestamp_date(value);
		if (timestamp > 0) {
			return Xapian::sortable_serialise(timestamp);
		} else {
			LOG_ERR(this, "ERROR: Format date (%s) must be ISO 8601: YYYY-MM-DDThh:mm:ss.sss[+-]hh:mm (eg 1997-07-16T19:20:30.451+05:00)\n", value.c_str());
			return std::string("");
		}
	} else if (name.at(0) == 'g' && name.at(1) == '_') {
		Xapian::LatLongCoords coords;
		double latitude, longitude;
		int len = (int) value.size(), Ncoord = 0, offset = 0, size = 9; // 3 * 3
		bool end = false;
		int grv[size];
		while (lat_lon(value, grv, size, offset)) {
			group *g = (group *) grv;
			std::string parse(value, g[1].start, g[1].end - g[1].start);
			latitude = strtodouble(parse);
			parse = std::string(value, g[2].start, g[2].end - g[2].start);
			longitude = strtodouble(parse);
			Ncoord++;
			try {
				coords.append(Xapian::LatLongCoord(latitude, longitude));
			} catch (Xapian::Error &e) {
				LOG_ERR(this, "latitude or longitude out-of-range\n");
				return std::string("");
			}
			LOG(this, "Coord %d: %f, %f\n", Ncoord, latitude, longitude);
			if (g[2].end == len) {
				end = true;
				break;
			}
			offset = g[2].end;
		}
		if (Ncoord == 0 || !end) {
			LOG_ERR(this, "ERROR: %s must be an array of doubles [lat, lon, lat, lon, ...]\n", value.c_str());
			return std::string("");
		}
		return coords.serialise();
	} else if (name.at(0) == 'b' && name.at(1) == '_') {
		return parser_bool(value);
	} else if (name.at(1) == '_') {
		LOG_ERR(this, "ERROR: type %c%c no defined, you can only use [n_, g_, s_, b_, d_]\n", name.at(0), name.at(1));
		return std::string("");
	} else {
		return value;
	}
}

std::string 
Database::parser_bool(const std::string &value) {
	if (!value.c_str()) {
		return std::string("f");
	} else if(value.size() > 1) {
		if (strcasecmp(value.c_str(), "TRUE") == 0) {
			return std::string("t");
		} else if (strcasecmp(value.c_str(), "FALSE") == 0) {
			return std::string("f");
		} else {
			return std::string("t");	
		}
	} else {
		switch (tolower(value.at(0))) {
			case '1':
				return std::string("t");
			case '0':
				return std::string("f");
			case 't':
				return std::string("t");
			case 'f':
				return std::string("f");
			default:
				return std::string("t");
		}
	}
}

bool
Database::lat_lon(const std::string &str, int *grv, int size, int offset)
{
	int erroffset, ret;
	const char *errptr;
	int len = (int) str.size();

	if (!compiled_coords_re) {
		compiled_coords_re = pcre_compile(COORDS_RE, 0, &errptr, &erroffset, 0);
		if (compiled_coords_re == NULL) {
			LOG_ERR(this, "ERROR: Could not compile '%s': %s\n", COORDS_RE, errptr);
			return false;
		}
	}
	ret = pcre_exec(compiled_coords_re, 0, str.c_str(), len, offset, 0,  grv, size);
	if (ret == 3) {
		return true;
	}

	return false;
}


void
Database::print_hexstr(const std::string &str)
{
	unsigned char c;
	for (unsigned int i = 0; i < str.size(); i++) {
		c = str.at(i);
		printf("%.2x", c);
	}
	printf("\n");
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


int
Database::find_field(const char *str, group g[], int size_g) {
    const char *error;
    int   erroffset;
    
    // First, the regex string must be compiled.
    if (!compiled_find_field_re) {
        //pcre_free is not use because we use a struct pcre static and gets free at the end of the program
        compiled_find_field_re = pcre_compile (FIND_FIELD_RE, 0, &error, &erroffset, 0);
        if (!compiled_find_field_re) {
            LOG_ERR(this,"pcre_compile failed (offset: %d), %s\n", erroffset, error);
            return -1;
        }
    }
    if (compiled_find_field_re != NULL) {
        unsigned int offset = g[0].end;
        size_t len = strlen(str);
        //LOG(this,"sizeof*3 %lu sizeof/sizeof: %lu\n",sizeof(group)*3, sizeof(gr)/sizeof(int));
        if(pcre_exec(compiled_find_field_re, 0, str, (int)len, offset, 0, (int *)g, size_g) >= 0){
            return 0;
        }
        else return -1;
    } return -1;
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
Database::search(struct query_t e)
{
    Xapian::QueryParser queryparser;
    group g[4]; //pcre_exec needs a multiple of 3
    int size_g = sizeof(g)*3;
    memset(&g, 0, sizeof(g));
    int re;
    
    LOG(this,"sizeof de g: %d\n",size_g);
    
    if (writable) {
        LOG_ERR(this, "ERROR: database is %s\n", writable ? "w" : "r");
        return false;
    }
    
    if (e.query.size() != 0) {
        while ((re = find_field(e.query.c_str(), g, size_g)) != -1) {
            //group *g = (group *) gr;
            LOG(this,"group[1] %s\n" , std::string(e.query.c_str() + g[1].start, g[1].end - g[1].start).c_str());
            LOG(this,"group[2] %s\n" , std::string(e.query.c_str() + g[2].start, g[2].end - g[2].start).c_str());
            LOG(this,"group[3] %s\n" , std::string(e.query.c_str() + g[3].start, g[3].end - g[3].start).c_str());
            
            std::string prefix (e.query.c_str() + g[1].start, g[1].end - g[1].start);
            std::string field (e.query.c_str() + g[2].start, g[2].end - g[2].start);
            
            //case add_boolean_prefix
            if (isbooleanprefix(field)) {
                field = stringtoupper(field);
                LOG(this,"boolean Field: %s\n",field.c_str());
            }
            //case add_prefix
            else {
                LOG(this,"not boolean Field: %s\n",field.c_str());
                /*if ((g[3].end - g[3].start) == 0) {
                 
                 }else {
                 if ((g[2].end - g[2].start) == 0) {
                 //Chema fix this.
                 //queryparser.add_prefix(field.c_str(), get_prefix(field.c_str(),"s_" + field));
                 } else queryparser.add_prefix(field.c_str(), prefix);
                 
                 }*/
            }
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

