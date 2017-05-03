/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include "database_utils.h"      // for get_hashed, RESERVED_BOOLEAN, RESERV...
#include "msgpack.h"             // for MsgPack
#include "xxh64.hpp"             // for xxh64


enum class FieldType : uint8_t;


namespace Cast {
	enum class Hash : uint64_t {
		INTEGER           = xxh64::hash(RESERVED_INTEGER),
		POSITIVE          = xxh64::hash(RESERVED_POSITIVE),
		FLOAT             = xxh64::hash(RESERVED_FLOAT),
		BOOLEAN           = xxh64::hash(RESERVED_BOOLEAN),
		TERM              = xxh64::hash(RESERVED_TERM),
		TEXT              = xxh64::hash(RESERVED_TEXT),
		STRING            = xxh64::hash(RESERVED_STRING),
		UUID              = xxh64::hash(RESERVED_UUID),
		DATE              = xxh64::hash(RESERVED_DATE),
		TIME              = xxh64::hash(RESERVED_TIME),
		TIMEDELTA         = xxh64::hash(RESERVED_TIMEDELTA),
		EWKT              = xxh64::hash(RESERVED_EWKT),
		POINT             = xxh64::hash(RESERVED_POINT),
		CIRCLE            = xxh64::hash(RESERVED_CIRCLE),
		CONVEX            = xxh64::hash(RESERVED_CONVEX),
		POLYGON           = xxh64::hash(RESERVED_POLYGON),
		CHULL             = xxh64::hash(RESERVED_CHULL),
		MULTIPOINT        = xxh64::hash(RESERVED_MULTIPOINT),
		MULTICIRCLE       = xxh64::hash(RESERVED_MULTICIRCLE),
		MULTICONVEX       = xxh64::hash(RESERVED_MULTICONVEX),
		MULTIPOLYGON      = xxh64::hash(RESERVED_MULTIPOLYGON),
		MULTICHULL        = xxh64::hash(RESERVED_MULTICHULL),
		GEO_COLLECTION    = xxh64::hash(RESERVED_GEO_COLLECTION),
		GEO_INTERSECTION  = xxh64::hash(RESERVED_GEO_INTERSECTION),
	};

	/*
	 * Functions for doing cast between types.
	 */

	MsgPack cast(const MsgPack& obj);
	MsgPack cast(FieldType type, const std::string& field_value);
	int64_t integer(const MsgPack& obj);
	uint64_t positive(const MsgPack& obj);
	double _float(const MsgPack& obj);
	std::string string(const MsgPack& obj);
	bool boolean(const MsgPack& obj);
	std::string uuid(const MsgPack& obj);
	MsgPack date(const MsgPack& obj);
	std::string time(const MsgPack& obj);
	std::string timedelta(const MsgPack& obj);
	std::string ewkt(const MsgPack& obj);

	FieldType getType(const std::string& cast_word);
};
