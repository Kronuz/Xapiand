/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "wkt_parser.h"
#include "utils.h"
#include "log.h"
#include "hash/sha256.h"
#include "multivalue.h"

#include <xapian.h>


std::string
Serialise::serialise(char field_type, const MsgPack& field_value)
{
	switch (field_value.obj->type) {
		case msgpack::type::NIL:
			return boolean(field_type, false);
		case msgpack::type::BOOLEAN:
			return boolean(field_type, field_value.obj->via.boolean);
		case msgpack::type::POSITIVE_INTEGER:
			return numeric(field_type, field_value.obj->via.u64);
		case msgpack::type::NEGATIVE_INTEGER:
			return numeric(field_type, field_value.obj->via.i64);
		case msgpack::type::FLOAT:
			return numeric(field_type, field_value.obj->via.f64);
		case msgpack::type::STR:
			return string(field_type, std::string(field_value.obj->via.str.ptr, field_value.obj->via.str.size));
		default:
			throw MSG_Error("msgpack::type [%d] is not supported", field_value.obj->type);
	}
}


std::string
Serialise::serialise(char field_type, const std::string& field_value)
{
	if (field_value.empty()) {
		throw MSG_Error("Field value must be defined");
	}

	switch (field_type) {
		case NUMERIC_TYPE:
			return numeric(field_value);
		case DATE_TYPE:
			return date(field_value);
		case BOOLEAN_TYPE:
			return boolean(field_value);
		case STRING_TYPE:
			return field_value;
		case GEO_TYPE:
			return ewkt(field_value);
		default:
			throw MSG_Error("Type: '%c' is an unknown type", type);
	}
}


std::string
Serialise::string(char field_type, const std::string& field_value)
{
	if (field_value.empty()) {
		throw MSG_Error("Field value must be defined");
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
			throw MSG_Error("%s is not string", type(field_type).c_str());
	}
}


std::string
Serialise::numeric(char field_type, double field_value)
{
	switch (field_type) {
		case DATE_TYPE:
		case NUMERIC_TYPE:
			return Xapian::sortable_serialise(field_value);
		case BOOLEAN_TYPE:
			return field_value ? std::string("t") : std::string("f");
		default:
			throw MSG_Error("%s is not numeric", type(field_type).c_str());
	}
}


std::string
Serialise::boolean(char field_type, bool field_value)
{
	if (field_type == BOOLEAN_TYPE) {
		return field_value ? std::string("t") : std::string("f");
	} else {
		throw MSG_Error("%s is not boolean", type(field_type).c_str());
	}
}


std::string
Serialise::date(const std::string& field_value)
{
	double timestamp = Datetime::timestamp(field_value);
	return Xapian::sortable_serialise(timestamp);
}


std::string
Serialise::date(const MsgPack& value, Datetime::tm_t& tm)
{
	double timestamp;
	switch (value.obj->type) {
		case msgpack::type::POSITIVE_INTEGER:
			timestamp = value.obj->via.u64;
			tm = Datetime::to_tm_t(timestamp);
			return Xapian::sortable_serialise(timestamp);
		case msgpack::type::NEGATIVE_INTEGER:
			timestamp = value.obj->via.i64;
			tm = Datetime::to_tm_t(timestamp);
			return Xapian::sortable_serialise(timestamp);
		case msgpack::type::FLOAT:
			timestamp = value.obj->via.f64;
			tm = Datetime::to_tm_t(timestamp);
			return Xapian::sortable_serialise(timestamp);
		case msgpack::type::STR:
			timestamp = Datetime::timestamp(std::string(value.obj->via.str.ptr, value.obj->via.str.size), tm);
			return Xapian::sortable_serialise(timestamp);
		default:
			throw MSG_Error("Date value must be numeric or string");
	}
}


std::string
Serialise::date_with_math(Datetime::tm_t tm, const std::string& op, const std::string& units)
{
	Datetime::computeDateMath(tm, op, units);
	return Xapian::sortable_serialise(Datetime::mtimegm(tm));
}


std::string
Serialise::numeric(const std::string& field_value)
{
	if (isNumeric(field_value)) {
		return Xapian::sortable_serialise(std::stod(field_value));
	}

	throw MSG_Error("Invalid numeric format");
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

	throw MSG_Error("Boolean format is not valid");
}


std::string
Serialise::cartesian(const Cartesian& norm_cartesian)
{
	unsigned int x = Swap4Bytes(((unsigned int)(norm_cartesian.x * DOUBLE2INT) + MAXDOU2INT));
	unsigned int y = Swap4Bytes(((unsigned int)(norm_cartesian.y * DOUBLE2INT) + MAXDOU2INT));
	unsigned int z = Swap4Bytes(((unsigned int)(norm_cartesian.z * DOUBLE2INT) + MAXDOU2INT));
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
		case STRING_TYPE:  return STRING_STR;
		case NUMERIC_TYPE: return NUMERIC_STR;
		case BOOLEAN_TYPE: return BOOLEAN_STR;
		case GEO_TYPE:     return GEO_STR;
		case DATE_TYPE:    return DATE_STR;
		case OBJECT_TYPE:  return OBJECT_STR;
		case ARRAY_TYPE:   return ARRAY_STR;
	}

	throw MSG_Error("Type: '%c' is an unknown type", type);
}


void
Unserialise::unserialise(char field_type, const std::string& serialise_val, MsgPack& result)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			result = numeric(serialise_val);
			return;
		case DATE_TYPE:
			result = date(serialise_val);
			return;
		case BOOLEAN_TYPE:
			result = boolean(serialise_val);
			return;
		case STRING_TYPE:
			result = serialise_val;
			return;
		case GEO_TYPE:
			result = geo(serialise_val);
			return;
		default:
			throw MSG_Error("Type: '%c' is an unknown type", field_type);
	}
}


std::string
Unserialise::unserialise(char field_type, const std::string& serialise_val)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return std::to_string(numeric(serialise_val));
		case DATE_TYPE:
			return date(serialise_val);
		case BOOLEAN_TYPE:
			return std::string(boolean(serialise_val) ? "true" : "false");
		case STRING_TYPE:
			return serialise_val;
		case GEO_TYPE:
			return geo(serialise_val);
		default:
			throw MSG_Error("Type: '%c' is an unknown type", field_type);
	}
}


double
Unserialise::numeric(const std::string& serialise_val)
{
	return Xapian::sortable_unserialise(serialise_val);
}


std::string
Unserialise::date(const std::string& serialise_val)
{
	static char date[25];
	double epoch = Xapian::sortable_unserialise(serialise_val);
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
		throw MSG_Error("Can not unserialise cartesian: %s [%zu]", serialise_val.c_str(), serialise_val.size());
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
		throw MSG_Error("Can not unserialise trixel_id: %s [%zu]", serialise_val.c_str(), serialise_val.size());
	}

	uint64_t id = (((uint64_t)serialise_val[0] << 48) & 0xFF000000000000) | (((uint64_t)serialise_val[1] << 40) & 0xFF0000000000) | \
				  (((uint64_t)serialise_val[2] << 32) & 0xFF00000000)     | (((uint64_t)serialise_val[3] << 24) & 0xFF000000)     | \
				  (((uint64_t)serialise_val[4] << 16) & 0xFF0000)         | (((uint64_t)serialise_val[5] <<  8) & 0xFF00)         | \
				  (serialise_val[6] & 0xFF);
	return id;
}


std::string
Unserialise::geo(const std::string& serialise_val)
{
	StringList s_geo;
	s_geo.unserialise(serialise_val);
	uInt64List ranges;
	ranges.unserialise(s_geo.at(0));
	std::string res("Ranges: { ");
	for (auto it = ranges.begin(); it != ranges.end(); ++it) {
		res += "[" + std::to_string(*it) + ", " + std::to_string(*(++it)) + "] ";
	}
	res += "}";

	CartesianList centroids;
	centroids.unserialise(s_geo.at(1));
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
	std::string low = lower_string(str_type);
	if (low.size() == 1) {
		switch (low[0]) {
			case NUMERIC_TYPE:
				return std::string(1, toupper(NUMERIC_TYPE));
			case GEO_TYPE:
				return std::string(1, toupper(GEO_TYPE));
			case STRING_TYPE:
				return std::string(1, toupper(STRING_TYPE));
			case BOOLEAN_TYPE:
				return std::string(1, toupper(BOOLEAN_TYPE));
			case DATE_TYPE:
				return std::string(1, toupper(DATE_TYPE));
		}
	} else if (low.compare(NUMERIC_STR) == 0) {
		return std::string(1, toupper(NUMERIC_TYPE));
	} else if (low.compare(GEO_STR) == 0) {
		return std::string(1, toupper(GEO_TYPE));
	} else if (low.compare(STRING_STR) == 0 ) {
		return std::string(1, toupper(STRING_TYPE));
	} else if (low.compare(BOOLEAN_STR) == 0 ) {
		return std::string(1, toupper(BOOLEAN_TYPE));
	} else if (low.compare(DATE_STR) == 0) {
		return std::string(1, toupper(DATE_TYPE));
	}

	throw MSG_Error("Type: %s is an unknown type", str_type.c_str());
}
