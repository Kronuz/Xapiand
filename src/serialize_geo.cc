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

// Serialize's geospatial serializers: ranges, centroids, trixel ids and ranges.

#include "serialize.h"
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads

#include <cerrno>                                     // for EINVAL, ERANGE
#include <cmath>                                      // for std::isnan
#include <cstdint>                                    // for int64_t, uint64_t
#include <limits>                                     // for std::numeric_limits
#include <optional>                                   // for std::optional
#include <stdexcept>                                  // for std::out_of_range, std::invalid_argument
#include <string_view>                                // for std::string_view

#include "base_x.hh"                                  // for base62
#include "cast.h"                                     // for Cast
#include "chars.hh"                                   // for iskeyword
#include "cuuid.hh"                                // for UUID, cuuid::serialize/unserialize dispatch
#include "opts.h"                                   // for opts.legacy_ids (v1 vs v6 codec)
#include "database/schema.h"                          // for FieldType, FieldType::keyword, Fiel...
#include "endian.hh"                                  // for htobe32, htobe56
#include "exception_engine.h"                          // for CastError, SerializationError
#include "xapiand_exception.h"                                // for SerializationError, ...
#include "geospatial/geospatial.h"                    // for GeoSpatial, EWKT
#include "htm.h"                           // for Cartesian, HTM_MAX_LENGTH_NAME, HTM_BYTES_ID, range_t
#include "msgpack.h"                                  // for MsgPack, object::object, type_error
#include "phf.hh"                                     // for phf
#include "query_dsl.h"                                // for QUERYDSL_FROM, QUERYDSL_TO
#include "repr.hh"                                    // for repr
#include "reserved/fields.h"                           // for ID_FIELD_NAME
#include "serialize_list.h"                           // for StringList, CartesianList and RangeList
#include "sha256.h"                                   // for kronuz::sha256::hasher
#include "sortable.hh"                                // for sortable::encode, sortable::decode
#include "split.h"                                    // for Split
#include "utype.hh"                                   // for toUType


std::string
Serialize::ranges_centroids(const std::vector<range_t>& ranges, const std::vector<Cartesian>& centroids)
{
	std::vector<std::string> data = { RangeList::serialize(ranges.begin(), ranges.end()), CartesianList::serialize(centroids.begin(), centroids.end()) };
	return StringList::serialize(data.begin(), data.end());
}


std::string
Serialize::ranges(const std::vector<range_t>& ranges)
{
	return RangeList::serialize(ranges.begin(), ranges.end());
}


std::string
Serialize::centroids(const std::vector<Cartesian>& centroids)
{
	std::vector<std::string> data = { "", CartesianList::serialize(centroids.begin(), centroids.end()) };
	return StringList::serialize(data.begin(), data.end());
}


// The first 16 bytes of SHA-256 over the ranges' fixed-width encodings: n distinct range lists collide with
// probability at most n(n-1)/2^129, and the term's 16 bytes never equal an older 1 to 10 byte length-prefixed hash.
std::string
Serialize::ranges_hash(const std::vector<range_t>& ranges)
{
	kronuz::sha256::hasher hasher;
	for (const auto& r : ranges) {
		hasher.update(range(r));
	}
	const auto digest = hasher.final();
	return std::string(reinterpret_cast<const char*>(digest.data()), SERIALIZED_LENGTH_RANGES_HASH);
}


std::string
Serialize::geospatial(const class MsgPack& field_value)
{
	GeoSpatial geo(field_value);
	return Serialize::ranges_hash(geo.getGeometry()->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR));
}


std::string
Serialize::geospatial(std::string_view field_value)
{
	EWKT ewkt(field_value);
	return Serialize::ranges_hash(ewkt.getGeometry()->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR));
}


std::string
Serialize::cartesian(const Cartesian& norm_cartesian)
{
	uint32_t x = htobe32((uint32_t)((long long)(norm_cartesian.x * DOUBLE2INT) + MAXDOU2INT));
	uint32_t y = htobe32((uint32_t)((long long)(norm_cartesian.y * DOUBLE2INT) + MAXDOU2INT));
	uint32_t z = htobe32((uint32_t)((long long)(norm_cartesian.z * DOUBLE2INT) + MAXDOU2INT));
	const char serialized[] = { (char)(x & 0xFF), (char)((x >> 8) & 0xFF), (char)((x >> 16) & 0xFF), (char)((x >> 24) & 0xFF),
								(char)(y & 0xFF), (char)((y >> 8) & 0xFF), (char)((y >> 16) & 0xFF), (char)((y >> 24) & 0xFF),
								(char)(z & 0xFF), (char)((z >> 8) & 0xFF), (char)((z >> 16) & 0xFF), (char)((z >> 24) & 0xFF) };
	return std::string(serialized, SERIALIZED_LENGTH_CARTESIAN);
}


std::string
Serialize::trixel_id(uint64_t id)
{
	id = htobe56(id);
	const char serialized[] = { (char)(id & 0xFF), (char)((id >> 8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF),
								(char)((id >> 32) & 0xFF), (char)((id >> 40) & 0xFF), (char)((id >> 48) & 0xFF) };
	return std::string(serialized, HTM_BYTES_ID);
}


std::string
Serialize::range(const range_t& range)
{
	uint64_t start = htobe56(range.start);
	uint64_t end = htobe56(range.end);
	const char serialized[] = { (char)(start & 0xFF), (char)((start >> 8) & 0xFF), (char)((start >> 16) & 0xFF), (char)((start >> 24) & 0xFF),
								(char)((start >> 32) & 0xFF), (char)((start >> 40) & 0xFF), (char)((start >> 48) & 0xFF),
								(char)(end & 0xFF), (char)((end >> 8) & 0xFF), (char)((end >> 16) & 0xFF), (char)((end >> 24) & 0xFF),
								(char)((end >> 32) & 0xFF), (char)((end >> 40) & 0xFF), (char)((end >> 48) & 0xFF) };
	return std::string(serialized, SERIALIZED_LENGTH_RANGE);
}
