/*
 * Copyright (C) 2015-2019 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <string>                  // for string
#include "string_view.hh"          // for std::string_view
#include <vector>                  // for vector

#include <xapian.h>                // for valueno

#include "msgpack.h"               // for object
#include "rapidjson/document.h"    // for Document
#include "sortable_serialise.h"    // for sortable_serialise
#include "hashes.hh"               // for xxh64
#include "database_data.h"         // for ct_type_t, Data


// Reserved field names.
constexpr const char ID_FIELD_NAME[]                = "_id";
constexpr const char UUID_FIELD_NAME[]              = "<uuid_field>";

// Reserved words used in schema.
constexpr const char RESERVED_WEIGHT[]              = "_weight";
constexpr const char RESERVED_POSITION[]            = "_position";
constexpr const char RESERVED_SPELLING[]            = "_spelling";
constexpr const char RESERVED_POSITIONS[]           = "_positions";
constexpr const char RESERVED_LANGUAGE[]            = "_language";
constexpr const char RESERVED_ACCURACY[]            = "_accuracy";
constexpr const char RESERVED_ACC_PREFIX[]          = "_accuracy_prefix";
constexpr const char RESERVED_STORE[]               = "_store";
constexpr const char RESERVED_TYPE[]                = "_type";
constexpr const char RESERVED_DYNAMIC[]             = "_dynamic";
constexpr const char RESERVED_STRICT[]              = "_strict";
constexpr const char RESERVED_BOOL_TERM[]           = "_bool_term";
constexpr const char RESERVED_VALUE[]               = "_value";
constexpr const char RESERVED_SLOT[]                = "_slot";
constexpr const char RESERVED_INDEX[]               = "_index";
constexpr const char RESERVED_PREFIX[]              = "_prefix";
constexpr const char RESERVED_CHAI[]                = "_chai";
constexpr const char RESERVED_ECMA[]                = "_ecma";
constexpr const char RESERVED_SCRIPT[]              = "_script";
constexpr const char RESERVED_NAME[]                = "_name";
constexpr const char RESERVED_BODY[]                = "_body";
constexpr const char RESERVED_HASH[]                = "_hash";
constexpr const char RESERVED_BODY_HASH[]           = "_body_hash";
constexpr const char RESERVED_RECURSE[]             = "_recurse";
constexpr const char RESERVED_NAMESPACE[]           = "_namespace";
constexpr const char RESERVED_PARTIAL_PATHS[]       = "_partial_paths";
constexpr const char RESERVED_INDEX_UUID_FIELD[]    = "_index_uuid_field";
constexpr const char RESERVED_SCHEMA[]              = "_schema";
constexpr const char RESERVED_ENDPOINT[]            = "_endpoint";
// Reserved words for detecting types.
constexpr const char RESERVED_DATE_DETECTION[]      = "_date_detection";
constexpr const char RESERVED_TIME_DETECTION[]      = "_time_detection";
constexpr const char RESERVED_TIMEDELTA_DETECTION[] = "_timedelta_detection";
constexpr const char RESERVED_NUMERIC_DETECTION[]   = "_numeric_detection";
constexpr const char RESERVED_GEO_DETECTION[]       = "_geo_detection";
constexpr const char RESERVED_BOOL_DETECTION[]      = "_bool_detection";
constexpr const char RESERVED_TEXT_DETECTION[]      = "_text_detection";
constexpr const char RESERVED_TERM_DETECTION[]      = "_term_detection";
constexpr const char RESERVED_UUID_DETECTION[]      = "_uuid_detection";
// Reserved words used only in the root of the schema.
constexpr const char RESERVED_VALUES[]              = "_values";
constexpr const char RESERVED_TERMS[]               = "_terms";
constexpr const char RESERVED_DATA[]                = "_data";
constexpr const char RESERVED_BLOB[]                = "_blob";
constexpr const char RESERVED_CONTENT_TYPE[]        = "_content_type";
// Reserved words used in schema only for TEXT fields.
constexpr const char RESERVED_STOP_STRATEGY[]       = "_stop_strategy";
constexpr const char RESERVED_STEM_STRATEGY[]       = "_stem_strategy";
constexpr const char RESERVED_STEM_LANGUAGE[]       = "_stem_language";
// Reserved words used in schema only for GEO fields.
constexpr const char RESERVED_PARTIALS[]            = "_partials";
constexpr const char RESERVED_ERROR[]               = "_error";
// Reserved words used for doing explicit cast convertions
constexpr const char RESERVED_FLOAT[]               = "_float";
constexpr const char RESERVED_POSITIVE[]            = "_positive";
constexpr const char RESERVED_INTEGER[]             = "_integer";
constexpr const char RESERVED_BOOLEAN[]             = "_boolean";
constexpr const char RESERVED_TERM[]                = "_term";  // FIXME: remove legacy term
constexpr const char RESERVED_KEYWORD[]             = "_keyword";
constexpr const char RESERVED_TEXT[]                = "_text";
constexpr const char RESERVED_STRING[]              = "_string";
constexpr const char RESERVED_DATE[]                = "_date";
constexpr const char RESERVED_TIME[]                = "_time";
constexpr const char RESERVED_TIMEDELTA[]           = "_timedelta";
constexpr const char RESERVED_UUID[]                = "_uuid";
constexpr const char RESERVED_EWKT[]                = "_ewkt";
constexpr const char RESERVED_POINT[]               = "_point";
constexpr const char RESERVED_CIRCLE[]              = "_circle";
constexpr const char RESERVED_CONVEX[]              = "_convex";
constexpr const char RESERVED_POLYGON[]             = "_polygon";
constexpr const char RESERVED_CHULL[]               = "_chull";
constexpr const char RESERVED_MULTIPOINT[]          = "_multipoint";
constexpr const char RESERVED_MULTICIRCLE[]         = "_multicircle";
constexpr const char RESERVED_MULTICONVEX[]         = "_multiconvex";
constexpr const char RESERVED_MULTIPOLYGON[]        = "_multipolygon";
constexpr const char RESERVED_MULTICHULL[]          = "_multichull";
constexpr const char RESERVED_GEO_COLLECTION[]      = "_geometrycollection";
constexpr const char RESERVED_GEO_INTERSECTION[]    = "_geometryintersection";

constexpr const char SCHEMA_FIELD_NAME[]            = "schema";
constexpr const char VERSION_FIELD_NAME[]           = "version";

constexpr char DB_OFFSPRING_UNION  = '.';
constexpr double DB_VERSION_SCHEMA = 2.0;

constexpr Xapian::valueno DB_SLOT_RESERVED     = 20; // Reserved slots by special data
constexpr Xapian::valueno DB_SLOT_ID           = 0;  // Slot for document ID
constexpr Xapian::valueno DB_SLOT_CONTENT_TYPE = 1;  // Slot for data content type
constexpr Xapian::valueno DB_SLOT_ROOT         = 9;  // Slot for root
constexpr Xapian::valueno DB_SLOT_NUMERIC      = 10; // Slot for saving global float/integer/positive values
constexpr Xapian::valueno DB_SLOT_DATE         = 11; // Slot for saving global date values
constexpr Xapian::valueno DB_SLOT_GEO          = 12; // Slot for saving global geo values
constexpr Xapian::valueno DB_SLOT_STRING       = 13; // Slot for saving global string/text values.
constexpr Xapian::valueno DB_SLOT_BOOLEAN      = 14; // Slot for saving global boolean values.
constexpr Xapian::valueno DB_SLOT_UUID         = 15; // Slot for saving global uuid values.
constexpr Xapian::valueno DB_SLOT_TIME         = 16; // Slot for saving global time values.
constexpr Xapian::valueno DB_SLOT_TIMEDELTA    = 17; // Slot for saving global timedelta values.


// Default prefixes
constexpr const char DOCUMENT_ID_TERM_PREFIX[]           = "Q";
constexpr const char DOCUMENT_CONTENT_TYPE_TERM_PREFIX[] = "C";

constexpr const char DOCUMENT_DB_MASTER[] = "M";
constexpr const char DOCUMENT_DB_SLAVE[]  = "S";

enum class FieldType : uint8_t;


struct similar_field_t {
	unsigned n_rset;
	unsigned n_eset;
	unsigned n_term; // If the number of subqueries is less than this threshold, OP_ELITE_SET behaves identically to OP_OR
	std::vector<std::string> field;
	std::vector<std::string> type;

	similar_field_t()
		: n_rset(5), n_eset(32), n_term(10) { }
};


struct query_field_t {
	unsigned offset;
	unsigned limit;
	unsigned check_at_least;
	bool as_volatile;
	bool spelling;
	bool synonyms;
	bool commit;
	bool unique_doc;
	bool is_fuzzy;
	bool is_nearest;
	std::string collapse;
	unsigned collapse_max;
	std::vector<std::string> query;
	std::vector<std::string> sort;
	similar_field_t fuzzy;
	similar_field_t nearest;
	std::string time;
	std::string period;
	std::string selector;

	// Only used when the sort type is string.
	std::string metric;
	bool icase;

	query_field_t()
		: offset(0), limit(10), check_at_least(0), as_volatile(false), spelling(true), synonyms(false), commit(false),
		  unique_doc(false), is_fuzzy(false), is_nearest(false), collapse_max(1), icase(false) { }
};


// All non-empty field names not starting with underscore are valid.
inline bool is_valid(std::string_view field_name) {
	return !field_name.empty() && field_name.at(0) != '_';
}


inline std::string get_hashed(std::string_view name) {
	return sortable_serialise(xxh64::hash(name));
}


std::string prefixed(std::string_view term, std::string_view field_prefix, char field_type);
Xapian::valueno get_slot(std::string_view field_prefix, char field_type);
std::string get_prefix(unsigned long long field_number);
std::string get_prefix(std::string_view field_name);
std::string normalize_uuid(std::string_view uuid);
std::string normalize_uuid(const std::string& uuid);
MsgPack normalize_uuid(const MsgPack& uuid);
int read_uuid(std::string_view dir, std::array<unsigned char, 16>& uuid);
void json_load(rapidjson::Document& doc, std::string_view str);
rapidjson::Document to_json(std::string_view str);
std::string msgpack_to_html(const msgpack::object& o);
std::string msgpack_map_value_to_html(const msgpack::object& o);
std::string msgpack_to_html_error(const msgpack::object& o);

void split_path_id(std::string_view path_id, std::string_view& path, std::string_view& id);
