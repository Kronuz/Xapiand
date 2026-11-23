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

// Unserialize: serialized values turned back into MsgPack, strings and numbers.

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
#include "xapiand_strings.hh"                         // for strings::join
#include "geospatial/geospatial.h"                    // for GeoSpatial, EWKT
#include "htm.h"                           // for Cartesian, HTM_MAX_LENGTH_NAME, HTM_BYTES_ID, range_t
#include "msgpack.h"                                  // for MsgPack, object::object, type_error
#include "phf.hh"                                     // for phf
#include "repr.hh"                                    // for repr
#include "reserved/fields.h"                           // for ID_FIELD_NAME
#include "serialize_list.h"                           // for StringList, CartesianList and RangeList
#include "sha256.h"                                   // for kronuz::sha256::hasher
#include "sortable.hh"                                // for sortable::encode, sortable::decode
#include "split.h"                                    // for Split
#include "utype.hh"                                   // for toUType
#include "serialize_detail.h"


MsgPack
Unserialize::MsgPack(FieldType field_type, std::string_view serialized_val)
{
	class MsgPack result;
	switch (field_type) {
		case FieldType::floating:
			result = floating(serialized_val);
			break;
		case FieldType::integer:
			result = integer(serialized_val);
			break;
		case FieldType::positive:
			result = positive(serialized_val);
			break;
		case FieldType::date:
		case FieldType::datetime:
			result = datetime(serialized_val);
			break;
		case FieldType::time:
			result = time(serialized_val);
			break;
		case FieldType::timedelta:
			result = timedelta(serialized_val);
			break;
		case FieldType::boolean:
			result = boolean(serialized_val);
			break;
		case FieldType::keyword:
		case FieldType::text:
			result = serialized_val;
			break;
		case FieldType::geo: {
			const auto unser_geo = ranges_centroids(serialized_val);
			auto& ranges = result["Ranges"];
			int i = 0;
			for (const auto& range : unser_geo.first) {
				ranges[i++] = { range.start, range.end };
			}
			auto& centroids = result["Centroids"];
			i = 0;
			for (const auto& centroid : unser_geo.second) {
				centroids[i++] = { centroid.x, centroid.y, centroid.z };
			}
			break;
		}
		case FieldType::uuid:
			result = uuid(serialized_val);
			break;
		default:
			THROW(SerializationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}

	return result;
}


void
Unserialize::invalid_number(std::string_view serialized_number)
{
	THROW(SerializationError, "Not a serialized number: {}", repr(serialized_number));
}


sortable::number
Unserialize::number(std::string_view serialized_number)
{
	if (const auto value = sortable::decode(serialized_number)) {
		return *value;
	}
	invalid_number(serialized_number);
}


std::string
Unserialize::datetime(std::string_view serialized_date)
{
	return Datetime::iso8601(timestamp(serialized_date));
}


std::string
Unserialize::time(std::string_view serialized_time)
{
	return Datetime::time_to_string(floating(serialized_time));
}


double
Unserialize::time_d(std::string_view serialized_time)
{
	auto t = floating(serialized_time);
	if (Datetime::isvalidTime(t)) {
		return t;
	}

	THROW(SerializationError, "Unserialized time: {} is out of range", t);
}


std::string
Unserialize::timedelta(std::string_view serialized_timedelta)
{
	return Datetime::timedelta_to_string(floating(serialized_timedelta));
}


double
Unserialize::timedelta_d(std::string_view serialized_time)
{
	auto t = floating(serialized_time);
	if (Datetime::isvalidTimedelta(t)) {
		return t;
	}

	THROW(SerializationError, "Unserialized timedelta: {} is out of range", t);
}


std::string
Unserialize::uuid(std::string_view serialized_uuid, UUIDRepr repr)
{
	std::string result;
	std::vector<UUID> uuids;
	const auto codec = opts.legacy_ids ? cuuid::Codec::v1 : cuuid::Codec::v6;
	switch (repr) {
#ifdef XAPIAND_UUID_GUID
		case UUIDRepr::guid:
			// {00000000-0000-1000-8000-010000000000}
			cuuid::unserialize(serialized_uuid, std::back_inserter(uuids), codec);
			result.push_back('{');
			result.append(strings::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			result.push_back('}');
			break;
#endif
#ifdef XAPIAND_UUID_URN
		case UUIDRepr::urn:
			// urn:uuid:00000000-0000-1000-8000-010000000000
			cuuid::unserialize(serialized_uuid, std::back_inserter(uuids), codec);
			result.append("urn:uuid:");
			result.append(strings::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			break;
#endif
#ifdef XAPIAND_UUID_ENCODED
		case UUIDRepr::encoded:
			if (
				// Is UUID condensed
				serialized_uuid.front() != 1 && (
					// and compacted
					((serialized_uuid.back() & 1) != 0) ||
					// or has node multicast bit "on" for (uuid with Data)
					(serialized_uuid.size() > 5 && ((*(serialized_uuid.rbegin() + 5) & 2) != 0))
				)
			) {
				result.append("~" + UUID_ENCODER.encode(serialized_uuid));
				break;
			}
			[[fallthrough]];
#endif
		default:
		case UUIDRepr::vanilla:
			// 00000000-0000-1000-8000-010000000000
			cuuid::unserialize(serialized_uuid, std::back_inserter(uuids), codec);
			result.append(strings::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			break;
	}
	return result;
}


std::pair<RangeList, CartesianList>
Unserialize::ranges_centroids(std::string_view serialized_geo)
{
	StringList data(serialized_geo);
	switch (data.size()) {
		case 0:
			return std::make_pair(RangeList(std::string_view("")), CartesianList(std::string_view("")));
		case 1:
			return std::make_pair(RangeList(data.front()), CartesianList(std::string_view("")));
		case 2:
			return std::make_pair(RangeList(data.front()), CartesianList(data.back()));
		default:
			THROW(SerializationError, "Serialized geospatial must contain at most two elements");
	}
}


RangeList
Unserialize::ranges(std::string_view serialized_geo)
{
	StringList data(serialized_geo);
	switch (data.size()) {
		case 0:
			return RangeList(std::string_view(""));
		case 1:
		case 2:
			return RangeList(data.front());
		default:
			THROW(SerializationError, "Serialized geospatial must contain at most two elements");
	}
}


CartesianList
Unserialize::centroids(std::string_view serialized_geo)
{
	StringList data(serialized_geo);
	switch (data.size()) {
		case 0:
		case 1:
			return CartesianList(std::string_view(""));
		case 2:
			return CartesianList(data.back());
		default:
			THROW(SerializationError, "Serialized geospatial must contain at most two elements");
	}
}


Cartesian
Unserialize::cartesian(std::string_view serialized_val)
{
	if (serialized_val.size() != SERIALIZED_LENGTH_CARTESIAN) {
		THROW(SerializationError, "Cannot unserialize cartesian: {} [{}]", repr(serialized_val), serialized_val.size());
	}

	double x = (((uint32_t)serialized_val[0] << 24) & 0xFF000000) | (((uint32_t)serialized_val[1] << 16) & 0xFF0000) | (((uint32_t)serialized_val[2] << 8) & 0xFF00)  | (((uint32_t)serialized_val[3]) & 0xFF);
	double y = (((uint32_t)serialized_val[4] << 24) & 0xFF000000) | (((uint32_t)serialized_val[5] << 16) & 0xFF0000) | (((uint32_t)serialized_val[6] << 8) & 0xFF00)  | (((uint32_t)serialized_val[7]) & 0xFF);
	double z = (((uint32_t)serialized_val[8] << 24) & 0xFF000000) | (((uint32_t)serialized_val[9] << 16) & 0xFF0000) | (((uint32_t)serialized_val[10] << 8) & 0xFF00) | (((uint32_t)serialized_val[11]) & 0xFF);
	return {(x - MAXDOU2INT) / DOUBLE2INT, (y - MAXDOU2INT) / DOUBLE2INT, (z - MAXDOU2INT) / DOUBLE2INT};
}


uint64_t
Unserialize::trixel_id(std::string_view serialized_id)
{
	if (serialized_id.size() != HTM_BYTES_ID) {
		THROW(SerializationError, "Cannot unserialize trixel_id: {} [{}]", repr(serialized_id), serialized_id.size());
	}

	uint64_t id = (((uint64_t)serialized_id[0] << 48) & 0xFF000000000000) | (((uint64_t)serialized_id[1] << 40) & 0xFF0000000000) | \
				  (((uint64_t)serialized_id[2] << 32) & 0xFF00000000)     | (((uint64_t)serialized_id[3] << 24) & 0xFF000000)     | \
				  (((uint64_t)serialized_id[4] << 16) & 0xFF0000)         | (((uint64_t)serialized_id[5] <<  8) & 0xFF00)         | \
				  (serialized_id[6] & 0xFF);
	return id;
}


range_t
Unserialize::range(std::string_view serialized_range)
{
	if (serialized_range.size() != SERIALIZED_LENGTH_RANGE) {
		THROW(SerializationError, "Cannot unserialize range_t: {} [{}]", repr(serialized_range), serialized_range.size());
	}

	uint64_t start = (((uint64_t)serialized_range[0] << 48) & 0xFF000000000000) | (((uint64_t)serialized_range[1] << 40) & 0xFF0000000000) | \
					 (((uint64_t)serialized_range[2] << 32) & 0xFF00000000)     | (((uint64_t)serialized_range[3] << 24) & 0xFF000000)     | \
					 (((uint64_t)serialized_range[4] << 16) & 0xFF0000)         | (((uint64_t)serialized_range[5] <<  8) & 0xFF00)         | \
					 (serialized_range[6] & 0xFF);

	uint64_t end = (((uint64_t)serialized_range[7] << 48) & 0xFF000000000000) | (((uint64_t)serialized_range[8] << 40) & 0xFF0000000000) | \
				   (((uint64_t)serialized_range[9] << 32) & 0xFF00000000)     | (((uint64_t)serialized_range[10] << 24) & 0xFF000000)    | \
				   (((uint64_t)serialized_range[11] << 16) & 0xFF0000)        | (((uint64_t)serialized_range[12] <<  8) & 0xFF00)        | \
				   (serialized_range[13] & 0xFF);

	return {start, end};
}


FieldType
Unserialize::get_field_type(std::string_view str_type)
{
	constexpr static auto _ = phf::make_phf({
		hhl(" "),
		hhl("e"),
		hhl("a"),
		hhl("b"),
		hhl("d"),
		hhl("f"),
		hhl("g"),
		hhl("i"),
		hhl("o"),
		hhl("p"),
		hhl("s"),
		hhl("k"),
		hhl("u"),
		hhl("x"),
		hhl("t"),
		hhl("datetime"),
		hhl("term"),  // FIXME: remove legacy term
		hhl("text"),
		hhl("time"),
		hhl("array"),
		hhl("empty"),
		hhl("float"),
		hhl("floating"),
		hhl("object"),
		hhl("script"),
		hhl("string"),
		hhl("boolean"),
		hhl("foreign"),
		hhl("integer"),
		hhl("keyword"),
		hhl("positive"),
		hhl("timedelta"),
		hhl("geospatial"),
	});

	switch (_.fhhl(str_type)) {
		case _.fhhl(" "):
		case _.fhhl("e"):
		case _.fhhl("empty"):
			return FieldType::empty;
		case _.fhhl("a"):
		case _.fhhl("array"):
			return FieldType::array;
		case _.fhhl("b"):
		case _.fhhl("boolean"):
			return FieldType::boolean;
		case _.fhhl("d"):
		case _.fhhl("datetime"):
			return FieldType::datetime;
		case _.fhhl("f"):
		case _.fhhl("float"):
		case _.fhhl("floating"):
			return FieldType::floating;
		case _.fhhl("g"):
		case _.fhhl("geospatial"):
			return FieldType::geo;
		case _.fhhl("i"):
		case _.fhhl("integer"):
			return FieldType::integer;
		case _.fhhl("o"):
		case _.fhhl("object"):
			return FieldType::object;
		case _.fhhl("p"):
		case _.fhhl("positive"):
			return FieldType::positive;
		case _.fhhl("term"):  // FIXME: remove legacy term
		case _.fhhl("k"):
		case _.fhhl("keyword"):
			return FieldType::keyword;
		case _.fhhl("u"):
			return FieldType::uuid;
		case _.fhhl("x"):
		case _.fhhl("script"):
			return FieldType::script;
		case _.fhhl("s"):
		case _.fhhl("string"):
		case _.fhhl("t"):
		case _.fhhl("text"):
			return FieldType::text;
		case _.fhhl("time"):
			return FieldType::time;
		case _.fhhl("foreign"):
			return FieldType::foreign;
		case _.fhhl("timedelta"):
			return FieldType::timedelta;
		default:
			THROW(SerializationError, "Type: {} is an unsupported type", repr(str_type));
	}
}
