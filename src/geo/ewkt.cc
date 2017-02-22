/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include "ewkt.h"

#include "../split.h"              // for Split
#include "../utils.h"              // for stox


const std::regex find_geometry_re("(SRID[\\s]*=[\\s]*([0-9]{4})[\\s]*\\;[\\s]*)?([A-Z]{5,20})[\\s]*\\(([()-.0-9\\s,A-Z]*)\\)", std::regex::optimize);
const std::regex find_circle_re("[\\s]*([-.0-9]+)[\\s]+([-.0-9]+)([\\s]+([-.0-9]+))?[\\s]*\\,[\\s]*([.0-9]+)[\\s]*");
const std::regex parenthesis_list_split_re("[\\s]*\\((.*?)\\)[\\s]*(\\,|$)", std::regex::optimize);
const std::regex find_collection_re("[\\s]*([A-Z]{5,12})[\\s]*\\(([()-.0-9\\s,]*)\\)[\\s]*(\\,|$)?", std::regex::optimize);


const std::unordered_map<std::string, EWKT::dispatch_func> EWKT::map_dispatch({
	{ "POINT",                   &EWKT::parse_point                   },
	{ "CIRCLE",                  &EWKT::parse_circle                  },
	{ "CONVEX",                  &EWKT::parse_convex                  },
	// { "POLYGON",                 &EWKT::parse_polygon                 },
	// { "CHULL",                   &EWKT::parse_chull                   },
	{ "MULTIPOINT",              &EWKT::parse_multipoint              },
	{ "MULTICIRCLE",             &EWKT::parse_multicircle             },
	// { "MULTIPOLYGON",            &EWKT::parse_multipolygon            },
	// { "MULTICHULL",              &EWKT::parse_multichull              },
	// { "GEOMETRYCOLLECTION",      &EWKT::parse_geometry_collection     },
	// { "GEOMETRYINTERSECTION",    &EWKT::parse_geometry_intersection   },
});


/*
 * Parser for EWKT (A PostGIS-specific format that includes the spatial reference system identifier (SRID))
 * Geometric objects WKT supported:
 *  POINT
 *  MULTIPOINT
 *  POLYGON       // Polygon should be convex. Otherwise it should be used CHULL.
 *  MULTIPOLYGON
 *  GEOMETRYCOLLECTION
 *
 * Geometric objects not defined in wkt, but defined here by their relevance:
 *  CIRCLE
 *  MULTICIRCLE
 *  CHULL  			// Convex Hull from a points' set.
 *  MULTICHULL
 *  GEOMETRYINTERSECTION
 *
 * Coordinates for geometries can be:
 * (lat lon) or (lat lon height)
 *
 * This parser do not accept EMPTY geometries and
 * polygons are not required to be repeated first coordinate to end like EWKT.
 */
EWKT::EWKT(const std::string& str)
{
	std::smatch m;
	if (std::regex_match(str, m, find_geometry_re) && static_cast<size_t>(m.length(0)) == str.length()) {
		int SRID = WGS84; // Default.
		if (m.length(2) != 0) {
			SRID = std::stoi(m.str(2));
			if (!Cartesian::is_SRID_supported(SRID)) {
				THROW(EWKTError, "SRID = %d is not supported", SRID);
			}
		}
		static const auto it_e = map_dispatch.end();
		const auto geometry = m.str(3);
		const auto it = map_dispatch.find(geometry);
		if (it == it_e) {
			THROW(EWKTError, "Syntax error in %s, geometry '%s' is not supported", str.c_str(), geometry.c_str());
		} else {
			(this->*it->second)(SRID, m.str(4));
		}
	} else {
		THROW(EWKTError, "Syntax error in %s", str.c_str());
	}
}


Point
EWKT::_parse_point(int SRID, const std::string& specification)
{
	const auto coords = Split::split(specification, ' ');
	switch (coords.size()) {
		case 2:
			return Point(Cartesian(stox(std::stod, coords.at(0)), stox(std::stod, coords.at(1)), 0, Cartesian::Units::DEGREES, SRID));
		case 3:
			return Point(Cartesian(stox(std::stod, coords.at(0)), stox(std::stod, coords.at(1)), stox(std::stod, coords.at(2)), Cartesian::Units::DEGREES, SRID));
		default:
			THROW(InvalidArgument, "Invalid specification");
	}
}


/*
 * The specification is (lat lon[ height])
 * lat and lon in degrees.
 * height in meters.
 */
void
EWKT::parse_point(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Point>(_parse_point(SRID, specification));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for POINT is '(lat lon[ height])' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for POINT is '(lat lon[ height])' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


Circle
EWKT::_parse_circle(int SRID, const std::string& specification)
{
	std::smatch m;
	if (std::regex_match(specification, m, find_circle_re) && static_cast<size_t>(m.length(0)) == specification.size()) {
		double lat = stox(std::stod, m.str(1));
		double lon = stox(std::stod, m.str(2));
		double h = m.length(4) == 0 ? 0 : stox(std::stod, m.str(4));
		double radius = stox(std::stod, m.str(5));
		return Circle(Cartesian(lat, lon, h, Cartesian::Units::DEGREES, SRID), radius);
	} else {
		THROW(InvalidArgument, "Invalid specification");
	}
}


/*
 * The specification is: (lat lon[ height], radius)
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_circle(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Circle>(_parse_circle(SRID, specification));
	} catch (const std::exception& err) {
		THROW(EWKTError, "Specification for CIRCLE is '(lat lon[ height], radius)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for CIRCLE is '(lat lon[ height], radius)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


Convex
EWKT::_parse_convex(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	Convex convex;

	std::sregex_iterator next(specification.begin(), specification.end(), parenthesis_list_split_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		convex.add(_parse_circle(SRID, next->str(1)));
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return convex;
}


/*
 * The specification is: ((lat lon[ height], radius), ... (lat lon[ height], radius))
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_convex(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Convex>(_parse_convex(SRID, specification));
	} catch (const std::exception& err) {
		THROW(EWKTError, "Specification for CONVEX is '((lat lon[ height], radius), ... (lat lon[ height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for CONVEX is '((lat lon[ height], radius), ... (lat lon[ height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


MultiPoint
EWKT::_parse_multipoint(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	MultiPoint multipoint;

	std::sregex_iterator next(specification.begin(), specification.end(), parenthesis_list_split_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		multipoint.add(_parse_point(SRID, next->str(1)));
		match_len += next->length(0);
		++next;
	}

	if (match_len == 0) {
		const auto spc_points = Split::split(specification, ',');
		for (const auto& spc_point : spc_points) {
			multipoint.add(_parse_point(SRID, spc_point));
		}
	} else if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return multipoint;
}


/*
 * The specification is (lat lon [height], ..., lat lon [height]) or ((lat lon [height]), ..., (lat lon [height]))
 * lat and lon in degrees.
 * height in meters.
 */
void
EWKT::parse_multipoint(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<MultiPoint>(_parse_multipoint(SRID, specification));
	} catch (const std::exception& err) {
		THROW(EWKTError, "Specification for MULTIPOINT is '(lat lon [height], ..., lat lon [height]) or ((lat lon [height]), ..., (lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTIPOINT is '(lat lon [height], ..., lat lon [height]) or ((lat lon [height]), ..., (lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


MultiCircle
EWKT::_parse_multicircle(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	MultiCircle multicircle;

	std::sregex_iterator next(specification.begin(), specification.end(), parenthesis_list_split_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		multicircle.add(_parse_circle(SRID, next->str(1)));
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return multicircle;
}


/*
 * The specification is: ((lat lon [height], radius), ... (lat lon [height], radius))
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_multicircle(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<MultiCircle>(_parse_multicircle(SRID, specification));
	} catch (const std::exception& err) {
		THROW(EWKTError, "Specification for MULTICIRCLE is '((lat lon [height], radius), ... (lat lon [height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTICIRCLE is '((lat lon [height], radius), ... (lat lon [height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


bool
EWKT::isEWKT(const std::string& str)
{
	std::smatch m;
	static const auto it_e = map_dispatch.end();
	return std::regex_match(str, m, find_geometry_re) && static_cast<size_t>(m.length(0)) == str.length() && map_dispatch.find(m.str(3)) != it_e;
}
