/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "xapiand.h"

#include <string>                  // for string
#include <string_view>             // for std::string_view
#include <vector>                  // for vector

#include <xapian.h>                // for valueno

#include "msgpack.h"               // for object
#include "rapidjson/document.h"    // for Document
#include "sortable_serialise.h"    // for sortable_serialise
#include "hashes.hh"               // for xxh64


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
constexpr const char RESERVED_STRING_DETECTION[]    = "_string_detection";
constexpr const char RESERVED_TEXT_DETECTION[]      = "_text_detection";
constexpr const char RESERVED_TERM_DETECTION[]      = "_term_detection";
constexpr const char RESERVED_UUID_DETECTION[]      = "_uuid_detection";
// Reserved words used only in the root of the schema.
constexpr const char RESERVED_VALUES[]              = "_values";
constexpr const char RESERVED_TERMS[]               = "_terms";
constexpr const char RESERVED_DATA[]                = "_data";
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
constexpr const char RESERVED_TERM[]                = "_term";
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
constexpr int DB_RETRIES           = 3;   // Number of tries to do an operation on a Xapian::Database or Document

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

constexpr char DATABASE_DATA_HEADER_MAGIC        = 0x11;
constexpr char DATABASE_DATA_HEADER_MAGIC_STORED = 0x12;
constexpr char DATABASE_DATA_FOOTER_MAGIC        = 0x15;

// Default prefixes
constexpr const char DOCUMENT_ID_TERM_PREFIX[]           = "Q";
constexpr const char DOCUMENT_CONTENT_TYPE_TERM_PREFIX[] = "C";

constexpr const char DOCUMENT_DB_MASTER[] = "M";
constexpr const char DOCUMENT_DB_SLAVE[]  = "S";

constexpr const char ANY_CONTENT_TYPE[]               = "*/*";
constexpr const char HTML_CONTENT_TYPE[]              = "text/html";
constexpr const char TEXT_CONTENT_TYPE[]              = "text/plain";
constexpr const char JSON_CONTENT_TYPE[]              = "application/json";
constexpr const char MSGPACK_CONTENT_TYPE[]           = "application/msgpack";
constexpr const char X_MSGPACK_CONTENT_TYPE[]         = "application/x-msgpack";
constexpr const char FORM_URLENCODED_CONTENT_TYPE[]   = "application/www-form-urlencoded";
constexpr const char X_FORM_URLENCODED_CONTENT_TYPE[] = "application/x-www-form-urlencoded";


struct ct_type_t {
	std::string first;
	std::string second;

	ct_type_t() = default;

	template<typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value>>
	ct_type_t(S&& first_, S&& second_)
		: first(std::forward<S>(first_)),
		  second(std::forward<S>(second_)) { }

	explicit ct_type_t(std::string_view ct_type_str) {
		const auto found = ct_type_str.rfind('/');
		if (found != std::string::npos) {
			first = ct_type_str.substr(0, found);
			second = ct_type_str.substr(found + 1);
		}
	}


	bool operator==(const ct_type_t& other) const noexcept {
		return first == other.first && second == other.second;
	}

	bool operator!=(const ct_type_t& other) const {
		return !operator==(other);
	}

	void clear() noexcept {
		first.clear();
		second.clear();
	}

	bool empty() const noexcept {
		return first.empty() && second.empty();
	}

	std::string to_string() const {
		std::string res;
		res.reserve(first.length() + second.length() + 1);
		res.assign(first).push_back('/');
		res.append(second);
		return res;
	}
};


static const ct_type_t no_type{};
static const ct_type_t any_type(ANY_CONTENT_TYPE);
static const ct_type_t html_type(HTML_CONTENT_TYPE);
static const ct_type_t text_type(TEXT_CONTENT_TYPE);
static const ct_type_t json_type(JSON_CONTENT_TYPE);
static const ct_type_t msgpack_type(MSGPACK_CONTENT_TYPE);
static const ct_type_t x_msgpack_type(X_MSGPACK_CONTENT_TYPE);
static const std::vector<ct_type_t> msgpack_serializers({ json_type, msgpack_type, x_msgpack_type, html_type, text_type });

constexpr int STORED_BLOB_CONTENT_TYPE  = 0;
constexpr int STORED_BLOB_DATA          = 1;

constexpr int DB_OPEN         = 0x0000; // Opens a database
constexpr int DB_WRITABLE     = 0x0001; // Opens as writable
constexpr int DB_SPAWN        = 0x0002; // Automatically creates the database if it doesn't exist
constexpr int DB_PERSISTENT   = 0x0004; // Always try keeping the database in the database pool
constexpr int DB_INIT_REF     = 0x0008; // Initializes the writable index in the database .refs
constexpr int DB_VOLATILE     = 0x0010; // Always drop the database from the database pool as soon as possible
constexpr int DB_REPLICATION  = 0x0020; // Use conditional pop in the queue, only pop when replication is done
constexpr int DB_NOWAL        = 0x0040; // Disable open wal file
constexpr int DB_NOSTORAGE    = 0x0080; // Disable separate data storage file for the database
constexpr int DB_COMMIT       = 0x0101; // Commits database when needed


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
	bool volatile_;
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

	// Only used when the sort type is string.
	std::string metric;
	bool icase;

	query_field_t()
		: offset(0), limit(10), check_at_least(0), volatile_(false), spelling(true), synonyms(false), commit(false),
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
long long read_mastery(std::string_view dir, bool force);
void json_load(rapidjson::Document& doc, std::string_view str);
rapidjson::Document to_json(std::string_view str);
std::string msgpack_to_html(const msgpack::object& o);
std::string msgpack_map_value_to_html(const msgpack::object& o);
std::string msgpack_to_html_error(const msgpack::object& o);


std::string join_data(bool stored, std::string_view stored_locator, std::string_view obj, std::string_view blob);
std::pair<bool, std::string_view> split_data_store(std::string_view data);
std::string_view split_data_obj(std::string_view data);
std::string_view split_data_blob(std::string_view data);
std::string get_data_content_type(std::string_view data);
void split_path_id(std::string_view path_id, std::string_view& path, std::string_view& id);
#ifdef XAPIAND_DATA_STORAGE
std::tuple<ssize_t, size_t, size_t, std::string> storage_unserialise_locator(std::string_view store);
std::string storage_serialise_locator(ssize_t volume, size_t offset, size_t size, std::string_view content_type);
#endif /* XAPIAND_DATA_STORAGE */
