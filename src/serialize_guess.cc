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

// Type detection and ids: text and UUID predicates, guessing a value's type, and id serialization.

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
				positive_of(&errno_save, str_value);
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
					case Cast::HashType::DATE:
						return FieldType::date;
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
					case Cast::HashType::MULTICONVEX:
					case Cast::HashType::MULTIPOLYGON:
					case Cast::HashType::MULTICHULL:
					case Cast::HashType::GEO_COLLECTION:
					case Cast::HashType::GEO_INTERSECTION:
						return FieldType::geo;
					case Cast::HashType::LUA:
						THROW(SerializationError, "{} is a script, not a value", repr(str_key));
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
			// A cast object serializes as a field of the type it names would.
			required_spc_t field_spc;
			field_spc.set_type(guess_type(field_value));
			return std::make_pair(field_spc.get_type(), object(field_spc, field_value));
		}

		default:
			THROW(SerializationError, "Unexpected type {}", enum_name(field_value.get_type()));
	}
}
