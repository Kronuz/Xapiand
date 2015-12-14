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

#include "datetime.h"
#include "wkt_parser.h"
#include "utils.h"
#include "log.h"
#include "hash/sha256.h"

#include <xapian.h>


std::string
Serialise::serialise(char field_type, const std::string &field_value)
{
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
	}
	return "";
}


std::string
Serialise::numeric(const std::string &field_value)
{
	double val;
	if (isNumeric(field_value)) {
		val = std::stod(field_value);
		return Xapian::sortable_serialise(val);
	}
	return "";
}


std::string
Serialise::date(const std::string &field_value)
{
	try {
		double timestamp = Datetime::timestamp(field_value);
		return Xapian::sortable_serialise(timestamp);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
	}
	return "";
}


std::string
Serialise::date(int timeinfo_[])
{
	time_t tt = 0;
	struct tm *timeinfo = gmtime(&tt);
	timeinfo->tm_year   = timeinfo_[5];
	timeinfo->tm_mon    = timeinfo_[4];
	timeinfo->tm_mday   = timeinfo_[3];
	timeinfo->tm_hour   = timeinfo_[2];
	timeinfo->tm_min    = timeinfo_[1];
	timeinfo->tm_sec    = timeinfo_[0];
	return std::to_string(Datetime::timegm(timeinfo));
}


std::string
Serialise::cartesian(const Cartesian &norm_cartesian)
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
Serialise::trixel_id(uInt64 id)
{
	id = Swap7Bytes(id);
	const char serialise[] = { (char)(id & 0xFF), (char)((id >>  8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF),
							   (char)((id >> 32) & 0xFF), (char)((id >> 40) & 0xFF), (char)((id >> 48) & 0xFF) };
	return std::string(serialise, SIZE_BYTES_ID);
}


std::string
Serialise::ewkt(const std::string &field_value)
{
	std::string result;

	EWKT_Parser ewkt(field_value, false, HTM_MIN_ERROR);

	if (ewkt.trixels.empty()) return result;

	for (auto it = ewkt.trixels.begin(); it != ewkt.trixels.end(); ++it) {
		result += *it;
	}

	SHA256 sha256;
	return sha256(result);
}


std::string
Serialise::boolean(const std::string &field_value)
{
	if (field_value.empty()) {
		return "f";
	} else if (strcasecmp(field_value.c_str(), "true") == 0) {
		return "t";
	} else if (strcasecmp(field_value.c_str(), "false") == 0) {
		return "f";
	} else if (strcasecmp(field_value.c_str(), "1") == 0) {
		return "t";
	} else if (strcasecmp(field_value.c_str(), "0") == 0) {
		return "f";
	} else {
		return "";
	}
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
	return "";
}


std::string
Unserialise::unserialise(char field_type, const std::string &serialise_val)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return numeric(serialise_val);
		case DATE_TYPE:
			return date(serialise_val);
		case BOOLEAN_TYPE:
			return boolean(serialise_val);
		case STRING_TYPE:
			return serialise_val;
		case GEO_TYPE: {
			return geo(serialise_val);
		}
	}
	return "";
}


std::string
Unserialise::numeric(const std::string &serialise_val)
{
	return std::to_string(Xapian::sortable_unserialise(serialise_val));
}


std::string
Unserialise::date(const std::string &serialise_val)
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
Unserialise::cartesian(const std::string &str)
{
	double x = (((unsigned int)str.at(0) << 24) & 0xFF000000) | (((unsigned int)str.at(1) << 16) & 0xFF0000) | (((unsigned int)str.at(2) << 8) & 0xFF00) | (((unsigned int)str.at(3)) & 0xFF);
	double y = (((unsigned int)str.at(4) << 24) & 0xFF000000) | (((unsigned int)str.at(5) << 16) & 0xFF0000) | (((unsigned int)str.at(6) << 8) & 0xFF00) | (((unsigned int)str.at(7)) & 0xFF);
	double z = (((unsigned int)str.at(8) << 24) & 0xFF000000) | (((unsigned int)str.at(9) << 16) & 0xFF0000) | (((unsigned int)str.at(10) << 8) & 0xFF00) | (((unsigned int)str.at(11)) & 0xFF);
	return Cartesian((x - MAXDOU2INT) / DOUBLE2INT, (y - MAXDOU2INT) / DOUBLE2INT, (z - MAXDOU2INT) / DOUBLE2INT);
}


uInt64
Unserialise::trixel_id(const std::string &str)
{
	uInt64 id = (((uInt64)str.at(0) << 48) & 0xFF000000000000) | (((uInt64)str.at(1) << 40) & 0xFF0000000000) | \
				(((uInt64)str.at(2) << 32) & 0xFF00000000)     | (((uInt64)str.at(3) << 24) & 0xFF000000)     | \
				(((uInt64)str.at(4) << 16) & 0xFF0000)         | (((uInt64)str.at(5) <<  8) & 0xFF00)         | \
				(str.at(6) & 0xFF);
	return id;
}


std::string
Unserialise::boolean(const std::string &serialise_val)
{
	return serialise_val.at(0) == 'f' ? "false" : "true";
}


std::string
Unserialise::geo(const std::string &serialise_val)
{
	StringList s_geo;
	s_geo.unserialise(serialise_val);
	uInt64List ranges;
	ranges.unserialise(s_geo.at(0));
	std::string res("Ranges: { ");
	for (uInt64List::const_iterator it(ranges.begin()); it != ranges.end(); ++it) {
		res += "[" + std::to_string(*it) + ", " + std::to_string(*(++it)) + "] ";
	}
	res += "}";

	CartesianList centroids;
	centroids.unserialise(s_geo.at(1));
	res += "  Centroids: { ";
	for (CartesianList::const_iterator it(centroids.begin()); it != centroids.end(); ++it) {
		res += "(" + std::to_string(it->x) + ", " + std::to_string(it->y) + ", " + std::to_string(it->z) + ") ";
	}
	res += "}";

	return res;
}


std::string
Unserialise::type(const std::string &str)
{
	std::string low(stringtolower(str));
	if (low.compare(NUMERIC_STR) == 0 || (low.size() == 1 && low[0] == NUMERIC_TYPE)) {
		return std::string(1, toupper(NUMERIC_TYPE));
	} else if (low.compare(GEO_STR) == 0     || (low.size() == 1 && low[0] == GEO_TYPE)) {
		return std::string(1, toupper(GEO_TYPE));
	} else if (low.compare(STRING_STR) == 0  || (low.size() == 1 && low[0] == STRING_TYPE)) {
		return std::string(1, toupper(STRING_TYPE));
	} else if (low.compare(BOOLEAN_STR) == 0 || (low.size() == 1 && low[0] == BOOLEAN_TYPE)) {
		return std::string(1, toupper(BOOLEAN_TYPE));
	} else if (low.compare(DATE_STR) == 0    || (low.size() == 1 && low[0] == DATE_TYPE)) {
		return std::string(1, toupper(DATE_TYPE));
	} else {
		return std::string(1, toupper(STRING_TYPE));
	}
}
