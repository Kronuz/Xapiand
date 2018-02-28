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

#include <string_view>           // for std::string_view

#include "database_utils.h"      // for get_hashed, RESERVED_BOOLEAN, RESERV...
#include "msgpack.h"             // for MsgPack
#include "hashes.hh"             // for fnv1ah32


enum class FieldType : uint8_t;


namespace Cast {
	enum class Hash : uint32_t {
		INTEGER           = fnv1ah32::hash(RESERVED_INTEGER),
		POSITIVE          = fnv1ah32::hash(RESERVED_POSITIVE),
		FLOAT             = fnv1ah32::hash(RESERVED_FLOAT),
		BOOLEAN           = fnv1ah32::hash(RESERVED_BOOLEAN),
		TERM              = fnv1ah32::hash(RESERVED_TERM),
		TEXT              = fnv1ah32::hash(RESERVED_TEXT),
		STRING            = fnv1ah32::hash(RESERVED_STRING),
		UUID              = fnv1ah32::hash(RESERVED_UUID),
		DATE              = fnv1ah32::hash(RESERVED_DATE),
		TIME              = fnv1ah32::hash(RESERVED_TIME),
		TIMEDELTA         = fnv1ah32::hash(RESERVED_TIMEDELTA),
		EWKT              = fnv1ah32::hash(RESERVED_EWKT),
		POINT             = fnv1ah32::hash(RESERVED_POINT),
		CIRCLE            = fnv1ah32::hash(RESERVED_CIRCLE),
		CONVEX            = fnv1ah32::hash(RESERVED_CONVEX),
		POLYGON           = fnv1ah32::hash(RESERVED_POLYGON),
		CHULL             = fnv1ah32::hash(RESERVED_CHULL),
		MULTIPOINT        = fnv1ah32::hash(RESERVED_MULTIPOINT),
		MULTICIRCLE       = fnv1ah32::hash(RESERVED_MULTICIRCLE),
		MULTICONVEX       = fnv1ah32::hash(RESERVED_MULTICONVEX),
		MULTIPOLYGON      = fnv1ah32::hash(RESERVED_MULTIPOLYGON),
		MULTICHULL        = fnv1ah32::hash(RESERVED_MULTICHULL),
		GEO_COLLECTION    = fnv1ah32::hash(RESERVED_GEO_COLLECTION),
		GEO_INTERSECTION  = fnv1ah32::hash(RESERVED_GEO_INTERSECTION),
		CHAI              = fnv1ah32::hash(RESERVED_CHAI),
		ECMA              = fnv1ah32::hash(RESERVED_ECMA),
	};

	/*
	 * Functions for doing cast between types.
	 */

	MsgPack cast(const MsgPack& obj);
	MsgPack cast(FieldType type, std::string_view field_value);
	int64_t integer(const MsgPack& obj);
	uint64_t positive(const MsgPack& obj);
	double _float(const MsgPack& obj);
	std::string string(const MsgPack& obj);
	bool boolean(const MsgPack& obj);
	std::string uuid(const MsgPack& obj);
	MsgPack date(const MsgPack& obj);
	MsgPack time(const MsgPack& obj);
	MsgPack timedelta(const MsgPack& obj);
	std::string ewkt(const MsgPack& obj);

	FieldType getType(std::string_view cast_word);
};
