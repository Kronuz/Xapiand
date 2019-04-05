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

#include "serialise.h"

#include <stdexcept>                                  // for std::out_of_range, std::invalid_argument
#include <string_view>                                // for std::string_view

#include "base_x.hh"                                  // for base62
#include "cast.h"                                     // for Cast
#include "chars.hh"                                   // for iskeyword
#include "cuuid/uuid.h"                               // for UUID
#include "database/schema.h"                          // for FieldType, FieldType::keyword, Fiel...
#include "endian.hh"                                  // for htobe32, htobe56
#include "exception.h"                                // for SerialisationError, ...
#include "geospatial/geospatial.h"                    // for GeoSpatial, EWKT
#include "geospatial/htm.h"                           // for Cartesian, HTM_MAX_LENGTH_NAME, HTM_BYTES_ID, range_t
#include "msgpack.h"                                  // for MsgPack, object::object, type_error
#include "nameof.hh"                                  // for NAMEOF_ENUM
#include "phf.hh"                                     // for phf
#include "query_dsl.h"                                // for QUERYDSL_FROM, QUERYDSL_TO
#include "repr.hh"                                    // for repr
#include "serialise_list.h"                           // for StringList, CartesianList and RangeList
#include "split.h"                                    // for Split
#include "utype.hh"                                   // for toUType


constexpr char UUID_SEPARATOR_LIST = ';';

#ifdef XAPIAND_UUID_ENCODED
#define UUID_ENCODER (Base59::dubaluchk())
#endif


bool
Serialise::isText(std::string_view field_value) noexcept
{
	auto size = field_value.size();
	if (!size || size >= 128) {
		return true;
	}
	for (size_t i = 0; i < size; ++i) {
		if (!chars::is_keyword(field_value[i])) {
			return true;
		}
	}
	return false;
}


bool
Serialise::possiblyUUID(std::string_view field_value) noexcept
{
	auto field_value_sz = field_value.size();
	if (field_value_sz > 2) {
		Split<std::string_view> split(field_value, UUID_SEPARATOR_LIST);
		if (field_value.front() == '{' && field_value.back() == '}') {
			split = Split<std::string_view>(field_value.substr(1, field_value_sz - 2), UUID_SEPARATOR_LIST);
		} else if (field_value.compare(0, 9, "urn:uuid:") == 0) {
			split = Split<std::string_view>(field_value.substr(9), UUID_SEPARATOR_LIST);
		}
		for (const auto& uuid : split) {
			auto uuid_sz = uuid.size();
			if (uuid_sz != 0u) {
				if (uuid_sz == UUID_LENGTH) {
					if (UUID::is_valid(uuid)) {
						continue;
					}
				}
#ifdef XAPIAND_UUID_ENCODED
				if (uuid_sz >= 7 && uuid.front() == '~') {  // floor((4 * 8) / log2(59)) + 2
					if (UUID_ENCODER.is_valid(uuid)) {
						continue;
					}
				}
#endif
				return false;
			}
		}
		return true;
	}
	return false;
}


bool
Serialise::isUUID(std::string_view field_value) noexcept
{
	auto field_value_sz = field_value.size();
	if (field_value_sz > 2) {
		Split<std::string_view> split(field_value, UUID_SEPARATOR_LIST);
		if (field_value.front() == '{' && field_value.back() == '}') {
			split = Split<std::string_view>(field_value.substr(1, field_value_sz - 2), UUID_SEPARATOR_LIST);
		} else if (field_value.compare(0, 9, "urn:uuid:") == 0) {
			split = Split<std::string_view>(field_value.substr(9), UUID_SEPARATOR_LIST);
		}
		for (const auto& uuid : split) {
			auto uuid_sz = uuid.size();
			if (uuid_sz != 0u) {
				if (uuid_sz == UUID_LENGTH) {
					if (UUID::is_valid(uuid)) {
						continue;
					}
				}
#ifdef XAPIAND_UUID_ENCODED
				if (uuid_sz >= 7 && uuid.front() == '~') {  // floor((4 * 8) / log2(59)) + 2
					try {
						auto decoded = UUID_ENCODER.decode(uuid);
						if (UUID::is_serialised(decoded)) {
							continue;
						}
					} catch (const std::invalid_argument&) { }
				}
#endif
				return false;
			}
		}
		return true;
	}
	return false;
}


std::string
Serialise::MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::BOOLEAN:
			return boolean(field_spc.get_type(), field_value.boolean());
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.i64());
		case MsgPack::Type::FLOAT:
			return floating(field_spc.get_type(), field_value.f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.str_view());
		case MsgPack::Type::MAP:
			return object(field_spc, field_value);
		default:
			THROW(SerialisationError, "msgpack::type {} is not supported", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::object(const required_spc_t& field_spc, const class MsgPack& o)
{
	if (o.size() == 1) {
		auto str_key = o.begin()->str_view();
		if (str_key.empty() || str_key[0] != reserved__) {
			THROW(SerialisationError, "Unknown cast type: {}", repr(str_key));
		}
		switch (Cast::get_hash_type(str_key)) {
			case Cast::HashType::INTEGER:
				return integer(field_spc.get_type(), Cast::integer(o.at(str_key)));
			case Cast::HashType::POSITIVE:
				return positive(field_spc.get_type(), Cast::positive(o.at(str_key)));
			case Cast::HashType::FLOAT:
				return floating(field_spc.get_type(), Cast::floating(o.at(str_key)));
			case Cast::HashType::BOOLEAN:
				return boolean(field_spc.get_type(), Cast::boolean(o.at(str_key)));
			case Cast::HashType::KEYWORD:
			case Cast::HashType::TEXT:
			case Cast::HashType::STRING:
				return string(field_spc, Cast::string(o.at(str_key)));
			case Cast::HashType::UUID:
				return string(field_spc, Cast::uuid(o.at(str_key)));
			case Cast::HashType::DATETIME:
				return datetime(field_spc, Cast::datetime(o.at(str_key)));
			case Cast::HashType::TIME:
				return time(field_spc, Cast::time(o.at(str_key)));
			case Cast::HashType::TIMEDELTA:
				return timedelta(field_spc, Cast::timedelta(o.at(str_key)));
			case Cast::HashType::EWKT:
				return string(field_spc, Cast::ewkt(o.at(str_key)));
			case Cast::HashType::POINT:
			case Cast::HashType::CIRCLE:
			case Cast::HashType::CONVEX:
			case Cast::HashType::POLYGON:
			case Cast::HashType::CHULL:
			case Cast::HashType::MULTIPOINT:
			case Cast::HashType::MULTICIRCLE:
			case Cast::HashType::MULTIPOLYGON:
			case Cast::HashType::MULTICHULL:
			case Cast::HashType::GEO_COLLECTION:
			case Cast::HashType::GEO_INTERSECTION:
				return geospatial(field_spc.get_type(), o);
			default:
				THROW(SerialisationError, "Unknown cast type {}", repr(str_key));
		}
	}

	THROW(SerialisationError, "Expected map with one element");
}


std::string
Serialise::serialise(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	auto field_type = field_spc.get_type();

	switch (field_type) {
		case FieldType::integer:
			return integer(field_value.i64());
		case FieldType::positive:
			return positive(field_value.u64());
		case FieldType::floating:
			return floating(field_value.f64());
		case FieldType::date:
		case FieldType::datetime:
			return datetime(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::boolean:
			return boolean(field_value.boolean());
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			return field_value.str();
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value.str_view());
		default:
			THROW(SerialisationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


std::string
Serialise::serialise(const required_spc_t& field_spc, std::string_view field_value)
{
	auto field_type = field_spc.get_type();

	switch (field_type) {
		case FieldType::integer:
			return integer(field_value);
		case FieldType::positive:
			return positive(field_value);
		case FieldType::floating:
			return floating(field_value);
		case FieldType::date:
		case FieldType::datetime:
			return datetime(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::boolean:
			return boolean(field_value);
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			return std::string(field_value);
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value);
		default:
			THROW(SerialisationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


std::string
Serialise::string(const required_spc_t& field_spc, std::string_view field_value)
{
	switch (field_spc.get_type()) {
		case FieldType::date:
		case FieldType::datetime:
			return datetime(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::boolean:
			return boolean(field_value);
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			return std::string(field_value);
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value);
		default:
			THROW(SerialisationError, "Type: {} is not string", NAMEOF_ENUM(field_spc.get_type()));
	}
}


std::string
Serialise::datetime(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.i64());
		case MsgPack::Type::FLOAT:
			return floating(field_spc.get_type(), field_value.f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.str_view());
		case MsgPack::Type::MAP:
			switch (field_spc.get_type()) {
				case FieldType::floating:
					return floating(Datetime::timestamp(Datetime::DatetimeParser(field_value)));
				case FieldType::date:
				case FieldType::datetime:
					return datetime(field_value);
				case FieldType::time:
					return time(Datetime::timestamp(Datetime::DatetimeParser(field_value)));
				case FieldType::timedelta:
					return timedelta(Datetime::timestamp(Datetime::DatetimeParser(field_value)));
				case FieldType::string:
					return Datetime::iso8601(Datetime::DatetimeParser(field_value));
				default:
					THROW(SerialisationError, "Type: {} is not a datetime", NAMEOF_ENUM(field_value.get_type()));
			}
		default:
			THROW(SerialisationError, "Type: {} is not a datetime", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::time(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.i64());
		case MsgPack::Type::FLOAT:
			return floating(field_spc.get_type(), field_value.f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.str_view());
		default:
			THROW(SerialisationError, "Type: {} is not a time", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::timedelta(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.i64());
		case MsgPack::Type::FLOAT:
			return floating(field_spc.get_type(), field_value.f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.str_view());
		default:
			THROW(SerialisationError, "Type: {} is not a timedelta", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::floating(FieldType field_type, long double field_value)
{
	switch (field_type) {
		case FieldType::date:
		case FieldType::datetime:
			return timestamp(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::floating:
			return floating(field_value);
		default:
			THROW(SerialisationError, "Type: {} is not a float", NAMEOF_ENUM(field_type));
	}
}


std::string
Serialise::integer(FieldType field_type, int64_t field_value)
{
	switch (field_type) {
		case FieldType::positive:
			if (field_value < 0) {
				THROW(SerialisationError, "Type: {} must be a positive number [{}]", NAMEOF_ENUM(field_type), field_value);
			}
			return positive(field_value);
		case FieldType::date:
		case FieldType::datetime:
			return timestamp(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::floating:
			return floating(field_value);
		case FieldType::integer:
			return integer(field_value);
		default:
			THROW(SerialisationError, "Type: {} is not a integer [{}]", NAMEOF_ENUM(field_type), field_value);
	}
}


std::string
Serialise::positive(FieldType field_type, uint64_t field_value)
{
	switch (field_type) {
		case FieldType::date:
		case FieldType::datetime:
			return timestamp(field_value);
		case FieldType::floating:
			return floating(field_value);
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::integer:
			return integer(field_value);
		case FieldType::positive:
			return positive(field_value);
		default:
			THROW(SerialisationError, "Type: {} is not a positive integer [{}]", NAMEOF_ENUM(field_type), field_value);
	}
}


std::string
Serialise::boolean(FieldType field_type, bool field_value)
{
	if (field_type == FieldType::boolean) {
		return boolean(field_value);
	}

	THROW(SerialisationError, "Type: {} is not boolean", NAMEOF_ENUM(field_type));
}


std::string
Serialise::geospatial(FieldType field_type, const class MsgPack& field_value)
{
	if (field_type == FieldType::geo) {
		return geospatial(field_value);
	}

	THROW(SerialisationError, "Type: {} is not geospatial", NAMEOF_ENUM(field_type));
}


std::string
Serialise::datetime(std::string_view field_value)
{
	return datetime(Datetime::DatetimeParser(field_value));
}


std::string
Serialise::datetime(const class MsgPack& field_value)
{
	return datetime(Datetime::DatetimeParser(field_value));
}


std::string
Serialise::datetime(const class MsgPack& value, Datetime::tm_t& tm)
{
	tm = Datetime::DatetimeParser(value);
	return datetime(tm);
}


std::string
Serialise::time(std::string_view field_value)
{
	return timestamp(Datetime::time_to_double(Datetime::TimeParser(field_value)));
}


std::string
Serialise::time(const class MsgPack& field_value)
{
	return timestamp(Datetime::time_to_double(field_value));
}


std::string
Serialise::time(const class MsgPack& field_value, double& t_val)
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
			THROW(SerialisationError, "Type: {} is not time", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::time(double field_value)
{
	if (Datetime::isvalidTime(field_value)) {
		return timestamp(field_value);
	}

	THROW(SerialisationError, "Time: {} is out of range", field_value);
}


std::string
Serialise::timedelta(std::string_view field_value)
{
	return timestamp(Datetime::timedelta_to_double(Datetime::TimedeltaParser(field_value)));
}


std::string
Serialise::timedelta(const class MsgPack& field_value)
{
	return timestamp(Datetime::timedelta_to_double(field_value));
}


std::string
Serialise::timedelta(const class MsgPack& field_value, double& t_val)
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
			THROW(SerialisationError, "Type: {} is not timedelta", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::string
Serialise::timedelta(double field_value)
{
	if (Datetime::isvalidTimedelta(field_value)) {
		return timestamp(field_value);
	}

	THROW(SerialisationError, "Timedelta: {} is out of range", field_value);
}


std::string
Serialise::floating(std::string_view field_value)
{
	try {
		return floating(strict_stold(field_value));
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerialisationError, "Invalid float format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerialisationError, "Out of range float format: {}", repr(field_value));
	}
}


std::string
Serialise::integer(std::string_view field_value)
{
	try {
		return integer(strict_stoll(field_value));
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerialisationError, "Invalid integer format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerialisationError, "Out of range integer format: {}", repr(field_value));
	}
}


std::string
Serialise::positive(std::string_view field_value)
{
	try {
		return positive(strict_stoull(field_value));
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerialisationError, "Invalid positive integer format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerialisationError, "Out of range positive integer format: {}", repr(field_value));
	}
}


std::string
Serialise::uuid(std::string_view field_value)
{
	auto field_value_sz = field_value.size();
	if (field_value_sz > 2) {
		Split<std::string_view> split(field_value, UUID_SEPARATOR_LIST);
		if (field_value.front() == '{' && field_value.back() == '}') {
			split = Split<std::string_view>(field_value.substr(1, field_value_sz - 2), UUID_SEPARATOR_LIST);
		} else if (field_value.compare(0, 9, "urn:uuid:") == 0) {
			split = Split<std::string_view>(field_value.substr(9), UUID_SEPARATOR_LIST);
		}
		std::string serialised;
		for (const auto& uuid : split) {
			auto uuid_sz = uuid.size();
			if (uuid_sz != 0u) {
				if (uuid_sz == UUID_LENGTH) {
					try {
						serialised.append(UUID(uuid).serialise());
						continue;
					} catch (const std::invalid_argument&) { }
				}
			#ifdef XAPIAND_UUID_ENCODED
				auto uuid_front = uuid.front();
				if (uuid_sz >= 7 && uuid_front == '~') {  // floor((4 * 8) / log2(59)) + 2
					try {
						auto decoded = UUID_ENCODER.decode(uuid);
						if (UUID::is_serialised(decoded)) {
							serialised.append(decoded);
							continue;
						}
					} catch (const std::invalid_argument&) { }
				}
			#endif
				THROW(SerialisationError, "Invalid encoded UUID format in: {}", uuid);
			}
		}
		return serialised;
	}
	THROW(SerialisationError, "Invalid UUID format in: {}", repr(field_value));
}


std::string
Serialise::boolean(std::string_view field_value)
{
	switch (field_value.size()) {
		case 0:
			return std::string(1, SERIALISED_FALSE);
		case 1:
			switch (field_value[0]) {
				case '0':
				case 'f':
				case 'F':
					return std::string(1, SERIALISED_FALSE);
				case '1':
				case 't':
				case 'T':
					return std::string(1, SERIALISED_TRUE);
			}
			break;
		case 4:
			switch (field_value[0]) {
				case 't':
				case 'T': {
					auto lower_value = string::lower(field_value);
					if (lower_value == "true") {
						return std::string(1, SERIALISED_TRUE);
					}
				}
			}
			break;
		case 5:
			switch (field_value[0]) {
				case 'f':
				case 'F': {
					auto lower_value = string::lower(field_value);
					if (lower_value == "false") {
						return std::string(1, SERIALISED_FALSE);
					}
				}
			}
			break;
	}

	THROW(SerialisationError, "Boolean format is not valid: {}", repr(field_value));
}


std::string
Serialise::geospatial(std::string_view field_value)
{
	EWKT ewkt(field_value);
	return Serialise::ranges(ewkt.getGeometry()->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR));
}


std::string
Serialise::geospatial(const class MsgPack& field_value)
{
	GeoSpatial geo(field_value);
	return Serialise::ranges(geo.getGeometry()->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR));
}


std::string
Serialise::ranges_centroids(const std::vector<range_t>& ranges, const std::vector<Cartesian>& centroids)
{
	std::vector<std::string> data = { RangeList::serialise(ranges.begin(), ranges.end()), CartesianList::serialise(centroids.begin(), centroids.end()) };
	return StringList::serialise(data.begin(), data.end());
}


std::string
Serialise::ranges(const std::vector<range_t>& ranges)
{
	if (ranges.empty()) {
		return "";
	}

	size_t hash = 0;
	for (const auto& range : ranges) {
		hash ^= std::hash<range_t>{}(range);
	}

	return sortable_serialise(hash);
}


std::string
Serialise::cartesian(const Cartesian& norm_cartesian)
{
	uint32_t x = htobe32(((uint32_t)(norm_cartesian.x * DOUBLE2INT) + MAXDOU2INT));
	uint32_t y = htobe32(((uint32_t)(norm_cartesian.y * DOUBLE2INT) + MAXDOU2INT));
	uint32_t z = htobe32(((uint32_t)(norm_cartesian.z * DOUBLE2INT) + MAXDOU2INT));
	const char serialised[] = { (char)(x & 0xFF), (char)((x >> 8) & 0xFF), (char)((x >> 16) & 0xFF), (char)((x >> 24) & 0xFF),
								(char)(y & 0xFF), (char)((y >> 8) & 0xFF), (char)((y >> 16) & 0xFF), (char)((y >> 24) & 0xFF),
								(char)(z & 0xFF), (char)((z >> 8) & 0xFF), (char)((z >> 16) & 0xFF), (char)((z >> 24) & 0xFF) };
	return std::string(serialised, SERIALISED_LENGTH_CARTESIAN);
}


std::string
Serialise::trixel_id(uint64_t id)
{
	id = htobe56(id);
	const char serialised[] = { (char)(id & 0xFF), (char)((id >> 8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF),
								(char)((id >> 32) & 0xFF), (char)((id >> 40) & 0xFF), (char)((id >> 48) & 0xFF) };
	return std::string(serialised, HTM_BYTES_ID);
}


std::string
Serialise::range(const range_t& range)
{
	uint64_t start = htobe56(range.start);
	uint64_t end = htobe56(range.end);
	const char serialised[] = { (char)(start & 0xFF), (char)((start >> 8) & 0xFF), (char)((start >> 16) & 0xFF), (char)((start >> 24) & 0xFF),
								(char)((start >> 32) & 0xFF), (char)((start >> 40) & 0xFF), (char)((start >> 48) & 0xFF),
								(char)(end & 0xFF), (char)((end >> 8) & 0xFF), (char)((end >> 16) & 0xFF), (char)((end >> 24) & 0xFF),
								(char)((end >> 32) & 0xFF), (char)((end >> 40) & 0xFF), (char)((end >> 48) & 0xFF) };
	return std::string(serialised, SERIALISED_LENGTH_RANGE);
}


FieldType
Serialise::guess_type(const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return FieldType::integer;

		case MsgPack::Type::POSITIVE_INTEGER:
			return FieldType::positive;

		case MsgPack::Type::FLOAT:
			return FieldType::floating;

		case MsgPack::Type::BOOLEAN:
			return FieldType::boolean;

		case MsgPack::Type::STR: {
			const auto str_value = field_value.str_view();

			if (isUUID(str_value)) {
				return FieldType::uuid;
			}

			if (Datetime::isDate(str_value)) {
				return FieldType::date;
			}

			if (Datetime::isDatetime(str_value)) {
				return FieldType::datetime;
			}

			if (Datetime::isTime(str_value)) {
				return FieldType::time;
			}

			if (Datetime::isTimedelta(str_value)) {
				return FieldType::timedelta;
			}

			if (EWKT::isEWKT(str_value)) {
				return FieldType::geo;
			}

			// Try like INTEGER.
			{
				int errno_save;
				strict_stoll(&errno_save, str_value);
				if (errno_save == 0) {
					return FieldType::integer;
				}
			}

			// Try like POSITIVE.
			{
				int errno_save;
				strict_stoull(&errno_save, str_value);
				if (errno_save == 0) {
					return FieldType::positive;
				}
			}

			// Try like FLOAT
			{
				int errno_save;
				strict_stold(&errno_save, str_value);
				if (errno_save == 0) {
					return FieldType::floating;
				}
			}

			// Try like TEXT
			if (isText(str_value)) {
				return FieldType::text;
			}

			return FieldType::keyword;
		}

		case MsgPack::Type::MAP: {
			if (field_value.size() == 1) {
				const auto str_key = field_value.begin()->str_view();
				if (str_key.empty() || str_key[0] != reserved__) {
					THROW(SerialisationError, "Unknown cast type: {}", repr(str_key));
				}
				switch (Cast::get_hash_type(str_key)) {
					case Cast::HashType::INTEGER:
						return FieldType::integer;
					case Cast::HashType::POSITIVE:
						return FieldType::positive;
					case Cast::HashType::FLOAT:
						return FieldType::floating;
					case Cast::HashType::BOOLEAN:
						return FieldType::boolean;
					case Cast::HashType::KEYWORD:
						return FieldType::keyword;
					case Cast::HashType::TEXT:
						return FieldType::text;
					case Cast::HashType::STRING:
						return FieldType::string;
					case Cast::HashType::UUID:
						return FieldType::uuid;
					case Cast::HashType::DATETIME:
						return FieldType::datetime;
					case Cast::HashType::TIME:
						return FieldType::time;
					case Cast::HashType::TIMEDELTA:
						return FieldType::timedelta;
					case Cast::HashType::EWKT:
					case Cast::HashType::POINT:
					case Cast::HashType::CIRCLE:
					case Cast::HashType::CONVEX:
					case Cast::HashType::POLYGON:
					case Cast::HashType::CHULL:
					case Cast::HashType::MULTIPOINT:
					case Cast::HashType::MULTICIRCLE:
					case Cast::HashType::MULTIPOLYGON:
					case Cast::HashType::MULTICHULL:
					case Cast::HashType::GEO_COLLECTION:
					case Cast::HashType::GEO_INTERSECTION:
						return FieldType::geo;
					default:
						THROW(SerialisationError, "Unknown cast type: {}", repr(str_key));
				}
			} else {
				THROW(SerialisationError, "Expected map with one element");
			}
		}

		default:
			THROW(SerialisationError, "Unexpected type {}", NAMEOF_ENUM(field_value.get_type()));
	}
}


std::pair<FieldType, std::string>
Serialise::guess_serialise(const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return std::make_pair(FieldType::integer, integer(field_value.i64()));

		case MsgPack::Type::POSITIVE_INTEGER:
			return std::make_pair(FieldType::positive, positive(field_value.u64()));

		case MsgPack::Type::FLOAT:
			return std::make_pair(FieldType::floating, floating(field_value.f64()));

		case MsgPack::Type::BOOLEAN:
			return std::make_pair(FieldType::boolean, boolean(field_value.boolean()));

		case MsgPack::Type::STR: {
			auto str_value = field_value.str_view();

			// Try like UUID
			try {
				return std::make_pair(FieldType::uuid, uuid(str_value));
			} catch (const SerialisationError&) { }

			// Try like DATETIME
			try {
				return std::make_pair(FieldType::datetime, datetime(str_value));
			} catch (const DatetimeError&) { }

			// Try like TIME
			try {
				return std::make_pair(FieldType::time, time(str_value));
			} catch (const TimeError&) { }

			// Try like TIMEDELTA
			try {
				return std::make_pair(FieldType::timedelta, timedelta(str_value));
			} catch (const TimedeltaError&) { }

			// Try like GEO
			try {
				return std::make_pair(FieldType::geo, geospatial(str_value));
			} catch (const EWKTError&) { }

			// Try like INTEGER.
			try {
				return std::make_pair(FieldType::integer, integer(str_value));
			} catch (const SerialisationError&) { }

			// Try like POSITIVE.
			try {
				return std::make_pair(FieldType::positive, positive(str_value));
			} catch (const SerialisationError&) { }

			// Try like FLOAT
			try {
				return std::make_pair(FieldType::floating, floating(str_value));
			} catch (const SerialisationError&) { }

			// Try like TEXT
			if (isText(str_value)) {
				return std::make_pair(FieldType::text, std::string(str_value));
			}

			// Default type KEYWORD.
			return std::make_pair(FieldType::keyword, std::string(str_value));
		}

		case MsgPack::Type::MAP: {
			if (field_value.size() == 1) {
				const auto it = field_value.begin();
				const auto str_key = it->str_view();
				if (str_key.empty() || str_key[0] != reserved__) {
					THROW(SerialisationError, "Unknown cast type: {}", repr(str_key));
				}
				switch (Cast::get_hash_type(str_key)) {
					case Cast::HashType::INTEGER:
						return std::make_pair(FieldType::integer, integer(Cast::integer(it.value())));
					case Cast::HashType::POSITIVE:
						return std::make_pair(FieldType::positive, positive(Cast::positive(it.value())));
					case Cast::HashType::FLOAT:
						return std::make_pair(FieldType::floating, floating(Cast::floating(it.value())));
					case Cast::HashType::BOOLEAN:
						return std::make_pair(FieldType::boolean, boolean(Cast::boolean(it.value())));
					case Cast::HashType::KEYWORD:
						return std::make_pair(FieldType::keyword, Cast::string(it.value()));
					case Cast::HashType::TEXT:
						return std::make_pair(FieldType::text, Cast::string(it.value()));
					case Cast::HashType::STRING:
						return std::make_pair(FieldType::string, Cast::string(it.value()));
					case Cast::HashType::UUID:
						return std::make_pair(FieldType::uuid, uuid(Cast::uuid(it.value())));
					case Cast::HashType::DATETIME:
						return std::make_pair(FieldType::datetime, datetime(Cast::datetime(it.value())));
					case Cast::HashType::TIME:
						return std::make_pair(FieldType::time, datetime(Cast::time(it.value())));
					case Cast::HashType::TIMEDELTA:
						return std::make_pair(FieldType::timedelta, datetime(Cast::timedelta(it.value())));
					case Cast::HashType::EWKT:
					case Cast::HashType::POINT:
					case Cast::HashType::CIRCLE:
					case Cast::HashType::CONVEX:
					case Cast::HashType::POLYGON:
					case Cast::HashType::CHULL:
					case Cast::HashType::MULTIPOINT:
					case Cast::HashType::MULTICIRCLE:
					case Cast::HashType::MULTIPOLYGON:
					case Cast::HashType::MULTICHULL:
					case Cast::HashType::GEO_COLLECTION:
					case Cast::HashType::GEO_INTERSECTION:
						return std::make_pair(FieldType::geo, geospatial(field_value));
					default:
						THROW(SerialisationError, "Unknown cast type: {}", repr(str_key));
				}
			} else {
				THROW(SerialisationError, "Expected map with one element");
			}
		}

		default:
			THROW(SerialisationError, "Unexpected type {}", NAMEOF_ENUM(field_value.get_type()));
	}
}


MsgPack
Unserialise::MsgPack(FieldType field_type, std::string_view serialised_val)
{
	class MsgPack result;
	switch (field_type) {
		case FieldType::floating:
			result = static_cast<double>(floating(serialised_val));
			break;
		case FieldType::integer:
			result = integer(serialised_val);
			break;
		case FieldType::positive:
			result = positive(serialised_val);
			break;
		case FieldType::date:
		case FieldType::datetime:
			result = datetime(serialised_val);
			break;
		case FieldType::time:
			result = time(serialised_val);
			break;
		case FieldType::timedelta:
			result = timedelta(serialised_val);
			break;
		case FieldType::boolean:
			result = boolean(serialised_val);
			break;
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			result = serialised_val;
			break;
		case FieldType::geo: {
			const auto unser_geo = ranges_centroids(serialised_val);
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
			result = uuid(serialised_val);
			break;
		default:
			THROW(SerialisationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}

	return result;
}


std::string
Unserialise::datetime(std::string_view serialised_date)
{
	return Datetime::iso8601(timestamp(serialised_date));
}


std::string
Unserialise::time(std::string_view serialised_time)
{
	return Datetime::time_to_string(sortable_unserialise(serialised_time));
}


double
Unserialise::time_d(std::string_view serialised_time)
{
	auto t = sortable_unserialise(serialised_time);
	if (Datetime::isvalidTime(t)) {
		return t;
	}

	THROW(SerialisationError, "Unserialised time: {} is out of range", t);
}


std::string
Unserialise::timedelta(std::string_view serialised_timedelta)
{
	return Datetime::timedelta_to_string(sortable_unserialise(serialised_timedelta));
}


double
Unserialise::timedelta_d(std::string_view serialised_time)
{
	auto t = sortable_unserialise(serialised_time);
	if (Datetime::isvalidTimedelta(t)) {
		return t;
	}

	THROW(SerialisationError, "Unserialised timedelta: {} is out of range", t);
}


std::string
Unserialise::uuid(std::string_view serialised_uuid, UUIDRepr repr)
{
	std::string result;
	std::vector<UUID> uuids;
	switch (repr) {
#ifdef XAPIAND_UUID_GUID
		case UUIDRepr::guid:
			// {00000000-0000-1000-8000-010000000000}
			UUID::unserialise(serialised_uuid, std::back_inserter(uuids));
			result.push_back('{');
			result.append(string::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			result.push_back('}');
			break;
#endif
#ifdef XAPIAND_UUID_URN
		case UUIDRepr::urn:
			// urn:uuid:00000000-0000-1000-8000-010000000000
			UUID::unserialise(serialised_uuid, std::back_inserter(uuids));
			result.append("urn:uuid:");
			result.append(string::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			break;
#endif
#ifdef XAPIAND_UUID_ENCODED
		case UUIDRepr::encoded:
			if (
				// Is UUID condensed
				serialised_uuid.front() != 1 && (
					// and compacted
					((serialised_uuid.back() & 1) != 0) ||
					// or has node multicast bit "on" for (uuid with Data)
					(serialised_uuid.size() > 5 && ((*(serialised_uuid.rbegin() + 5) & 2) != 0))
				)
			) {
				result.append("~" + UUID_ENCODER.encode(serialised_uuid));
				break;
			}
			[[fallthrough]];
#endif
		default:
		case UUIDRepr::vanilla:
			// 00000000-0000-1000-8000-010000000000
			UUID::unserialise(serialised_uuid, std::back_inserter(uuids));
			result.append(string::join(uuids, std::string(1, UUID_SEPARATOR_LIST)));
			break;
	}
	return result;
}


std::pair<RangeList, CartesianList>
Unserialise::ranges_centroids(std::string_view serialised_geo)
{
	StringList data(serialised_geo);
	switch (data.size()) {
		case 0:
			return std::make_pair(RangeList(std::string_view("")), CartesianList(std::string_view("")));
		case 1:
			return std::make_pair(RangeList(data.front()), CartesianList(std::string_view("")));
		case 2:
			return std::make_pair(RangeList(data.front()), CartesianList(data.back()));
		default:
			THROW(SerialisationError, "Serialised geospatial must contain at most two elements");
	}
}


RangeList
Unserialise::ranges(std::string_view serialised_geo)
{
	StringList data(serialised_geo);
	switch (data.size()) {
		case 0:
			return RangeList(std::string_view(""));
		case 1:
		case 2:
			return RangeList(data.front());
		default:
			THROW(SerialisationError, "Serialised geospatial must contain at most two elements");
	}
}


CartesianList
Unserialise::centroids(std::string_view serialised_geo)
{
	StringList data(serialised_geo);
	switch (data.size()) {
		case 0:
		case 1:
			return CartesianList(std::string_view(""));
		case 2:
			return CartesianList(data.back());
		default:
			THROW(SerialisationError, "Serialised geospatial must contain at most two elements");
	}
}


Cartesian
Unserialise::cartesian(std::string_view serialised_val)
{
	if (serialised_val.size() != SERIALISED_LENGTH_CARTESIAN) {
		THROW(SerialisationError, "Cannot unserialise cartesian: {} [{}]", repr(serialised_val), serialised_val.size());
	}

	double x = (((unsigned)serialised_val[0] << 24) & 0xFF000000) | (((unsigned)serialised_val[1] << 16) & 0xFF0000) | (((unsigned)serialised_val[2] << 8) & 0xFF00)  | (((unsigned)serialised_val[3]) & 0xFF);
	double y = (((unsigned)serialised_val[4] << 24) & 0xFF000000) | (((unsigned)serialised_val[5] << 16) & 0xFF0000) | (((unsigned)serialised_val[6] << 8) & 0xFF00)  | (((unsigned)serialised_val[7]) & 0xFF);
	double z = (((unsigned)serialised_val[8] << 24) & 0xFF000000) | (((unsigned)serialised_val[9] << 16) & 0xFF0000) | (((unsigned)serialised_val[10] << 8) & 0xFF00) | (((unsigned)serialised_val[11]) & 0xFF);
	return {(x - MAXDOU2INT) / DOUBLE2INT, (y - MAXDOU2INT) / DOUBLE2INT, (z - MAXDOU2INT) / DOUBLE2INT};
}


uint64_t
Unserialise::trixel_id(std::string_view serialised_id)
{
	if (serialised_id.size() != HTM_BYTES_ID) {
		THROW(SerialisationError, "Cannot unserialise trixel_id: {} [{}]", repr(serialised_id), serialised_id.size());
	}

	uint64_t id = (((uint64_t)serialised_id[0] << 48) & 0xFF000000000000) | (((uint64_t)serialised_id[1] << 40) & 0xFF0000000000) | \
				  (((uint64_t)serialised_id[2] << 32) & 0xFF00000000)     | (((uint64_t)serialised_id[3] << 24) & 0xFF000000)     | \
				  (((uint64_t)serialised_id[4] << 16) & 0xFF0000)         | (((uint64_t)serialised_id[5] <<  8) & 0xFF00)         | \
				  (serialised_id[6] & 0xFF);
	return id;
}


range_t
Unserialise::range(std::string_view serialised_range)
{
	if (serialised_range.size() != SERIALISED_LENGTH_RANGE) {
		THROW(SerialisationError, "Cannot unserialise range_t: {} [{}]", repr(serialised_range), serialised_range.size());
	}

	uint64_t start = (((uint64_t)serialised_range[0] << 48) & 0xFF000000000000) | (((uint64_t)serialised_range[1] << 40) & 0xFF0000000000) | \
					 (((uint64_t)serialised_range[2] << 32) & 0xFF00000000)     | (((uint64_t)serialised_range[3] << 24) & 0xFF000000)     | \
					 (((uint64_t)serialised_range[4] << 16) & 0xFF0000)         | (((uint64_t)serialised_range[5] <<  8) & 0xFF00)         | \
					 (serialised_range[6] & 0xFF);

	uint64_t end = (((uint64_t)serialised_range[7] << 48) & 0xFF000000000000) | (((uint64_t)serialised_range[8] << 40) & 0xFF0000000000) | \
				   (((uint64_t)serialised_range[9] << 32) & 0xFF00000000)     | (((uint64_t)serialised_range[10] << 24) & 0xFF000000)    | \
				   (((uint64_t)serialised_range[11] << 16) & 0xFF0000)        | (((uint64_t)serialised_range[12] <<  8) & 0xFF00)        | \
				   (serialised_range[13] & 0xFF);

	return {start, end};
}


FieldType
Unserialise::get_field_type(std::string_view str_type)
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
			return FieldType::boolean;
		case _.fhhl("boolean"):
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
		case _.fhhl("s"):
		case _.fhhl("string"):
			return FieldType::string;
		case _.fhhl("term"):  // FIXME: remove legacy term
		case _.fhhl("k"):
		case _.fhhl("keyword"):
			return FieldType::keyword;
		case _.fhhl("u"):
			return FieldType::uuid;
		case _.fhhl("x"):
		case _.fhhl("script"):
			return FieldType::script;
		case _.fhhl("text"):
			return FieldType::text;
		case _.fhhl("time"):
			return FieldType::time;
		case _.fhhl("foreign"):
			return FieldType::foreign;
		case _.fhhl("timedelta"):
			return FieldType::timedelta;
		default:
			THROW(SerialisationError, "Type: {} is an unsupported type", repr(str_type));
	}
}
