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

#include "database_utils.h"
#include "datetime.h"
#include "hash/endian.h"
#include "msgpack.h"
#include "sortable_serialise.h"
#include "xxh64.hpp"


#ifndef __has_builtin         // Optional of course
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers
#endif
#if (defined(__clang__) && __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap64)) || (defined(__GNUC__ ) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
//  GCC and Clang recent versions provide intrinsic byte swaps via builtins
//  prior to 4.8, gcc did not provide __builtin_bswap16 on some platforms so we emulate it
//  see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52624
//  Clang has a similar problem, but their feature test macros make it easier to detect
#  if (defined(__clang__) && __has_builtin(__builtin_bswap16)) || (defined(__GNUC__) &&(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
#    define BYTE_SWAP_2(x) (__builtin_bswap16(x))
#  else
#    define BYTE_SWAP_2(x) (__builtin_bswap32((x) << 16))
#  endif
#  define BYTE_SWAP_4(x) (__builtin_bswap32(x))
#  define BYTE_SWAP_8(x) (__builtin_bswap64(x))
#elif defined(__linux__)
// Linux systems provide the byteswap.h header, with
// don't check for obsolete forms defined(linux) and defined(__linux) on the theory that
// compilers that predefine only these are so old that byteswap.h probably isn't present.
#  include <byteswap.h>
#  define BYTE_SWAP_2(x) (bswap_16(x))
#  define BYTE_SWAP_4(x) (bswap_32(x))
#  define BYTE_SWAP_8(x) (bswap_64(x))
#elif defined(_MSC_VER)
// Microsoft documents these as being compatible since Windows 95 and specificly
// lists runtime library support since Visual Studio 2003 (aka 7.1).
#  include <cstdlib>
#  define BYTE_SWAP_2(x) (_byteswap_ushort(x))
#  define BYTE_SWAP_4(x) (_byteswap_ulong(x))
#  define BYTE_SWAP_8(x) (_byteswap_uint64(x))
#else
#  define BYTE_SWAP_2(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#  define BYTE_SWAP_4(x) ((BYTE_SWAP_2(((x) & 0xFFFF0000) >> 16)) | ((BYTE_SWAP_2((x) & 0x0000FFFF)) << 16))
#  define BYTE_SWAP_8(x) ((BYTE_SWAP_8(((x) & 0xFFFFFFFF00000000) >> 32)) | ((BYTE_SWAP_8((x) & 0x00000000FFFFFFFF)) << 32))
#endif


#if __BYTE_ORDER == __BIG_ENDIAN
// No translation needed for big endian system.
#  define Swap7Bytes(val) (val) // HTM's trixel's ids are represent in 7 bytes.
#  define Swap2Bytes(val) (val) // uint16_t, short in 2 bytes
#  define Swap4Bytes(val) (val) // Unsigned int is represent in 4 bytes
#  define Swap8Bytes(val) (val) // uint64_t is represent in 8 bytes
#elif __BYTE_ORDER == __LITTLE_ENDIAN
// Swap 7 byte, 56 bit values. (If it is not big endian, It is considered little endian)
#  define Swap2Bytes(val) BYTE_SWAP_2(val)
#  define Swap4Bytes(val) BYTE_SWAP_4(val)
#  define Swap7Bytes(val) (BYTE_SWAP_8((val) << 8))
#  define Swap8Bytes(val) BYTE_SWAP_8(val)
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


namespace Cast {
	/*
	 * Functions for doing cast between types.
	 */

	int64_t integer(const MsgPack& obj);
	uint64_t positive(const MsgPack& obj);
	double _float(const MsgPack& obj);
	std::string string(const MsgPack& obj);
	bool boolean(const MsgPack& obj);
};


class Cartesian;
class CartesianUSet;
class RangeList;
struct required_spc_t;
enum class FieldType : uint8_t;


namespace Serialise {
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
	 * Functions for doing cast and serialise.
	 */

	std::string integer_cast(const required_spc_t& field_spc, const MsgPack& obj);
	std::string positive_cast(const required_spc_t& field_spc, const MsgPack& obj);
	std::string float_cast(const required_spc_t& field_spc, const MsgPack& obj);
	std::string boolean_cast(const required_spc_t& field_spc, const MsgPack& obj);
	std::string string_cast(const required_spc_t& field_spc, const MsgPack& obj);

	/*
	 * Serialise field_value according to field_spc.
	 */

	std::string MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value);
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
	 * Returns the type and the serialised values according to type.
	 */

	std::pair<FieldType, std::string> get_type(const std::string& field_value, bool bool_term=false);
	std::tuple<FieldType, std::string, std::string> get_range_type(const std::string& start, const std::string& end, bool bool_term=false);
	std::pair<FieldType, std::string> get_type(const class MsgPack& field_value, bool bool_term=false);
	std::tuple<FieldType, std::string, std::string> get_range_type(const class MsgPack& start, const class MsgPack& end, bool bool_term=false);

	// Serialise field_value like date.
	std::string date(const std::string& field_value);

	inline std::string timestamp(double field_value) {
		return sortable_serialise(field_value);
	}

	// Serialise value like date and fill tm.
	std::string date(const class MsgPack& value, Datetime::tm_t& tm);

	inline std::string date(Datetime::tm_t& tm) {
		return sortable_serialise(Datetime::timestamp(tm));
	}

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

	// Function for serialization of namespace field.
	inline std::string namespace_field(const std::string& field_name) {
		return sortable_serialise(xxh64::hash(field_name) & 0xffffffff);
	}

	// Function for serialization of dynamic namespace field.
	inline std::string dynamic_namespace_field(const std::string& field_name) {
		return get_hashed(field_name);
	}

	inline std::string serialise(const std::string& val) {
		return val;
	}

	inline std::string serialise(int64_t val) {
		return Serialise::integer(val);
	}

	inline std::string serialise(uint64_t val) {
		return Serialise::positive(val);
	}

	inline std::string serialise(bool val) {
		return Serialise::boolean(val);
	}

	inline std::string serialise(double val) {
		return Serialise::_float(val);
	}

	inline std::string serialise(Datetime::tm_t& tm) {
		return Serialise::date(tm);
	}

	inline std::string serialise(const std::vector<std::string>& val) {
		return Serialise::trixels(val);
	}
};


namespace Unserialise {
	// Unserialise serialised_val according to field_type and returns a MsgPack.
	MsgPack MsgPack(FieldType field_type, const std::string& serialised_val);

	// Unserialise serialised_val according to field_type.
	std::string unserialise(FieldType field_type, const std::string& serialised_val);

	// Unserialise a serialised float.
	inline double _float(const std::string& serialised_float) {
		return sortable_unserialise(serialised_float);
	}

	// Unserialise a serialised integer.
	inline int64_t integer(const std::string& serialised_integer) {
		return sortable_unserialise(serialised_integer);
	}

	// Unserialise a serialised positive.
	inline uint64_t positive(const std::string& serialised_positive) {
		return sortable_unserialise(serialised_positive);
	}

	// Unserialise a serialised date.
	std::string date(const std::string& serialised_date);

	// Unserialise a serialised date and returns the timestamp.
	inline double timestamp(const std::string& serialised_timestamp) {
		return sortable_unserialise(serialised_timestamp);
	}

	// Unserialise a serialised boolean.
	inline bool boolean(const std::string& serialised_boolean) {
		return serialised_boolean.at(0) == TRUE_SERIALISED;
	}

	// Unserialise a serialised cartesian coordinate.
	Cartesian cartesian(const std::string& serialised_cartesian);

	// Unserialise a serialised trixel's id (HTM).
	uint64_t trixel_id(const std::string& serialised_trixel_id);

	// Unserialise a serialised UUID.
	std::string uuid(const std::string& serialised_uuid);

	// Unserialise a serialised EWKT (save as value), in serialised ranges and serialises centroids.
	std::string ewkt(const std::string& serialised_ewkt);

	// Unserialise a serialised EWKT (Save as a Value), in unserialised ranges and unserialised centroids.
	std::pair<std::string, std::string> geo(const std::string& serialised_ewkt);

	// Unserialise str_type to its FieldType.
	FieldType type(const std::string& str_type);
};

using dispatch_cast = std::string (*) (const required_spc_t& field_spc, const MsgPack&);
