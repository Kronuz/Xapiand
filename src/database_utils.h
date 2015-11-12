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

#pragma once

#include "serialise.h"
#include "cJSON.h"

#include <regex>
#include <vector>
#include <xapian.h>

#define RESERVED_WEIGHT "_weight"
#define RESERVED_POSITION "_position"
#define RESERVED_LANGUAGE "_language"
#define RESERVED_SPELLING "_spelling"
#define RESERVED_POSITIONS "_positions"
#define RESERVED_TEXTS "_texts"
#define RESERVED_VALUES "_values"
#define RESERVED_TERMS "_terms"
#define RESERVED_DATA "_data"
#define RESERVED_ACCURACY "_accuracy"
#define RESERVED_ACC_PREFIX "_acc_prefix"
#define RESERVED_STORE "_store"
#define RESERVED_TYPE "_type"
#define RESERVED_ANALYZER "_analyzer"
#define RESERVED_DYNAMIC "_dynamic"
#define RESERVED_D_DETECTION "_date_detection"
#define RESERVED_N_DETECTION "_numeric_detection"
#define RESERVED_G_DETECTION "_geo_detection"
#define RESERVED_B_DETECTION "_bool_detection"
#define RESERVED_S_DETECTION "_string_detection"
#define RESERVED_BOOL_TERM "_bool_term"
#define RESERVED_VALUE "_value"
#define RESERVED_NAME "_name"
#define RESERVED_SLOT "_slot"
#define RESERVED_INDEX "_index"
#define RESERVED_PREFIX "_prefix"
#define RESERVED_ID "_id"
#define RESERVED_SCHEMA "_schema"
#define RESERVED_VERSION "_version"

#define DB_OFFSPRING_UNION "__"
#define DB_LANGUAGES "da nl en lovins porter fi fr de hu it nb nn no pt ro ru es sv tr"
#define DB_VERSION_SCHEMA 2.0

// Default prefixes
#define DOCUMENT_ID_TERM_PREFIX "Q"
#define DOCUMENT_CUSTOM_TERM_PREFIX "X"

enum enum_time     { DB_SECOND2INT, DB_MINUTE2INT,  DB_HOUR2INT, DB_DAY2INT, DB_MONTH2INT, DB_YEAR2INT };
enum enum_index    { ALL, TERM, VALUE };

const std::vector<std::string> str_time({ "second", "minute", "hour", "day", "month", "year" });
const std::vector<std::string> str_analizer({ "STEM_NONE", "STEM_SOME", "STEM_ALL", "STEM_ALL_Z" });
const std::vector<std::string> str_index({ "ALL", "TERM", "VALUE" });

extern std::regex find_types_re;

typedef struct specifications_s {
	std::vector<int> position;
	std::vector<int> weight;
	std::vector<std::string> language;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	std::vector<int> analyzer;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	unsigned int slot;
	char sep_types[3];
	std::string prefix;
	int index;
	bool store;
	bool dynamic;
	bool date_detection;
	bool numeric_detection;
	bool geo_detection;
	bool bool_detection;
	bool string_detection;
	bool bool_term;
} specifications_t;

const std::vector<double> def_accuracy_geo  { 1, 0.2, 0, 5, 10, 15, 20, 25 }; // { partials, error, accuracy levels }
const std::vector<double> def_accuracy_num  { 100, 1000, 10000, 100000 };
const std::vector<double> def_acc_date      { DB_HOUR2INT, DB_DAY2INT, DB_MONTH2INT, DB_YEAR2INT };

const specifications_t default_spc = {
	{ -1 },									// position
	{ 1 },									// weight
	{ "en" },								// language
	{ false },								// spelling
	{ false },								// positions
	{ Xapian::TermGenerator::STEM_SOME },	// analyzer
	std::vector<double>(),					// accuracy
	std::vector<std::string>(),				// accuracy prefix
	0,										// slot
	{ NO_TYPE, NO_TYPE, NO_TYPE },			// sep_types
	"",										// prefix
	ALL,									// index
	true,									// store
	true,									// dynamic
	true,									// date detection
	true,									// numeric detection
	true,									// geo detection
	true,									// bool detection
	true,									// string detection
	false									// bool term
};


typedef struct data_field_s {
	unsigned int slot;
	std::string prefix;
	char type;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	bool bool_term;
} data_field_t;


struct similar_field {
	unsigned int n_rset;
	unsigned int n_eset;
	unsigned int n_term; //If the number of subqueries is less than this threshold, OP_ELITE_SET behaves identically to OP_OR
	std::vector <std::string> field;
	std::vector <std::string> type;

	similar_field(): n_rset(5), n_eset(32), n_term(10) {}
};

struct query_field {
	unsigned int offset;
	unsigned int limit;
	unsigned int check_at_least;
	bool spelling;
	bool synonyms;
	bool pretty;
	bool commit;
	bool server;
	bool database;
	std::string document;
	bool unique_doc;
	bool is_fuzzy;
	bool is_nearest;
	std::string stats;
	std::string collapse;
	unsigned int collapse_max;
	std::vector <std::string> language;
	std::vector <std::string> query;
	std::vector <std::string> partial;
	std::vector <std::string> terms;
	std::vector <std::string> sort;
	std::vector <std::string> facets;
	similar_field fuzzy;
	similar_field nearest;

	query_field()
	: offset(0),
	  limit(10),
	  check_at_least(0),
	  spelling(true),
	  synonyms(false),
	  pretty(false),
	  commit(false),
	  server(false),
	  database(false),
	  document(""),
	  unique_doc(false),
	  is_fuzzy(false),
	  is_nearest(false),
	  stats(""),
	  collapse(""),
	  collapse_max(1),
	  fuzzy(),
	  nearest(){}

};

long long read_mastery(const std::string &dir, bool force);
// All the field that start with '_' are considered reserved word.
bool is_reserved(const std::string &word);
bool is_language(const std::string &language);
char get_type(cJSON *field, specifications_t &spc);
bool set_types(const std::string &type, char sep_types[]);
std::string str_type(const char sep_types[]);
std::vector<std::string> split_fields(const std::string &field_name);
void clean_reserved(cJSON *root);
void clean_reserved(cJSON *root, cJSON *item);
// For updating the specifications, first we check whether the document contains reserved words that can be
// modified, otherwise we check in the schema and if reserved words do not exist, we take the values
// of the parent if they are heritable.
void update_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema, bool root = false);
// All the reserved word found in a new field are added in schema.
void insert_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema, bool root = false);
// For updating required data in the schema. When a new field is inserted in the scheme it is
// necessary verify that all the required reserved words are defined, otherwise they are defined.
void update_required_data(specifications_t &spc, const std::string &name, cJSON *schema);
// It inserts fields that are not hereditary and if _type has been fixed  these can not be modified.
void insert_inheritable_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema);
//Pass the struct specifications_t to string.
std::string specificationstostr(const specifications_t &spc);
// Accuracy, type, analyzer and index are saved like numbers into schema, this function transforms
// this reserved word to their readable form.
void readable_schema(cJSON *schema);
void readable_field(cJSON *field);
