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

#pragma once

#include <memory>              // for default_delete, unique_ptr
#include <string_view>         // for std::string_view
#include <unordered_map>       // for unordered_map

#include "collection.h"



class GeoSpatial;


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
 *   (lon lat) or (lon lat height)
 *
 * This parser do not accept EMPTY geometries and
 * polygons are not required to be repeated first coordinate to end like EWKT.
 */
class EWKT {
	using Iterator = std::string_view::const_iterator;

	std::shared_ptr<Geometry> geometry;

	void parse_geometry(int SRID, Geometry::Type type, bool empty, Iterator first, Iterator last);

	static Iterator closed_parenthesis(Iterator first, Iterator last);
	static std::pair<Geometry::Type, bool> find_geometry(Iterator& first, Iterator& last);

	static Cartesian _parse_cartesian(int SRID, Iterator first, Iterator last);
	static Point _parse_point(int SRID, Iterator first, Iterator last);
	static Circle _parse_circle(int SRID, Iterator first, Iterator last);
	static Convex _parse_convex(int SRID, Iterator first, Iterator last);
	static Polygon _parse_polygon(int SRID, Iterator first, Iterator last, Geometry::Type type);
	static MultiPoint _parse_multipoint(int SRID, Iterator first, Iterator last);
	static MultiCircle _parse_multicircle(int SRID, Iterator first, Iterator last);
	static MultiConvex _parse_multiconvex(int SRID, Iterator first, Iterator last);
	static MultiPolygon _parse_multipolygon(int SRID, Iterator first, Iterator last, Geometry::Type type);
	static Collection _parse_geometry_collection(int SRID, Iterator first, Iterator last);
	static Intersection _parse_geometry_intersection(int SRID, Iterator first, Iterator last);

	static bool _isEWKT(Iterator first, Iterator last);

public:
	explicit EWKT(std::string_view str);

	EWKT(const EWKT& ewkt) noexcept
		: geometry(ewkt.geometry) { }

	EWKT& operator=(const EWKT& ewkt) {
		geometry = ewkt.geometry;
		return *this;
	}

	~EWKT() = default;

	static bool isEWKT(std::string_view str);

	std::shared_ptr<Geometry> getGeometry() const {
		geometry->simplify();
		return geometry;
	}
};
