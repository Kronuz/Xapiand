/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"                 // for XAPIAND_UUID_ENCODED, XAPIAND_UUID_GUID, XAPIAND_UUID_URN

#include <string>                   // for std::string
#include <string_view>              // for std::string_view
#include <cmath>                    // for std::round
#include <cstdint>                  // for std::uint64_t, std::int64_t, std::uint32_t, std::uint8_t
#include <optional>                 // for std::optional
#include <utility>                  // for std::pair
#include <vector>                   // for std::vector

#include "datetime.h"               // for Datetime::tm_t (ptr only), Datetime::timestamp
#include "cartesian.h"              // for Cartesian
#include "htm.h"         // for range_t
#include "msgpack.h"                // for MsgPack
#include "sortable.hh"              // for sortable::encode, sortable::to_double, sortable::decode
#include "hashes.hh"                // for fnv1ah32


constexpr const char FLOATING_STR[]  = "floating";
constexpr const char INTEGER_STR[]   = "integer";
constexpr const char POSITIVE_STR[]  = "positive";
constexpr const char KEYWORD_STR[]   = "keyword";
constexpr const char TEXT_STR[]      = "text";
constexpr const char DATE_STR[]      = "date";
constexpr const char DATETIME_STR[]  = "datetime";
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


constexpr char SERIALIZED_FALSE      = 'f';
constexpr char SERIALIZED_TRUE       = 't';


constexpr std::uint8_t SERIALIZED_LENGTH_CARTESIAN = 12;
constexpr std::uint8_t SERIALIZED_LENGTH_RANGE     = 2 * HTM_BYTES_ID;
constexpr std::uint8_t SERIALIZED_LENGTH_RANGES_HASH = 16;


constexpr std::uint32_t DOUBLE2INT = 1000000000;
constexpr std::uint32_t MAXDOU2INT = 2000000000;

enum class UUIDRepr : std::uint32_t {
	vanilla = fnv1ah32::hash("vanilla"),
#ifdef XAPIAND_UUID_GUID
	guid = fnv1ah32::hash("guid"),
#endif
#ifdef XAPIAND_UUID_URN
	urn = fnv1ah32::hash("urn"),
#endif
#ifdef XAPIAND_UUID_ENCODED
	encoded = fnv1ah32::hash("encoded"),
#endif
};

class CartesianList;
class RangeList;
enum class FieldType : std::uint8_t;
struct required_spc_t;


namespace Serialize {
	bool isText(std::string_view field_value) noexcept;

	// Returns if field_value is UUID.
	bool possiblyUUID(std::string_view field_value) noexcept;
	bool isUUID(std::string_view field_value) noexcept;

	// The one rule a value meets a typed field by (D92): `value` as exactly a value of `type`, where text spells a
	// number as JSON does and a cast object ({"_integer": 1.5}) converts on request; else a ClientError naming
	// `field`, the value and the type. Numeric, keyword, text and uuid types are checked; others pass as given.
	class MsgPack exact(std::string_view field, FieldType type, const class MsgPack& value);


	/*
	 * Serialize field_value according to field_spc.
	 */

	std::string MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string object(const required_spc_t& field_spc, const class MsgPack& o);

	std::string serialize(const required_spc_t& field_spc, const class MsgPack& field_value);
	std::string serialize(const required_spc_t& field_spc, std::string_view field_value);
	inline std::string serialize(const required_spc_t& field_spc, const std::string& field_value) {
		return serialize(field_spc, std::string_view(field_value));
	}

	std::string string(const required_spc_t& field_spc, std::string_view field_value);

	std::string datetime(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string time(const required_spc_t& field_spc, const class MsgPack& field_value);

	std::string timedelta(const required_spc_t& field_spc, const class MsgPack& field_value);


	/*
	 * Serialize field_value according to field_type.
	 */

	std::string floating(FieldType field_type, double field_value);
	std::string integer(FieldType field_type, std::int64_t field_value);
	std::string positive(FieldType field_type, std::uint64_t field_value);
	std::string boolean(FieldType field_type, bool field_value);
	std::string geospatial(FieldType field_type, const class MsgPack& field_value);

	// Serialize field_value like datetime.
	std::string datetime(std::string_view field_value);
	inline std::string datetime(const std::string& field_value) {
		return datetime(std::string_view(field_value));
	}
	std::string datetime(const class MsgPack& field_value);

	inline std::string timestamp(double field_value) {
		return sortable::encode(std::round(field_value * DATETIME_MICROSECONDS) / DATETIME_MICROSECONDS);
	}

	// Serialize value like datetime and fill tm.
	std::string datetime(const class MsgPack& value, Datetime::tm_t& tm);

	inline std::string datetime(const Datetime::tm_t& tm) {
		return timestamp(Datetime::timestamp(tm));
	}

	// Serialize value like time.
	std::string time(std::string_view field_value);
	inline std::string time(const std::string& field_value) {
		return time(std::string_view(field_value));
	}
	std::string time(const class MsgPack& field_value);
	std::string time(const class MsgPack& field_value, double& t_val);
	std::string time(double field_value);

	// Serialize value like timedelta.
	std::string timedelta(std::string_view field_value);
	inline std::string timedelta(const std::string& field_value) {
		return timedelta(std::string_view(field_value));
	}
	std::string timedelta(const class MsgPack& field_value);
	std::string timedelta(const class MsgPack& field_value, double& t_val);
	std::string timedelta(double field_value);

	// Serialize field_value like float.
	std::string floating(std::string_view field_value);

	// Total: -0.0 is 0 and NaN is one value; the entry points for user values refuse NaN (D4).
	inline std::string floating(double field_value) {
		return sortable::encode(field_value);
	}

	// Serialize field_value like integer.
	std::string integer(std::string_view field_value);

	inline std::string integer(std::int64_t field_value) {
		return sortable::encode(field_value);
	}

	// Serialize field_value like positive integer.
	std::string positive(std::string_view field_value);

	inline std::string positive(std::uint64_t field_value) {
		return sortable::encode(field_value);
	}

	// A document version's bytes in DB_SLOT_VERSION, the slot the `_version` field (positive) sorts and ranges.
	inline std::string version(std::uint64_t document_version) {
		return sortable::encode(document_version);
	}

	// Serialize field_value like UUID.
	std::string uuid(std::string_view field_value);

	// Serialize field_value like boolean.
	std::string boolean(std::string_view field_value);

	inline std::string boolean(bool field_value) {
		return std::string(1, field_value ? SERIALIZED_TRUE : SERIALIZED_FALSE);
	}

	// Serialize a vector of ranges and a vector of centroids generate by GeoSpatial.
	std::string ranges_centroids(const std::vector<range_t>& ranges, const std::vector<Cartesian>& centroids);

	// Serialize a vector of ranges generates by GeoSpatial.
	std::string ranges(const std::vector<range_t>& ranges);

	// Serialize a vector of centroids generates by GeoSpatial.
	std::string centroids(const std::vector<Cartesian>& centroids);

	// A geometry's exact-match term: a 128-bit hash of its ranges.
	std::string ranges_hash(const std::vector<range_t>& ranges);

	// Serialize field_value like geospatial.
	std::string geospatial(const class MsgPack& field_value);
	std::string geospatial(std::string_view field_value);

	// Serialize a normalize cartesian coordinate in SERIALIZED_LENGTH_CARTESIAN bytes.
	std::string cartesian(const Cartesian& norm_cartesian);

	// Serialize a HTM trixel's id.
	std::string trixel_id(std::uint64_t id);

	// Serialize a range_t.
	std::string range(const range_t& range);

	// Guess type of field_value. If bool_term can not return FieldType::text.
	FieldType guess_type(const class MsgPack& field_value);


	/*
	 * Given a field_value, it guess the type and serialize. If bool_term can not return FieldType::text.
	 *
	 * Returns the guess type and the serialized values according to type.
	 */

	std::pair<FieldType, std::string> guess_serialize(const class MsgPack& field_value);

	// A document id's bytes after its type character (docs/design/exact-id-keys.md). A numeric id's are
	// Kronuz/sortable's, exact for every int64, uint64 and double; any other id's are serialize()'s.
	std::string id(const required_spc_t& spc_id, const class MsgPack& document_id);

	// The bytes of the integer id N, as id() gives them for N in an integer, positive or floating id field.
	std::string id(std::uint64_t document_id);

	// guess_serialize() of an id in an index whose id type is not set yet, with id()'s bytes.
	std::pair<FieldType, std::string> guess_id(const class MsgPack& document_id);

	inline std::string serialize(std::string_view val) {
		return std::string(val);
	}

	inline std::string serialize(std::int64_t val) {
		return integer(val);
	}

	inline std::string serialize(std::uint64_t val) {
		return positive(val);
	}

	inline std::string serialize(bool val) {
		return boolean(val);
	}

	inline std::string serialize(double val) {
		return floating(val);
	}

	inline std::string serialize(Datetime::tm_t& tm) {
		return datetime(tm);
	}

	inline std::string serialize(const std::vector<range_t>& val) {
		return ranges(val);
	}
}


namespace Unserialize {
	// The integer a numeric id's bytes (Serialize::id) name exactly, else nothing (a fraction, a negative).
	std::optional<std::uint64_t> id(std::string_view serialized_id);

	// A numeric id from its bytes, in the id field's type (integer, positive or floating).
	class MsgPack id(FieldType field_type, std::string_view serialized_id);

	// Unserialize serialized_val according to field_type and returns a MsgPack.
	MsgPack MsgPack(FieldType field_type, std::string_view serialized_val);

	// Unserialize a serialized datetime.
	std::string datetime(std::string_view serialized_date);

	// Raises SerializationError naming bytes that are not a serialized number.
	[[noreturn]] void invalid_number(std::string_view serialized_number);

	// Unserialize a serialized float: stored bytes, read without the canonical checks (design D2).
	inline double floating(std::string_view serialized_float) {
		if (const auto value = sortable::to_double(serialized_float)) {
			return *value;
		}
		invalid_number(serialized_float);
	}

	// Unserialize a serialized datetime returns the timestamp.
	inline double timestamp(std::string_view serialized_timestamp) {
		return floating(serialized_timestamp);
	}

	// Unserialize a serialized time.
	std::string time(std::string_view serialized_time);

	// Unserialize a serialized time and returns the timestamp.
	double time_d(std::string_view serialized_time);

	// Unserialize a serialized timedelta.
	std::string timedelta(std::string_view serialized_timedelta);

	// Unserialize a serialized timedelta and returns the timestamp.
	double timedelta_d(std::string_view serialized_time);

	// The number serialized_number names, checked strictly (sortable::decode), else SerializationError.
	sortable::number number(std::string_view serialized_number);

	// Unserialize a serialized integer; a fraction truncates toward zero and a value out of range saturates
	// (a slot shared by integer and floating fields holds both).
	inline std::int64_t integer(std::string_view serialized_integer) {
		if (const auto value = sortable::to_int64(serialized_integer)) {
			return *value;
		}
		return number(serialized_integer).saturate_int64();
	}

	// Unserialize a serialized positive, truncating and saturating like integer().
	inline std::uint64_t positive(std::string_view serialized_positive) {
		if (const auto value = sortable::to_uint64(serialized_positive)) {
			return *value;
		}
		return number(serialized_positive).saturate_uint64();
	}

	// A document version from its DB_SLOT_VERSION bytes (Serialize::version).
	inline std::uint64_t version(std::string_view serialized_version) {
		if (const auto value = sortable::to_uint64(serialized_version)) {
			return *value;
		}
		invalid_number(serialized_version);
	}

	// Unserialize a serialized boolean.
	inline bool boolean(std::string_view serialized_boolean) {
		return serialized_boolean.at(0) == SERIALIZED_TRUE;
	}

	// Unserialize a serialized pair of ranges and centroids.
	std::pair<RangeList, CartesianList> ranges_centroids(std::string_view serialized_geo);

	// Unserialize ranges from serialized pair of ranges and centroids.
	RangeList ranges(std::string_view serialized_geo);

	// Unserialize centroids from serialized pair of ranges and centroids.
	CartesianList centroids(std::string_view serialized_geo);

	// Unserialize a serialized UUID.
	std::string uuid(std::string_view serialized_uuid, UUIDRepr repr=UUIDRepr::vanilla);

	// Unserialize a serialized cartesian coordinate.
	Cartesian cartesian(std::string_view serialized_val);

	// Unserialize a serialized HTM trixel's id.
	std::uint64_t trixel_id(std::string_view serialized_id);

	// Unserialize a serialized range_t
	range_t range(std::string_view serialized_range);

	// Unserialize str_type to its FieldType.
	FieldType get_field_type(std::string_view str_type);
}
