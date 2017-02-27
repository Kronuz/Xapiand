/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "serialise.h"

#include <algorithm>                                  // for move
#include <ctype.h>                                    // for toupper
#include <functional>                                 // for cref
#include <math.h>                                     // for round
#include <stdexcept>                                  // for out_of_range, invalid_argument
#include <stdio.h>                                    // for sprintf
#include <strings.h>                                  // for strcasecmp
#include <time.h>                                     // for tm, gmtime, time_t

#include "cppcodec/base64_default_url_unpadded.hpp"   // for base64 namespace
#include "exception.h"                                // for SerialisationError, MSG_Serialisat...
#include "geo/cartesian.h"                            // for Cartesian
#include "geo/ewkt.h"                                 // for EWKT
#include "geo/htm.h"                                  // for HTM_MAX_LENGTH_NAME, HTM_BYTES_ID, range_t
#include "guid/guid.h"                                // for Guid
#include "msgpack.h"                                  // for MsgPack, object::object, type_error
#include "query_dsl.h"                                // for QUERYDSL_FROM, QUERYDSL_TO
#include "schema.h"                                   // for FieldType, FieldType::TERM, Fiel...
#include "serialise_list.h"                           // for StringList, CartesianList and RangeList
#include "utils.h"                                    // for toUType, stox, repr


MsgPack
Cast::cast(const MsgPack& obj)
{
	if (obj.size() == 1) {
		auto str_key = obj.begin()->as_string();
		switch ((Hash)xxh64::hash(str_key)) {
			case Hash::INTEGER:
				return integer(obj.at(str_key));
			case Hash::POSITIVE:
				return positive(obj.at(str_key));
			case Hash::FLOAT:
				return _float(obj.at(str_key));
			case Hash::BOOLEAN:
				return boolean(obj.at(str_key));
			case Hash::TERM:
			case Hash::TEXT:
			case Hash::STRING:
			case Hash::UUID:
			case Hash::EWKT:
				return string(obj.at(str_key));
			case Hash::DATE:
				return date(obj.at(str_key));
			default:
				THROW(SerialisationError, "Unknown cast type %s", str_key.c_str());
		}
	}

	THROW(SerialisationError, "Expected map with one element");
}


MsgPack
Cast::cast(FieldType type, const std::string& field_value)
{
	switch (type) {
		case FieldType::INTEGER:
			try {
				return MsgPack(stox(std::stoll, field_value));
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to integer", field_value.c_str());
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to integer", field_value.c_str());
			}
		case FieldType::POSITIVE:
			try {
				return MsgPack(stox(std::stoull, field_value));
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to positive", field_value.c_str());
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to positive", field_value.c_str());
			}
		case FieldType::FLOAT:
			try {
				return MsgPack(stox(std::stod, field_value));
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to float", field_value.c_str());
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to float", field_value.c_str());
			}
		case FieldType::EMPTY:
			// Try like INTEGER.
			try {
				return MsgPack(stox(std::stoll, field_value));
			} catch (const std::invalid_argument&) {
			} catch (const std::out_of_range&) { }

			// Try like POSITIVE.
			try {
				return  MsgPack(stox(std::stoull, field_value));
			} catch (const std::invalid_argument&) {
			} catch (const std::out_of_range&) { }

			// Try like FLOAT
			try {
				return MsgPack(stox(std::stod, field_value));
			} catch (const std::invalid_argument&) {
			} catch (const std::out_of_range&) { }
		default:
			// Default type TERM.
			return MsgPack(field_value);
	}
}


int64_t
Cast::integer(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.as_u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.as_i64();
		case MsgPack::Type::FLOAT:
			return obj.as_f64();
		case MsgPack::Type::STR:
			try {
				return stox(std::stoll, obj.as_string());
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to integer", MsgPackTypes[toUType(obj.getType())]);
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to integer", MsgPackTypes[toUType(obj.getType())]);
			}
		case MsgPack::Type::BOOLEAN:
			return obj.as_bool();
		default:
			THROW(SerialisationError, "Type %s cannot be cast to integer", MsgPackTypes[toUType(obj.getType())]);
	}
}


uint64_t
Cast::positive(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.as_u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.as_i64();
		case MsgPack::Type::FLOAT:
			return obj.as_f64();
		case MsgPack::Type::STR:
			try {
				return stox(std::stoull, obj.as_string());
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to positive", MsgPackTypes[toUType(obj.getType())]);
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to positive", MsgPackTypes[toUType(obj.getType())]);
			}
		case MsgPack::Type::BOOLEAN:
			return obj.as_bool();
		default:
			THROW(SerialisationError, "Type %s cannot be cast to positive", MsgPackTypes[toUType(obj.getType())]);
	}
}


double
Cast::_float(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.as_u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.as_i64();
		case MsgPack::Type::FLOAT:
			return obj.as_f64();
		case MsgPack::Type::STR:
			try{
				return stox(std::stod, obj.as_string());
			} catch (const std::invalid_argument&) {
				THROW(SerialisationError, "Value %s cannot be cast to float", MsgPackTypes[toUType(obj.getType())]);
			} catch (const std::out_of_range&) {
				THROW(SerialisationError, "Value %s cannot be cast to float", MsgPackTypes[toUType(obj.getType())]);
			}
		case MsgPack::Type::BOOLEAN:
			return obj.as_bool();
		default:
			THROW(SerialisationError, "Type %s cannot be cast to float", MsgPackTypes[toUType(obj.getType())]);
	}
}


std::string
Cast::string(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return std::to_string(obj.as_u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return std::to_string(obj.as_i64());
		case MsgPack::Type::FLOAT:
			return std::to_string(obj.as_f64());
		case MsgPack::Type::STR:
			return obj.as_string();
		case MsgPack::Type::BOOLEAN:
			return obj.as_bool() ? "true" : "false";
		default:
			return obj.to_string();
	}
}


bool
Cast::boolean(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.as_u64() != 0;
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.as_i64() != 0;
		case MsgPack::Type::FLOAT:
			return obj.as_f64() != 0;
		case MsgPack::Type::STR: {
			const char *value = obj.as_string().c_str();
			switch (value[0]) {
				case '\0':
					return false;
				case '0':
				case 'f':
				case 'F':
					if (value[1] == '\0' || strcasecmp(value, "false") == 0) {
						return false;
					}
				default:
					return true;
			}
		}
		case MsgPack::Type::BOOLEAN:
			return obj.as_bool();
		default:
			THROW(SerialisationError, "Type %s cannot be cast to boolean", MsgPackTypes[toUType(obj.getType())]);
	}
}


MsgPack
Cast::date(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
		case MsgPack::Type::MAP:
			return obj;
		default:
			THROW(SerialisationError, "Type %s cannot be cast to date", MsgPackTypes[toUType(obj.getType())]);
	}
}


FieldType
Cast::getType(const std::string& cast_word)
{
	switch ((Hash)xxh64::hash(cast_word)) {
		case Hash::INTEGER:           return FieldType::INTEGER;
		case Hash::POSITIVE:          return FieldType::POSITIVE;
		case Hash::FLOAT:             return FieldType::FLOAT;
		case Hash::BOOLEAN:           return FieldType::BOOLEAN;
		case Hash::TERM:              return FieldType::TERM;
		case Hash::TEXT:              return FieldType::TEXT;
		case Hash::STRING:            return FieldType::STRING;
		case Hash::UUID:              return FieldType::UUID;
		case Hash::DATE:              return FieldType::DATE;
		case Hash::EWKT:              return FieldType::GEO;
		case Hash::POINT:             return FieldType::GEO;
		case Hash::POLYGON:           return FieldType::GEO;
		case Hash::CIRCLE:            return FieldType::GEO;
		case Hash::CHULL:             return FieldType::GEO;
		case Hash::MULTIPOINT:        return FieldType::GEO;
		case Hash::MULTIPOLYGON:      return FieldType::GEO;
		case Hash::MULTICIRCLE:       return FieldType::GEO;
		case Hash::MULTICHULL:        return FieldType::GEO;
		case Hash::GEO_COLLECTION:    return FieldType::GEO;
		case Hash::GEO_INTERSECTION:  return FieldType::GEO;
		default:
			THROW(SerialisationError, "Unknown cast type %s", cast_word.c_str());
	}
}


bool
Serialise::isUUID(const std::string& field_value) noexcept
{
	switch (field_value.length()) {
		case SIZE_CURLY_BRACES_UUID:
			if (field_value.front() == '{' && field_value.back() == '}') {
				if (field_value[9] != '-' || field_value[14] != '-' || field_value[19] != '-' || field_value[24] != '-') {
					return false;
				}
				static const size_t stop = SIZE_CURLY_BRACES_UUID - 1;
				for (size_t i = 1; i < stop; ++i) {
					if (!std::isxdigit(field_value.at(i)) && i != 9 && i != 14 && i != 19 && i != 24) {
						return false;
					}
				}
				return true;
			}
			return false;

		case SIZE_UUID:
			if (field_value[8] != '-' || field_value[13] != '-' || field_value[18] != '-' || field_value[23] != '-') {
				return false;
			}
			for (size_t i = 0; i < SIZE_UUID; ++i) {
				if (!std::isxdigit(field_value.at(i)) && i != 8 && i != 13 && i != 18 && i != 23) {
					return false;
				}
			}
			return true;

		default:
			if (field_value.length() >= 3 && field_value.length() <= MAX_SIZE_BASE64_UUID && field_value.front() == '{' && field_value.back() == '}') {
				return true;
			}
			return false;
	}
}


std::string
Serialise::MsgPack(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.getType()) {
		case MsgPack::Type::BOOLEAN:
			return boolean(field_spc.get_type(), field_value.as_bool());
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.as_u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.as_i64());
		case MsgPack::Type::FLOAT:
			return _float(field_spc.get_type(), field_value.as_f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.as_string());
		case MsgPack::Type::MAP:
			return cast_object(field_spc, field_value);
		default:
			THROW(SerialisationError, "msgpack::type %s is not supported", MsgPackTypes[toUType(field_value.getType())]);
	}
}


std::string
Serialise::cast_object(const required_spc_t& field_spc, const class MsgPack& o)
{
	if (o.size() == 1) {
		auto str_key = o.begin()->as_string();
		switch ((Cast::Hash)xxh64::hash(str_key)) {
			case Cast::Hash::INTEGER:
				return Serialise::integer(field_spc.get_type(), Cast::integer(o.at(str_key)));
			case Cast::Hash::POSITIVE:
				return Serialise::positive(field_spc.get_type(), Cast::positive(o.at(str_key)));
			case Cast::Hash::FLOAT:
				return Serialise::_float(field_spc.get_type(), Cast::_float(o.at(str_key)));
			case Cast::Hash::BOOLEAN:
				return Serialise::boolean(field_spc.get_type(), Cast::boolean(o.at(str_key)));
			case Cast::Hash::TERM:
			case Cast::Hash::TEXT:
			case Cast::Hash::STRING:
			case Cast::Hash::UUID:
			case Cast::Hash::EWKT:
				return Serialise::string(field_spc, Cast::string(o.at(str_key)));
			case Cast::Hash::DATE:
				return Serialise::date(field_spc, Cast::date(o.at(str_key)));
			default:
				THROW(SerialisationError, "Unknown cast type %s", str_key.c_str());
		}
	}

	THROW(SerialisationError, "Expected map with one element");
}


std::string
Serialise::serialise(const required_spc_t& field_spc, const std::string& field_value)
{
	auto field_type = field_spc.get_type();

	switch (field_type) {
		case FieldType::INTEGER:
			return integer(field_value);
		case FieldType::POSITIVE:
			return positive(field_value);
		case FieldType::FLOAT:
			return _float(field_value);
		case FieldType::DATE:
			return date(field_value);
		case FieldType::BOOLEAN:
			return boolean(field_value);
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING:
			return field_value;
		case FieldType::GEO:
			return ewkt(field_value, field_spc.flags.partials, field_spc.error);
		case FieldType::UUID:
			return uuid(field_value);
		default:
			THROW(SerialisationError, "Type: %s is an unknown type", type(field_type).c_str());
	}
}


std::string
Serialise::string(const required_spc_t& field_spc, const std::string& field_value)
{
	switch (field_spc.get_type()) {
		case FieldType::DATE:
			return date(field_value);
		case FieldType::BOOLEAN:
			return boolean(field_value);
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING:
			return field_value;
		case FieldType::GEO:
			return ewkt(field_value, field_spc.flags.partials, field_spc.error);
		case FieldType::UUID:
			return uuid(field_value);
		default:
			THROW(SerialisationError, "Type: %s is not string", type(field_spc.get_type()).c_str());
	}
}


std::string
Serialise::date(const required_spc_t& field_spc, const class MsgPack& field_value)
{
	switch (field_value.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.as_u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.as_i64());
		case MsgPack::Type::FLOAT:
			return _float(field_spc.get_type(), field_value.as_f64());
		case MsgPack::Type::STR:
			return string(field_spc, field_value.as_string());
		case MsgPack::Type::MAP:
			return _float(field_spc.get_type(), Datetime::timestamp(field_value));
		default:
			THROW(SerialisationError, "Type: %s is not a date", MsgPackTypes[toUType(field_value.getType())]);
	}
}


std::string
Serialise::_float(FieldType field_type, double field_value)
{
	switch (field_type) {
		case FieldType::DATE:
			return timestamp(field_value);
		case FieldType::FLOAT:
			return _float(field_value);
		default:
			THROW(SerialisationError, "Type: %s is not a float", type(field_type).c_str());
	}
}


std::string
Serialise::integer(FieldType field_type, int64_t field_value)
{
	switch (field_type) {
		case FieldType::POSITIVE:
			if (field_value < 0) {
				THROW(SerialisationError, "Type: %s must be a positive number [%lld]", type(field_type).c_str(), field_value);
			}
			return positive(field_value);
		case FieldType::DATE:
			return timestamp(field_value);
		case FieldType::FLOAT:
			return _float(field_value);
		case FieldType::INTEGER:
			return integer(field_value);
		default:
			THROW(SerialisationError, "Type: %s is not a integer [%lld]", type(field_type).c_str(), field_value);
	}
}


std::string
Serialise::positive(FieldType field_type, uint64_t field_value)
{
	switch (field_type) {
		case FieldType::DATE:
			return timestamp(field_value);
		case FieldType::FLOAT:
			return _float(field_value);
		case FieldType::INTEGER:
			return integer(field_value);
		case FieldType::POSITIVE:
			return positive(field_value);
		default:
			THROW(SerialisationError, "Type: %s is not a positive integer [%llu]", type(field_type).c_str(), field_value);
	}
}


std::string
Serialise::boolean(FieldType field_type, bool field_value)
{
	if (field_type == FieldType::BOOLEAN) {
		return boolean(field_value);
	}

	THROW(SerialisationError, "Type: %s is not boolean", type(field_type).c_str());
}


std::pair<FieldType, std::string>
Serialise::get_type(const std::string& field_value, bool bool_term)
{
	if (field_value.empty()) {
		std::make_pair(FieldType::TERM, field_value);
	}

	// Try like UUID
	try {
		return std::make_pair(FieldType::UUID, uuid(field_value));
	} catch (const SerialisationError&) { }

	// Try like INTEGER.
	try {
		return std::make_pair(FieldType::INTEGER, integer(field_value));
	} catch (const SerialisationError&) { }

	// Try like POSITIVE.
	try {
		return std::make_pair(FieldType::POSITIVE, positive(field_value));
	} catch (const SerialisationError&) { }

	// Try like FLOAT
	try {
		return std::make_pair(FieldType::FLOAT, _float(field_value));
	} catch (const SerialisationError&) { }

	// Try like DATE
	try {
		return std::make_pair(FieldType::DATE, date(field_value));
	} catch (const DatetimeError&) { }

	// Try like GEO
	try {
		return std::make_pair(FieldType::GEO, ewkt(field_value, default_spc.flags.partials, default_spc.error));
	} catch (const EWKTError&) { }

	// Try like BOOLEAN
	try {
		return std::make_pair(FieldType::BOOLEAN, boolean(field_value));
	} catch (const SerialisationError&) { }

	// Like TEXT
	if (isText(field_value, bool_term)) {
		return std::make_pair(FieldType::TEXT, field_value);
	}

	// Default type STRING.
	return std::make_pair(FieldType::STRING, field_value);
}


std::pair<FieldType, std::string>
Serialise::get_type(const class MsgPack& field_value, bool bool_term)
{
	switch (field_value.getType()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return std::make_pair(FieldType::INTEGER, integer(field_value.as_i64()));

		case MsgPack::Type::POSITIVE_INTEGER:
			return std::make_pair(FieldType::POSITIVE, positive(field_value.as_u64()));

		case MsgPack::Type::FLOAT:
			return std::make_pair(FieldType::FLOAT, _float(field_value.as_f64()));

		case MsgPack::Type::BOOLEAN:
			return std::make_pair(FieldType::BOOLEAN, boolean(field_value.as_bool()));

		case MsgPack::Type::STR: {
			auto str_obj = field_value.as_string();

			// Try like UUID
			try {
				return std::make_pair(FieldType::UUID, uuid(str_obj));
			} catch (const SerialisationError&) { }

			// Try like DATE
			try {
				return std::make_pair(FieldType::DATE, date(str_obj));
			} catch (const DatetimeError&) { }

			// Try like GEO
			try {
				return std::make_pair(FieldType::GEO, ewkt(str_obj, default_spc.flags.partials, default_spc.error));
			} catch (const EWKTError&) { }

			if (bool_term) {
				return std::make_pair(FieldType::TERM, str_obj);
			}

			// Like TEXT
			if (isText(str_obj, bool_term)) {
				return std::make_pair(FieldType::TEXT, str_obj);
			}

			// Default type STRING.
			return std::make_pair(FieldType::STRING, str_obj);
		}

		case MsgPack::Type::MAP: {
			if (field_value.size() == 1) {
				auto str_key = field_value.begin()->as_string();
				switch ((Cast::Hash)xxh64::hash(str_key)) {
					case Cast::Hash::INTEGER:
						return std::make_pair(FieldType::INTEGER, integer(Cast::integer(field_value.at(str_key))));
					case Cast::Hash::POSITIVE:
						return std::make_pair(FieldType::POSITIVE, positive(Cast::positive(field_value.at(str_key))));
					case Cast::Hash::FLOAT:
						return std::make_pair(FieldType::FLOAT, _float(Cast::_float(field_value.at(str_key))));
					case Cast::Hash::BOOLEAN:
						return std::make_pair(FieldType::BOOLEAN, boolean(Cast::boolean(field_value.at(str_key))));
					case Cast::Hash::TERM:
						return std::make_pair(FieldType::TERM, Cast::string(field_value.at(str_key)));
					case Cast::Hash::TEXT:
						return std::make_pair(FieldType::TEXT, Cast::string(field_value.at(str_key)));
					case Cast::Hash::STRING:
						return std::make_pair(FieldType::STRING, Cast::string(field_value.at(str_key)));
					case Cast::Hash::UUID:
						return std::make_pair(FieldType::UUID, uuid(Cast::string(field_value.at(str_key))));
					case Cast::Hash::EWKT:
						return std::make_pair(FieldType::GEO, ewkt(Cast::string(field_value.at(str_key)), default_spc.flags.partials, default_spc.error));
					case Cast::Hash::DATE:
						return std::make_pair(FieldType::DATE, date(Cast::date(field_value.at(str_key))));
					default:
						THROW(SerialisationError, "Unknown cast type: %s", str_key.c_str());
				}
			}
		}

		case MsgPack::Type::UNDEFINED:
		case MsgPack::Type::NIL:
			if (bool_term) {
				return std::make_pair(FieldType::TERM, std::string());
			}

			// Default type STRING.
			return std::make_pair(FieldType::STRING, std::string());

		default:
			THROW(SerialisationError, "Unexpected type %s", MsgPackTypes[toUType(field_value.getType())]);
	}
}


std::tuple<FieldType, std::string, std::string>
Serialise::get_range_type(const std::string& start, const std::string& end, bool bool_term)
{
	if (start.empty()) {
		auto res = get_type(end, bool_term);
		return std::make_tuple(res.first, start, res.second);
	}

	if (end.empty()) {
		auto res = get_type(start, bool_term);
		return std::make_tuple(res.first, res.second, end);
	}

	auto res = get_type(start, bool_term);
	switch (res.first) {
		case FieldType::POSITIVE:
			try {
				return std::make_tuple(FieldType::POSITIVE, res.second, positive(end));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::INTEGER:
			try {
				return std::make_tuple(FieldType::INTEGER, res.second, integer(end));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::FLOAT:
			try {
				return std::make_tuple(FieldType::FLOAT, res.second, _float(end));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::DATE:
			try {
				return std::make_tuple(FieldType::DATE, res.second, date(end));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::GEO:
			try {
				return std::make_tuple(FieldType::GEO, res.second, ewkt(end, default_spc.flags.partials, default_spc.error));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::UUID:
			if (isUUID(end)) {
				return std::make_tuple(FieldType::UUID, res.second, uuid(end));
			} else {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::BOOLEAN:
			try {
				return std::make_tuple(FieldType::BOOLEAN, res.second, boolean(end));
			} catch (const SerialisationError&) {
				return std::make_tuple(FieldType::TERM, start, end);
			}
		case FieldType::TERM:
			return std::make_tuple(FieldType::TERM, start, end);
		case FieldType::TEXT:
			return std::make_tuple(FieldType::TEXT, start, end);
		default:
			return std::make_tuple(FieldType::STRING, start, end);

	}
}


std::tuple<FieldType, std::string, std::string>
Serialise::get_range_type(const class MsgPack& obj, bool bool_term)
{
	class MsgPack start;
	class MsgPack end;

	try {
		start = obj.at(QUERYDSL_FROM);
		try {
			end = obj.at(QUERYDSL_TO);
		} catch (const std::out_of_range&) {
			auto res = get_type(start, bool_term);
			return std::make_tuple(res.first, res.second, std::string());
		}
	} catch (const std::out_of_range&) {
		try {
			end = obj.at(QUERYDSL_TO);
			auto res = get_type(end, bool_term);
			return std::make_tuple(res.first, std::string(), res.second);
		} catch (const std::out_of_range&) {
			auto res = get_type(start, bool_term);
			return std::make_tuple(res.first, res.second, res.second);
		}
	}

	auto typ_start = get_type(start, bool_term);
	auto typ_end = get_type(end, bool_term);

	if (typ_start.first == typ_end.first) {
		return std::make_tuple(typ_start.first, typ_start.second, typ_end.second);
	}

	THROW(SerialisationError, "Range type is ambiguous");
}


std::string
Serialise::date(const std::string& field_value)
{
	return timestamp(Datetime::timestamp(field_value));
}


std::string
Serialise::date(const class MsgPack& field_value)
{
	return timestamp(Datetime::timestamp(field_value));
}


std::string
Serialise::date(const class MsgPack& value, Datetime::tm_t& tm)
{
	tm = Datetime::to_tm_t(value);
	return timestamp(Datetime::timestamp(tm));
}


std::string
Serialise::_float(const std::string& field_value)
{
	try {
		return _float(stox(std::stod, field_value));
	} catch (const std::invalid_argument&) {
		THROW(SerialisationError, "Invalid float format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		THROW(SerialisationError, "Out of range float format: %s", field_value.c_str());
	}
}


std::string
Serialise::integer(const std::string& field_value)
{
	try {
		return integer(stox(std::stoll, field_value));
	} catch (const std::invalid_argument&) {
		THROW(SerialisationError, "Invalid integer format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		THROW(SerialisationError, "Out of range integer format: %s", field_value.c_str());
	}
}


std::string
Serialise::positive(const std::string& field_value)
{
	try {
		return positive(stox(std::stoull, field_value));
	} catch (const std::invalid_argument&) {
		THROW(SerialisationError, "Invalid positive integer format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		THROW(SerialisationError, "Out of range positive integer format: %s", field_value.c_str());
	}
}


std::string
Serialise::uuid(const std::string& field_value)
{
	switch (field_value.length()) {
		case SIZE_CURLY_BRACES_UUID: {
			if (field_value.front() == '{' && field_value.back() == '}') {
				// Remove curly braces.
				auto _field_value = field_value.substr(1, field_value.length() - 2);
				if (_field_value[8] != '-' || _field_value[13] != '-' || _field_value[18] != '-' || _field_value[23] != '-') {
					THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
				}
				for (size_t i = 0; i < SIZE_UUID; ++i) {
					if (!std::isxdigit(_field_value.at(i)) && i != 8 && i != 13 && i != 18 && i != 23) {
						THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
					}
				}
				Guid guid(_field_value);
				return guid.serialise();
			}

			THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
		}

		case SIZE_UUID: {
			if (field_value[8] != '-' || field_value[13] != '-' || field_value[18] != '-' || field_value[23] != '-') {
				THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
			}
			for (size_t i = 0; i < SIZE_UUID; ++i) {
				if (!std::isxdigit(field_value.at(i)) && i != 8 && i != 13 && i != 18 && i != 23) {
					THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
				}
			}
			Guid guid(field_value);
			return guid.serialise();
		}

		default: {
			if (field_value.length() >= 3 && field_value.length() <= MAX_SIZE_BASE64_UUID && field_value.front() == '{' && field_value.back() == '}') {
				// Remove curly braces.
				auto _field_value = field_value.substr(1, field_value.length() - 2);
				return base64::decode<std::string>(_field_value);
			}

			THROW(SerialisationError, "Invalid UUID format in: %s", field_value.c_str());
		}
	}
}


std::string
Serialise::ewkt(const std::string& field_value, bool partials, double error)
{
	EWKT ewkt(field_value);
	return Serialise::ranges(ewkt.geometry->getRanges(partials, error));
}


std::string
Serialise::ranges(const std::vector<range_t>& ranges)
{
	if (ranges.empty()) {
		return std::string();
	}

	size_t hash = 0;
	for (const auto& range : ranges) {
		hash ^= std::hash<range_t>{}(range);
	}

	return sortable_serialise(hash);
}


std::string
Serialise::geo(const std::vector<range_t>& ranges, const std::vector<Cartesian>& centroids)
{
	auto aux = RangeList::serialise(ranges.begin(), ranges.end());
	auto values = serialise_length(aux.length());
	values.append(aux);
	aux = CartesianList::serialise(centroids.begin(), centroids.end());
	values.append(serialise_length(aux.length()));
	values.append(aux);
	values.insert(0, serialise_length(values.length()));
	return values;
}


std::string
Serialise::boolean(const std::string& field_value)
{
	const char *value = field_value.c_str();
	switch (value[0]) {
		case '\0':
			return std::string(1, SERIALISED_FALSE);
		case '1':
		case 't':
		case 'T':
			if (value[1] == '\0' || strcasecmp(value, "true") == 0) {
				return std::string(1, SERIALISED_TRUE);
			}
			break;
		case '0':
		case 'f':
		case 'F':
			if (value[1] == '\0' || strcasecmp(value, "false") == 0) {
				return std::string(1, SERIALISED_FALSE);
			}
			break;
		default:
			break;
	}

	THROW(SerialisationError, "Boolean format is not valid");
}


std::string
Serialise::cartesian(const Cartesian& norm_cartesian)
{
	uint32_t x = Swap4Bytes(((uint32_t)(norm_cartesian.x * DOUBLE2INT) + MAXDOU2INT));
	uint32_t y = Swap4Bytes(((uint32_t)(norm_cartesian.y * DOUBLE2INT) + MAXDOU2INT));
	uint32_t z = Swap4Bytes(((uint32_t)(norm_cartesian.z * DOUBLE2INT) + MAXDOU2INT));
	const char serialise[] = { (char)(x & 0xFF), (char)((x >>  8) & 0xFF), (char)((x >> 16) & 0xFF), (char)((x >> 24) & 0xFF),
							   (char)(y & 0xFF), (char)((y >>  8) & 0xFF), (char)((y >> 16) & 0xFF), (char)((y >> 24) & 0xFF),
							   (char)(z & 0xFF), (char)((z >>  8) & 0xFF), (char)((z >> 16) & 0xFF), (char)((z >> 24) & 0xFF) };
	return std::string(serialise, SERIALISED_LENGTH_CARTESIAN);
}


std::string
Serialise::trixel_id(uint64_t id)
{
	id = Swap7Bytes(id);
	const char serialised[] = { (char)(id & 0xFF), (char)((id >>  8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF),
								(char)((id >> 32) & 0xFF), (char)((id >> 40) & 0xFF), (char)((id >> 48) & 0xFF) };
	return std::string(serialised, HTM_BYTES_ID);
}


std::string
Serialise::range(const range_t& range)
{
	std::string serialised;
	serialised.reserve(SERIALISED_LENGTH_RANGE);

	uint64_t start = Swap7Bytes(range.start);
	const char serialised_start[] = { (char)(start & 0xFF), (char)((start >>  8) & 0xFF), (char)((start >> 16) & 0xFF), (char)((start >> 24) & 0xFF),
									  (char)((start >> 32) & 0xFF), (char)((start >> 40) & 0xFF), (char)((start >> 48) & 0xFF) };

	uint64_t end = Swap7Bytes(range.end);
	const char serialised_end[] = { (char)(end & 0xFF), (char)((end >>  8) & 0xFF), (char)((end >> 16) & 0xFF), (char)((end >> 24) & 0xFF),
									  (char)((end >> 32) & 0xFF), (char)((end >> 40) & 0xFF), (char)((end >> 48) & 0xFF) };

	serialised.assign(serialised_start, HTM_BYTES_ID).append(serialised_end, HTM_BYTES_ID);
	return serialised;
}


std::string
Serialise::type(FieldType field_type)
{
	switch (field_type) {
		case FieldType::TERM:     return TERM_STR;
		case FieldType::TEXT:     return TEXT_STR;
		case FieldType::STRING:   return STRING_STR;
		case FieldType::FLOAT:    return FLOAT_STR;
		case FieldType::INTEGER:  return INTEGER_STR;
		case FieldType::POSITIVE: return POSITIVE_STR;
		case FieldType::BOOLEAN:  return BOOLEAN_STR;
		case FieldType::GEO:      return GEO_STR;
		case FieldType::DATE:     return DATE_STR;
		case FieldType::UUID:     return UUID_STR;
		case FieldType::OBJECT:   return OBJECT_STR;
		case FieldType::ARRAY:    return ARRAY_STR;
		case FieldType::EMPTY:    return EMPTY_STR;
		default:                  return "unknown";
	}
}


MsgPack
Unserialise::MsgPack(FieldType field_type, const std::string& serialised_val)
{
	class MsgPack result;
	switch (field_type) {
		case FieldType::FLOAT:
			result = _float(serialised_val);
			break;
		case FieldType::INTEGER:
			result = integer(serialised_val);
			break;
		case FieldType::POSITIVE:
			result = positive(serialised_val);
			break;
		case FieldType::DATE:
			result = date(serialised_val);
			break;
		case FieldType::BOOLEAN:
			result = boolean(serialised_val);
			break;
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING:
			result = serialised_val;
			break;
		case FieldType::GEO:
			result = geo(serialised_val);
			break;
		case FieldType::UUID:
			result = uuid(serialised_val);
			break;
		default:
			THROW(SerialisationError, "Type: %s is an unknown type", Serialise::type(field_type).c_str());
	}

	return result;
}


std::string
Unserialise::unserialise(FieldType field_type, const std::string& serialised_val)
{
	switch (field_type) {
		case FieldType::FLOAT:
			return std::to_string(_float(serialised_val));
		case FieldType::INTEGER:
			return std::to_string(integer(serialised_val));
		case FieldType::POSITIVE:
			return std::to_string(positive(serialised_val));
		case FieldType::DATE:
			return date(serialised_val);
		case FieldType::BOOLEAN:
			return std::string(boolean(serialised_val) ? "true" : "false");
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING:
			return serialised_val;
		case FieldType::GEO:
			return ewkt(serialised_val);
		case FieldType::UUID:
			return uuid(serialised_val);
		default:
			THROW(SerialisationError, "Type: %s is an unknown type", Serialise::type(field_type).c_str());
	}
}


std::string
Unserialise::date(const std::string& serialised_date)
{
	static char date[25];
	double epoch = timestamp(serialised_date);
	time_t timestamp = (time_t) epoch;
	int msec = round((epoch - timestamp) * 1000);
	struct tm *timeinfo = gmtime(&timestamp);
	sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3d", timeinfo->tm_year + _START_YEAR,
		timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
		timeinfo->tm_sec, msec);
	return date;
}


Cartesian
Unserialise::cartesian(const std::string& serialised_val)
{
	if (serialised_val.size() != SERIALISED_LENGTH_CARTESIAN) {
		THROW(SerialisationError, "Cannot unserialise cartesian: %s [%zu]", repr(serialised_val).c_str(), serialised_val.size());
	}

	double x = (((unsigned)serialised_val[0] << 24) & 0xFF000000) | (((unsigned)serialised_val[1] << 16) & 0xFF0000) | (((unsigned)serialised_val[2] << 8) & 0xFF00)  | (((unsigned)serialised_val[3]) & 0xFF);
	double y = (((unsigned)serialised_val[4] << 24) & 0xFF000000) | (((unsigned)serialised_val[5] << 16) & 0xFF0000) | (((unsigned)serialised_val[6] << 8) & 0xFF00)  | (((unsigned)serialised_val[7]) & 0xFF);
	double z = (((unsigned)serialised_val[8] << 24) & 0xFF000000) | (((unsigned)serialised_val[9] << 16) & 0xFF0000) | (((unsigned)serialised_val[10] << 8) & 0xFF00) | (((unsigned)serialised_val[11]) & 0xFF);
	return Cartesian((x - MAXDOU2INT) / DOUBLE2INT, (y - MAXDOU2INT) / DOUBLE2INT, (z - MAXDOU2INT) / DOUBLE2INT);
}


uint64_t
Unserialise::trixel_id(const std::string& serialised_id)
{
	if (serialised_id.length() != HTM_BYTES_ID) {
		THROW(SerialisationError, "Cannot unserialise trixel_id: %s [%zu]", repr(serialised_id).c_str(), serialised_id.length());
	}

	uint64_t id = (((uint64_t)serialised_id[0] << 48) & 0xFF000000000000) | (((uint64_t)serialised_id[1] << 40) & 0xFF0000000000) | \
				  (((uint64_t)serialised_id[2] << 32) & 0xFF00000000)     | (((uint64_t)serialised_id[3] << 24) & 0xFF000000)     | \
				  (((uint64_t)serialised_id[4] << 16) & 0xFF0000)         | (((uint64_t)serialised_id[5] <<  8) & 0xFF00)         | \
				  (serialised_id[6] & 0xFF);
	return id;
}


range_t
Unserialise::range(const std::string& serialised_range)
{
	if (serialised_range.length() != SERIALISED_LENGTH_RANGE) {
		THROW(SerialisationError, "Cannot unserialise range_t: %s [%zu]", repr(serialised_range).c_str(), serialised_range.length());
	}

	uint64_t start = (((uint64_t)serialised_range[0] << 48) & 0xFF000000000000) | (((uint64_t)serialised_range[1] << 40) & 0xFF0000000000) | \
					 (((uint64_t)serialised_range[2] << 32) & 0xFF00000000)     | (((uint64_t)serialised_range[3] << 24) & 0xFF000000)     | \
					 (((uint64_t)serialised_range[4] << 16) & 0xFF0000)         | (((uint64_t)serialised_range[5] <<  8) & 0xFF00)         | \
					 (serialised_range[6] & 0xFF);

	uint64_t end = (((uint64_t)serialised_range[7] << 48) & 0xFF000000000000) | (((uint64_t)serialised_range[8] << 40) & 0xFF0000000000) | \
				   (((uint64_t)serialised_range[9] << 32) & 0xFF00000000)     | (((uint64_t)serialised_range[10] << 24) & 0xFF000000)     | \
				   (((uint64_t)serialised_range[11] << 16) & 0xFF0000)         | (((uint64_t)serialised_range[12] <<  8) & 0xFF00)         | \
				   (serialised_range[13] & 0xFF);

	return range_t(start, end);
}


std::string
Unserialise::uuid(const std::string& serialised_uuid)
{
	auto guid = Guid::unserialise(serialised_uuid);
	return guid.to_string();
	// auto res = base64::encode(serialised_uuid);
	// res.insert(0, 1, '{').push_back('}');
	// return res;
}


std::vector<range_t>
Unserialise::ranges(const std::string& serialised_geo)
{
	const char* pos = serialised_geo.data();
	const char* end = pos + serialised_geo.length();
	try {
		unserialise_length(&pos, end, true);
		const auto length = unserialise_length(&pos, end, true);
		std::vector<range_t> ranges;
		RangeList::unserialise(std::string(pos, length), std::back_inserter(ranges));
		return ranges;
	} catch (const SerialisationError&) {
		return std::vector<range_t>();
	}
}


std::pair<std::string, std::string>
Unserialise::geo(const std::string& serialised_geo)
{
	const char* pos = serialised_geo.data();
	const char* end = pos + serialised_geo.length();
	try {
		unserialise_length(&pos, end, true);
		auto length = unserialise_length(&pos, end, true);
		std::string serialise_ranges(pos, length);
		pos += length;
		length = unserialise_length(&pos, end, true);
		return std::make_pair(std::move(serialise_ranges), std::string(pos, length));
	} catch (const SerialisationError&) {
		return std::make_pair(std::string(), std::string());
	}
}


std::string
Unserialise::ewkt(const std::string& serialised_ewkt)
{
	auto unserialise = geo(serialised_ewkt);
	RangeList ranges(std::move(unserialise.first));
	std::string res("Ranges: { ");
	for (const auto& range : ranges) {
		res += "[" + std::to_string(range.start) + ", " + std::to_string(range.end) + "] ";
	}
	res += "}";

	CartesianList centroids(std::move(unserialise.second));
	res += "  Centroids: { ";
	for (const auto& centroid : centroids) {
		res += "(" + std::to_string(centroid.x) + ", " + std::to_string(centroid.y) + ", " + std::to_string(centroid.z) + ") ";
	}
	res += "}";

	return res;
}


FieldType
Unserialise::type(const std::string& str_type)
{
	const char *value = str_type.c_str();
	switch ((FieldType)(toupper(value[0]))) {
		case FieldType::FLOAT:
			if (value[1] == '\0' || strcasecmp(value, FLOAT_STR) == 0) {
				return FieldType::FLOAT;
			}
			break;
		case FieldType::INTEGER:
			if (value[1] == '\0' || strcasecmp(value, INTEGER_STR) == 0) {
				return FieldType::INTEGER;
			}
			break;
		case FieldType::POSITIVE:
			if (value[1] == '\0' || strcasecmp(value, POSITIVE_STR) == 0) {
				return FieldType::POSITIVE;
			}
			break;
		case FieldType::GEO:
			if (value[1] == '\0' || strcasecmp(value, GEO_STR) == 0) {
				return FieldType::GEO;
			}
			break;
		case FieldType::TERM:
			if (value[1] == '\0' || strcasecmp(value, TERM_STR) == 0) {
				return FieldType::TERM;
			}
			break;
		case FieldType::TEXT:
			if (value[1] == '\0' || strcasecmp(value, TEXT_STR) == 0) {
				return FieldType::TEXT;
			}
			break;
		case FieldType::STRING:
			if (value[1] == '\0' || strcasecmp(value, STRING_STR) == 0) {
				return FieldType::STRING;
			}
			break;
		case FieldType::BOOLEAN:
			if (value[1] == '\0' || strcasecmp(value, BOOLEAN_STR) == 0) {
				return FieldType::BOOLEAN;
			}
			break;
		case FieldType::DATE:
			if (value[1] == '\0' || strcasecmp(value, DATE_STR) == 0) {
				return FieldType::DATE;
			}
			break;
		default:
			break;
	}

	THROW(SerialisationError, "Type: %s is an unknown type", repr(str_type).c_str());
}
