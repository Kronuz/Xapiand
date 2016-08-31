/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "hash/sha256.h"
#include "length.h"
#include "schema.h"
#include "utils.h"
#include "wkt_parser.h"


std::string
Serialise::MsgPack(const required_spc_t& field_spc, const ::MsgPack& field_value)
{
	switch (field_value.type()) {
		case msgpack::type::NIL:
			throw MSG_DummyException();
		case msgpack::type::BOOLEAN:
			return boolean(field_spc.get_type(), field_value.as_bool());
		case msgpack::type::POSITIVE_INTEGER:
			return positive(field_spc.get_type(), field_value.as_u64());
		case msgpack::type::NEGATIVE_INTEGER:
			return integer(field_spc.get_type(), field_value.as_i64());
		case msgpack::type::FLOAT:
			return _float(field_spc.get_type(), field_value.as_f64());
		case msgpack::type::STR:
			return string(field_spc, field_value.as_string());
		default:
			throw MSG_SerialisationError("msgpack::type [%d] is not supported", field_value.type());
	}
}


std::string
Serialise::serialise(const required_spc_t& field_spc, const std::string& field_value)
{
	auto type = field_spc.get_type();
	if (field_value.empty() && type != FieldType::STRING && type != FieldType::TEXT) {
		throw MSG_SerialisationError("Field value must be defined");
	}

	switch (type) {
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
		case FieldType::STRING:
		case FieldType::TEXT:
			return field_value;
		case FieldType::GEO:
			return ewkt(field_value, field_spc.partials, field_spc.error);
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", toUType(type));
	}
}


std::string
Serialise::string(const required_spc_t& field_spc, const std::string& field_value)
{
	auto field_type = field_spc.get_type();
	if (field_value.empty() && field_type != FieldType::STRING && field_type != FieldType::TEXT) {
		throw MSG_SerialisationError("Field value must be defined");
	}

	switch (field_type) {
		case FieldType::DATE:
			return date(field_value);
		case FieldType::BOOLEAN:
			return boolean(field_value);
		case FieldType::STRING:
		case FieldType::TEXT:
			return field_value;
		case FieldType::GEO:
			return ewkt(field_value, field_spc.partials, field_spc.error);
		default:
			throw MSG_SerialisationError("Type: %s is not string", type(field_type).c_str());
	}
}


std::string
Serialise::_float(FieldType field_type, double field_value)
{
	switch (field_type) {
		case FieldType::DATE:
			return timestamp(field_value);
		case  FieldType::FLOAT:
			return _float(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not a float", type(field_type).c_str());
	}
}


std::string
Serialise::integer(FieldType field_type, int64_t field_value)
{
	switch (field_type) {
		case FieldType::POSITIVE:
			if (field_value < 0) {
				throw MSG_SerialisationError("Type: %s must be a positive number [%lld]", type(field_type).c_str(), field_value);
			}
			return positive(field_value);
		case FieldType::DATE:
			return timestamp(field_value);
		case FieldType::FLOAT:
			return _float(field_value);
		case FieldType::INTEGER:
			return integer(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not a integer [%lld]", type(field_type).c_str(), field_value);
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
			throw MSG_SerialisationError("Type: %s is not a positive integer [%llu]", type(field_type).c_str(), field_value);
	}
}


std::string
Serialise::boolean(FieldType field_type, bool field_value)
{
	if (field_type == FieldType::BOOLEAN) {
		return boolean(field_value);
	}

	throw MSG_SerialisationError("%s is not boolean", type(field_type).c_str());
}


std::pair<FieldType, std::string>
Serialise::get_type(const std::string& field_value, bool bool_term)
{
	if (field_value.empty()) {
		return std::make_pair(FieldType::STRING, field_value);
	}

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
		return std::make_pair(FieldType::GEO, ewkt(field_value, GEO_DEF_PARTIALS, GEO_DEF_ERROR));
	} catch (const EWKTError&) { }

	// Like TEXT
	if (isText(field_value, bool_term)) {
		return std::make_pair(FieldType::TEXT, field_value);
	}

	// Default type STRING.
	return std::make_pair(FieldType::STRING, field_value);
}


std::string
Serialise::date(const std::string& field_value)
{
	return timestamp(Datetime::timestamp(field_value));
}


std::string
Serialise::date(const ::MsgPack& value, Datetime::tm_t& tm)
{
	double _timestamp;
	switch (value.type()) {
		case msgpack::type::POSITIVE_INTEGER:
			_timestamp = value.as_u64();
			tm = Datetime::to_tm_t(_timestamp);
			return timestamp(_timestamp);
		case msgpack::type::NEGATIVE_INTEGER:
			_timestamp = value.as_i64();
			tm = Datetime::to_tm_t(_timestamp);
			return timestamp(_timestamp);
		case msgpack::type::FLOAT:
			_timestamp = value.as_f64();
			tm = Datetime::to_tm_t(_timestamp);
			return timestamp(_timestamp);
		case msgpack::type::STR:
			_timestamp = Datetime::timestamp(value.as_string(), tm);
			tm = Datetime::to_tm_t(_timestamp);
			return timestamp(_timestamp);
		default:
			throw MSG_SerialisationError("Date value must be numeric or string");
	}
}


std::string
Serialise::_float(const std::string& field_value)
{
	try {
		return _float(strict(std::stod, field_value));
	} catch (const std::invalid_argument&) {
		throw MSG_SerialisationError("Invalid float format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		throw MSG_SerialisationError("Out of range float format: %s", field_value.c_str());
	}
}


std::string
Serialise::integer(const std::string& field_value)
{
	try {
		return integer(strict(std::stoll, field_value));
	} catch (const std::invalid_argument&) {
		throw MSG_SerialisationError("Invalid integer format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		throw MSG_SerialisationError("Out of range integer format: %s", field_value.c_str());
	}
}


std::string
Serialise::positive(const std::string& field_value)
{
	try {
		return positive(strict(std::stoull, field_value));
	} catch (const std::invalid_argument&) {
		throw MSG_SerialisationError("Invalid positive integer format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		throw MSG_SerialisationError("Out of range positive integer format: %s", field_value.c_str());
	}
}


std::string
Serialise::ewkt(const std::string& field_value, bool partials, double error)
{
	EWKT_Parser ewkt(field_value, partials, error);
	if (ewkt.trixels.empty()) {
		return std::string();
	}

	std::string result;
	result.reserve(MAX_SIZE_NAME * ewkt.trixels.size());
	for (const auto& trixel : ewkt.trixels) {
		result.append(trixel);
	}

	SHA256 sha256;
	return sha256(result);
}


std::string
Serialise::trixels(const std::vector<std::string>& trixels)
{
	if (trixels.empty()) {
		return std::string();
	}

	std::string result;
	result.reserve(MAX_SIZE_NAME * trixels.size());
	for (const auto& trixel : trixels) {
		result.append(trixel);
	}

	SHA256 sha256;
	return sha256(result);
}


std::string
Serialise::geo(const RangeList& ranges, const CartesianUSet& centroids)
{
	auto aux = ranges.serialise();
	auto values = serialise_length(aux.size());
	values.append(aux);
	aux = centroids.serialise();
	values.append(serialise_length(aux.size()));
	values.append(aux);
	return serialise_length(values.size()) + values;
}


std::string
Serialise::boolean(const std::string& field_value)
{
	const char *value = field_value.c_str();
	switch (value[0]) {
		case '\0':
			return std::string(1, FALSE_SERIALISED);
		case '1':
		case 't':
		case 'T':
			if (value[1] == '\0' || strcasecmp(value, "true") == 0) {
				return std::string(1, TRUE_SERIALISED);
			}
			break;
		case '0':
		case 'f':
		case 'F':
			if (value[1] == '\0' || strcasecmp(value, "false") == 0) {
				return std::string(1, FALSE_SERIALISED);
			}
			break;
		default:
			break;
	}

	throw MSG_SerialisationError("Boolean format is not valid");
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
	return std::string(serialise, SIZE_SERIALISE_CARTESIAN);
}


std::string
Serialise::trixel_id(uint64_t id)
{
	id = Swap7Bytes(id);
	const char serialise[] = { (char)(id & 0xFF), (char)((id >>  8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF),
							   (char)((id >> 32) & 0xFF), (char)((id >> 40) & 0xFF), (char)((id >> 48) & 0xFF) };
	return std::string(serialise, SIZE_BYTES_ID);
}


std::string
Serialise::type(FieldType type)
{
	switch (type) {
		case FieldType::STRING:   return STRING_STR;
		case FieldType::TEXT:     return TEXT_STR;
		case FieldType::FLOAT:    return FLOAT_STR;
		case FieldType::INTEGER:  return INTEGER_STR;
		case FieldType::POSITIVE: return POSITIVE_STR;
		case FieldType::BOOLEAN:  return BOOLEAN_STR;
		case FieldType::GEO:      return GEO_STR;
		case FieldType::DATE:     return DATE_STR;
		case FieldType::UUID:     return UUID_STR;
		case FieldType::OBJECT:   return OBJECT_STR;
		case FieldType::ARRAY:    return ARRAY_STR;
		case FieldType::EMPTY:    return std::string();
	}
}


::MsgPack
Unserialise::MsgPack(FieldType field_type, const std::string& serialise_val)
{
	::MsgPack result;
	switch (field_type) {
		case FieldType::FLOAT:
			result = _float(serialise_val);
			break;
		case FieldType::INTEGER:
			result = integer(serialise_val);
			break;
		case FieldType::POSITIVE:
			result = positive(serialise_val);
			break;
		case FieldType::DATE:
			result = date(serialise_val);
			break;
		case FieldType::BOOLEAN:
			result = boolean(serialise_val);
			break;
		case FieldType::STRING:
		case FieldType::TEXT:
			result = serialise_val;
			break;
		case FieldType::GEO:
			result = geo(serialise_val);
			break;
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", toUType(field_type));
	}

	return result;
}


std::string
Unserialise::unserialise(FieldType field_type, const std::string& serialise_val)
{
	switch (field_type) {
		case FieldType::FLOAT:
			return std::to_string(_float(serialise_val));
		case FieldType::INTEGER:
			return std::to_string(integer(serialise_val));
		case FieldType::POSITIVE:
			return std::to_string(positive(serialise_val));
		case FieldType::DATE:
			return date(serialise_val);
		case FieldType::BOOLEAN:
			return std::string(boolean(serialise_val) ? "true" : "false");
		case FieldType::STRING:
		case FieldType::TEXT:
			return serialise_val;
		case FieldType::GEO:
			return ewkt(serialise_val);
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", field_type);
	}
}


std::string
Unserialise::date(const std::string& serialise_val)
{
	static char date[25];
	double epoch = timestamp(serialise_val);
	time_t timestamp = (time_t) epoch;
	int msec = round((epoch - timestamp) * 1000);
	struct tm *timeinfo = gmtime(&timestamp);
	sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3d", timeinfo->tm_year + _START_YEAR,
		timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
		timeinfo->tm_sec, msec);
	return date;
}


Cartesian
Unserialise::cartesian(const std::string& serialise_val)
{
	if (serialise_val.size() != SIZE_SERIALISE_CARTESIAN) {
		throw MSG_SerialisationError("Can not unserialise cartesian: %s [%zu]", serialise_val.c_str(), serialise_val.size());
	}

	double x = (((unsigned)serialise_val[0] << 24) & 0xFF000000) | (((unsigned)serialise_val[1] << 16) & 0xFF0000) | (((unsigned)serialise_val[2] << 8) & 0xFF00)  | (((unsigned)serialise_val[3]) & 0xFF);
	double y = (((unsigned)serialise_val[4] << 24) & 0xFF000000) | (((unsigned)serialise_val[5] << 16) & 0xFF0000) | (((unsigned)serialise_val[6] << 8) & 0xFF00)  | (((unsigned)serialise_val[7]) & 0xFF);
	double z = (((unsigned)serialise_val[8] << 24) & 0xFF000000) | (((unsigned)serialise_val[9] << 16) & 0xFF0000) | (((unsigned)serialise_val[10] << 8) & 0xFF00) | (((unsigned)serialise_val[11]) & 0xFF);
	return Cartesian((x - MAXDOU2INT) / DOUBLE2INT, (y - MAXDOU2INT) / DOUBLE2INT, (z - MAXDOU2INT) / DOUBLE2INT);
}


uint64_t
Unserialise::trixel_id(const std::string& serialise_val)
{
	if (serialise_val.size() != SIZE_BYTES_ID) {
		throw MSG_SerialisationError("Can not unserialise trixel_id: %s [%zu]", serialise_val.c_str(), serialise_val.size());
	}

	uint64_t id = (((uint64_t)serialise_val[0] << 48) & 0xFF000000000000) | (((uint64_t)serialise_val[1] << 40) & 0xFF0000000000) | \
				  (((uint64_t)serialise_val[2] << 32) & 0xFF00000000)     | (((uint64_t)serialise_val[3] << 24) & 0xFF000000)     | \
				  (((uint64_t)serialise_val[4] << 16) & 0xFF0000)         | (((uint64_t)serialise_val[5] <<  8) & 0xFF00)         | \
				  (serialise_val[6] & 0xFF);
	return id;
}


std::pair<std::string, std::string>
Unserialise::geo(const std::string& serialise_ewkt)
{
	const char* pos = serialise_ewkt.data();
	const char* end = pos + serialise_ewkt.length();
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
Unserialise::ewkt(const std::string& serialise_ewkt)
{
	auto unserialise = geo(serialise_ewkt);
	RangeList ranges;
	ranges.unserialise(unserialise.first);
	std::string res("Ranges: { ");
	for (const auto& range : ranges) {
		res += "[" + std::to_string(range.start) + ", " + std::to_string(range.end) + "] ";
	}
	res += "}";

	CartesianUSet centroids;
	centroids.unserialise(unserialise.second);
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
		case FieldType::STRING:
			if (value[1] == '\0' || strcasecmp(value, STRING_STR) == 0) {
				return FieldType::STRING;
			}
			break;
		case FieldType::TEXT:
			if (value[1] == '\0' || strcasecmp(value, TEXT_STR) == 0) {
				return FieldType::TEXT;
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

	throw MSG_SerialisationError("Type: %s is an unknown type", str_type.c_str());
}
