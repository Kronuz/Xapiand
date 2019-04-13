/*
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

#include <string_view>           // for std::string_view

#include "database/utils.h"      // for get_hashed
#include "msgpack.h"             // for MsgPack
#include "enum.h"                // for ENUM
#include "reserved/schema.h"     // for RESERVED_CHAI
#include "reserved/types.h"      // for RESERVED_BOOLEAN, ...


enum class FieldType : uint8_t;

ENUM(CastHashType, uint32_t,
	INTEGER,
	POSITIVE,
	FLOAT,
	BOOLEAN,
	KEYWORD,
	TEXT,
	STRING,
	UUID,
	DATE,
	DATETIME,
	TIME,
	TIMEDELTA,
	EWKT,
	POINT,
	CIRCLE,
	CONVEX,
	POLYGON,
	CHULL,
	MULTIPOINT,
	MULTICIRCLE,
	MULTICONVEX,
	MULTIPOLYGON,
	MULTICHULL,
	GEO_COLLECTION,
	GEO_INTERSECTION,
	CHAI
)

namespace Cast {
	/*
	 * Functions for doing cast between types.
	 */

	using HashType = ::CastHashType;

	MsgPack cast(const MsgPack& obj);
	MsgPack cast(FieldType type, const MsgPack& obj);
	int64_t integer(const MsgPack& obj);
	uint64_t positive(const MsgPack& obj);
	long double floating(const MsgPack& obj);
	std::string string(const MsgPack& obj);
	bool boolean(const MsgPack& obj);
	std::string uuid(const MsgPack& obj);
	MsgPack datetime(const MsgPack& obj);
	MsgPack time(const MsgPack& obj);
	MsgPack timedelta(const MsgPack& obj);
	std::string ewkt(const MsgPack& obj);

	HashType get_hash_type(std::string_view cast_word);
	FieldType get_field_type(std::string_view cast_word);
};
