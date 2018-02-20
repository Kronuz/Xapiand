/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

#include "cast.h"

#include "schema.h"


MsgPack
Cast::cast(const MsgPack& obj)
{
	if (obj.size() == 1) {
		const auto str_key = obj.begin()->str();
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
				return string(obj.at(str_key));
			case Hash::UUID:
				return uuid(obj.at(str_key));
			case Hash::DATE:
				return date(obj.at(str_key));
			case Hash::TIME:
				return time(obj.at(str_key));
			case Hash::TIMEDELTA:
				return timedelta(obj.at(str_key));
			case Hash::EWKT:
				return ewkt(obj.at(str_key));
			case Hash::POINT:
			case Hash::CIRCLE:
			case Hash::CONVEX:
			case Hash::POLYGON:
			case Hash::CHULL:
			case Hash::MULTIPOINT:
			case Hash::MULTICIRCLE:
			case Hash::MULTIPOLYGON:
			case Hash::MULTICHULL:
			case Hash::GEO_COLLECTION:
			case Hash::GEO_INTERSECTION:
				return obj;
			default:
				THROW(CastError, "Unknown cast type %s", str_key.c_str());
		}
	}

	THROW(CastError, "Expected map with one element");
}


MsgPack
Cast::cast(FieldType type, string_view field_value)
{
	switch (type) {
		case FieldType::INTEGER: {
			int errno_save;
			auto r = strict_stoll(errno_save, field_value);
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to integer", repr(field_value).c_str());
			}
			return MsgPack(r);
		}
		case FieldType::POSITIVE: {
			int errno_save;
			auto r = strict_stoull(errno_save, field_value);
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to positive", repr(field_value).c_str());
			}
			return MsgPack(r);
		}
		case FieldType::FLOAT: {
			int errno_save;
			auto r = strict_stod(errno_save, field_value);
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to float", repr(field_value).c_str());
			}
			return MsgPack(r);
		}
		case FieldType::EMPTY:
			{
				// Try like INTEGER.
				int errno_save;
				auto r = strict_stoll(errno_save, field_value);
				if (!errno_save) {
					return MsgPack(r);
				}
			}
			{
				// Try like POSITIVE.
				int errno_save;
				auto r = strict_stoull(errno_save, field_value);
				if (!errno_save) {
					return MsgPack(r);
				}
			}
			{
				// Try like FLOAT
				int errno_save;
				auto r = strict_stod(errno_save, field_value);
				if (!errno_save) {
					return MsgPack(r);
				}
			}
		default:
			// Default type String.
			return MsgPack(field_value);
	}
}


int64_t
Cast::integer(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stoll(errno_save, obj.str_view());
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to integer", obj.getStrType().c_str());
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return obj.boolean();
		default:
			THROW(CastError, "Type %s cannot be cast to integer", obj.getStrType().c_str());
	}
}


uint64_t
Cast::positive(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stoull(errno_save, obj.str_view());
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to positive", obj.getStrType().c_str());
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return obj.boolean();
		default:
			THROW(CastError, "Type %s cannot be cast to positive", obj.getStrType().c_str());
	}
}


double
Cast::_float(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return obj.u64();
		case MsgPack::Type::NEGATIVE_INTEGER:
			return obj.i64();
		case MsgPack::Type::FLOAT:
			return obj.f64();
		case MsgPack::Type::STR: {
			int errno_save;
			auto r = strict_stod(errno_save, obj.str_view());
			if (errno_save) {
				THROW(CastError, "Value %s cannot be cast to float", obj.getStrType().c_str());
			}
			return r;
		}
		case MsgPack::Type::BOOLEAN:
			return obj.boolean();
		default:
			THROW(CastError, "Type %s cannot be cast to float", obj.getStrType().c_str());
	}
}


std::string
Cast::string(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return std::to_string(obj.u64());
		case MsgPack::Type::NEGATIVE_INTEGER:
			return std::to_string(obj.i64());
		case MsgPack::Type::FLOAT:
			return std::to_string(obj.f64());
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
	switch (obj.getType()) {
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
				case 4:
					switch (value[0]) {
						case 'f':
						case 'F':
						case 't':
						case 'T': {
							auto lower_value = lower_string(value);
							if (lower_value == "false") {
								return false;
							// } else if (lower_value == "true") {
							// 	return true;
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
			THROW(CastError, "Type %s cannot be cast to boolean", obj.getStrType().c_str());
	}
}


std::string
Cast::uuid(const MsgPack& obj)
{
	if (obj.is_string()) {
		return obj.str();
	}
	THROW(CastError, "Type %s cannot be cast to uuid", obj.getStrType().c_str());
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
			THROW(CastError, "Type %s cannot be cast to date", obj.getStrType().c_str());
	}
}


MsgPack
Cast::time(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
			return obj;
		default:
			THROW(CastError, "Type %s cannot be cast to time", obj.getStrType().c_str());
	}
}


MsgPack
Cast::timedelta(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
		case MsgPack::Type::STR:
			return obj;
		default:
			THROW(CastError, "Type %s cannot be cast to timedelta", obj.getStrType().c_str());
	}
}


std::string
Cast::ewkt(const MsgPack& obj)
{
	if (obj.is_string()) {
		return obj.str();
	}
	THROW(CastError, "Type %s cannot be cast to ewkt", obj.getStrType().c_str());
}


FieldType
Cast::getType(string_view cast_word)
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
		case Hash::TIME:              return FieldType::TIME;
		case Hash::TIMEDELTA:         return FieldType::TIMEDELTA;
		case Hash::EWKT:              return FieldType::GEO;
		case Hash::POINT:             return FieldType::GEO;
		case Hash::CIRCLE:            return FieldType::GEO;
		case Hash::CONVEX:            return FieldType::GEO;
		case Hash::POLYGON:           return FieldType::GEO;
		case Hash::CHULL:             return FieldType::GEO;
		case Hash::MULTIPOINT:        return FieldType::GEO;
		case Hash::MULTICIRCLE:       return FieldType::GEO;
		case Hash::MULTIPOLYGON:      return FieldType::GEO;
		case Hash::MULTICHULL:        return FieldType::GEO;
		case Hash::GEO_COLLECTION:    return FieldType::GEO;
		case Hash::GEO_INTERSECTION:  return FieldType::GEO;
		case Hash::CHAI:              return FieldType::SCRIPT;
		case Hash::ECMA:              return FieldType::SCRIPT;
		default:
			THROW(CastError, "Unknown cast type %s", repr(cast_word).c_str());
	}
}
