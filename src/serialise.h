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

#include "datetime.h"
#include "geo/wkt_parser.h"
#include "hash/endian.h"
#include "msgpack.h"
#include "sortable_serialise.h"


#if __BYTE_ORDER == __BIG_ENDIAN
// No translation needed for big endian system.
#define Swap7Bytes(val) val // HTM's trixel's ids are represent in 7 bytes.
#define Swap2Bytes(val) val // uint16_t, short in 2 bytes
#define Swap4Bytes(val) val // Unsigned int is represent in 4 bytes
#define Swap8Bytes(val) val // uint64_t is represent in 8 bytes
#elif __BYTE_ORDER == __LITTLE_ENDIAN
// Swap 7 byte, 56 bit values. (If it is not big endian, It is considered little endian)
#define Swap7Bytes(val) ((((val) >> 48) & 0xFF) | (((val) >> 32) & 0xFF00) | (((val) >> 16) & 0xFF0000) | \
						((val) & 0xFF000000) | (((val) << 16) & 0xFF00000000) | \
						(((val) << 32) & 0xFF0000000000) | (((val) << 48) & 0xFF000000000000))
#define Swap2Bytes(val) (((val & 0xFF00) >> 8) | ((val & 0x00FF) << 8))
#define Swap4Bytes(val) ((Swap2Bytes((val & 0xFFFF0000) >> 16)) | ((Swap2Bytes(val & 0x0000FFFF)) << 16))
#define Swap8Bytes(val) ((Swap4Bytes((val & 0xFFFFFFFF00000000) >> 32)) | ((Swap4Bytes(val & 0x00000000FFFFFFFF)) << 32))

#endif


#define FLOAT_STR    "float"
#define INTEGER_STR  "integer"
#define POSITIVE_STR "positive"
#define STRING_STR   "string"
#define TEXT_STR     "text"
#define DATE_STR     "date"
#define GEO_STR      "geospatial"
#define BOOLEAN_STR  "boolean"
#define UUID_STR     "uuid"
#define ARRAY_STR    "array"
#define OBJECT_STR   "object"


#define FALSE_SERIALISED 'f'
#define TRUE_SERIALISED  't'


constexpr uint32_t SIZE_SERIALISE_CARTESIAN = 12;
constexpr uint32_t DOUBLE2INT               = 1000000000;
constexpr uint32_t MAXDOU2INT               =  999999999;


constexpr uint8_t SIZE_UUID = 36;


struct required_spc_t;
enum class FieldType : uint8_t;


namespace Serialise{
	inline static bool isText(const std::string& field_value, bool bool_term) noexcept {
		return !bool_term && field_value.find(' ') != std::string::npos;
	}

	inline static bool isUUID(const std::string& field_name) noexcept {
		if (field_name.length() != SIZE_UUID) {
			return false;
		}
		if (field_name[8] != '-' || field_name[13] != '-' || field_name[18] != '-' || field_name[23] != '-') {
			return false;
		}
		for (size_t i = 0; i < SIZE_UUID; ++i) {
			if (!std::isxdigit(field_name.at(i)) && i != 8 && i != 13 && i != 18 && i != 23) {
				return false;
			}
		}
		return true;
	}

	/*
	 * Serialise field_value according to field_spc.
	 */

	std::string MsgPack(const required_spc_t& field_spc, const MsgPack& field_value);
	std::string serialise(const required_spc_t& field_spc, const std::string& field_value);
	std::string string(const required_spc_t& field_spc, const std::string& field_value);

	/*
	 * Serialise field_value according to field_type.
	 */
	std::string _float(FieldType field_type, double field_value);
	std::string integer(FieldType field_type, int64_t field_value);
	std::string positive(FieldType field_type, uint64_t field_value);
	std::string boolean(FieldType field_type, bool field_value);

	/*
	 * Given a field_value, it gets the type.
	 *
	 * If bool_term can not return FieldType::TEXT.
	 *
	 * Returns the type and the serialise values according to type.
	 */
	std::pair<FieldType, std::string> get_type(const std::string& field_value, bool bool_term=false);
	std::tuple<FieldType, std::string, std::string> get_range_type(const std::string& start, const std::string& end, bool bool_term=false);


	// Serialise field_value like date.
	std::string date(const std::string& field_value);

	inline std::string timestamp(double field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise value like date and fill tm.
	std::string date(const ::MsgPack& value, Datetime::tm_t& tm);

	// Serialise field_value like float.
	std::string _float(const std::string& field_value);

	inline std::string _float(double field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like integer.
	std::string integer(const std::string& field_value);

	inline std::string integer(int64_t field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like positive integer.
	std::string positive(const std::string& field_value);

	inline std::string positive(uint64_t field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise field_value like UUID.
	std::string uuid(const std::string& field_value);

	// Serialise field_value like EWKT.
	std::string ewkt(const std::string& field_value, bool partials, double error);

	// Serialise a vector of trixel's id (HTM).
	std::string trixels(const std::vector<std::string>& trixels);

	// Serialise a geo specification.
	std::string geo(const RangeList& ranges, const CartesianUSet& centroids);

	// Serialise field_value like boolean.
	std::string boolean(const std::string& field_value);

	inline std::string boolean(bool field_value) {
		return std::string(1, field_value ? TRUE_SERIALISED : FALSE_SERIALISED);
	}

	// Serialise a normalize cartesian coordinate in SIZE_SERIALISE_CARTESIAN bytes.
	std::string cartesian(const Cartesian& norm_cartesian);

	// Serialise a trixel's id (HTM).
	std::string trixel_id(uint64_t id);

	// Serialise type to its string representation.
	std::string type(FieldType type);
};


namespace Unserialise {
	// Unserialise serialise_val according to field_type and returns a MsgPack.
	MsgPack MsgPack(FieldType field_type, const std::string& serialise_val);

	// Unserialise serialise_val according to field_type.
	std::string unserialise(FieldType field_type, const std::string& serialise_val);

	// Unserialise a serialised float.
	inline double _float(const std::string& serialise_float) {
		return sortable_unserialise(serialise_float);
	}

	// Unserialise a serialised integer.
	inline int64_t integer(const std::string& serialise_integer) {
		return sortable_unserialise(serialise_integer);
	}

	// Unserialise a serialised positive.
	inline uint64_t positive(const std::string& serialise_positive) {
		return sortable_unserialise(serialise_positive);
	}

	// Unserialise a serialised date.
	std::string date(const std::string& serialise_date);

	// Unserialise a serialised date and returns the timestamp.
	inline double timestamp(const std::string& serialise_timestamp) {
		return sortable_unserialise(serialise_timestamp);
	}

	// Unserialise a serialised boolean.
	inline bool boolean(const std::string& serialise_boolean) {
		return serialise_boolean.at(0) == TRUE_SERIALISED;
	}

	// Unserialise a serialised cartesian coordinate.
	Cartesian cartesian(const std::string& serialise_cartesian);

	// Unserialise a serialised trixel's id (HTM).
	uint64_t trixel_id(const std::string& serialise_trixel_id);

	// Unserialise a serialised UUID.
	std::string uuid(const std::string& serialised_uuid);

	// Unserialise a serialised EWKT (save as value), in serialised ranges and serialises centroids.
	std::string ewkt(const std::string& serialise_ewkt);

	// Unserialise a serialised EWKT (Save as a Value), in unserialised ranges and unserialised centroids.
	std::pair<std::string, std::string> geo(const std::string& serialise_ewkt);

	// Unserialise str_type to its FieldType.
	FieldType type(const std::string& str_type);
};
