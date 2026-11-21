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

// Serialize's dispatch: a MsgPack or string value serialized by its field spec's type.

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
			case Cast::HashType::DATE:
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
			case Cast::HashType::MULTICONVEX:
			case Cast::HashType::MULTIPOLYGON:
			case Cast::HashType::MULTICHULL:
			case Cast::HashType::GEO_COLLECTION:
			case Cast::HashType::GEO_INTERSECTION:
				return geospatial(field_spc.get_type(), o);
			case Cast::HashType::LUA:
				THROW(SerializationError, "{} is a script, not a value", repr(str_key));
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

