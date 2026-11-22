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

// Serialize's per-type value serializers: datetime, time, timedelta, numbers, uuid, boolean and geospatial.

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
#include "repr.hh"                                    // for repr
#include "reserved/fields.h"                           // for ID_FIELD_NAME
#include "serialize_list.h"                           // for StringList, CartesianList and RangeList
#include "sha256.h"                                   // for kronuz::sha256::hasher
#include "sortable.hh"                                // for sortable::encode, sortable::decode
#include "split.h"                                    // for Split
#include "strict_stox.hh"                             // for strict_stod, strict_stoll
#include "utype.hh"                                   // for toUType
#include "serialize_detail.h"

using serialize_detail::positive_of;


std::string
Serialize::datetime(std::string_view field_value)
{
	return datetime(Datetime::DatetimeParser(field_value));
}


std::string
Serialize::datetime(const class MsgPack& field_value)
{
	return datetime(Datetime::DatetimeParser(field_value));
}


std::string
Serialize::datetime(const class MsgPack& value, Datetime::tm_t& tm)
{
	tm = Datetime::DatetimeParser(value);
	return datetime(tm);
}


std::string
Serialize::time(std::string_view field_value)
{
	return timestamp(Datetime::time_to_double(Datetime::TimeParser(field_value)));
}


std::string
Serialize::time(const class MsgPack& field_value)
{
	return timestamp(Datetime::time_to_double(field_value));
}


std::string
Serialize::time(const class MsgPack& field_value, double& t_val)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			t_val = field_value.u64();
			return time(t_val);
		case MsgPack::Type::NEGATIVE_INTEGER:
			t_val = field_value.i64();
			return time(t_val);
		case MsgPack::Type::FLOAT:
			t_val = field_value.f64();
			return time(t_val);
		case MsgPack::Type::STR:
			t_val = Datetime::time_to_double(Datetime::TimeParser(field_value.str_view()));
			return timestamp(t_val);
		default:
			THROW(SerializationError, "Type: {} is not time", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::time(double field_value)
{
	if (Datetime::isvalidTime(field_value)) {
		return timestamp(field_value);
	}

	THROW(SerializationError, "Time: {} is out of range", field_value);
}


std::string
Serialize::timedelta(std::string_view field_value)
{
	return timestamp(Datetime::timedelta_to_double(Datetime::TimedeltaParser(field_value)));
}


std::string
Serialize::timedelta(const class MsgPack& field_value)
{
	return timestamp(Datetime::timedelta_to_double(field_value));
}


std::string
Serialize::timedelta(const class MsgPack& field_value, double& t_val)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			t_val = field_value.u64();
			return timedelta(t_val);
		case MsgPack::Type::NEGATIVE_INTEGER:
			t_val = field_value.i64();
			return timedelta(t_val);
		case MsgPack::Type::FLOAT:
			t_val = field_value.f64();
			return timedelta(t_val);
		case MsgPack::Type::STR:
			t_val = Datetime::timedelta_to_double(Datetime::TimedeltaParser(field_value.str_view()));
			return timestamp(t_val);
		default:
			THROW(SerializationError, "Type: {} is not timedelta", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::timedelta(double field_value)
{
	if (Datetime::isvalidTimedelta(field_value)) {
		return timestamp(field_value);
	}

	THROW(SerializationError, "Timedelta: {} is out of range", field_value);
}


std::string
Serialize::floating(std::string_view field_value)
{
	try {
		const double value = strict_stod(field_value);
		if (std::isnan(value)) {
			THROW(SerializationError, "Float cannot be NaN: {}", repr(field_value));
		}
		return floating(value);
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerializationError, "Invalid float format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerializationError, "Out of range float format: {}", repr(field_value));
	}
}


std::string
Serialize::integer(std::string_view field_value)
{
	try {
		return integer(strict_stoll(field_value));
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerializationError, "Invalid integer format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerializationError, "Out of range integer format: {}", repr(field_value));
	}
}


std::string
Serialize::positive(std::string_view field_value)
{
	int errno_save;
	const auto value = positive_of(&errno_save, field_value);
	switch (errno_save) {
		case 0:
			return positive(value);
		case ERANGE:
			THROW(SerializationError, "Out of range positive integer format: {}", repr(field_value));
		default:
			THROW(SerializationError, "Invalid positive integer format: {}", repr(field_value));
	}
}


std::string
Serialize::uuid(std::string_view field_value)
{
	auto field_value_sz = field_value.size();
	if (field_value_sz > 2) {
		Split<std::string_view> split(field_value, UUID_SEPARATOR_LIST);
		if (field_value.front() == '{' && field_value.back() == '}') {
			split = Split<std::string_view>(field_value.substr(1, field_value_sz - 2), UUID_SEPARATOR_LIST);
		} else if (field_value.compare(0, 9, "urn:uuid:") == 0) {
			split = Split<std::string_view>(field_value.substr(9), UUID_SEPARATOR_LIST);
		}
		std::string serialized;
		for (const auto& uuid : split) {
			auto uuid_sz = uuid.size();
			if (uuid_sz != 0u) {
				if (uuid_sz == UUID_LENGTH) {
					try {
					serialized.append(cuuid::serialize(UUID(uuid)));
						continue;
					} catch (const std::invalid_argument&) { }
				}
			#ifdef XAPIAND_UUID_ENCODED
				auto uuid_front = uuid.front();
				if (uuid_sz >= 7 && uuid_front == '~') {  // floor((4 * 8) / log2(59)) + 2
					try {
						auto decoded = UUID_ENCODER.decode(uuid);
						if (UUID::is_serialized(decoded)) {
							serialized.append(decoded);
							continue;
						}
					} catch (const std::invalid_argument&) { }
				}
			#endif
				THROW(SerializationError, "Invalid encoded UUID format: {}", repr(uuid));
			}
		}
		return serialized;
	}
	THROW(SerializationError, "Invalid UUID format: {}", repr(field_value));
}


std::string
Serialize::boolean(std::string_view field_value)
{
	switch (field_value.size()) {
		case 0:
			return std::string(1, SERIALIZED_FALSE);
		case 1:
			switch (field_value[0]) {
				case '0':
				case 'f':
				case 'F':
					return std::string(1, SERIALIZED_FALSE);
				case '1':
				case 't':
				case 'T':
					return std::string(1, SERIALIZED_TRUE);
			}
			break;
		case 4:
			switch (field_value[0]) {
				case 't':
				case 'T': {
					auto lower_value = strings::lower(field_value);
					if (lower_value == "true") {
						return std::string(1, SERIALIZED_TRUE);
					}
				}
			}
			break;
		case 5:
			switch (field_value[0]) {
				case 'f':
				case 'F': {
					auto lower_value = strings::lower(field_value);
					if (lower_value == "false") {
						return std::string(1, SERIALIZED_FALSE);
					}
				}
			}
			break;
	}

	THROW(SerializationError, "Boolean format is not valid: {}", repr(field_value));
}



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
