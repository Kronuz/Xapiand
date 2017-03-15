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

#pragma once

#include <memory>              // for default_delete, unique_ptr
#include <regex>               // for regex
#include <unordered_map>       // for regex

#include "collection.h"


extern const std::regex find_geometry_re;
extern const std::regex find_circle_re;
extern const std::regex find_parenthesis_list_re;
extern const std::regex find_nested_parenthesis_list_re;
extern const std::regex find_collection_re;


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
	using dispatch_func = void (EWKT::*)(int, const std::string&);

	static const std::unordered_map<std::string, dispatch_func> map_dispatch;
	static const std::unordered_map<std::string, Geometry::Type> map_recursive_dispatch;

	std::unique_ptr<Geometry> geometry;

	static Point _parse_point(int SRID, const std::string& specification);
	static Circle _parse_circle(int SRID, const std::string& specification);
	static Convex _parse_convex(int SRID, const std::string& specification);
	static Polygon _parse_polygon(int SRID, const std::string& specification, Geometry::Type type);
	static MultiPoint _parse_multipoint(int SRID, const std::string& specification);
	static MultiCircle _parse_multicircle(int SRID, const std::string& specification);
	static MultiConvex _parse_multiconvex(int SRID, const std::string& specification);
	static MultiPolygon _parse_multipolygon(int SRID, const std::string& specification, Geometry::Type type);
	static Collection _parse_geometry_collection(int SRID, const std::string& specification);
	static Intersection _parse_geometry_intersection(int SRID, const std::string& specification);

	void parse_point(int SRID, const std::string& specification);
	void parse_circle(int SRID, const std::string& specification);
	void parse_convex(int SRID, const std::string& specification);
	void parse_polygon(int SRID, const std::string& specification);
	void parse_chull(int SRID, const std::string& specification);
	void parse_multipoint(int SRID, const std::string& specification);
	void parse_multicircle(int SRID, const std::string& specification);
	void parse_multiconvex(int SRID, const std::string& specification);
	void parse_multipolygon(int SRID, const std::string& specification);
	void parse_multichull(int SRID, const std::string& specification);
	void parse_geometry_collection(int SRID, const std::string& specification);
	void parse_geometry_intersection(int SRID, const std::string& specification);

	friend class GeoSpatial;

public:
	EWKT(const std::string& str);

	EWKT(EWKT&& ewkt) noexcept
		: geometry(std::move(ewkt.geometry)) { }

	EWKT(const EWKT&) = delete;

	~EWKT() = default;

	EWKT& operator=(EWKT&& ewkt) noexcept {
		geometry = std::move(ewkt.geometry);
		return *this;
	}

	EWKT& operator=(const EWKT&) = delete;

	static bool isEWKT(const std::string& str);

	const std::unique_ptr<Geometry>& getGeometry() const {
		geometry->simplify();
		return geometry;
	}
};
