/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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
#include "htm.h"
#include "msgpack.h"
#include "stl_serialise.h"

#include "hash/endian.h"


// Data types
#define NUMERIC_TYPE 'n'
#define STRING_TYPE  's'
#define DATE_TYPE    'd'
#define GEO_TYPE     'g'
#define BOOLEAN_TYPE 'b'
#define ARRAY_TYPE   'a'
#define OBJECT_TYPE  'o'
#define NO_TYPE      ' '


#define NUMERIC_STR "numeric"
#define STRING_STR  "string"
#define DATE_STR    "date"
#define GEO_STR     "geospatial"
#define BOOLEAN_STR "boolean"
#define ARRAY_STR   "array"
#define OBJECT_STR  "object"


#if __BYTE_ORDER == __BIG_ENDIAN
// No translation needed for big endian system.
#define Swap7Bytes(val) val // HTM's trixel's ids are represent in 7 bytes.
#define Swap4Bytes(val) val // Unsigned int is represent in 4 bytes
#elif __BYTE_ORDER == __LITTLE_ENDIAN
// Swap 7 byte, 56 bit values. (If it is not big endian, It is considered little endian)
#define Swap7Bytes(val) ((((val) >> 48) & 0xFF) | (((val) >> 32) & 0xFF00) | (((val) >> 16) & 0xFF0000) | \
						((val) & 0xFF000000) | (((val) << 16) & 0xFF00000000) | \
						(((val) << 32) & 0xFF0000000000) | (((val) << 48) & 0xFF000000000000))
#define Swap4Bytes(val) ((((val) >> 24) & 0xFF) | (((val) >> 8) & 0xFF00) | \
						(((val) <<  8) & 0xFF0000) | ((val << 24) & 0xFF000000))
#endif


constexpr uint32_t SIZE_SERIALISE_CARTESIAN = 12;
constexpr uint32_t DOUBLE2INT = 1000000000;
constexpr uint32_t MAXDOU2INT =  999999999;


namespace Serialise {
	/*
	 * Serialise field_value according to field_type.
	 */
	std::string serialise(char field_type, const MsgPack& field_value);
	std::string serialise(char field_type, const std::string& field_value);
	std::string string(char field_type, const std::string& field_value);
	std::string numeric(char field_type, double field_value);
	std::string boolean(char field_type, bool field_value);

	/*
	 * Given a field_value, it gets the type.
	 * returns the type and the serialise value.
	 */
	std::pair<char, std::string> serialise(const std::string& field_value);


	// Serialise field_value like date.
	std::string date(const std::string& field_value);

	// Serialise value like date and fill tm.
	std::string date(const MsgPack& value, Datetime::tm_t& tm);

	// Serialise struct tm with math like date.
	std::string date_with_math(Datetime::tm_t tm, const std::string& op, const std::string& units);

	// Serialise field_value like numeric.
	std::string numeric(const std::string& field_value);

	// Serialise field_value like EWKT.
	std::string ewkt(const std::string& field_value);

	// Serialise a geo specification.
	std::string geo(const RangeList& ranges, const CartesianUSet& centroids);

	// Serialise field_value like boolean.
	std::string boolean(const std::string& field_value);

	// Serialise a normalize cartesian coordinate in SIZE_SERIALISE_CARTESIAN bytes.
	std::string cartesian(const Cartesian& norm_cartesian);

	// Serialise a trixel's id (HTM).
	std::string trixel_id(uint64_t id);

	// Serialise type to its string representation.
	std::string type(char type);
};


namespace Unserialise {
	// Unserialise serialise_val according to field_type and save the value in result.
	void unserialise(char field_type, const std::string& serialise_val, MsgPack& result);

	// Unserialise serialise_val according to field_type.
	std::string unserialise(char field_type, const std::string& serialise_val);

	// Unserialise a serialised numeric.
	double numeric(const std::string& serialise_numeric);

	// Unserialise a serialised date.
	std::string date(const std::string& serialise_date);

	// Unserialise a serialised boolean.
	bool boolean(const std::string& serialise_boolean);

	// Unserialise a serialised cartesian coordinate.
	Cartesian cartesian(const std::string& serialise_cartesian);

	// Unserialise a serialised trixel's id (HTM).
	uint64_t trixel_id(const std::string& serialise_trixel_id);

	// Unserialise a serialised EWKT (save as value), in serialised ranges and serialises centroids.
	std::string ewkt(const std::string& serialise_ewkt);

	// Unserialise a serialised EWKT (Save as a Value), in unserialised ranges and unserialised centroids.
	std::pair<std::string, std::string> geo(const std::string& serialise_ewkt);

	// Unserialise str_type to its char representation.
	std::string type(const std::string& str_type);
};
