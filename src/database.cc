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
#include "multivalue.h"
#include "multivaluerange.h"
#include "cJSON_Utils.h"
#include "generate_terms.h"

#include <assert.h>
#include <bitset>

#define XAPIAN_LOCAL_DB_FALLBACK 1

#define DATABASE_UPDATE_TIME 10
#define FIND_FIELD_RE "(([_a-zA-Z][_a-zA-Z0-9]*):)?(\"[^\"]+\"|[^\" ]+)"

#define getPos(pos, size) ((pos) < (size) ? (pos) : (size))

pcre *Database::compiled_find_field_re = NULL;


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
			}
#ifdef HAVE_REMOTE_PROTOCOL
			else {
				local = false;
				// Writable remote databases do not have a local fallback
				wdb = Xapian::Remote::open_writable(e->host, e->port, 0, 10000, e->path);
			}
#endif
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
			}
#ifdef HAVE_REMOTE_PROTOCOL
			else {
				local = false;
# ifdef XAPIAN_LOCAL_DB_FALLBACK
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
# else
				rdb = Xapian::Remote::open(e->host, e->port, 0, 10000, e->path);
# endif
			}
#endif
			db->add_database(rdb);
		}
	}
}


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
		}catch (const Xapian::DatabaseCorruptError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			return false;
		}catch (const Xapian::DatabaseError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			return false;
		}catch (const Xapian::Error &e) {
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

	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(_document_id.at(0))) prefix += ":";
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	Xapian::Query query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + _document_id);

	Xapian::Enquire enquire(*db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	Xapian::MSetIterator m = mset.begin();

	for (int t = 3; t >= 0; --t) {
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

	unique_cJSON data_json(cJSON_Parse(document.get_data().c_str()), cJSON_Delete);
	if (!data_json) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		return false;
	}

	if (cJSONUtils_ApplyPatches(data_json.get(), patches) == 0) {
		// Object patched
		return index(data_json.get(), _document_id, commit);
	}

	// Object no patched
	return false;
}


void
Database::index_fields(cJSON *item, const std::string &item_name, specifications_t &spc_now, Xapian::Document &doc, cJSON *properties, bool find, bool is_value)
{
	std::string subitem_name;
	specifications_t spc_bef = spc_now;
	if (item->type == cJSON_Object) {
		find ? update_specifications(item, spc_now, properties) : insert_specifications(item, spc_now, properties);
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
					subproperties = cJSON_CreateObject(); // It is managed by properties.
					cJSON_AddItemToObject(properties, subitem->string, subproperties);
				}
				subitem_name = !item_name.empty() ? item_name + DB_OFFSPRING_UNION + subitem->string : subitem->string;
				if (size_t pfound = subitem_name.rfind(DB_OFFSPRING_UNION) != std::string::npos) {
					std::string language(subitem_name.substr(pfound + strlen(DB_OFFSPRING_UNION)));
					if (is_language(language)) {
						spc_now.language.clear();
						spc_now.language.push_back(language);
					}
				}
				index_fields(subitem, subitem_name, spc_now, doc, subproperties, find, is_value);
				offspring++;
			} else if (strcmp(subitem->string, RESERVED_VALUE) == 0) {
				if (spc_now.sep_types[2] == NO_TYPE) {
					spc_now.sep_types[2] = get_type(subitem, spc_now);
					update_required_data(spc_now, item_name, properties);
				}
				if (is_value || spc_now.index == VALUE) {
					index_values(doc, subitem, spc_now, item_name, properties, find);
				} else if (spc_now.index == TERM) {
					index_terms(doc, subitem, spc_now, item_name, properties, find);
				} else {
					index_terms(doc, subitem, spc_now, item_name, properties, find);
					index_values(doc, subitem, spc_now, item_name, properties);
				}
			}
		}
		if (offspring != 0) {
			cJSON *_type = cJSON_GetObjectItem(properties, RESERVED_TYPE); // It is managed by schema.
			if (_type && cJSON_GetArrayItem(_type, 0)->valueint == NO_TYPE)
				cJSON_ReplaceItemInArray(_type, 0, cJSON_CreateNumber(OBJECT_TYPE));
		}
	} else {
		find ? update_specifications(item, spc_now, properties) : insert_specifications(item, spc_now, properties);
		if (spc_now.sep_types[2] == NO_TYPE) {
			spc_now.sep_types[2] = get_type(item, spc_now);
			update_required_data(spc_now, item_name, properties);
		}
		if (is_value || spc_now.index == VALUE) {
			index_values(doc, item, spc_now, item_name, properties, find);
		} else if (spc_now.index == TERM) {
			index_terms(doc, item, spc_now, item_name, properties, find);
		} else {
			index_terms(doc, item, spc_now, item_name, properties, find);
			index_values(doc, item, spc_now, item_name, properties);
		}
	}
	spc_now = spc_bef;
}


void
Database::index_texts(Xapian::Document &doc, cJSON *texts, specifications_t &spc, const std::string &name, cJSON *schema, bool find)
{
	//LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!(find || spc.dynamic)) throw MSG_Error("This object is not dynamic");

	if (!spc.store) return;

	if (spc.bool_term) throw MSG_Error("A boolean term can not be indexed as text");

	int elements = 1;
	if (texts->type == cJSON_Array) {
		elements = cJSON_GetArraySize(texts);
		cJSON *value = cJSON_GetArrayItem(texts, 0);
		if (value->type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");

		// If the type in schema is not array, schema is updated.
		cJSON *_type = cJSON_GetObjectItem(schema, RESERVED_TYPE); // It is managed by schema.
		if (_type && cJSON_GetArrayItem(_type, 1)->valueint == NO_TYPE)
			cJSON_ReplaceItemInArray(_type, 1, cJSON_CreateNumber(ARRAY_TYPE));
	}

	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	for (int j = 0; j < elements; j++) {
		cJSON *text = (texts->type == cJSON_Array) ? cJSON_GetArrayItem(texts, j) : texts;
		if (text->type != cJSON_String) throw MSG_Error("Text should be string or array of strings");
		Xapian::TermGenerator term_generator;
		term_generator.set_document(doc);
		term_generator.set_stemmer(Xapian::Stem(spc.language[getPos(j, spc.language.size())]));
		if (spc.spelling[getPos(j, spc.spelling.size())]) {
			term_generator.set_database(*wdb);
			term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
			term_generator.set_stemming_strategy((Xapian::TermGenerator::stem_strategy)spc.analyzer[getPos(j, spc.analyzer.size())]);
		}

		if (spc.positions[getPos(j, spc.positions.size())]) {
			spc.prefix.empty() ? term_generator.index_text_without_positions(text->valuestring, spc.weight[getPos(j, spc.weight.size())]) : term_generator.index_text_without_positions(text->valuestring, spc.weight[getPos(j, spc.weight.size())], spc.prefix);
			LOG_DATABASE_WRAP(this, "Text to Index with positions = %s: %s\n", spc.prefix.c_str(), text->valuestring);
		} else {
			spc.prefix.empty() ? term_generator.index_text(text->valuestring, spc.weight[getPos(j, spc.weight.size())]) : term_generator.index_text(text->valuestring, spc.weight[getPos(j, spc.weight.size())], spc.prefix);
			LOG_DATABASE_WRAP(this, "Text to Index = %s: %s\n", spc.prefix.c_str(), text->valuestring);
		}
	}
}


void
Database::index_terms(Xapian::Document &doc, cJSON *terms, specifications_t &spc, const std::string &name, cJSON *schema, bool find)
{
	//LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!(find || spc.dynamic)) throw MSG_Error("This object is not dynamic");

	if (!spc.store) return;

	int elements = 1;
	if (terms->type == cJSON_Array) {
		elements = cJSON_GetArraySize(terms);
		cJSON *value = cJSON_GetArrayItem(terms, 0);
		if (value->type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");

		// If the type in schema is not array, schema is updated.
		cJSON *_type = cJSON_GetObjectItem(schema, RESERVED_TYPE); // It is managed by schema.
		if (_type && cJSON_GetArrayItem(_type, 1)->valueint == NO_TYPE)
			cJSON_ReplaceItemInArray(_type, 1, cJSON_CreateNumber(ARRAY_TYPE));
	}

	for (int j = 0; j < elements; j++) {
		cJSON *term = (terms->type == cJSON_Array) ? cJSON_GetArrayItem(terms, j) : terms;
		unique_char_ptr _cprint(cJSON_Print(term));
		std::string term_v(_cprint.get());
		if (term->type == cJSON_String) {
			term_v.assign(term_v, 1, term_v.size() - 2);
			if(!spc.bool_term && term_v.find(" ") != std::string::npos && spc.sep_types[2] == STRING_TYPE) {
				index_texts(doc, term, spc, name, schema);
				continue;
			}
		} else if (term->type == cJSON_Number) term_v = std::to_string(term->valuedouble);

		LOG_DATABASE_WRAP(this, "%d Term -> %s: %s\n", j, spc.prefix.c_str(), term_v.c_str());

		if (spc.sep_types[2] == GEO_TYPE) {
			std::vector<std::string> geo_terms;
			EWKT_Parser::getIndexTerms(term_v, spc.accuracy[0], spc.accuracy[1], geo_terms);
			std::vector<std::string>::const_iterator it(geo_terms.begin());
			if (spc.position[getPos(j, spc.position.size())] >= 0) {
				for ( ; it != geo_terms.end(); it++) {
					std::string nameterm(prefixed(*it, spc.prefix));
					doc.add_posting(nameterm, spc.position[getPos(j, spc.position.size())], spc.weight[getPos(j, spc.weight.size())]);
				}
			} else {
				for ( ; it != geo_terms.end(); it++) {
					std::string nameterm(prefixed(*it, spc.prefix));
					spc.bool_term ? doc.add_boolean_term(nameterm) : doc.add_term(nameterm, spc.weight[getPos(j, spc.weight.size())]);
				}
			}
		} else {
			term_v = Serialise::serialise(spc.sep_types[2], term_v);
			if (term_v.empty()) throw MSG_Error("%s: %s can not be serialized", name.c_str(), term_v.c_str());
			if (spc.sep_types[2] == STRING_TYPE && !spc.bool_term) term_v = stringtolower(term_v);

			if (spc.position[getPos(j, spc.position.size())] >= 0) {
				std::string nameterm(prefixed(term_v, spc.prefix));
				doc.add_posting(nameterm, spc.position[getPos(j, spc.position.size())], spc.bool_term ? 0: spc.weight[getPos(j, spc.weight.size())]);
				LOG_DATABASE_WRAP(this, "Bool: %d  Posting: %s\n", spc.bool_term, repr(nameterm).c_str());
			} else {
				std::string nameterm(prefixed(term_v, spc.prefix));
				spc.bool_term ? doc.add_boolean_term(nameterm) : doc.add_term(nameterm, spc.weight[getPos(j, spc.weight.size())]);
				LOG_DATABASE_WRAP(this, "Bool: %d  Term: %s\n", spc.bool_term, repr(nameterm).c_str());
			}
		}
	}
}


void
Database::index_values(Xapian::Document &doc, cJSON *values, specifications_t &spc, const std::string &name, cJSON *schema, bool find)
{
	LOG_DATABASE_WRAP(this, "specifications: %s", specificationstostr(spc).c_str());
	if (!(find || spc.dynamic)) throw MSG_Error("This object is not dynamic");

	if (!spc.store) return;

	int elements = 1;
	if (values->type == cJSON_Array) {
		elements = cJSON_GetArraySize(values);
		cJSON *value = cJSON_GetArrayItem(values, 0);
		if (value->type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");

		// If the type in schema is not array, schema is updated.
		cJSON *_type = cJSON_GetObjectItem(schema, RESERVED_TYPE); // It is managed by schema.
		if (_type && cJSON_GetArrayItem(_type, 1)->valueint == NO_TYPE)
			cJSON_ReplaceItemInArray(_type, 1, cJSON_CreateNumber(ARRAY_TYPE));
	}

	StringList s;
	for (int j = 0; j < elements; j++) {
		cJSON *value = (values->type == cJSON_Array) ? cJSON_GetArrayItem(values, j) : values;
		unique_char_ptr _cprint(cJSON_Print(value));
		std::string value_v(_cprint.get());
		if (value->type == cJSON_String) {
			value_v.assign(value_v, 1, value_v.size() - 2);
		} else if (value->type == cJSON_Number) {
			value_v = std::to_string(value->valuedouble);
		}

		LOG_DATABASE_WRAP(this, "Name: (%s) Value: (%s)\n", name.c_str(), value_v.c_str());

		if (spc.sep_types[2] == GEO_TYPE) {
			std::vector<range_t> ranges;
			CartesianList centroids;
			uInt64List start_end;
			EWKT_Parser::getRanges(value_v, spc.accuracy[0], spc.accuracy[1], ranges, centroids);
			// Index Values and looking for terms generated by accuracy.
			std::set<std::string> set_terms;
			std::vector<range_t>::iterator it(ranges.begin());
			for ( ; it != ranges.end(); it++) {
				start_end.push_back(it->start);
				start_end.push_back(it->end);
				int idx = -1;
				uInt64 val;
				if (it->start != it->end) {
					std::bitset<SIZE_BITS_ID> b1(it->start), b2(it->end), res;
					idx = SIZE_BITS_ID - 1;
					for ( ; b1.test(idx) == b2.test(idx); idx--) res.set(idx, b1.test(idx));
					val = res.to_ullong();
				} else val = it->start;
				for (int i = 2; i < spc.accuracy.size(); i++) {
					int pos = START_POS - spc.accuracy[i] * 2;
					if (idx < pos) {
						uInt64 vterm = val >> pos;
						set_terms.insert(prefixed(Serialise::trixel_id(vterm), spc.acc_prefix[i - 2]));
					} else break;
				}
			}
			// Insert terms generated by accuracy.
			for (std::set<std::string>::iterator it(set_terms.begin()); it != set_terms.end(); it++)
				doc.add_term(*it);
			s.push_back(start_end.serialise());
			s.push_back(centroids.serialise());
		} else {
			std::string value_s = Serialise::serialise(spc.sep_types[2], value_v);
			if (value_s.empty()) {
				throw MSG_Error("%s: %s can not serialized", name.c_str(), value_v.c_str());
			}
			s.push_back(value_s);

			// Index terms generated by accuracy.
			switch (spc.sep_types[2]) {
				case NUMERIC_TYPE: {
					for (size_t len = spc.accuracy.size(), i = 0; i < len; i++) {
						long long int _v = strtollong(value_v);
						std::string term_v = std::to_string(_v - _v % (long long)spc.accuracy[i]);
						term_v = Serialise::numeric(term_v);
						std::string nameterm(prefixed(term_v, spc.acc_prefix[i]));
						doc.add_term(nameterm);
					}
					break;
				}
				case DATE_TYPE: {
					bool findMath = value_v.find("||") != std::string::npos;
					for (size_t len = spc.accuracy.size(), i = 0; i < len; i++) {
						std::string acc(value_v);
						switch ((char)spc.accuracy[i]) {
							case DB_YEAR2INT:
								acc += findMath ? "//y" : "||//y";
								break;
							case DB_MONTH2INT:
								acc += findMath ? "//M" : "||//M";
								break;
							case DB_DAY2INT:
								acc += findMath ? "//d" : "||//d";
								break;
							case DB_HOUR2INT:
								acc += findMath ? "//h" : "||//h";
								break;
							case DB_MINUTE2INT:
								acc += findMath ? "//m" : "||//m";
								break;
							case DB_SECOND2INT:
								acc += findMath ? "//s" : "||//s";
								break;
						}
						acc.assign(Serialise::date(acc));
						std::string nameterm(prefixed(acc, spc.acc_prefix[i]));
						doc.add_term(nameterm);
					}
					break;
				}
			}
		}
	}

	doc.add_value(spc.slot, s.serialise());
	LOG_DATABASE_WRAP(this, "Slot: %u serialized: %s\n", spc.slot, repr(s.serialise()).c_str());
}


bool
Database::index(cJSON *document, const std::string &_document_id, bool commit)
{
	if (!(flags & DB_WRITABLE)) {
		LOG_ERR(this, "ERROR: database is read-only\n");
		return false;
	}

	Xapian::Document doc;

	unique_char_ptr _cprint(cJSON_Print(document));
	std::string doc_data(_cprint.get());
	LOG_DATABASE_WRAP(this, "Document to index: %s\n", doc_data.c_str());
	doc.set_data(doc_data);

	cJSON *document_terms = cJSON_GetObjectItem(document, RESERVED_TERMS);
	cJSON *document_texts = cJSON_GetObjectItem(document, RESERVED_TEXTS);

	std::string s_schema = db->get_metadata(RESERVED_SCHEMA);

	// There are several throws and returns, so we use unique_ptr
	// to call automatically cJSON_Delete. Only schema need to be released.
	unique_cJSON schema(cJSON_CreateObject(), cJSON_Delete);
	cJSON *properties;
	bool find = false;
	if (s_schema.empty()) {
		properties = cJSON_CreateObject(); // It is managed by chema.
		cJSON_AddItemToObject(schema.get(), RESERVED_VERSION, cJSON_CreateNumber(DB_VERSION_SCHEMA));
		cJSON_AddItemToObject(schema.get(), RESERVED_SCHEMA, properties);
	} else {
		schema = std::move(unique_cJSON(cJSON_Parse(s_schema.c_str()), cJSON_Delete));
		if (!schema) {
			LOG_ERR(this, "ERROR: Schema is corrupt, you need provide a new one. JSON Before: [%s]\n", cJSON_GetErrorPtr());
			return false;
		}
		cJSON *_version = cJSON_GetObjectItem(schema.get(), RESERVED_VERSION);
		if (_version == NULL || _version->valuedouble != DB_VERSION_SCHEMA) {
			LOG_ERR(this, "ERROR: Different database's version schemas, the current version is %1.1f\n", DB_VERSION_SCHEMA);
			return false;
		}
		properties = cJSON_GetObjectItem(schema.get(), RESERVED_SCHEMA);
		find = true;
	}

	std::string document_id;
	cJSON *subproperties = NULL;
	if (_document_id.c_str()) {
		//Make sure document_id is also a boolean term (otherwise it doesn't replace an existing document)
		doc.add_value(0, _document_id);
		document_id = prefixed(_document_id, DOCUMENT_ID_TERM_PREFIX);
		LOG_DATABASE_WRAP(this, "Slot: 0 _id: %s  term: %s\n", _document_id.c_str(), document_id.c_str());
		doc.add_boolean_term(document_id);

		subproperties = cJSON_GetObjectItem(properties, RESERVED_ID);
		if (!subproperties) {
			subproperties = cJSON_CreateObject(); // It is managed by properties.
			cJSON *type = cJSON_CreateArray(); // Managed by shema
			cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
			cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
			cJSON_AddItemToArray(type, cJSON_CreateNumber(STRING_TYPE));
			cJSON_AddItemToObject(subproperties, RESERVED_TYPE, type);
			cJSON_AddItemToObject(subproperties, RESERVED_INDEX, cJSON_CreateNumber(ALL));
			cJSON_AddItemToObject(subproperties, RESERVED_SLOT, cJSON_CreateNumber(0));
			cJSON_AddItemToObject(subproperties, RESERVED_PREFIX, cJSON_CreateString(DOCUMENT_ID_TERM_PREFIX));
			cJSON_AddItemToObject(subproperties, RESERVED_BOOL_TERM, cJSON_CreateTrue());
			cJSON_AddItemToObject(properties, RESERVED_ID, subproperties);
		}
	} else {
		LOG_ERR(this, "ERROR: Document must have an 'id'\n");
		return false;
	}

	try {
		//Default specifications
		specifications_t spc_now = default_spc;
		find ? update_specifications(document, spc_now, properties, true) : insert_specifications(document, spc_now, properties);
		specifications_t spc_bef = spc_now;

		if (document_texts) {
			for (int _size = cJSON_GetArraySize(document_texts), i = 0; i < _size; i++) {
				cJSON *texts = cJSON_GetArrayItem(document_texts, i);
				cJSON *name = cJSON_GetObjectItem(texts, RESERVED_NAME);
				cJSON *text = cJSON_GetObjectItem(texts, RESERVED_VALUE);
				if (text) {
					bool find = true;
					std::string name_s = name ? name->type == cJSON_String ? name->valuestring : throw MSG_Error("%s should be string", RESERVED_NAME) : std::string();
					if (!name_s.empty()) {
						subproperties = cJSON_GetObjectItem(properties, name_s.c_str());
						if (!subproperties) {
							find = false;
							subproperties = cJSON_CreateObject(); // It is managed by properties.
							cJSON_AddItemToObject(properties, name_s.c_str(), subproperties);
						}
						find ? update_specifications(texts, spc_now, subproperties) : insert_specifications(texts, spc_now, subproperties);
						if (size_t pfound = name_s.rfind(DB_OFFSPRING_UNION) != std::string::npos) {
							std::string language(name_s.substr(pfound + strlen(DB_OFFSPRING_UNION)));
							if (is_language(language)) {
								spc_now.language.clear();
								spc_now.language.push_back(language);
							}
						}
					} else { // Only need to get the specifications.
						unique_cJSON t(cJSON_CreateObject(), cJSON_Delete);
						update_specifications(texts, spc_now, t.get());
					}
					if (spc_now.sep_types[2] == NO_TYPE) {
						spc_now.sep_types[2] = get_type(text, spc_now);
						update_required_data(spc_now, name_s, subproperties);
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
							subproperties = cJSON_CreateObject(); // It is managed by properties.
							cJSON_AddItemToObject(properties, name_s.c_str(), subproperties);
						}
						find ? update_specifications(data_terms, spc_now, subproperties) : insert_specifications(data_terms, spc_now, subproperties);
					} else {
						unique_cJSON t(cJSON_CreateObject(), cJSON_Delete);
						update_specifications(data_terms, spc_now, t.get());
					}
					if (spc_now.sep_types[2] == NO_TYPE) {
						get_type(terms, spc_now);
						update_required_data(spc_now, name_s, subproperties);
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
					subproperties = cJSON_CreateObject(); // It is managed by properties.
					cJSON_AddItemToObject(properties, item->string, subproperties);
				}
				index_fields(item, item->string, spc_now, doc, subproperties, find, false);
			} else if (strcmp(item->string, RESERVED_VALUES) == 0) {
				index_fields(item, "", spc_now, doc, properties, find);
			}
		}

	} catch (const std::exception &err) {
		LOG_DATABASE_WRAP(this, "ERROR: %s\n", err.what());
		return false;
	}

	Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
	_cprint = std::move(unique_char_ptr(cJSON_Print(schema.get())));
	wdb->set_metadata(RESERVED_SCHEMA, _cprint.get());
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


data_field_t
Database::get_data_field(const std::string &field_name)
{
	data_field_t res = {Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false};

	if (field_name.empty()) {
		return res;
	}

	std::string json = db->get_metadata(RESERVED_SCHEMA);
	if (json.empty()) return res;

	unique_cJSON schema(cJSON_Parse(json.c_str()), cJSON_Delete);
	if (!schema) {
		throw MSG_Error("Schema's database is corrupt");
	}
	cJSON *properties = cJSON_GetObjectItem(schema.get(), RESERVED_SCHEMA);
	if (!properties) {
		throw MSG_Error("Schema's database was not found");
	}

	std::vector<std::string> fields = split_fields(field_name);
	std::vector<std::string>::const_iterator it = fields.begin();
	for ( ; it != fields.end(); it++) {
		properties = cJSON_GetObjectItem(properties, (*it).c_str());
		if (!properties) break;
	}

	// If properties exits then the specifications too.
	if (properties) {
		cJSON *_aux = cJSON_GetObjectItem(properties, RESERVED_TYPE);
		res.type = cJSON_GetArrayItem(_aux, 2)->valueint;
		if (res.type == NO_TYPE) return res;

		_aux = cJSON_GetObjectItem(properties, RESERVED_SLOT);
		unique_char_ptr _cprint(cJSON_Print(_aux));
		res.slot = strtouint(_cprint.get());

		_aux = cJSON_GetObjectItem(properties, RESERVED_PREFIX);
		res.prefix = _aux->valuestring;

		_aux = cJSON_GetObjectItem(properties, RESERVED_BOOL_TERM);
		res.bool_term = _aux->type == cJSON_False ? false : true;

		// Strings and booleans do not have accuracy.
		if (res.type != STRING_TYPE && res.type != BOOLEAN_TYPE) {
			_aux = cJSON_GetObjectItem(properties, RESERVED_ACCURACY);
			int elements = cJSON_GetArraySize(_aux);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(_aux, i);
				res.accuracy.push_back(acc->valuedouble);
			}

			_aux = cJSON_GetObjectItem(properties, RESERVED_ACC_PREFIX);
			elements = cJSON_GetArraySize(_aux);
			for (int i = 0; i < elements; i++) {
				cJSON *acc = cJSON_GetArrayItem(_aux, i);
				res.acc_prefix.push_back(acc->valuestring);
			}
		}
	}

	return res;
}


data_field_t
Database::get_slot_field(const std::string &field_name)
{
	data_field_t res = {Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false};

	if (field_name.empty()) {
		return res;
	}

	std::string json = db->get_metadata(RESERVED_SCHEMA);
	if (json.empty()) return res;

	unique_cJSON schema(cJSON_Parse(json.c_str()), cJSON_Delete);
	if (!schema) {
		throw MSG_Error("Schema's database is corrupt.");
	}
	cJSON *properties = cJSON_GetObjectItem(schema.get(), RESERVED_SCHEMA);
	if (!properties) {
		throw MSG_Error("Schema's database is corrupt.");
	}

	std::vector<std::string> fields = split_fields(field_name);
	std::vector<std::string>::const_iterator it = fields.begin();
	for ( ; it != fields.end(); it++) {
		properties = cJSON_GetObjectItem(properties, (*it).c_str());
		if (!properties) break;
	}

	if (properties) {
		cJSON *_aux = cJSON_GetObjectItem(properties, RESERVED_SLOT);
		unique_char_ptr _cprint(cJSON_Print(_aux));
		res.slot = strtouint(_cprint.get());

		_aux = cJSON_GetObjectItem(properties, RESERVED_TYPE);
		res.type = cJSON_GetArrayItem(_aux, 2)->valueint;
	}

	return res;
}


Database::search_t
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
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
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
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	LOG(this, "e.partial: %s\n", queryP.get_description().c_str());


	LOG(this, "e.terms size: %d\n", e.terms.size());
	std::vector<std::string>::const_iterator tit(e.terms.begin());
	flags = Xapian::QueryParser::FLAG_BOOLEAN | Xapian::QueryParser::FLAG_PURE_NOT;
	if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
	if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
	first = true;
	for (; tit != e.terms.end(); tit++) {
		srch = _search(*tit, flags, false, "", e.unique_doc);
		if (first) {
			queryT = srch.query;
			first = false;
		} else {
			queryT = Xapian::Query(Xapian::Query::OP_AND, queryT, srch.query);
		}
		sug_query.push_back(srch.suggested_query.back());
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	LOG(this, "e.terms: %s\n", repr(queryT.get_description()).c_str());

	first = true;
	if (!e.query.empty()) {
		queryF = queryQ;
		first = false;
	}
	if (!e.partial.empty()) {
		if (first) {
			queryF = queryP;
			first = false;
		} else {
			queryF = Xapian::Query(Xapian::Query::OP_AND, queryF, queryP);
		}
	}
	if (!e.terms.empty()) {
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
}


Database::search_t
Database::_search(const std::string &query, unsigned int flags, bool text, const std::string &lan, bool unique_doc)
{
	search_t srch;

	if (query.compare("*") == 0) {
		srch.query = Xapian::Query::MatchAll;
		srch.suggested_query.push_back("");
		return srch;
	}

	int len = (int)query.size(), offset = 0;
	unique_group unique_gr;
	bool first_time = true, first_timeR = true;
	std::string querystring;
	Xapian::Query queryRange;
	Xapian::QueryParser queryparser;
	queryparser.set_database(*db);

	if (text) {
		queryparser.set_stemming_strategy(queryparser.STEM_SOME);
		lan.empty() ? queryparser.set_stemmer(Xapian::Stem(default_spc.language[0])) : queryparser.set_stemmer(Xapian::Stem(lan));
	}

	std::vector<std::string> added_prefixes;
	NumericFieldProcessor *nfp;
	DateFieldProcessor *dfp;
	GeoFieldProcessor *gfp;
	BooleanFieldProcessor *bfp;

	while ((pcre_search(query.c_str(), len, offset, 0, FIND_FIELD_RE, &compiled_find_field_re, unique_gr)) != -1) {
		group_t *g = unique_gr.get();
		offset = g[0].end;
		std::string field(query.c_str() + g[0].start, g[0].end - g[0].start);
		std::string field_name_dot(query.c_str() + g[1].start, g[1].end - g[1].start);
		std::string field_name(query.c_str() + g[2].start, g[2].end - g[2].start);
		std::string field_value(query.c_str() + g[3].start, g[3].end - g[3].start);
		data_field_t field_t = get_data_field(field_name);

		// Auxiliary variables.
		std::vector<std::string> prefixes;
		std::vector<std::string>::const_iterator it;
		std::vector<range_t> ranges;
		std::vector<range_t>::const_iterator rit;
		CartesianList centroids;
		std::string filter_term, start, end, prefix;
		unique_group unique_Range;

		if (isRange(field_value, unique_Range)) {
			// If this field is not indexed as value, not process this query.
			if (field_t.slot == Xapian::BAD_VALUENO) continue;

			switch (field_t.type) {
				case NUMERIC_TYPE:
					g = unique_Range.get();
					start = std::string(field_value.c_str() + g[1].start, g[1].end - g[1].start);
					end = std::string(field_value.c_str() + g[2].start, g[2].end - g[2].start);
					GenerateTerms::numeric(filter_term, start, end, field_t.accuracy, field_t.acc_prefix, prefixes);
					queryRange = MultipleValueRange::getQuery(field_t.slot, NUMERIC_TYPE, start, end, field_name);
					if (!filter_term.empty()) {
						for (it = prefixes.begin(); it != prefixes.end(); it++) {
							// Xapian does not allow repeat prefixes.
							if (std::find(added_prefixes.begin(), added_prefixes.end(), *it) == added_prefixes.end()) {
								nfp = new NumericFieldProcessor(*it);
								srch.nfps.push_back(std::move(std::unique_ptr<NumericFieldProcessor>(nfp)));
								queryparser.add_prefix(*it, nfp);
								added_prefixes.push_back(*it);
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
					}
					break;
				case STRING_TYPE:
					g = unique_Range.get();
					start = std::string(field_value.c_str() + g[1].start, g[1].end - g[1].start);
					end = std::string(field_value.c_str() + g[2].start, g[2].end - g[2].start);
					queryRange = MultipleValueRange::getQuery(field_t.slot, STRING_TYPE, start, end, field_name);
					break;
				case DATE_TYPE:
					g = unique_Range.get();
					start = std::string(field_value.c_str() + g[1].start, g[1].end - g[1].start);
					end = std::string(field_value.c_str() + g[2].start, g[2].end - g[2].start);
					GenerateTerms::date(filter_term, start, end, field_t.accuracy, field_t.acc_prefix, prefixes);
					queryRange = MultipleValueRange::getQuery(field_t.slot, DATE_TYPE, start, end, field_name);
					if (!filter_term.empty()) {
						for (it = prefixes.begin(); it != prefixes.end(); it++) {
							// Xapian does not allow repeat prefixes.
							if (std::find(added_prefixes.begin(), added_prefixes.end(), *it) == added_prefixes.end()) {
								dfp = new DateFieldProcessor(*it);
								srch.dfps.push_back(std::move(std::unique_ptr<DateFieldProcessor>(dfp)));
								queryparser.add_prefix(*it, dfp);
								added_prefixes.push_back(*it);
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
					}
					break;
				case GEO_TYPE:
					// Delete double quotes and .. (always): "..EWKT" -> EWKT
					field_value.assign(field_value.c_str(), 3, field_value.size() - 4);
					EWKT_Parser::getRanges(field_value, field_t.accuracy[0], field_t.accuracy[1], ranges, centroids);

					// If the region for search is empty, not process this query.
					if (ranges.empty()) continue;

					queryRange = GeoSpatialRange::getQuery(field_t.slot, ranges, centroids);
					GenerateTerms::geo(filter_term, ranges, field_t.accuracy, field_t.acc_prefix, prefixes);
					if (!filter_term.empty()) {
						// Xapian does not allow repeat prefixes.
						for (it = prefixes.begin(); it != prefixes.end(); it++) {
							// Xapian does not allow repeat prefixes.
							if (std::find(added_prefixes.begin(), added_prefixes.end(), *it) == added_prefixes.end()) {
								gfp = new GeoFieldProcessor(*it);
								srch.gfps.push_back(std::move(std::unique_ptr<GeoFieldProcessor>(gfp)));
								queryparser.add_prefix(*it, gfp);
								added_prefixes.push_back(*it);
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
					}
					break;
			}

			// Concatenate with OR all the ranges queries.
			if (first_timeR) {
				srch.query = queryRange;
				first_timeR = false;
			} else srch.query = Xapian::Query(Xapian::Query::OP_OR, srch.query, queryRange);
		} else {
			// If the field has not been indexed as a term, not process this query.
			if (!field_name.empty() && field_t.prefix.empty()) continue;

			switch (field_t.type) {
				case NUMERIC_TYPE:
					// Xapian does not allow repeat prefixes.
					if (std::find(added_prefixes.begin(), added_prefixes.end(), field_t.prefix) == added_prefixes.end()) {
						nfp = new NumericFieldProcessor(field_t.prefix);
						srch.nfps.push_back(std::move(std::unique_ptr<NumericFieldProcessor>(nfp)));
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, nfp) : queryparser.add_prefix(field_name, nfp);
						added_prefixes.push_back(field_t.prefix);
					}
					if (field_value.at(0) == '-') {
						field_value.at(0) = '_';
						field = field_name_dot + field_value;
					}
					break;
				case STRING_TYPE:
					// Xapian does not allow repeat prefixes.
					if (std::find(added_prefixes.begin(), added_prefixes.end(), field_t.prefix) == added_prefixes.end()) {
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, field_t.prefix) : queryparser.add_prefix(field_name, field_t.prefix);
						added_prefixes.push_back(field_t.prefix);
					}
					break;
				case DATE_TYPE:
					// If there are double quotes, they are deleted: "date" -> date
					if (field_value.at(0) == '"') field_value.assign(field_value, 1, field_value.size() - 2);
					try {
						// Always pass to timestamp.
						field_value = std::to_string(Datetime::timestamp(field_value));
						// Xapian does not allow repeat prefixes.
						if (std::find(added_prefixes.begin(), added_prefixes.end(), field_t.prefix) == added_prefixes.end()) {
							dfp = new DateFieldProcessor(field_t.prefix);
							srch.dfps.push_back(std::move(std::unique_ptr<DateFieldProcessor>(dfp)));
							field_t.bool_term ? queryparser.add_boolean_prefix(field_name, dfp) : queryparser.add_prefix(field_name, dfp);
							added_prefixes.push_back(field_t.prefix);
						}
						if (field_value.at(0) == '-') field_value.at(0) = '_';
						field = field_name_dot + field_value;
					} catch (const std::exception &ex) {
						throw Xapian::QueryParserError("Didn't understand date field name's specification: '" + field_name + "'");
					}
					break;
				case GEO_TYPE:
					// Delete double quotes (always): "EWKT" -> EWKT
					field_value.assign(field_value, 1, field_value.size() - 2);
					EWKT_Parser::getSearchTerms(field_value, field_t.accuracy[0], field_t.accuracy[1], prefixes);

					// If the region for search is empty, not process this query.
					if (prefixes.empty()) continue;

					it = prefixes.begin();
					field = field_name_dot + *it;
					for (it++; it != prefixes.end(); it++)
						field += " AND " + field_name_dot + *it;
					// Xapian does not allow repeat prefixes.
					if (std::find(added_prefixes.begin(), added_prefixes.end(), field_t.prefix) == added_prefixes.end()) {
						gfp = new GeoFieldProcessor(field_t.prefix);
						srch.gfps.push_back(std::move(std::unique_ptr<GeoFieldProcessor>(gfp)));
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, gfp) : queryparser.add_prefix(field_name, gfp);
						added_prefixes.push_back(field_t.prefix);
					}
					break;
				case BOOLEAN_TYPE:
					// Xapian does not allow repeat prefixes.
					if (std::find(added_prefixes.begin(), added_prefixes.end(), field_t.prefix) == added_prefixes.end()) {
						bfp = new BooleanFieldProcessor(field_t.prefix);
						srch.bfps.push_back(std::move(std::unique_ptr<BooleanFieldProcessor>(bfp)));
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, bfp) : queryparser.add_prefix(field_name, bfp);
						added_prefixes.push_back(field_t.prefix);
					}
					break;
			}

			// Concatenate with OR all the queries.
			if (first_time) {
				querystring = field;
				first_time = false;
			} else querystring += " OR " + field;
		}
	}

	if (offset != len) {
		throw Xapian::QueryParserError("Query '" + query + "' contains errors.\n" );
	}

	switch (first_time << 1 | first_timeR) {
		case 0:
			try {
				srch.query = Xapian::Query(Xapian::Query::OP_OR, queryparser.parse_query(querystring, flags), srch.query);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError &er) {
				LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
				reopen();
				srch.query = Xapian::Query(Xapian::Query::OP_OR, queryparser.parse_query(querystring, flags), srch.query);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			}
			break;
		case 1:
			try {
				srch.query = queryparser.parse_query(querystring, flags);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError &er) {
				LOG_ERR(this, "ERROR: %s\n", er.get_msg().c_str());
				reopen();
				queryparser.set_database(*db);
				srch.query = queryparser.parse_query(querystring, flags);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			}
			break;
		case 2:
			srch.suggested_query.push_back("");
			break;
		case 3:
			srch.query = Xapian::Query::MatchNothing;
			srch.suggested_query.push_back("");
			break;
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
			Xapian::Enquire renquire = get_enquire(query, Xapian::BAD_VALUENO, 1, NULL, NULL, NULL, NULL, NULL);
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
			prefixes.push_back(DOCUMENT_CUSTOM_TERM_PREFIX + Unserialise::type(*it));
		}
		for(it = similar->field.begin(); it != similar->field.end(); it++) {
			data_field_t field_t = get_data_field(*it);
			prefixes.push_back(field_t.prefix);
		}
		ExpandDeciderFilterPrefixes efp(prefixes);
		Xapian::ESet eset = enquire.get_eset(similar->n_eset, rset, &efp);

		if (is_fuzzy) {
			query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar->n_term));
		} else {
			query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar->n_term);
		}
		return;
	}
}


Xapian::Enquire
Database::get_enquire(Xapian::Query &query, const Xapian::valueno &collapse_key, const Xapian::valueno &collapse_max,
		Multi_MultiValueKeyMaker *sorter, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies,
		similar_t *nearest, similar_t *fuzzy, std::vector<std::string> *facets)
{
	std::string field;
	MultiValueCountMatchSpy *spy;
	Xapian::Enquire enquire(*db);

	if (nearest) get_similar(false, enquire, query, nearest);

	if (fuzzy) get_similar(true, enquire, query, fuzzy);

	enquire.set_query(query);

	if (sorter) enquire.set_sort_by_key_then_relevance(sorter, false);

	if (spies) {
		if (!facets->empty()) {
			std::vector<std::string>::const_iterator fit(facets->begin());
			for ( ; fit != facets->end(); fit++) {
				spy = new MultiValueCountMatchSpy(get_slot(*fit));
				spies->push_back(std::make_pair (*fit, std::move(std::unique_ptr<MultiValueCountMatchSpy>(spy))));
				enquire.add_matchspy(spy);
				LOG_ERR(this, "added spy de -%s-\n", (*fit).c_str());
			}
		}
	}

	if (collapse_key != Xapian::BAD_VALUENO) {
		enquire.set_collapse_key(collapse_key, collapse_max);
	}

	return enquire;
}


int
Database::get_mset(query_t &e, Xapian::MSet &mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> &spies, std::vector<std::string> &suggestions, int offset)
{
	std::unique_ptr<Multi_MultiValueKeyMaker> unique_sorter;
	bool decreasing;
	std::string field;
	unsigned int doccount = db->get_doccount();
	unsigned int check_at_least = std::max(std::min(doccount, e.check_at_least), (unsigned int)0);
	Xapian::valueno collapse_key;

	// Get the collapse key to use for queries.
	if (!e.collapse.empty()) {
		data_field_t field_t = get_slot_field(e.collapse);
		collapse_key = field_t.slot;
	} else {
		collapse_key = Xapian::BAD_VALUENO;
	}

	if (!e.sort.empty()) {
		Multi_MultiValueKeyMaker *sorter = new Multi_MultiValueKeyMaker();
		unique_sorter = std::unique_ptr<Multi_MultiValueKeyMaker>(sorter);
		std::vector<std::string>::const_iterator oit(e.sort.begin());
		for ( ; oit != e.sort.end(); oit++) {
			if (startswith(*oit, "-")) {
				decreasing = true;
				field.assign(*oit, 1, (*oit).size() - 1);
				// If the field has not been indexed as a value or it is a geospatial, it isn't used like Multi_MultiValuesKeyMaker
				data_field_t field_t = get_slot_field(field);
				if (field_t.type == NO_TYPE || field_t.type == GEO_TYPE) continue;
				sorter->add_value(field_t.slot, decreasing);
			} else if (startswith(*oit, "+")) {
				decreasing = false;
				field.assign(*oit, 1, (*oit).size() - 1);
				// If the field has not been indexed as a value or it is a geospatial, it isn't used like Multi_MultiValuesKeyMaker
				data_field_t field_t = get_slot_field(field);
				if (field_t.type == NO_TYPE || field_t.type == GEO_TYPE) continue;
				sorter->add_value(field_t.slot, decreasing);
			} else {
				decreasing = false;
				// If the field has not been indexed as a value or it is a geospatial, it isn't used like Multi_MultiValuesKeyMaker
				data_field_t field_t = get_slot_field(*oit);
				if (field_t.type == NO_TYPE || field_t.type == GEO_TYPE) continue;
				sorter->add_value(field_t.slot, decreasing);
			}
		}
	}

	for (int t = 3; t >= 0; --t) {
		try {
			search_t srch = search(e);
			Xapian::Enquire enquire = get_enquire(srch.query, collapse_key, e.collapse_max, unique_sorter.get(), &spies, e.is_nearest ? &e.nearest : NULL, e.is_fuzzy ? &e.fuzzy : NULL, &e.facets);
			suggestions = srch.suggested_query;
			mset = enquire.get_mset(e.offset + offset, e.limit - offset, check_at_least);
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
		} catch (const std::exception &er) {
			LOG_DATABASE_WRAP(this, "ERROR: %s\n", er.what());
			return 1;
		}

		return 0;
	}

	LOG_ERR(this, "ERROR: The search was not performed!\n");
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
		return value.empty() ? false : true;
	}
	return false;
}


bool
Database::set_metadata(const std::string &key, const std::string &value, bool commit)
{
	for (int t = 3; t >= 0; --t) {
		LOG_DATABASE_WRAP(this, "Metadata: %d\n", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db);
		try {
			wdb->set_metadata(key, value);
		} catch (const Xapian::Error &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		LOG_DATABASE_WRAP(this, "set_metadata was done\n");
		return (commit) ? _commit() : true;
	}

	LOG_ERR(this, "ERROR: set_metadata can not be done!\n");
	return false;
}


bool
Database::get_document(const Xapian::docid &did, Xapian::Document &doc)
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


unique_cJSON
Database::get_stats_database()
{
	unique_cJSON database(cJSON_CreateObject(), cJSON_Delete);
	unsigned int doccount = db->get_doccount();
	unsigned int lastdocid = db->get_lastdocid();
	cJSON_AddStringToObject(database.get(), "uuid", db->get_uuid().c_str());
	cJSON_AddNumberToObject(database.get(), "doc_count", doccount);
	cJSON_AddNumberToObject(database.get(), "last_id", lastdocid);
	cJSON_AddNumberToObject(database.get(), "doc_del", lastdocid - doccount);
	cJSON_AddNumberToObject(database.get(), "av_length", db->get_avlength());
	cJSON_AddNumberToObject(database.get(), "doc_len_lower", db->get_doclength_lower_bound());
	cJSON_AddNumberToObject(database.get(), "doc_len_upper", db->get_doclength_upper_bound());
	(db->has_positions()) ? cJSON_AddTrueToObject(database.get(), "has_positions") : cJSON_AddFalseToObject(database.get(), "has_positions");
	return std::move(database);
}


unique_cJSON
Database::get_stats_docs(const std::string &document_id)
{
	unique_cJSON document(cJSON_CreateObject(), cJSON_Delete);

	Xapian::Document doc;
	Xapian::QueryParser queryparser;

	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(document_id.at(0))) prefix += ":";
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	Xapian::Query query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + document_id);

	Xapian::Enquire enquire(*db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	Xapian::MSetIterator m = mset.begin();

	for (int t = 3; t >= 0; --t) {
		try {
			doc = db->get_document(*m);
			break;
		} catch (Xapian::InvalidArgumentError &err) {
			cJSON_AddStringToObject(document.get(), RESERVED_ID, document_id.c_str());
			cJSON_AddStringToObject(document.get(), "_error",  "Document not found");
			return std::move(document);
		} catch (Xapian::DocNotFoundError &err) {
			cJSON_AddStringToObject(document.get(), RESERVED_ID, document_id.c_str());
			cJSON_AddStringToObject(document.get(), "_error",  "Document not found");
			return std::move(document);
		} catch (const Xapian::Error &err) {
			reopen();
			m = mset.begin();
		}
	}

	cJSON_AddStringToObject(document.get(), RESERVED_ID, document_id.c_str());
	cJSON_AddItemToObject(document.get(), RESERVED_DATA, cJSON_Parse(doc.get_data().c_str()));
	cJSON_AddNumberToObject(document.get(), "number_terms", doc.termlist_count());
	Xapian::TermIterator it(doc.termlist_begin());
	std::string terms;
	for ( ; it != doc.termlist_end(); it++) {
		terms = terms + repr(*it) + " ";
	}
	cJSON_AddStringToObject(document.get(), RESERVED_TERMS, terms.c_str());
	cJSON_AddNumberToObject(document.get(), "number_values", doc.values_count());
	Xapian::ValueIterator iv(doc.values_begin());
	std::string values;
	for ( ; iv != doc.values_end(); iv++) {
		values = values + std::to_string(iv.get_valueno()) + ":" + repr(*iv) + " ";
	}
	cJSON_AddStringToObject(document.get(), RESERVED_VALUES, values.c_str());

	return std::move(document);
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


void
DatabasePool::finish()
{
	pthread_mutex_lock(&qmtx);

	finished = true;

	pthread_mutex_unlock(&qmtx);
}


void
DatabasePool::add_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue)
{
	pthread_mutex_lock(&qmtx);

	size_t hash = endpoint.hash();
	std::unordered_set<DatabaseQueue *> &queues_set = queues[hash];
	queues_set.insert(queue);

	pthread_mutex_unlock(&qmtx);
}


void
DatabasePool::drop_endpoint_queue(const Endpoint &endpoint, DatabaseQueue *queue)
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

	assert(*database == NULL);

	time_t now = time(0);
	Database *database_ = NULL;

	pthread_mutex_lock(&qmtx);

	if (!finished) {
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

#ifdef HAVE_REMOTE_PROTOCOL
	if (database_->local) {
		database_->checkout_revision = database_->db->get_revision_info();
	}
#endif
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
#ifdef HAVE_REMOTE_PROTOCOL
		if (database_->local && database_->mastery_level != -1) {
			std::string new_revision = database_->db->get_revision_info();
			if (new_revision != database_->checkout_revision) {
				Endpoint endpoint = *database_->endpoints.begin();
				endpoint.mastery_level = database_->mastery_level;
				updated_databases.push(endpoint);
			}
		}
#endif
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


void DatabasePool::init_ref(const Endpoints &endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it(endpoints.begin());
	if (ref_database) {
		for ( ; endp_it != endpoints.end(); endp_it++) {
			std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
			Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
			if (p == ref_database->db->postlist_end(unique_id)) {
				doc.add_boolean_term(unique_id);
				doc.add_term(prefixed(endp_it->node_name, prefix_rf_node));
				doc.add_value(0, "0");
				ref_database->replace(unique_id, doc, true);
			} else {
				LOG(this,"The document already exists nothing to do\n");
			}
		}
	}
}


void DatabasePool::inc_ref(const Endpoints &endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it(endpoints.begin());
	for ( ; endp_it != endpoints.end(); endp_it++) {
		std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
		if (p == ref_database->db->postlist_end(unique_id)) {
			// QUESTION: Document not found - should add?
			// QUESTION: This case could happen?
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name, prefix_rf_node));
			doc.add_value(0, "0");
			ref_database->replace(unique_id, doc, true);
		} else {
			// Document found - reference increased
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name, prefix_rf_node));
			int nref = strtoint(doc.get_value(0));
			doc.add_value(0, std::to_string(nref + 1));
			ref_database->replace(unique_id, doc, true);
		}
	}
}


void DatabasePool::dec_ref(const Endpoints &endpoints)
{
	Xapian::Document doc;
	endpoints_set_t::iterator endp_it(endpoints.begin());
	for ( ; endp_it != endpoints.end(); endp_it++) {
		std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
		if (p != ref_database->db->postlist_end(unique_id)) {
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			doc.add_term(prefixed(endp_it->node_name, prefix_rf_node));
			int nref = strtoint(doc.get_value(0)) - 1;
			doc.add_value(0, std::to_string(nref));
			ref_database->replace(unique_id, doc, true);
			if (nref == 0) {
				// qmtx need a lock
				pthread_cond_wait(&checkin_cond, &qmtx);
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
	std::unordered_set<DatabaseQueue *>::const_iterator it_qs(queues_set.cbegin());

	bool switched = true;
	for ( ; it_qs != queues_set.cend(); it_qs++) {
		queue = *it_qs;
		queue->is_switch_db = true;
		if (queue->count != queue->size()) {
			switched = false;
			break;
		}
	}

	if (switched) {
		for (it_qs = queues_set.cbegin(); it_qs != queues_set.cend(); it_qs++) {
			queue = *it_qs;
			while (!queue->empty()) {
				if (queue->pop(database)) {
					delete database;
				}
			}
		}

		move_files(endpoint.path + "/.tmp", endpoint.path);

		for(it_qs = queues_set.cbegin(); it_qs != queues_set.cend(); it_qs++) {
			queue = *it_qs;
			queue->is_switch_db = false;

			pthread_cond_broadcast(&queue->switch_cond);
		}
	} else {
		LOG(this, "Inside switch_db not queue->count == queue->size()\n");
	}

	pthread_mutex_unlock(&qmtx);

	return switched;
}


bool
ExpandDeciderFilterPrefixes::operator()(const std::string &term) const
{
	std::vector<std::string>::const_iterator i(prefixes.cbegin());
	for ( ; i != prefixes.cend(); i++) {
		if (startswith(term, *i)) return true;
	}

	return prefixes.empty();
}