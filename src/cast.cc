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

#include "cast.h"

#include "database/schema.h"
#include "exception_xapian.h"                     // for CastError
#include "strings.hh"                             // for strings::format


MsgPack
Cast::cast(const MsgPack& obj)
{
	if (obj.size() == 1) {
		const auto str_key = obj.begin()->str();
		switch (get_hash_type(str_key)) {
			case HashType::INTEGER:
				return integer(obj.at(str_key));
			case HashType::POSITIVE:
				return positive(obj.at(str_key));
			case HashType::FLOAT:
				return static_cast<double>(floating(obj.at(str_key)));
			case HashType::BOOLEAN:
				return boolean(obj.at(str_key));
			case HashType::KEYWORD:
			case HashType::TEXT:
			case HashType::STRING:
				return string(obj.at(str_key));
			case HashType::UUID:
				return uuid(obj.at(str_key));
			case HashType::DATE:
			case HashType::DATETIME:
				return datetime(obj.at(str_key));
			case HashType::TIME:
				return time(obj.at(str_key));
			case HashType::TIMEDELTA:
				return timedelta(obj.at(str_key));
			case HashType::EWKT:
				return ewkt(obj.at(str_key));
			case HashType::POINT:
			case HashType::CIRCLE:
			case HashType::CONVEX:
			case HashType::POLYGON:
			case HashType::CHULL:
			case HashType::MULTIPOINT:
			case HashType::MULTICIRCLE:
			case HashType::MULTIPOLYGON:
			case HashType::MULTICHULL:
			case HashType::GEO_COLLECTION:
			case HashType::GEO_INTERSECTION:
				return obj;
			default:
				THROW(CastError, "Unknown cast type {}", str_key);
		}
	}

	THROW(CastError, "Expected map with one element");
}


MsgPack
Cast::cast(FieldType type, const MsgPack& obj)
{
	switch (type) {
		case FieldType::integer:
			return integer(obj);
		case FieldType::positive:
			return positive(obj);
		case FieldType::floating:
			return static_cast<double>(floating(obj));
		case FieldType::boolean:
			return boolean(obj);
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			return string(obj);
		case FieldType::uuid:
			return uuid(obj);
		case FieldType::date:
		case FieldType::datetime:
			return datetime(obj);
		case FieldType::time:
			return time(obj);
		case FieldType::timedelta:
			return timedelta(obj);
		case FieldType::script:
			if (obj.is_map()) {
				return obj;
			}
			THROW(CastError, "Type {} cannot be cast to script", enum_name(obj.get_type()));
		case FieldType::geo:
			if (obj.is_map() || obj.is_string()) {
				return obj;
			}
			THROW(CastError, "Type {} cannot be cast to geo", enum_name(obj.get_type()));
		case FieldType::empty:
			if (obj.is_string()) {
				{
					// Try like INTEGER.
					int errno_save;
					auto r = strict_stoll(&errno_save, obj.str_view());
					if (errno_save == 0) {
						return MsgPack(r);
					}
				}
				{
					// Try like POSITIVE.
					int errno_save;
					auto r = strict_stoull(&errno_save, obj.str_view());
					if (errno_save == 0) {
						return MsgPack(r);
					}
				}
				{
					// Try like FLOAT
					int errno_save;
					auto r = strict_stod(&errno_save, obj.str_view());
					if (errno_save == 0) {
						return MsgPack(r);
					}
				}
				return obj;
			}
			[[fallthrough]];
		default:
			THROW(CastError, "Type {} cannot be cast", enum_name(obj.get_type()));
	}
}


int64_t
Cast::integer(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stoll(&errno_save, obj.str_view());
			if (errno_save != 0) {
				THROW(CastError, "Value {} cannot be cast to integer", enum_name(obj.get_type()));
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return static_cast<int64_t>(obj.boolean());
		default:
			THROW(CastError, "Type {} cannot be cast to integer", enum_name(obj.get_type()));
	}
}


uint64_t
Cast::positive(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stoull(&errno_save, obj.str_view());
			if (errno_save != 0) {
				THROW(CastError, "Value {} cannot be cast to positive", enum_name(obj.get_type()));
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return static_cast<uint64_t>(obj.boolean());
		default:
			THROW(CastError, "Type {} cannot be cast to positive", enum_name(obj.get_type()));
	}
}


long double
Cast::floating(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stod(&errno_save, obj.str_view());
			if (errno_save != 0) {
				THROW(CastError, "Value {} cannot be cast to float", enum_name(obj.get_type()));
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return static_cast<double>(obj.boolean());
		default:
			THROW(CastError, "Type {} cannot be cast to float", enum_name(obj.get_type()));
	}
}


std::string
Cast::string(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return strings::format("{}", obj.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return strings::format("{}", obj.i64());
		case MsgPack::Type::FLOAT:
			return strings::format("{}", obj.f64());
		case MsgPack::Type::STR:
			return obj.str();
		case MsgPack::Type::BOOLEAN:
			return obj.boolean() ? "true" : "false";
		default:
			return obj.to_string();
	}
}


bool
Cast::boolean(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64() != 0;
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64() != 0;
		case MsgPack::Type::FLOAT:
			return obj.f64() != 0;
		case MsgPack::Type::STR: {
			auto value = obj.str_view();
			switch (value.size()) {
				case 0:
					return false;
				case 1:
					switch (value[0]) {
						case '0':
						case 'f':
						case 'F':
							return false;
						// case '1':
						// case 't':
						// case 'T':
						// 	return true;
					}
					break;
				// case 4:
				// 	switch (value[0]) {
				// 		case 't':
				// 		case 'T': {
				// 			auto lower_value = strings::lower(value);
				// 			if (lower_value == "true") {
				// 				return true;
				// 			}
				// 		}
				// 	}
				// 	break;
				case 5:
					switch (value[0]) {
						case 'f':
						case 'F': {
							auto lower_value = strings::lower(value);
							if (lower_value == "false") {
								return false;
							}
						}
					}
					break;
			}
			return true;
		}
		case MsgPack::Type::BOOLEAN:
			return obj.boolean();
		default:
			THROW(CastError, "Type {} cannot be cast to boolean", enum_name(obj.get_type()));
	}
}


std::string
Cast::uuid(const MsgPack& obj)
{
	if (obj.is_string()) {
		return obj.str();
	}
	THROW(CastError, "Type {} cannot be cast to uuid", enum_name(obj.get_type()));
}


MsgPack
Cast::datetime(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
		case MsgPack::Type::MAP:
			return obj;
		default:
			THROW(CastError, "Type {} cannot be cast to datetime", enum_name(obj.get_type()));
	}
}


MsgPack
Cast::time(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
			return obj;
		default:
			THROW(CastError, "Type {} cannot be cast to time", enum_name(obj.get_type()));
	}
}


MsgPack
Cast::timedelta(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
			return obj;
		default:
			THROW(CastError, "Type {} cannot be cast to timedelta", enum_name(obj.get_type()));
	}
}


std::string
Cast::ewkt(const MsgPack& obj)
{
	if (obj.is_string()) {
		return obj.str();
	}
	THROW(CastError, "Type {} cannot be cast to ewkt", enum_name(obj.get_type()));
}


Cast::HashType
Cast::get_hash_type(std::string_view cast_word)
{
	static const auto _ = cast_hash;
	return static_cast<HashType>(_.fhh(cast_word));
}


FieldType
Cast::get_field_type(std::string_view cast_word)
{
	if (cast_word.empty() || cast_word[0] != reserved__) {
		THROW(CastError, "Unknown cast type {}", repr(cast_word));
	}
	switch (get_hash_type(cast_word)) {
		case HashType::INTEGER:           return FieldType::integer;
		case HashType::POSITIVE:          return FieldType::positive;
		case HashType::FLOAT:             return FieldType::floating;
		case HashType::BOOLEAN:           return FieldType::boolean;
		case HashType::KEYWORD:           return FieldType::keyword;
		case HashType::TEXT:              return FieldType::text;
		case HashType::STRING:            return FieldType::string;
		case HashType::UUID:              return FieldType::uuid;
		case HashType::DATETIME:          return FieldType::datetime;
		case HashType::TIME:              return FieldType::time;
		case HashType::TIMEDELTA:         return FieldType::timedelta;
		case HashType::EWKT:              return FieldType::geo;
		case HashType::POINT:             return FieldType::geo;
		case HashType::CIRCLE:            return FieldType::geo;
		case HashType::CONVEX:            return FieldType::geo;
		case HashType::POLYGON:           return FieldType::geo;
		case HashType::CHULL:             return FieldType::geo;
		case HashType::MULTIPOINT:        return FieldType::geo;
		case HashType::MULTICIRCLE:       return FieldType::geo;
		case HashType::MULTIPOLYGON:      return FieldType::geo;
		case HashType::MULTICHULL:        return FieldType::geo;
		case HashType::GEO_COLLECTION:    return FieldType::geo;
		case HashType::GEO_INTERSECTION:  return FieldType::geo;
		case HashType::CHAI:              return FieldType::script;
		default:
			THROW(CastError, "Unknown cast type {}", repr(cast_word));
	}
}
