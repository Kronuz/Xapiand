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
#include "log.h"
#include "sortable_serialise.h"
#include "utils.h"
#include "wkt_parser.h"

#include <xapian.h>


constexpr size_t SIZE_BYTES_POSITIVE = 8;


std::string
Serialise::serialise(char field_type, const MsgPack& field_value)
{
	switch (field_value.type()) {
		case msgpack::type::NIL:
			throw MSG_DummyException();
		case msgpack::type::BOOLEAN:
			return boolean(field_type, field_value.as_bool());
		case msgpack::type::POSITIVE_INTEGER:
			return positive(field_type, field_value.as_u64());
		case msgpack::type::NEGATIVE_INTEGER:
			return integer(field_type, field_value.as_i64());
		case msgpack::type::FLOAT:
			return _float(field_type, field_value.as_f64());
		case msgpack::type::STR:
			return string(field_type, field_value.as_string());
		default:
			throw MSG_SerialisationError("msgpack::type [%d] is not supported", field_value.type());
	}
}


std::string
Serialise::serialise(char field_type, const std::string& field_value)
{
	if (field_value.empty() && field_type != STRING_TYPE) {
		throw MSG_SerialisationError("Field value must be defined");
	}

	switch (field_type) {
		case INTEGER_TYPE:
			return integer(field_value);
		case POSITIVE_TYPE:
			return positive(field_value);
		case FLOAT_TYPE:
			return _float(field_value);
		case DATE_TYPE:
			return date(field_value);
		case BOOLEAN_TYPE:
			return boolean(field_value);
		case STRING_TYPE:
			return field_value;
		case GEO_TYPE:
			return ewkt(field_value);
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", type);
	}
}


std::pair<char, std::string>
Serialise::serialise(const std::string& field_value)
{
	// Try like integer.
	try {
		return std::make_pair(INTEGER_TYPE, Serialise::integer(field_value));
	} catch (const SerialisationError&) { }

	// Try like positive.
	try {
		return std::make_pair(POSITIVE_TYPE, Serialise::positive(field_value));
	} catch (const SerialisationError&) { }

	// Try like Float
	try {
		return std::make_pair(FLOAT_TYPE, Serialise::_float(field_value));
	} catch (const SerialisationError&) { }

	// Try like date
	try {
		return std::make_pair(DATE_TYPE, date(field_value));
	} catch (const DatetimeError&) { }

	// Try like GEO
	try {
		return std::make_pair(GEO_TYPE, ewkt(field_value));
	} catch (const EWKTError&) { }

	// Default type: String
	return std::make_pair(STRING_TYPE, field_value);
}


std::string
Serialise::string(char field_type, const std::string& field_value)
{
	if (field_value.empty() && field_type != STRING_TYPE) {
		throw MSG_SerialisationError("Field value must be defined");
	}

	switch (field_type) {
		case DATE_TYPE:
			return date(field_value);
		case BOOLEAN_TYPE:
			return boolean(field_value);
		case STRING_TYPE:
			return field_value;
		case GEO_TYPE:
			return ewkt(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not string", type(field_type).c_str());
	}
}


std::string
Serialise::_float(char field_type, double field_value)
{
	switch (field_type) {
		case DATE_TYPE:
		case FLOAT_TYPE:
			return sortable_serialise(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not a float", type(field_type).c_str());
	}
}


std::string
Serialise::integer(char field_type, int64_t field_value)
{
	switch (field_type) {
		case POSITIVE_TYPE:
			if (field_value < 0) {
				throw MSG_SerialisationError("Type: %s must be a positive number [%lld]", type(field_type).c_str(), field_value);
			}
			return sortable_serialise(field_value);
		case DATE_TYPE:
		case FLOAT_TYPE:
		case INTEGER_TYPE:
			return sortable_serialise(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not a integer [%lld]", type(field_type).c_str(), field_value);
	}
}


std::string
Serialise::positive(char field_type, uint64_t field_value)
{
	switch (field_type) {
		case DATE_TYPE:
		case FLOAT_TYPE:
		case INTEGER_TYPE:
		case POSITIVE_TYPE:
			return sortable_serialise(field_value);
		default:
			throw MSG_SerialisationError("Type: %s is not a positive integer [%lld]", type(field_type).c_str(), field_value);
	}
}


std::string
Serialise::boolean(char field_type, bool field_value)
{
	if (field_type == BOOLEAN_TYPE) {
		return field_value ? std::string("t") : std::string("f");
	} else {
		throw MSG_SerialisationError("%s is not boolean", type(field_type).c_str());
	}
}


std::string
Serialise::date(const std::string& field_value)
{
	return sortable_serialise(Datetime::timestamp(field_value));
}


std::string
Serialise::date(const MsgPack& value, Datetime::tm_t& tm)
{
	double timestamp;
	switch (value.type()) {
		case msgpack::type::POSITIVE_INTEGER:
			timestamp = value.as_u64();
			tm = Datetime::to_tm_t(timestamp);
			return sortable_serialise(timestamp);
		case msgpack::type::NEGATIVE_INTEGER:
			timestamp = value.as_i64();
			tm = Datetime::to_tm_t(timestamp);
			return sortable_serialise(timestamp);
		case msgpack::type::FLOAT:
			timestamp = value.as_f64();
			tm = Datetime::to_tm_t(timestamp);
			return sortable_serialise(timestamp);
		case msgpack::type::STR:
			timestamp = Datetime::timestamp(value.as_string(), tm);
			return sortable_serialise(timestamp);
		default:
			throw MSG_SerialisationError("Date value must be numeric or string");
	}
}


std::string
Serialise::date_with_math(Datetime::tm_t tm, const std::string& op, const std::string& units)
{
	Datetime::computeDateMath(tm, op, units);
	return sortable_serialise(Datetime::mtimegm(tm));
}


std::string
Serialise::_float(const std::string& field_value)
{
	try {
		return sortable_serialise(strict(std::stod, field_value));
	} catch (const std::invalid_argument&) {
		throw MSG_SerialisationError("Invalid float format: %s", field_value.c_str());
	}  catch (const std::out_of_range&) {
		throw MSG_SerialisationError("Out of range float format: %s", field_value.c_str());
	}
}


std::string
Serialise::integer(const std::string& field_value)
{
	try {
		return sortable_serialise(strict(std::stoll, field_value));
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
		return sortable_serialise(strict(std::stoull, field_value));
	} catch (const std::invalid_argument&) {
		throw MSG_SerialisationError("Invalid positive integer format: %s", field_value.c_str());
	} catch (const std::out_of_range&) {
		throw MSG_SerialisationError("Out of range positive integer format: %s", field_value.c_str());
	}
}


std::string
Serialise::ewkt(const std::string& field_value)
{
	std::string result;

	EWKT_Parser ewkt(field_value, false, HTM_MIN_ERROR);

	if (ewkt.trixels.empty()) {
		return std::string();
	}

	for (const auto& trixel : ewkt.trixels) {
		result += trixel;
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
			return std::string("f");

		case '1':
		case 't':
		case 'T':
			if (value[1] == '\0') {
				return std::string("t");
			}
			if (strcasecmp(value, "true") == 0) {
				return std::string("t");
			}
			break;

		case '0':
		case 'f':
		case 'F':
			if (value[1] == '\0') {
				return std::string("f");
			}
			if (strcasecmp(value, "false") == 0) {
				return std::string("f");
			}
			break;

		default:
			break;
	}

	throw MSG_SerialisationError("Boolean format is not valid");
}


std::string
Serialise::boolean(bool field_value)
{
	return field_value ? std::string("t") : std::string("f");
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
Serialise::type(char type)
{
	switch (type) {
		case STRING_TYPE:   return STRING_STR;
		case FLOAT_TYPE:    return FLOAT_STR;
		case INTEGER_TYPE:  return INTEGER_STR;
		case POSITIVE_TYPE: return POSITIVE_STR;
		case BOOLEAN_TYPE:  return BOOLEAN_STR;
		case GEO_TYPE:      return GEO_STR;
		case DATE_TYPE:     return DATE_STR;
		case OBJECT_TYPE:   return OBJECT_STR;
		case ARRAY_TYPE:    return ARRAY_STR;
		case NO_TYPE:       return std::string();
	}

	throw MSG_SerialisationError("Type: '%c' is an unknown type", type);
}


MsgPack
Unserialise::MsgPack(char field_type, const std::string& serialise_val)
{
	::MsgPack result;
	switch (field_type) {
		case FLOAT_TYPE:
			result = _float(serialise_val);
			break;
		case INTEGER_TYPE:
			result = integer(serialise_val);
			break;
		case POSITIVE_TYPE:
			result = static_cast<uint64_t>(positive(serialise_val));
			break;
		case DATE_TYPE:
			result = date(serialise_val);
			break;
		case BOOLEAN_TYPE:
			result = boolean(serialise_val);
			break;
		case STRING_TYPE:
			result = serialise_val;
			break;
		case GEO_TYPE:
			result = geo(serialise_val);
			break;
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", field_type);
	}

	return result;
}


std::string
Unserialise::unserialise(char field_type, const std::string& serialise_val)
{
	switch (field_type) {
		case FLOAT_TYPE:
			return std::to_string(_float(serialise_val));
		case INTEGER_TYPE:
			return std::to_string(integer(serialise_val));
		case POSITIVE_TYPE:
			return std::to_string(positive(serialise_val));
		case DATE_TYPE:
			return date(serialise_val);
		case BOOLEAN_TYPE:
			return std::string(boolean(serialise_val) ? "true" : "false");
		case STRING_TYPE:
			return serialise_val;
		case GEO_TYPE:
			return ewkt(serialise_val);
		default:
			throw MSG_SerialisationError("Type: '%c' is an unknown type", field_type);
	}
}


double
Unserialise::_float(const std::string& serialise_val)
{
	return sortable_unserialise(serialise_val);
}


int64_t
Unserialise::integer(const std::string& serialise_val)
{
	return sortable_unserialise(serialise_val);
}


long double
Unserialise::positive(const std::string& serialise_val)
{
	return sortable_unserialise(serialise_val);
}


std::string
Unserialise::date(const std::string& serialise_val)
{
	static char date[25];
	double epoch = sortable_unserialise(serialise_val);
	time_t timestamp = (time_t) epoch;
	int msec = round((epoch - timestamp) * 1000);
	struct tm *timeinfo = gmtime(&timestamp);
	sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3d", timeinfo->tm_year + _START_YEAR,
		timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
		timeinfo->tm_sec, msec);
	return date;
}


double
Unserialise::timestamp(const std::string& serialise_val)
{
	return sortable_unserialise(serialise_val);
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


bool
Unserialise::boolean(const std::string& serialise_val)
{
	return serialise_val.at(0) == 't';
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
	const char* end = pos + serialise_ewkt.size();
	try {
		unserialise_length(&pos, end, true);
		auto length = unserialise_length(&pos, end, true);
		std::string serialise_ranges(pos, length);
		pos += length;
		length = unserialise_length(&pos, end, true);
		return std::make_pair(std::move(serialise_ranges), std::string(pos, length));
	} catch (const Xapian::SerialisationError&) {
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


std::string
Unserialise::type(const std::string& str_type)
{
	const char *value = str_type.c_str();
	switch (toupper(value[0])) {
		case FLOAT_TYPE:
			if (value[1] == '\0') {
				return std::string(1, FLOAT_TYPE);
			}
			if (strcasecmp(value, FLOAT_STR) == 0) {
				return std::string(1, FLOAT_TYPE);
			}
			break;
		case INTEGER_TYPE:
			if (value[1] == '\0') {
				return std::string(1, INTEGER_TYPE);
			}
			if (strcasecmp(value, INTEGER_STR)) {
				return std::string(1, INTEGER_TYPE);
			}
			break;
		case POSITIVE_TYPE:
			if (value[1] == '\0') {
				return std::string(1, POSITIVE_TYPE);
			}
			if (strcasecmp(value, POSITIVE_STR)) {
				return std::string(1, POSITIVE_TYPE);
			}
			break;
		case GEO_TYPE:
			if (value[1] == '\0') {
				return std::string(1, GEO_TYPE);
			}
			if (strcasecmp(value, GEO_STR)) {
				return std::string(1, GEO_TYPE);
			}
			break;
		case STRING_TYPE:
			if (value[1] == '\0') {
				return std::string(1, STRING_TYPE);
			}
			if (strcasecmp(value, STRING_STR)) {
				return std::string(1, STRING_TYPE);
			}
			break;
		case BOOLEAN_TYPE:
			if (value[1] == '\0') {
				return std::string(1, BOOLEAN_TYPE);
			}
			if (strcasecmp(value, BOOLEAN_STR)) {
				return std::string(1, BOOLEAN_TYPE);
			}
			break;
		case DATE_TYPE:
			if (value[1] == '\0') {
				return std::string(1, DATE_TYPE);
			}
			if (strcasecmp(value, DATE_STR)) {
				return std::string(1, DATE_TYPE);
			}
			break;
		default:
			break;
	}

	throw MSG_SerialisationError("Type: %s is an unknown type", str_type.c_str());
}
