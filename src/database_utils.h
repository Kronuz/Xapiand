/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "fields.h"
#include "geospatialrange.h"
#include "multivalue/range.h"
#include "msgpack.h"
#include "rapidjson/document.h"

#include <regex>
#include <vector>
#include <xapian.h>

#define RESERVED_ENDPOINT    "_endpoint"
#define RESERVED_RANK        "_rank"
#define RESERVED_WEIGHT      "_weight"
#define RESERVED_PERCENT     "_percent"
#define RESERVED_POSITION    "_position"
#define RESERVED_LANGUAGE    "_language"
#define RESERVED_SPELLING    "_spelling"
#define RESERVED_POSITIONS   "_positions"
#define RESERVED_TEXTS       "_texts"
#define RESERVED_VALUES      "_values"
#define RESERVED_TERMS       "_terms"
#define RESERVED_DATA        "_data"
#define RESERVED_ACCURACY    "_accuracy"
#define RESERVED_ACC_PREFIX  "_accuracy_prefix"
#define RESERVED_STORE       "_store"
#define RESERVED_TYPE        "_type"
#define RESERVED_ANALYZER    "_analyzer"
#define RESERVED_DYNAMIC     "_dynamic"
#define RESERVED_D_DETECTION "_date_detection"
#define RESERVED_N_DETECTION "_numeric_detection"
#define RESERVED_G_DETECTION "_geo_detection"
#define RESERVED_B_DETECTION "_bool_detection"
#define RESERVED_S_DETECTION "_string_detection"
#define RESERVED_BOOL_TERM   "_bool_term"
#define RESERVED_VALUE       "_value"
#define RESERVED_NAME        "_name"
#define RESERVED_SLOT        "_slot"
#define RESERVED_INDEX       "_index"
#define RESERVED_PREFIX      "_prefix"
#define RESERVED_ID          "_id"
#define RESERVED_SCHEMA      "_schema"
#define RESERVED_VERSION     "_version"

#define DB_OFFSPRING_UNION "__"
#define DB_LANGUAGES       "da nl en lovins porter fi fr de hu it nb nn no pt ro ru es sv tr"
#define DB_VERSION_SCHEMA  2.0

#define DB_SLOT_ID     0 // Slot ID document
#define DB_SLOT_OFFSET 1 // Slot offset for data
#define DB_SLOT_TYPE   2 // Slot type data
#define DB_SLOT_LENGTH 3 // Slot length data
#define DB_SLOT_CREF   4 // Slot that saves the references counter

#define DEFAULT_OFFSET "0" /* Replace for the real offset */

// Default prefixes
#define DOCUMENT_ID_TERM_PREFIX     "Q"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

#define ANY_TYPE             "*/*"
#define JSON_TYPE            "application/json"
#define FORM_URLENCODED_TYPE "application/x-www-form-urlencoded"
#define MSGPACK_TYPE         "application/x-msgpack"
#define HTML_TYPE            "text/html"
#define TEXT_TYPE            "text/plain"

constexpr int DB_OPEN         = 0x00; // Opens a database
constexpr int DB_WRITABLE     = 0x01; // Opens as writable
constexpr int DB_SPAWN        = 0x02; // Automatically creates the database if it doesn't exist
constexpr int DB_PERSISTENT   = 0x04; // Always try keeping the database in the database pool
constexpr int DB_INIT_REF     = 0x08; // Initializes the writable index in the database .refs
constexpr int DB_VOLATILE     = 0x10; // Always drop the database from the database pool as soon as possible
constexpr int DB_REPLICATION  = 0x20; // Use conditional pop in the queue, only pop when replication is done
constexpr int DB_NOWAL        = 0x40; // Disable open wal file
constexpr int DB_DATA_STORAGE = 0x80; // Enable separate data storage file for the database


struct data_field_t {
	unsigned slot;
	std::string prefix;
	char type;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	bool bool_term;
};


struct similar_field_t {
	unsigned n_rset;
	unsigned n_eset;
	unsigned n_term; // If the number of subqueries is less than this threshold, OP_ELITE_SET behaves identically to OP_OR
	std::vector <std::string> field;
	std::vector <std::string> type;

	similar_field_t()
		: n_rset(5), n_eset(32), n_term(10) { }
};


struct query_field_t {
	unsigned offset;
	unsigned limit;
	unsigned check_at_least;
	bool spelling;
	bool synonyms;
	bool commit;
	bool unique_doc;
	bool is_fuzzy;
	bool is_nearest;
	std::string collapse;
	unsigned collapse_max;
	std::vector <std::string> language;
	std::vector <std::string> query;
	std::vector <std::string> partial;
	std::vector <std::string> terms;
	std::vector <std::string> sort;
	std::vector <std::string> facets;
	similar_field_t fuzzy;
	similar_field_t nearest;
	std::string time;

	// Only used when the sort type is string.
	std::string metric;
	bool icase;

	query_field_t()
		: offset(0), limit(10), check_at_least(0), spelling(true), synonyms(false), commit(false),
		  unique_doc(false), is_fuzzy(false), is_nearest(false), collapse_max(1), language({ "en" }), icase(true) { }

};


enum class MIMEType {
	APPLICATION_JSON,
	APPLICATION_XWWW_FORM_URLENCODED,
	APPLICATION_X_MSGPACK,
	UNKNOW
};


long long read_mastery(const std::string& dir, bool force);
// All the field names that start or end with '_', or contains DB_OFFSPRING_UNION are not valid field names.
bool is_valid(const std::string& word);
bool is_language(const std::string& language);
bool set_types(const std::string& type, std::vector<unsigned>& sep_types);
std::string str_type(const std::vector<unsigned>& sep_types);
void clean_reserved(MsgPack& document);
MIMEType get_mimetype(const std::string& type);
void json_load(rapidjson::Document& doc, const std::string& str);
rapidjson::Document to_json(const std::string& str);
void set_data(Xapian::Document& doc, const std::string& obj_data_str, const std::string& blob_str);
MsgPack get_MsgPack(const Xapian::Document& doc);
std::string get_blob(const Xapian::Document& doc);
std::string to_query_string(std::string str);
std::string msgpack_to_html(const msgpack::object& o);
std::string msgpack_map_value_to_html(const msgpack::object& o);
std::string msgpack_to_html_error(const msgpack::object& o);
