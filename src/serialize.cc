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

#include "serialize.h"
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads

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
#include "sortable.hh"                                // for sortable::encode, sortable::decode
#include "split.h"                                    // for Split
#include "utype.hh"                                   // for toUType


constexpr char UUID_SEPARATOR_LIST = ';';

#ifdef XAPIAND_UUID_ENCODED
#define UUID_ENCODER (Base59::dubaluchk())
#endif


bool
Serialize::isText(std::string_view field_value) noexcept
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
Serialize::possiblyUUID(std::string_view field_value) noexcept
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
Serialize::isUUID(std::string_view field_value) noexcept
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
						if (UUID::is_serialized(decoded)) {
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


namespace {

[[noreturn]] void
not_exact(std::string_view field, FieldType type, const MsgPack& value, std::string_view why = {})
{
	if (why.empty()) {
		THROW(ClientError, "Field {} expects a value of type {}, got {}", repr(field), enum_name(type), value.to_string());
	}
	THROW(ClientError, "Field {} expects a value of type {}, got {}: {}", repr(field), enum_name(type), value.to_string(), why);
}


// The number a text spells, read as JSON reads one (int64, else uint64, else double); undefined when it spells none.
MsgPack
number_of(std::string_view text)
{
	if (!Cast::bare_number_text(text)) {
		return MsgPack();
	}
	int errno_save;
	const auto i = strict_stoll(&errno_save, text);
	if (errno_save == 0) {
		return MsgPack(static_cast<int64_t>(i));
	}
	if (text.front() != '-') {  // strtoull wraps a negative
		const auto u = strict_stoull(&errno_save, text);
		if (errno_save == 0) {
			return MsgPack(static_cast<uint64_t>(u));
		}
	}
	const auto d = strict_stod(&errno_save, text);
	if (errno_save == 0) {
		return MsgPack(d);
	}
	return MsgPack();
}


// `v` as exactly a number of the numeric `type`: an integral float as the integer, text as the number it spells.
std::optional<MsgPack>
exact_number(FieldType type, const MsgPack& v)
{
	switch (v.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:  // an int64, of either sign
			if (type != FieldType::positive || v.i64() >= 0) {
				return v;
			}
			break;
		case MsgPack::Type::POSITIVE_INTEGER:  // a uint64
			if (type != FieldType::integer || v.u64() <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
				return v;
			}
			break;
		case MsgPack::Type::FLOAT: {
			const double d = v.f64();
			if (std::isnan(d)) {
				break;
			}
			if (type == FieldType::floating) {
				return v;
			}
			// From 2^53 on, doubles are 2 or more apart, so 9007199254740993.5 reads as the integral 9007199254740994.
			if (std::trunc(d) != d || !(std::fabs(d) < 0x1p53)) {
				break;
			}
			if (type == FieldType::integer) {
				return MsgPack(static_cast<int64_t>(d));
			}
			if (d >= 0) {
				return MsgPack(static_cast<uint64_t>(d));
			}
			break;
		}
		case MsgPack::Type::STR: {
			const auto number = number_of(v.str_view());
			if (number.is_number()) {
				return exact_number(type, number);
			}
			break;
		}
		default:
			break;
	}
	return std::nullopt;
}

}  // namespace


MsgPack
Serialize::exact(std::string_view field, FieldType type, const class MsgPack& value)
{
	switch (type) {
		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating:
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::uuid:
			break;
		default:
			return value;
	}

	::MsgPack converted;
	if (value.is_map()) {
		try {
			converted = Cast::cast(value);
		} catch (const CastError& exc) {
			not_exact(field, type, value, exc.get_message());
		}
	}
	const auto& v = value.is_map() ? converted : value;

	switch (type) {
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::uuid:
			if (v.is_string()) {
				return v;
			}
			break;
		default:
			if (auto number = exact_number(type, v)) {
				return std::move(*number);
			}
			break;
	}
	not_exact(field, type, value);
}


std::string
Serialize::MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value)
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
			THROW(SerializationError, "msgpack::type {} is not supported", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::object(const required_spc_t& field_spc, const class MsgPack& o)
{
	if (o.size() == 1) {
		auto str_key = o.begin()->str_view();
		if (str_key.empty() || str_key[0] != reserved__) {
			THROW(SerializationError, "Unknown cast type: {}", repr(str_key));
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
				THROW(SerializationError, "Unknown cast type {}", repr(str_key));
		}
	}

	THROW(SerializationError, "Expected map with one element");
}


std::string
Serialize::serialize(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	auto field_type = field_spc.get_type();

	switch (field_type) {
		case FieldType::integer:
			return integer(field_value.i64());
		case FieldType::positive:
			return positive(field_value.u64());
		case FieldType::floating:
			return floating(FieldType::floating, field_value.f64());
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
			return field_value.str();
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value.str_view());
		default:
			THROW(SerializationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


std::string
Serialize::serialize(const required_spc_t& field_spc, std::string_view field_value)
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
			return std::string(field_value);
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value);
		default:
			THROW(SerializationError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


std::string
Serialize::string(const required_spc_t& field_spc, std::string_view field_value)
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
			return std::string(field_value);
		case FieldType::geo:
			return geospatial(field_value);
		case FieldType::uuid:
			return uuid(field_value);
		default:
			THROW(SerializationError, "Type: {} is not string", enum_name(field_spc.get_type()));
	}
}


std::string
Serialize::datetime(const required_spc_t& field_spc, const class MsgPack& field_value)
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
				case FieldType::text:
					return Datetime::iso8601(Datetime::DatetimeParser(field_value));
				default:
					THROW(SerializationError, "Type: {} is not a datetime", enum_name(field_value.get_type()));
			}
		default:
			THROW(SerializationError, "Type: {} is not a datetime", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::time(const required_spc_t& field_spc, const class MsgPack& field_value)
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
			THROW(SerializationError, "Type: {} is not a time", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::timedelta(const required_spc_t& field_spc, const class MsgPack& field_value)
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
			THROW(SerializationError, "Type: {} is not a timedelta", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::floating(FieldType field_type, double field_value)
{
	if (std::isnan(field_value)) {
		THROW(SerializationError, "Type: {} cannot store NaN", enum_name(field_type));
	}
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
			THROW(SerializationError, "Type: {} is not a float", enum_name(field_type));
	}
}


std::string
Serialize::integer(FieldType field_type, int64_t field_value)
{
	switch (field_type) {
		case FieldType::positive:
			if (field_value < 0) {
				THROW(SerializationError, "Type: {} must be a positive number [{}]", enum_name(field_type), field_value);
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
		case FieldType::integer:
			return integer(field_value);  // exact: one number has one encoding whatever its type
		default:
			THROW(SerializationError, "Type: {} is not a integer [{}]", enum_name(field_type), field_value);
	}
}


std::string
Serialize::positive(FieldType field_type, uint64_t field_value)
{
	switch (field_type) {
		case FieldType::date:
		case FieldType::datetime:
			return timestamp(field_value);
		case FieldType::floating:
			return positive(field_value);  // exact: one number has one encoding whatever its type
		case FieldType::time:
			return time(field_value);
		case FieldType::timedelta:
			return timedelta(field_value);
		case FieldType::integer:
			if (field_value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
				THROW(SerializationError, "Type: {} is out of range [{}]", enum_name(field_type), field_value);
			}
			return integer(static_cast<int64_t>(field_value));
		case FieldType::positive:
			return positive(field_value);
		default:
			THROW(SerializationError, "Type: {} is not a positive integer [{}]", enum_name(field_type), field_value);
	}
}


std::string
Serialize::boolean(FieldType field_type, bool field_value)
{
	if (field_type == FieldType::boolean) {
		return boolean(field_value);
	}

	THROW(SerializationError, "Type: {} is not boolean", enum_name(field_type));
}


std::string
Serialize::geospatial(FieldType field_type, const class MsgPack& field_value)
{
	if (field_type == FieldType::geo) {
		return geospatial(field_value);
	}

	THROW(SerializationError, "Type: {} is not geospatial", enum_name(field_type));
}


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
	try {
		return positive(strict_stoull(field_value));
	} catch (const std::invalid_argument& exc) {
		RETHROW(SerializationError, "Invalid positive integer format: {}", repr(field_value));
	} catch (const std::out_of_range& exc) {
		RETHROW(SerializationError, "Out of range positive integer format: {}", repr(field_value));
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


std::string
Serialize::ranges_hash(const std::vector<range_t>& ranges)
{
	return serialize_length(std::hash<std::vector<range_t>>{}(ranges));
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


FieldType
Serialize::guess_type(const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return FieldType::integer;

		case MsgPack::Type::POSITIVE_INTEGER:
			// The value picks the type, not its encoding: msgpack sends 5 unsigned and JSON signed.
			return field_value.u64() > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ? FieldType::positive : FieldType::integer;

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
				strict_stod(&errno_save, str_value);
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
					THROW(SerializationError, "Unknown cast type: {}", repr(str_key));
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
						THROW(SerializationError, "Unknown cast type: {}", repr(str_key));
				}
			} else {
				THROW(SerializationError, "Expected map with one element");
			}
		}

		default:
			THROW(SerializationError, "Unexpected type {}", enum_name(field_value.get_type()));
	}
}


std::string
Serialize::id(const required_spc_t& spc_id, const class MsgPack& document_id)
{
	const auto type = spc_id.get_type();
	switch (type) {
		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating: {
			const auto value = exact(ID_FIELD_NAME, type, document_id);
			switch (value.get_type()) {
				case MsgPack::Type::NEGATIVE_INTEGER:
					return integer(value.i64());
				case MsgPack::Type::POSITIVE_INTEGER:
					return positive(value.u64());
				default:
					return floating(FieldType::floating, value.f64());
			}
		}
		default:
			return serialize(spc_id, document_id);
	}
}


std::string
Serialize::id(std::uint64_t document_id)
{
	return positive(document_id);
}


std::pair<FieldType, std::string>
Serialize::guess_id(const class MsgPack& document_id)
{
	auto guessed = guess_serialize(document_id);
	switch (guessed.first) {
		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating: {
			required_spc_t spc_id;
			spc_id.set_type(guessed.first);
			guessed.second = id(spc_id, document_id);
			break;
		}
		default:
			break;
	}
	return guessed;
}


std::optional<std::uint64_t>
Unserialize::id(std::string_view serialized_id)
{
	const auto number = sortable::decode(serialized_id);
	return number ? number->to_uint64() : std::nullopt;
}


MsgPack
Unserialize::id(FieldType field_type, std::string_view serialized_id)
{
	const auto number = sortable::decode(serialized_id);
	if (!number) {
		THROW(SerializationError, "Invalid numeric id: {}", repr(serialized_id));
	}
	switch (field_type) {
		case FieldType::integer:
			return number->to_int64().value_or(number->saturate_int64());
		case FieldType::positive:
			return number->to_uint64().value_or(number->saturate_uint64());
		default:
			return number->to_double();
	}
}


std::pair<FieldType, std::string>
Serialize::guess_serialize(const class MsgPack& field_value)
{
	switch (field_value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			if (guess_type(field_value) == FieldType::positive) {
				return std::make_pair(FieldType::positive, positive(field_value.u64()));
			}
			[[fallthrough]];
		case MsgPack::Type::NEGATIVE_INTEGER:
			return std::make_pair(FieldType::integer, integer(field_value.i64()));

		case MsgPack::Type::FLOAT:
			return std::make_pair(FieldType::floating, floating(FieldType::floating, field_value.f64()));

		case MsgPack::Type::BOOLEAN:
			return std::make_pair(FieldType::boolean, boolean(field_value.boolean()));

		case MsgPack::Type::STR: {
			auto str_value = field_value.str_view();

			// Fast path: the chain below guesses a value's type by trying each
			// typed serializer in turn, and every miss throws a C++ exception --
			// on this platform the unwind (dyld unwind-section lookup + libunwind
			// + personality) dominates the cost of guessing a plain value. A
			// string with none of the characters that the UUID, datetime, time,
			// timedelta, geospatial or number grammars require cannot be any of
			// them, so it is text or keyword and we can skip the eight throwing
			// trials. The guarded set covers digits plus '-' (uuid/date/number),
			// '~' and '{' (encoded/braced uuid), ':' (urn:uuid / time) and '('
			// (geospatial WKT).
			if (!str_value.empty()) {
				bool maybe_typed = false;
				for (char c : str_value) {
					if ((c >= '0' && c <= '9') || c == '-' || c == '~' ||
						c == '{' || c == ':' || c == '(') {
						maybe_typed = true;
						break;
					}
				}
				if (!maybe_typed) {
					if (isText(str_value)) {
						return std::make_pair(FieldType::text, std::string(str_value));
					}
					return std::make_pair(FieldType::keyword, std::string(str_value));
				}
			}

			// Try like UUID
			try {
				return std::make_pair(FieldType::uuid, uuid(str_value));
			} catch (const SerializationError&) { }

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
			} catch (const SerializationError&) { }

			// Try like POSITIVE.
			try {
				return std::make_pair(FieldType::positive, positive(str_value));
			} catch (const SerializationError&) { }

			// Try like FLOAT
			try {
				return std::make_pair(FieldType::floating, floating(str_value));
			} catch (const SerializationError&) { }

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
					THROW(SerializationError, "Unknown cast type: {}", repr(str_key));
				}
				switch (Cast::get_hash_type(str_key)) {
					case Cast::HashType::INTEGER:
						return std::make_pair(FieldType::integer, integer(Cast::integer(it.value())));
					case Cast::HashType::POSITIVE:
						return std::make_pair(FieldType::positive, positive(Cast::positive(it.value())));
					case Cast::HashType::FLOAT:
						return std::make_pair(FieldType::floating, floating(FieldType::floating, Cast::floating(it.value())));
					case Cast::HashType::BOOLEAN:
						return std::make_pair(FieldType::boolean, boolean(Cast::boolean(it.value())));
					case Cast::HashType::KEYWORD:
						return std::make_pair(FieldType::keyword, Cast::string(it.value()));
					case Cast::HashType::TEXT:
						return std::make_pair(FieldType::text, Cast::string(it.value()));
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
						THROW(SerializationError, "Unknown cast type: {}", repr(str_key));
				}
			} else {
				THROW(SerializationError, "Expected map with one element");
			}
		}

		default:
			THROW(SerializationError, "Unexpected type {}", enum_name(field_value.get_type()));
	}
}


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
