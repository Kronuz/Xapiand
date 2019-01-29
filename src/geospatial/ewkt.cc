/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "ewkt.h"

#include "hashes.hh"         // for fnv1ah32
#include "phf.hh"            // for phf
#include "repr.hh"           // for repr
#include "split.h"           // for Split
#include "strict_stox.hh"    // for strict_sto*
#include "utype.hh"          // for toUType


inline Geometry::Type
get_geometry_type(std::string_view str_geometry_type)
{
	constexpr static auto _ = phf::make_phf({
		hh("POINT"),
		hh("CIRCLE"),
		hh("CONVEX"),
		hh("POLYGON"),
		hh("CHULL"),
		hh("MULTIPOINT"),
		hh("MULTICIRCLE"),
		hh("MULTICONVEX"),
		hh("MULTIPOLYGON"),
		hh("MULTICHULL"),
		hh("GEOMETRYCOLLECTION"),
		hh("GEOMETRYINTERSECTION"),
	});

	switch (_.fhh(str_geometry_type)) {
		case _.fhh("POINT"):
			return Geometry::Type::POINT;
		case _.fhh("CIRCLE"):
			return Geometry::Type::CIRCLE;
		case _.fhh("CONVEX"):
			return Geometry::Type::CONVEX;
		case _.fhh("POLYGON"):
			return Geometry::Type::POLYGON;
		case _.fhh("CHULL"):
			return Geometry::Type::CHULL;
		case _.fhh("MULTIPOINT"):
			return Geometry::Type::MULTIPOINT;
		case _.fhh("MULTICIRCLE"):
			return Geometry::Type::MULTICIRCLE;
		case _.fhh("MULTICONVEX"):
			return Geometry::Type::MULTICONVEX;
		case _.fhh("MULTIPOLYGON"):
			return Geometry::Type::MULTIPOLYGON;
		case _.fhh("MULTICHULL"):
			return Geometry::Type::MULTICHULL;
		case _.fhh("GEOMETRYCOLLECTION"):
			return Geometry::Type::COLLECTION;
		case _.fhh("GEOMETRYINTERSECTION"):
			return Geometry::Type::INTERSECTION;
		default:
			throw std::out_of_range("Invalid geometry");
	}
}


EWKT::EWKT(std::string_view str)
{
	if (str.compare(0, 5, "SRID=") == 0) {
		const auto str_srid = str.substr(5, 4);
		if (str.size() > 9 && str[9] == ';') {
			try {
				auto SRID = strict_stoi(str_srid);
				if (!Cartesian::is_SRID_supported(SRID)) {
					THROW(EWKTError, "SRID=%d is not supported", SRID);
				}
				auto first = str.begin() + 10;
				auto last = str.end();
				auto data = find_geometry(first, last);
				if (data.second || str.end() == last + 1) {
					parse_geometry(SRID, data.first, data.second, first, last);
					return;
				}
				THROW(EWKTError, "Sintax error in '%s'", std::string(first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Invalid SRID '%s' [%s]", str_srid, err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Invalid SRID '%s' [%s]", str_srid, err.what());
			}
		}
		THROW(EWKTError, "Sintax error in %s", str);
	} else {
		auto first = str.begin();
		auto last = str.end();
		auto data = find_geometry(first, last);
		if (data.second || str.end() == last + 1) {
			parse_geometry(WGS84, data.first, data.second, first, last);
			return;
		}
		THROW(EWKTError, "Sintax error in '%s'", str);
	}
}


void
EWKT::parse_geometry(int SRID, Geometry::Type type, bool empty, Iterator first, Iterator last)
{
	if (empty) {
		geometry = std::make_shared<Collection>();
		return;
	}

	switch (type) {
		case Geometry::Type::POINT:
			try {
				geometry = std::make_shared<Point>(_parse_point(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for POINT is '(lon lat[ height])' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for POINT is '(lon lat[ height])' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::MULTIPOINT:
			try {
				geometry = std::make_shared<MultiPoint>(_parse_multipoint(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for MULTIPOINT is '(lon lat [height], ..., lon lat [height]) or ((lon lat [height]), ..., (lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for MULTIPOINT is '(lon lat [height], ..., lon lat [height]) or ((lon lat [height]), ..., (lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::CIRCLE:
			try {
				geometry = std::make_shared<Circle>(_parse_circle(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for CIRCLE is '(lon lat[ height], radius)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for CIRCLE is '(lon lat[ height], radius)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::CONVEX:
			try {
				geometry = std::make_shared<Convex>(_parse_convex(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for CONVEX is '((lon lat[ height], radius), ... (lon lat[ height], radius))' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for CONVEX is '((lon lat[ height], radius), ... (lon lat[ height], radius))' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::POLYGON:
			try {
				geometry = std::make_shared<Polygon>(_parse_polygon(SRID, first, last, Geometry::Type::POLYGON));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for POLYGON is '((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for POLYGON is '((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::CHULL:
			try {
				geometry = std::make_shared<Polygon>(_parse_polygon(SRID, first, last, Geometry::Type::CHULL));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for CHULL is '((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for CHULL is '((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height]))' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::MULTICIRCLE:
			try {
				geometry = std::make_shared<MultiCircle>(_parse_multicircle(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for MULTICIRCLE is '((lon lat [height], radius), ... (lon lat [height], radius))' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for MULTICIRCLE is '((lon lat [height], radius), ... (lon lat [height], radius))' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::MULTICONVEX:
			try {
				geometry = std::make_shared<MultiConvex>(_parse_multiconvex(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for MULTICONVEX is '(..., ((lon lat [height], radius), ... (lon lat [height], radius)), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for MULTICONVEX is '(..., ((lon lat [height], radius), ... (lon lat [height], radius)), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::MULTIPOLYGON:
			try {
				geometry = std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, first, last, Geometry::Type::POLYGON));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for MULTIPOLYGON is '(..., ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height])), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for MULTIPOLYGON is '(..., ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height])), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::MULTICHULL:
			try {
				geometry = std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, first, last, Geometry::Type::CHULL));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for MULTICHULL is '(..., ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height])), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for MULTICHULL is '(..., ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height])), ...)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::COLLECTION:
			try {
				geometry = std::make_shared<Collection>(_parse_geometry_collection(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for GEOMETRYCOLLECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for GEOMETRYCOLLECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		case Geometry::Type::INTERSECTION:
			try {
				geometry = std::make_shared<Intersection>(_parse_geometry_intersection(SRID, first, last));
			} catch (const InvalidArgument& err) {
				THROW(EWKTError, "Specification for GEOMETRYINTERSECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", std::string(first, last), err.what());
			} catch (const OutOfRange& err) {
				THROW(EWKTError, "Specification for GEOMETRYINTERSECTION is '(geometry_1, ..., geometry_n)' [(%s) -> %s]", std::string(first, last), err.what());
			}
			return;
		default:
			THROW(EWKTError, "Unknown type: %d", toUType(type));
	}
}


EWKT::Iterator
EWKT::closed_parenthesis(Iterator first, Iterator last)
{
	while (first != last) {
		switch (*first) {
			case '(':
				first = closed_parenthesis(first + 1, last);
				first += static_cast<int>(first != last);
				break;
			case ')':
				return first;
			default:
				++first;
				break;
		}
	}
	return first;
}


std::pair<Geometry::Type, bool>
EWKT::find_geometry(Iterator& first, Iterator& last)
{
	auto _first = first;
	while (first != last && *first != '(' && *first != ' ') {
		++first;
	}

	if (first == last) {
		THROW(EWKTError, "Syntax error in '%s'", repr(_first, last));
	} else {
		const std::string_view geometry(_first, first - _first);
		Geometry::Type geometry_type;
		try {
			geometry_type = get_geometry_type(geometry);
		} catch (const std::out_of_range&) {
			THROW(EWKTError, "Geometry %s is not supported", repr(geometry));
		}
		switch (*first) {
			case '(': {
				auto closed_it = closed_parenthesis(++first, last);
				if (closed_it == last) {
					THROW(EWKTError, "Sintax error [expected ')' at the end]");
				} else {
					last = closed_it;
				}
				return std::make_pair(geometry_type, false);
			}
			case ' ': {
				if (std::string_view(first + 1, last - first - 1).compare("EMPTY") == 0) {
					return std::make_pair(geometry_type, true);
				}
				THROW(EWKTError, "Syntax error in '%s'", repr(first, last));
			}
			default:
				THROW(EWKTError, "Syntax error in '%s'", repr(first, last));
		}
	}
}


Cartesian
EWKT::_parse_cartesian(int SRID, Iterator first, Iterator last)
{
	auto _first = first;
	while (first != last && *first != ' ') {
		++first;
	}
	if (first == last) {
		THROW(InvalidArgument, "Expected ' ' after of longitude [%s]");
	}
	double lon = strict_stod(std::string_view(_first, first - _first));

	_first = ++first;
	while (first != last && *first != ' ') {
		++first;
	}
	double lat = strict_stod(std::string_view(_first, first - _first));

	if (first == last) {
		return {lat, lon, 0, Cartesian::Units::DEGREES, SRID};
	}
	double height = strict_stod(std::string_view(first + 1, last - first - 1));
	return {lat, lon, height, Cartesian::Units::DEGREES, SRID};
}


/*
 * The specification is (lon lat[ height])
 * lon and lat in degrees.
 * height in meters.
 */
Point
EWKT::_parse_point(int SRID, Iterator first, Iterator last)
{
	return Point(_parse_cartesian(SRID, first, last));
}


/*
 * The specification is: (lon lat[ height], radius)
 * lon and lat in degrees.
 * height in meters.
 * radius in meters and positive.
 */
Circle
EWKT::_parse_circle(int SRID, Iterator first, Iterator last)
{
	auto _first = first;
	while (first != last && *first != ',') {
		++first;
	}
	if (first == last) {
		THROW(InvalidArgument, "Invalid CIRCLE specification, expected ', radius' after of center");
	}
	auto center = _parse_cartesian(SRID, _first, first);

	if (++first != last && *first == ' ') {
		return Circle(std::move(center), strict_stod(std::string_view(first + 1, last - first - 1)));
	}

	return Circle(std::move(center), strict_stod(std::string_view(first, last - first)));
}


/*
 * The specification is: ((lon lat[ height], radius), ... (lon lat[ height], radius))
 * lon and lat in degrees.
 * height in meters.
 * radius in meters and positive.
 */
Convex
EWKT::_parse_convex(int SRID, Iterator first, Iterator last)
{
	Convex convex;
	while (first != last) {
		if (*first == '(') {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it != last) {
				convex.add(_parse_circle(SRID, first, closed_it));
				first = closed_it;
				if (first == last - 1) {
					return convex;
				}
				if (*(++first) == ',') {
					if (*(++first) == ' ') {
						++first;
					}
				} else {
					THROW(EWKTError, "Invalid CONVEX specification [expected ',']");
				}
			} else {
				THROW(EWKTError, "Invalid CONVEX specification [expected ')' at the end]");
			}
		} else {
			THROW(EWKTError, "Invalid CONVEX specification [expected '('");
		}
	}

	THROW(EWKTError, "Invalid CONVEX specification [expected '('");
}


/*
 * The specification is ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height]))
 * lon and lat in degrees.
 * height in meters.
 */
Polygon
EWKT::_parse_polygon(int SRID, Iterator first, Iterator last, Geometry::Type type)
{
	Polygon polygon(type);
	while (first != last) {
		if (*first == '(') {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it != last) {
				std::vector<Cartesian> pts;
				auto _first = first;
				while (first != closed_it) {
					while (first != closed_it && *first != ',') {
						++first;
					}
					pts.push_back(_parse_cartesian(SRID, _first, first));
					if (first == closed_it) {
						polygon.add(std::move(pts));
						break;
					}
					if (*(++first) == ' ') {
						++first;
					}
					_first = first;
				}
				++first;
				if (first == last) {
					return polygon;
				}
				if (*first == ',') {
					if (*(++first) == ' ') {
						++first;
					}
				} else {
					THROW(EWKTError, "Invalid POLYGON specification [expected ',' between convex polygons]");
				}
			} else {
				THROW(EWKTError, "Invalid POLYGON specification [expected ')']");
			}
		} else {
			THROW(EWKTError, "Invalid POLYGON specification [expected '(']");
		}
	}

	THROW(EWKTError, "Invalid POLYGON specification [expected '(']");
}


/*
 * The specification is (lon lat [height], ..., lon lat [height]) or ((lon lat [height]), ..., (lon lat [height]))
 * lon and lat in degrees.
 * height in meters.
 */
MultiPoint
EWKT::_parse_multipoint(int SRID, Iterator first, Iterator last)
{
	MultiPoint multipoint;
	if (*first == '(') {
		while (first != last) {
			if (*first == '(') {
				auto closed_it = closed_parenthesis(++first, last);
				if (closed_it != last) {
					multipoint.add(Point(_parse_cartesian(SRID, first, closed_it)));
					first = closed_it;
					if (first == last - 1) {
						return multipoint;
					}
					if (*(++first) == ',') {
						if (*(++first) == ' ') {
							++first;
						}
					} else {
						THROW(EWKTError, "Invalid MULTIPOINT specification [expected ',']");
					}
				} else {
					THROW(EWKTError, "Invalid MULTIPOINT specification [expected ')' at the end]");
				}
			} else {
				THROW(EWKTError, "Invalid MULTIPOINT specification [expected '('");
			}
		}
		THROW(EWKTError, "Invalid MULTIPOINT specification [expected '('");
	} else {
		while (first != last) {
			while (first != last && *first != ',') {
				++first;
			}
			multipoint.add(Point(_parse_cartesian(SRID, first, first)));
			if (first == last) {
				return multipoint;
			}
			if (*(++first) == ' ') {
				++first;
			}
		}
		THROW(EWKTError, "Invalid MULTIPOINT specification [%s]", std::string(first, last));
	}
}


/*
 * The specification is: ((lon lat [height], radius), ... (lon lat [height], radius))
 * lon and lat in degrees.
 * height in meters.
 * radius in meters and positive.
 */
MultiCircle
EWKT::_parse_multicircle(int SRID, Iterator first, Iterator last)
{
	MultiCircle multicircle;
	while (first != last) {
		if (*first == '(') {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it != last) {
				multicircle.add(_parse_circle(SRID, first, closed_it));
				first = closed_it;
				if (first == last - 1) {
					return multicircle;
				}
				if (*(++first) == ',') {
					if (*(++first) == ' ') {
						++first;
					}
				} else {
					THROW(EWKTError, "Invalid MULTICIRCLE specification [expected ',']");
				}
			} else {
				THROW(EWKTError, "Invalid MULTICIRCLE specification [expected ')' at the end]");
			}
		} else {
			THROW(EWKTError, "Invalid MULTICIRCLE specification [expected '('");
		}
	}

	THROW(EWKTError, "Invalid MULTICIRCLE specification [expected '('");
}


/*
 * The specification is: (..., ((lon lat [height], radius), ... (lon lat [height], radius)), ...)
 * lon and lat in degrees.
 * height in meters.
 * radius in meters and positive.
 */
MultiConvex
EWKT::_parse_multiconvex(int SRID, Iterator first, Iterator last)
{
	MultiConvex multiconvex;
	while (first != last) {
		if (*first == '(') {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it != last) {
				multiconvex.add(_parse_convex(SRID, first, closed_it));
				first = closed_it;
				if (first == last - 1) {
					return multiconvex;
				}
				if (*(++first) == ',') {
					if (*(++first) == ' ') {
						++first;
					}
				} else {
					THROW(EWKTError, "Invalid MULTICONVEX specification [expected ',']");
				}
			} else {
				THROW(EWKTError, "Invalid MULTICONVEX specification [expected ')' at the end]");
			}
		} else {
			THROW(EWKTError, "Invalid MULTICONVEX specification [expected '('");
		}
	}

	THROW(EWKTError, "Invalid MULTICONVEX specification [expected '('");
}


/*
 * The specification is: (..., ((lon lat [height], ..., lon lat [height]), (lon lat [height], ..., lon lat [height])), ...)
 * lon and lat in degrees.
 * height in meters.
 * radius in meters and positive.
 */
MultiPolygon
EWKT::_parse_multipolygon(int SRID, Iterator first, Iterator last, Geometry::Type type)
{
	MultiPolygon multipolygon;
	while (first != last) {
		if (*first == '(') {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it != last) {
				multipolygon.add(_parse_polygon(SRID, first, closed_it, type));
				first = closed_it;
				if (first == last - 1) {
					return multipolygon;
				}
				if (*(++first) == ',') {
					if (*(++first) == ' ') {
						++first;
					}
				} else {
					THROW(EWKTError, "Invalid MULTIPOLYGON specification [expected ',']");
				}
			} else {
				THROW(EWKTError, "Invalid MULTIPOLYGON specification [expected ')' at the end]");
			}
		} else {
			THROW(EWKTError, "Invalid MULTIPOLYGON specification [expected '('");
		}
	}

	THROW(EWKTError, "Invalid MULTIPOLYGON specification [expected '('");
}


/*
 * The specification is: (geometry_1, ..., geometry_n)
 */
Collection
EWKT::_parse_geometry_collection(int SRID, Iterator first, Iterator last)
{
	Collection collection;

	while (first != last) {
		auto it_geo_last = last;
		auto data = find_geometry(first, it_geo_last);
		if (!data.second) {
			switch (data.first) {
				case Geometry::Type::POINT:
					collection.add_point(_parse_point(SRID, first, it_geo_last));
					break;
				case Geometry::Type::MULTIPOINT:
					collection.add_multipoint(_parse_multipoint(SRID, first, it_geo_last));
					break;
				case Geometry::Type::CIRCLE:
					collection.add_circle(_parse_circle(SRID, first, it_geo_last));
					break;
				case Geometry::Type::CONVEX:
					collection.add_convex(_parse_convex(SRID, first, it_geo_last));
					break;
				case Geometry::Type::POLYGON:
					collection.add_polygon(_parse_polygon(SRID, first, it_geo_last, Geometry::Type::POLYGON));
					break;
				case Geometry::Type::CHULL:
					collection.add_polygon(_parse_polygon(SRID, first, it_geo_last, Geometry::Type::CHULL));
					break;
				case Geometry::Type::MULTICIRCLE:
					collection.add_multicircle(_parse_multicircle(SRID, first, it_geo_last));
					break;
				case Geometry::Type::MULTICONVEX:
					collection.add_multiconvex(_parse_multiconvex(SRID, first, it_geo_last));
					break;
				case Geometry::Type::MULTIPOLYGON:
					collection.add_multipolygon(_parse_multipolygon(SRID, first, it_geo_last, Geometry::Type::POLYGON));
					break;
				case Geometry::Type::MULTICHULL:
					collection.add_multipolygon(_parse_multipolygon(SRID, first, it_geo_last, Geometry::Type::CHULL));
					break;
				case Geometry::Type::COLLECTION:
					collection.add(_parse_geometry_collection(SRID, first, it_geo_last));
					break;
				case Geometry::Type::INTERSECTION:
					collection.add_intersection(_parse_geometry_intersection(SRID, first, it_geo_last));
					break;
				default:
					THROW(EWKTError, "Unknown type: %d", toUType(data.first));
			}
		}
		first = it_geo_last;
		if (first == last - 1) {
			return collection;
		}
		if (*(++first) == ',') {
			if (*(++first) == ' ') {
				++first;
			}
		} else {
			THROW(EWKTError, "Invalid GEOMETRYCOLLECTION specification [expected ',']");
		}
	}

	THROW(EWKTError, "Invalid GEOMETRYCOLLECTION specification [expected '('");
}


/*
 * The specification is: (geometry_1, ..., geometry_n)
 */
Intersection
EWKT::_parse_geometry_intersection(int SRID, Iterator first, Iterator last)
{
	Intersection intersection;

	while (first != last) {
		auto it_geo_last = last;
		auto data = find_geometry(first, it_geo_last);
		if (!data.second) {
			switch (data.first) {
				case Geometry::Type::POINT:
					intersection.add(std::make_shared<Point>(_parse_point(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::MULTIPOINT:
					intersection.add(std::make_shared<MultiPoint>(_parse_multipoint(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::CIRCLE:
					intersection.add(std::make_shared<Circle>(_parse_circle(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::CONVEX:
					intersection.add(std::make_shared<Convex>(_parse_convex(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::POLYGON:
					intersection.add(std::make_shared<Polygon>(_parse_polygon(SRID, first, it_geo_last, Geometry::Type::POLYGON)));
					break;
				case Geometry::Type::CHULL:
					intersection.add(std::make_shared<Polygon>(_parse_polygon(SRID, first, it_geo_last, Geometry::Type::CHULL)));
					break;
				case Geometry::Type::MULTICIRCLE:
					intersection.add(std::make_shared<MultiCircle>(_parse_multicircle(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::MULTICONVEX:
					intersection.add(std::make_shared<MultiConvex>(_parse_multiconvex(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::MULTIPOLYGON:
					intersection.add(std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, first, it_geo_last, Geometry::Type::POLYGON)));
					break;
				case Geometry::Type::MULTICHULL:
					intersection.add(std::make_shared<MultiPolygon>(_parse_multipolygon(SRID, first, it_geo_last, Geometry::Type::CHULL)));
					break;
				case Geometry::Type::COLLECTION:
					intersection.add(std::make_shared<Collection>(_parse_geometry_collection(SRID, first, it_geo_last)));
					break;
				case Geometry::Type::INTERSECTION:
					intersection.add(std::make_shared<Intersection>(_parse_geometry_intersection(SRID, first, it_geo_last)));
					break;
				default:
					THROW(EWKTError, "Unknown type: %d", toUType(data.first));
			}
		}
		first = it_geo_last;
		if (first == last - 1) {
			return intersection;
		}
		if (*(++first) == ',') {
			if (*(++first) == ' ') {
				++first;
			}
		} else {
			THROW(EWKTError, "Invalid GEOMETRYINTERSECTION specification [expected ',']");
		}
	}

	THROW(EWKTError, "Invalid GEOMETRYINTERSECTION specification [expected '('");
}


bool
EWKT::_isEWKT(Iterator first, Iterator last)
{
	auto _first = first;
	while (first != last && *first != '(' && *first != ' ') {
		++first;
	}

	if (first == last) {
		return false;
	}

	try {
		get_geometry_type(std::string_view(_first, first - _first));
	} catch (const std::out_of_range&) {
		return false;
	}
	switch (*first) {
		case '(': {
			auto closed_it = closed_parenthesis(++first, last);
			if (closed_it == last) {
				return false;
			}
			return closed_it == (last - 1);
		}
		case ' ':
			return std::string(++first, last).compare("EMPTY") == 0;
		default:
			return false;
	}
}


bool
EWKT::isEWKT(std::string_view str)
{
	if (str.compare(0, 5, "SRID=") == 0) {
		if (str.size() > 9 && str[9] == ';') {
			return _isEWKT(str.begin() + 10, str.end());
		}
		return false;
	}
	return _isEWKT(str.begin(), str.end());
}
