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

#include "../split.h"      // for Split
#include "../utils.h"      // for stox


const std::regex find_geometry_re("(SRID[\\s]*=[\\s]*([0-9]{4})[\\s]*\\;[\\s]*)?([A-Z]{5,20})[\\s]*\\(([()-.0-9\\s,A-Z]*)\\)", std::regex::optimize);
const std::regex find_circle_re("[\\s]*([-.0-9]+)[\\s]+([-.0-9]+)([\\s]+([-.0-9]+))?[\\s]*\\,[\\s]*([.0-9]+)[\\s]*");
const std::regex find_parenthesis_list_re("[\\s]*\\((.*?)\\)[\\s]*(\\,|$)", std::regex::optimize);
const std::regex find_nested_parenthesis_list_re("[\\s]*[\\s]*\\([\\s]*(.*?\\))[\\s]*\\)[\\s]*(\\,|$)", std::regex::optimize);
const std::regex find_collection_re("[\\s]*([A-Z]{5,12})[\\s]*\\(([()-.0-9\\s,]*)\\)[\\s]*(\\,|$)?", std::regex::optimize);


const std::unordered_map<std::string, EWKT::dispatch_func> EWKT::map_dispatch({
	{ "POINT",                   &EWKT::parse_point                   },
	{ "CIRCLE",                  &EWKT::parse_circle                  },
	{ "CONVEX",                  &EWKT::parse_convex                  },
	{ "POLYGON",                 &EWKT::parse_polygon                 },
	{ "CHULL",                   &EWKT::parse_chull                   },
	{ "MULTIPOINT",              &EWKT::parse_multipoint              },
	{ "MULTICIRCLE",             &EWKT::parse_multicircle             },
	{ "MULTICONVEX",             &EWKT::parse_multiconvex             },
	{ "MULTIPOLYGON",            &EWKT::parse_multipolygon            },
	{ "MULTICHULL",              &EWKT::parse_multichull              },
	{ "GEOMETRYCOLLECTION",      &EWKT::parse_geometry_collection     },
	{ "GEOMETRYINTERSECTION",    &EWKT::parse_geometry_intersection   },
});


const std::unordered_map<std::string, Geometry::Type> EWKT::map_recursive_dispatch({
	{ "POINT",                   Geometry::Type::POINT              },
	{ "CIRCLE",                  Geometry::Type::CIRCLE             },
	{ "CONVEX",                  Geometry::Type::CONVEX             },
	{ "POLYGON",                 Geometry::Type::POLYGON            },
	{ "CHULL",                   Geometry::Type::CHULL              },
	{ "MULTIPOINT",              Geometry::Type::MULTIPOINT         },
	{ "MULTICIRCLE",             Geometry::Type::MULTICIRCLE        },
	{ "MULTICONVEX",             Geometry::Type::MULTICONVEX        },
	{ "MULTIPOLYGON",            Geometry::Type::MULTIPOLYGON       },
	{ "MULTICHULL",              Geometry::Type::MULTICHULL         },
	{ "GEOMETRYCOLLECTION",      Geometry::Type::COLLECTION         },
	{ "GEOMETRYINTERSECTION",    Geometry::Type::INTERSECTION       },
});


/*
 * Parser for EWKT (A PostGIS-specific format that includes the spatial reference system identifier (SRID))
 * Geometric objects EWKT supported:
 *  POINT
 *  MULTIPOINT
 *  POLYGON       // Polygon should be convex. Otherwise it should be used CHULL.
 *  MULTIPOLYGON
 *  GEOMETRYCOLLECTION
 *
 * Geometric objects not defined in EWKT, but defined here by their relevance:
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
	const Split<char> coords(specification, ' ');
	switch (coords.size()) {
		case 2: {
			auto it = coords.begin();
			return Point(Cartesian(stox(std::stod, *it), stox(std::stod, *++it), 0, Cartesian::Units::DEGREES, SRID));
		}
		case 3: {
			auto it = coords.begin();
			return Point(Cartesian(stox(std::stod, *it), stox(std::stod, *++it), stox(std::stod, *++it), Cartesian::Units::DEGREES, SRID));
		}
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
	} catch (const InvalidArgument& err) {
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

	std::sregex_iterator next(specification.begin(), specification.end(), find_parenthesis_list_re, std::regex_constants::match_continuous);
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
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for CONVEX is '((lat lon[ height], radius), ... (lat lon[ height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for CONVEX is '((lat lon[ height], radius), ... (lat lon[ height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}



Polygon
EWKT::_parse_polygon(int SRID, const std::string& specification, Geometry::Type type)
{
	size_t match_len = 0;
	Polygon polygon(type);

	std::sregex_iterator next(specification.begin(), specification.end(), find_parenthesis_list_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		std::vector<Cartesian> pts;
		// Split points
		const Split<char> points(next->str(1), ',');
		const auto num_pts = points.size();
		if (num_pts < 3) {
			THROW(InvalidArgument, "Polygon must have at least three points");
		}
		pts.reserve(num_pts);
		for (const auto& point : points) {
			// Get lat, lon and height.
			const Split<char> coords(point, ' ');
			switch (coords.size()) {
				case 2: {
					auto it = coords.begin();
					pts.emplace_back(stox(std::stod, *it), stox(std::stod, *++it), 0, Cartesian::Units::DEGREES, SRID);
					break;
				}
				case 3: {
					auto it = coords.begin();
					pts.emplace_back(stox(std::stod, *it), stox(std::stod, *++it), stox(std::stod, *++it), Cartesian::Units::DEGREES, SRID);
					break;
				}
				default:
					THROW(InvalidArgument, "Invalid specification");
			}
		}
		polygon.add(std::move(pts));
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return polygon;
}


/*
 * The specification is ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))
 * lat and lon in degrees.
 * height in meters.
 */
void
EWKT::parse_polygon(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Polygon>(_parse_polygon(SRID, specification, Geometry::Type::POLYGON));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for POLYGON is '((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for POLYGON is '((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


/*
 * The specification is ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))
 * lat and lon in degrees.
 * height in meters.
 */
void
EWKT::parse_chull(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Polygon>(_parse_polygon(SRID, specification, Geometry::Type::CHULL));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for CHULL is '((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for CHULL is '((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


MultiPoint
EWKT::_parse_multipoint(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	MultiPoint multipoint;

	std::sregex_iterator next(specification.begin(), specification.end(), find_parenthesis_list_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		multipoint.add(_parse_point(SRID, next->str(1)));
		match_len += next->length(0);
		++next;
	}

	if (match_len == 0) {
		Split<char> spc_points(specification, ',');
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
	} catch (const InvalidArgument& err) {
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

	std::sregex_iterator next(specification.begin(), specification.end(), find_parenthesis_list_re, std::regex_constants::match_continuous);
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
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for MULTICIRCLE is '((lat lon [height], radius), ... (lat lon [height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTICIRCLE is '((lat lon [height], radius), ... (lat lon [height], radius))' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


MultiConvex
EWKT::_parse_multiconvex(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	MultiConvex multiconvex;

	std::sregex_iterator next(specification.begin(), specification.end(), find_nested_parenthesis_list_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		multiconvex.add(_parse_convex(SRID, next->str(1)));
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return multiconvex;
}


/*
 * The specification is: (..., ((lat lon [height], radius), ... (lat lon [height], radius)), ...)
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_multiconvex(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<MultiConvex>(_parse_multiconvex(SRID, specification));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for MULTICONVEX is '(..., ((lat lon [height], radius), ... (lat lon [height], radius)), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTICONVEX is '(..., ((lat lon [height], radius), ... (lat lon [height], radius)), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


MultiPolygon
EWKT::_parse_multipolygon(int SRID, const std::string& specification, Geometry::Type type)
{
	size_t match_len = 0;
	MultiPolygon multipolygon;

	std::sregex_iterator next(specification.begin(), specification.end(), find_nested_parenthesis_list_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		multipolygon.add(_parse_polygon(SRID, next->str(1), type));
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return multipolygon;
}


/*
 * The specification is: (..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_multipolygon(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<MultiPolygon>(_parse_multipolygon(SRID, specification, Geometry::Type::POLYGON));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for MULTIPOLYGON is '(..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTIPOLYGON is '(..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


/*
 * The specification is: (..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)
 * lat and lon in degrees.
 * height in meters.
 * radius in meters and positive.
 */
void
EWKT::parse_multichull(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<MultiPolygon>(_parse_multipolygon(SRID, specification, Geometry::Type::CHULL));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for MULTICHULL is '(..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for MULTICHULL is '(..., ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height])), ...)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


Collection
EWKT::_parse_geometry_collection(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	Collection collection;

	std::sregex_iterator next(specification.begin(), specification.end(), find_collection_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		static const auto it_e = map_recursive_dispatch.end();
		const auto geometry = next->str(1);
		const auto it = map_recursive_dispatch.find(geometry);
		if (it == it_e) {
			THROW(InvalidArgument, "Geometry '%s' is not supported", geometry.c_str());
		} else {
			switch (it->second) {
				case Geometry::Type::POINT:
					collection.add_point(_parse_point(SRID, next->str(2)));
					break;
				case Geometry::Type::MULTIPOINT:
					collection.add_multipoint(_parse_multipoint(SRID, next->str(2)));
					break;
				case Geometry::Type::CIRCLE:
					collection.add_circle(_parse_circle(SRID, next->str(2)));
					break;
				case Geometry::Type::CONVEX:
					collection.add_convex(_parse_convex(SRID, next->str(2)));
					break;
				case Geometry::Type::POLYGON:
					collection.add_polygon(_parse_polygon(SRID, next->str(2), Geometry::Type::POLYGON));
					break;
				case Geometry::Type::CHULL:
					collection.add_polygon(_parse_polygon(SRID, next->str(2), Geometry::Type::CHULL));
					break;
				case Geometry::Type::MULTICIRCLE:
					collection.add_multicircle(_parse_multicircle(SRID, next->str(2)));
					break;
				case Geometry::Type::MULTICONVEX:
					collection.add_multiconvex(_parse_multiconvex(SRID, next->str(2)));
					break;
				case Geometry::Type::MULTIPOLYGON:
					collection.add_multipolygon(_parse_multipolygon(SRID, next->str(2), Geometry::Type::POLYGON));
					break;
				case Geometry::Type::MULTICHULL:
					collection.add_multipolygon(_parse_multipolygon(SRID, next->str(2), Geometry::Type::CHULL));
					break;
				case Geometry::Type::COLLECTION:
					collection.add(_parse_geometry_collection(SRID, next->str(2)));
					break;
				case Geometry::Type::INTERSECTION:
					collection.add_intersection(_parse_geometry_intersection(SRID, next->str(2)));
					break;
				default:
					break;
			}
		}
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return collection;
}


/*
 * The specification is: (geometry_1, ..., geometry_n)
 */
void
EWKT::parse_geometry_collection(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Collection>(_parse_geometry_collection(SRID, specification));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for GEOMETRYCOLLECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for GEOMETRYCOLLECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


Intersection
EWKT::_parse_geometry_intersection(int SRID, const std::string& specification)
{
	size_t match_len = 0;
	Intersection intersection;

	std::sregex_iterator next(specification.begin(), specification.end(), find_collection_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		static const auto it_e = map_recursive_dispatch.end();
		const auto geometry = next->str(1);
		const auto it = map_recursive_dispatch.find(geometry);
		if (it == it_e) {
			THROW(InvalidArgument, "Geometry '%s' is not supported", geometry.c_str());
		} else {
			switch (it->second) {
				case Geometry::Type::POINT:
					intersection.add(std::make_shared<Point>(_parse_point(SRID, next->str(2))));
					break;
				case Geometry::Type::MULTIPOINT:
					intersection.add(std::make_shared<MultiPoint>(_parse_multipoint(SRID, next->str(2))));
					break;
				case Geometry::Type::CIRCLE:
					intersection.add(std::make_shared<Circle>(_parse_circle(SRID, next->str(2))));
					break;
				case Geometry::Type::CONVEX:
					intersection.add(std::make_shared<Convex>(_parse_convex(SRID, next->str(2))));
					break;
				case Geometry::Type::POLYGON:
					intersection.add(std::make_shared<Polygon>(_parse_polygon(SRID, next->str(2), Geometry::Type::POLYGON)));
					break;
				case Geometry::Type::CHULL:
					intersection.add(std::make_shared<Polygon>(_parse_polygon(SRID, next->str(2), Geometry::Type::CHULL)));
					break;
				case Geometry::Type::MULTICIRCLE:
					intersection.add(std::make_shared<MultiCircle>(_parse_multicircle(SRID, next->str(2))));
					break;
				case Geometry::Type::MULTICONVEX:
					intersection.add(std::make_shared<MultiConvex>(_parse_multiconvex(SRID, next->str(2))));
					break;
				case Geometry::Type::MULTIPOLYGON:
					intersection.add(std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, next->str(2), Geometry::Type::POLYGON)));
					break;
				case Geometry::Type::MULTICHULL:
					intersection.add(std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, next->str(2), Geometry::Type::CHULL)));
					break;
				case Geometry::Type::COLLECTION:
					intersection.add(std::make_shared<Collection>(_parse_geometry_collection(SRID, next->str(2))));
					break;
				case Geometry::Type::INTERSECTION:
					intersection.add(std::make_shared<Intersection>(_parse_geometry_intersection(SRID, next->str(2))));
					break;
				default:
					break;
			}
		}
		match_len += next->length(0);
		++next;
	}

	if (match_len != specification.length()) {
		THROW(InvalidArgument, "Invalid specification [%zu]", match_len);
	}

	return intersection;
}


/*
 * The specification is: (geometry_1, ..., geometry_n)
 */
void
EWKT::parse_geometry_intersection(int SRID, const std::string& specification)
{
	try {
		geometry = std::make_unique<Intersection>(_parse_geometry_intersection(SRID, specification));
	} catch (const InvalidArgument& err) {
		THROW(EWKTError, "Specification for GEOMETRYINTERSECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", specification.c_str(), err.what());
	} catch (const OutOfRange& err) {
		THROW(EWKTError, "Specification for GEOMETRYINTERSECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", specification.c_str(), err.what());
	}
}


bool
EWKT::isEWKT(const std::string& str)
{
	std::smatch m;
	static const auto it_e = map_dispatch.end();
	return std::regex_match(str, m, find_geometry_re) && static_cast<size_t>(m.length(0)) == str.length() && map_dispatch.find(m.str(3)) != it_e;
}
