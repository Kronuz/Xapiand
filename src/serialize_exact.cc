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

// Serialize::exact: a value checked as exactly the type a field expects.

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
#include "strict_stox.hh"                             // for strict_stoll, strict_stod
#include "utype.hh"                                   // for toUType
#include "serialize_detail.h"


using serialize_detail::positive_of;


namespace {

[[noreturn]] void
not_exact(std::string_view field, FieldType type, const MsgPack& value, std::string_view why = {})
{
	if (why.empty()) {
		THROW(ClientError, "Field {} expects a value of type {}, got {}", repr(field), enum_name(type), value.to_string());
	}
	THROW(ClientError, "Field {} expects a value of type {}, got {}: {}", repr(field), enum_name(type), value.to_string(), why);
}


// Whether `text` is exactly a JSON number: -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?
bool
json_number_text(std::string_view text) noexcept
{
	std::size_t i = 0;
	const auto n = text.size();
	const auto digits = [&] {
		const auto start = i;
		while (i < n && text[i] >= '0' && text[i] <= '9') {
			++i;
		}
		return i - start;
	};
	if (i < n && text[i] == '-') {
		++i;
	}
	if (i < n && text[i] == '0') {
		++i;
	} else if (digits() == 0) {
		return false;
	}
	if (i < n && text[i] == '.') {
		++i;
		if (digits() == 0) {
			return false;
		}
	}
	if (i < n && (text[i] == 'e' || text[i] == 'E')) {
		++i;
		if (i < n && (text[i] == '+' || text[i] == '-')) {
			++i;
		}
		if (digits() == 0) {
			return false;
		}
	}
	return i == n;
}



// The number a text spells, read as JSON reads one (int64, else uint64, else double); undefined when it spells none.
MsgPack
number_of(std::string_view text)
{
	if (!json_number_text(text)) {
		return MsgPack();
	}
	int errno_save;
	const auto i = strict_stoll(&errno_save, text);
	if (errno_save == 0) {
		return MsgPack(static_cast<int64_t>(i));
	}
	const auto u = positive_of(&errno_save, text);
	if (errno_save == 0) {
		return MsgPack(static_cast<uint64_t>(u));
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

