/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

#include <cctype>                   // for isxdigit
#include <stddef.h>                 // for size_t
#include <string>                   // for string
#include <sys/types.h>              // for uint64_t, int64_t, uint32_t, uint8_t
#include <tuple>                    // for tuple
#include <unordered_map>            // for unordered_map
#include <utility>                  // for pair
#include <vector>                   // for vector

#include "datetime.h"               // for tm_t (ptr only), timestamp
#include "geospatial/cartesian.h"   // for Cartesian
#include "geospatial/htm.h"         // for range_t
#include "hash/endian.h"            // for __BYTE_ORDER, __BIG_ENDIAN, __LITTLE...
#include "length.h"                 // for serialise_length, unserialise_length
#include "msgpack.h"                // for MsgPack
#include "sortable_serialise.h"     // for sortable_serialise, sortable_unseria...
#include "hashes.hh"                // for fnv1a32

constexpr const char FLOAT_STR[]     = "float";
constexpr const char INTEGER_STR[]   = "integer";
constexpr const char POSITIVE_STR[]  = "positive";
constexpr const char TERM_STR[]      = "term";
constexpr const char TEXT_STR[]      = "text";
constexpr const char STRING_STR[]    = "string";
constexpr const char DATE_STR[]      = "date";
constexpr const char TIME_STR[]      = "time";
constexpr const char TIMEDELTA_STR[] = "timedelta";
constexpr const char GEO_STR[]       = "geospatial";
constexpr const char BOOLEAN_STR[]   = "boolean";
constexpr const char UUID_STR[]      = "uuid";
constexpr const char SCRIPT_STR[]    = "script";
constexpr const char ARRAY_STR[]     = "array";
constexpr const char OBJECT_STR[]    = "object";
constexpr const char FOREIGN_STR[]   = "foreign";
constexpr const char EMPTY_STR[]     = "empty";


constexpr char SERIALISED_FALSE      = 'f';
constexpr char SERIALISED_TRUE       = 't';


constexpr uint8_t SERIALISED_LENGTH_CARTESIAN = 12;
constexpr uint8_t SERIALISED_LENGTH_RANGE     = 2 * HTM_BYTES_ID;


constexpr uint32_t DOUBLE2INT = 1000000000;
constexpr uint32_t MAXDOU2INT = 2000000000;

enum class UUIDRepr : uint32_t {
	simple = fnv1a32::hash("simple"),
#ifdef XAPIAND_UUID_GUID
	guid = fnv1a32::hash("guid"),
#endif
#ifdef XAPIAND_UUID_URN
	urn = fnv1a32::hash("urn"),
#endif
#ifdef XAPIAND_UUID_ENCODED
	encoded = fnv1a32::hash("encoded"),
#endif
};

class CartesianList;
class RangeList;
enum class FieldType : uint8_t;
struct required_spc_t;


namespace Serialise {
	inline static bool isText(string_view field_value, bool bool_term) noexcept {
		return !bool_term && field_value.find(' ') != std::string::npos;
	}

	// Returns if field_value is UUID.
	bool possiblyUUID(string_view field_value) noexcept;
	bool isUUID(string_view field_value) noexcept;


	/*
	 * Serialise field_value according to field_spc.
	 */

	std::string MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string object(const required_spc_t& field_spc, const class MsgPack& o);

	std::string serialise(const required_spc_t& field_spc, const class MsgPack& field_value);
	std::string serialise(const required_spc_t& field_spc, string_view field_value);
	inline std::string serialise(const required_spc_t& field_spc, const std::string& field_value) {
		return serialise(field_spc, string_view(field_value));
	}

	std::string string(const required_spc_t& field_spc, string_view field_value);

	std::string date(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string time(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string timedelta(const required_spc_t& field_spc, const class MsgPack& field_value);


	/*
	 * Serialise field_value according to field_type.
	 */

	std::string _float(FieldType field_type, double field_value);
	std::string integer(FieldType field_type, int64_t field_value);
	std::string positive(FieldType field_type, uint64_t field_value);
	std::string boolean(FieldType field_type, bool field_value);
	std::string geospatial(FieldType field_type, const class MsgPack& field_value);

	// Serialise field_value like date.
	std::string date(string_view field_value);
	inline std::string date(const std::string& field_value) {
		return date(string_view(field_value));
	}
	std::string date(const class MsgPack& field_value);

	inline std::string timestamp(double field_value) {
		return sortable_serialise(std::round(field_value / DATETIME_MICROSECONDS) * DATETIME_MICROSECONDS);
	}

	// Serialise value like date and fill tm.
	std::string date(const class MsgPack& value, Datetime::tm_t& tm);

	inline std::string date(const Datetime::tm_t& tm) {
		return timestamp(Datetime::timestamp(tm));
	}

	// Serialise value like time.
	std::string time(string_view field_value);
	inline std::string time(const std::string& field_value) {
		return time(string_view(field_value));
	}
	std::string time(const class MsgPack& field_value);
	std::string time(const class MsgPack& field_value, double& t_val);
	std::string time(double field_value);

	// Serialise value like timedelta.
	std::string timedelta(string_view field_value);
	inline std::string timedelta(const std::string& field_value) {
		return timedelta(string_view(field_value));
	}
	std::string timedelta(const class MsgPack& field_value);
	std::string timedelta(const class MsgPack& field_value, double& t_val);
	std::string timedelta(double field_value);

	// Serialise field_value like float.
	std::string _float(string_view field_value);

	inline std::string _float(double field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like integer.
	std::string integer(string_view field_value);

	inline std::string integer(int64_t field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like positive integer.
	std::string positive(string_view field_value);

	inline std::string positive(uint64_t field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like UUID.
	std::string uuid(string_view field_value);

	// Serialise field_value like boolean.
	std::string boolean(string_view field_value);

	inline std::string boolean(bool field_value) {
		return std::string(1, field_value ? SERIALISED_TRUE : SERIALISED_FALSE);
	}

	// Serialise field_value like geospatial.
	std::string geospatial(string_view field_value);
	inline std::string geospatial(const std::string& field_value) {
		return geospatial(string_view(field_value));
	}
	std::string geospatial(const class MsgPack& field_value);

	// Serialise a vector of ranges and a vector of centroids generate by GeoSpatial.
	std::string ranges_centroids(const std::vector<range_t>& ranges, const std::vector<Cartesian>& centroids);

	// Serialise a vector of ranges generates by GeoSpatial.
	std::string ranges(const std::vector<range_t>& ranges);

	// Serialise a normalize cartesian coordinate in SERIALISED_LENGTH_CARTESIAN bytes.
	std::string cartesian(const Cartesian& norm_cartesian);

	// Serialise a HTM trixel's id.
	std::string trixel_id(uint64_t id);

	// Serialise a range_t.
	std::string range(const range_t& range);

	// Serialise type to its string representation.
	const std::string& type(FieldType type);

	// Guess type of field_value. If bool_term can not return FieldType::TEXT.
	FieldType guess_type(const class MsgPack& field_value, bool bool_term=false);


	/*
	 * Given a field_value, it guess the type and serialise. If bool_term can not return FieldType::TEXT.
	 *
	 * Returns the guess type and the serialised values according to type.
	 */

	std::pair<FieldType, std::string> guess_serialise(const class MsgPack& field_value, bool bool_term=false);

	inline std::string serialise(string_view val) {
		return std::string(val);
	}

	inline std::string serialise(int64_t val) {
		return integer(val);
	}

	inline std::string serialise(uint64_t val) {
		return positive(val);
	}

	inline std::string serialise(bool val) {
		return boolean(val);
	}

	inline std::string serialise(double val) {
		return _float(val);
	}

	inline std::string serialise(Datetime::tm_t& tm) {
		return date(tm);
	}

	inline std::string serialise(const std::vector<range_t>& val) {
		return ranges(val);
	}
};


namespace Unserialise {
	// Unserialise serialised_val according to field_type and returns a MsgPack.
	MsgPack MsgPack(FieldType field_type, string_view serialised_val);

	// Unserialise a serialised date.
	std::string date(string_view serialised_date);

	// Unserialise a serialised date returns the timestamp.
	inline double timestamp(string_view serialised_timestamp) {
		return sortable_unserialise(serialised_timestamp);
	}

	// Unserialise a serialised time.
	std::string time(string_view serialised_time);

	// Unserialise a serialised time and returns the timestamp.
	double time_d(string_view serialised_time);

	// Unserialise a serialised timedelta.
	std::string timedelta(string_view serialised_timedelta);

	// Unserialise a serialised timedelta and returns the timestamp.
	double timedelta_d(string_view serialised_timedelta);

	// Unserialise a serialised float.
	inline double _float(string_view serialised_float) {
		return sortable_unserialise(serialised_float);
	}

	// Unserialise a serialised integer.
	inline int64_t integer(string_view serialised_integer) {
		return sortable_unserialise(serialised_integer);
	}

	// Unserialise a serialised positive.
	inline uint64_t positive(string_view serialised_positive) {
		return sortable_unserialise(serialised_positive);
	}

	// Unserialise a serialised boolean.
	inline bool boolean(string_view serialised_boolean) {
		return serialised_boolean.at(0) == SERIALISED_TRUE;
	}

	// Unserialise a serialised pair of ranges and centroids.
	std::pair<RangeList, CartesianList> ranges_centroids(string_view serialised_geo);

	// Unserialise ranges from serialised pair of ranges and centroids.
	RangeList ranges(string_view serialised_geo);

	// Unserialise centroids from serialised pair of ranges and centroids.
	CartesianList centroids(string_view serialised_geo);

	// Unserialise a serialised UUID.
	std::string uuid(string_view serialised_uuid, UUIDRepr repr=UUIDRepr::simple);

	// Unserialise a serialised cartesian coordinate.
	Cartesian cartesian(string_view serialised_cartesian);

	// Unserialise a serialised HTM trixel's id.
	uint64_t trixel_id(string_view serialised_id);

	// Unserialise a serialised range_t
	range_t range(string_view serialised_range);

	// Unserialise str_type to its FieldType.
	FieldType type(string_view str_type);
};
