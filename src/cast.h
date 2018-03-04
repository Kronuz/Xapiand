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
#include "phf.hh"                // for phf


enum class FieldType : uint8_t;

namespace Cast {
	#define HASH_OPTIONS(name, ...) \
		OPTION(INTEGER, __VA_ARGS__) \
		OPTION(POSITIVE, __VA_ARGS__) \
		OPTION(FLOAT, __VA_ARGS__) \
		OPTION(BOOLEAN, __VA_ARGS__) \
		OPTION(TERM, __VA_ARGS__) \
		OPTION(TEXT, __VA_ARGS__) \
		OPTION(STRING, __VA_ARGS__) \
		OPTION(UUID, __VA_ARGS__) \
		OPTION(DATE, __VA_ARGS__) \
		OPTION(TIME, __VA_ARGS__) \
		OPTION(TIMEDELTA, __VA_ARGS__) \
		OPTION(EWKT, __VA_ARGS__) \
		OPTION(POINT, __VA_ARGS__) \
		OPTION(CIRCLE, __VA_ARGS__) \
		OPTION(CONVEX, __VA_ARGS__) \
		OPTION(POLYGON, __VA_ARGS__) \
		OPTION(CHULL, __VA_ARGS__) \
		OPTION(MULTIPOINT, __VA_ARGS__) \
		OPTION(MULTICIRCLE, __VA_ARGS__) \
		OPTION(MULTICONVEX, __VA_ARGS__) \
		OPTION(MULTIPOLYGON, __VA_ARGS__) \
		OPTION(MULTICHULL, __VA_ARGS__) \
		OPTION(GEO_COLLECTION, __VA_ARGS__) \
		OPTION(GEO_INTERSECTION, __VA_ARGS__) \
		OPTION(CHAI, __VA_ARGS__) \
		OPTION(ECMA, __VA_ARGS__)

	enum class Hash : uint32_t {
		#define OPTION(name, arg) name = hh(RESERVED_##name),
		HASH_OPTIONS(Hash)
		#undef OPTION
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

	Hash getHash(std::string_view cast_word);
	FieldType getType(std::string_view cast_word);
};
