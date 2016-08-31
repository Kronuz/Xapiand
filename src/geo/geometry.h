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

#pragma once

#include "cartesian.h"

#include <vector>

// Constants used for specify the sign of the bounding circle.
#define NEG -1
#define ZERO 0
#define POS 1
#define COLLINEAR 0
#define CLOCKWISE 1
#define COUNTERCLOCKWISE 2


// Earth radius in meters
constexpr double M_PER_RADIUS_EARTH = 6367444.7;

// Radius maximum in meters allowed in a constraint (all the earth)
constexpr double MAX_RADIUS_HALFSPACE_EARTH = 20003917.491659265;

// Constant for scaling the radius of a Polygon
constexpr double SCALE_RADIUS = 0.70710678118654752440084436;

// Min radius in meters allowed
constexpr double MIN_RADIUS_METERS = 0.1;

// Min radius in radians allowed, MIN_RADIUS_METERS / M_PER_RADIUS_EARTH.
constexpr double MIN_RADIUS_RADIANS = 0.00000001570488707974173690;

// Radius in meters of a great circle
constexpr double RADIUS_GREAT_CIRCLE = 10001958.7458296325;


enum class GeometryType {
	CONVEX_POLYGON,
	CONVEX_HULL
};


/*
 * A circular area, given by the plane slicing it off sphere.
 */
class Constraint {
	void set_data(double meters);

public:
	Cartesian center;
	double arcangle;
	double distance;
	double radius;    // Radius in meters
	int sign;

	/*
	 * Does a great circle with center in
	 * (lat = 0, lon = 0, h = 0) -> (x = 1, y = 0, z = 0)
	 */
	Constraint();
	// Does a great circle with the defined center
	Constraint(Cartesian&& _center);
	// Constraint in the Earth. Radius in meters
	Constraint(Cartesian&& _center, double radius);
	// Move constructor
	Constraint(Constraint&& c) = default;
	// Copy constructor
	Constraint(const Constraint& c) = default;

	// Move assignment
	Constraint& operator=(Constraint&& c) = default;
	// Copy assignment
	Constraint& operator=(const Constraint& c) = default;
	bool operator==(const Constraint& c) const noexcept;
	bool operator!=(const Constraint& c) const noexcept;
};


class Geometry {
	// Calculates the convex hull of a set of points using Graham Scan Algorithm
	void convexHull(std::vector<Cartesian>&& points, std::vector<Cartesian> &points_convex) const;

	// Set the Polygon's centroid
	void setCentroid();

	// Average angle from the vertices to the polygon's centroid
	double meanAngle2centroid() const;

	/*
	 * Obtains the direction of the vectors.
	 * Returns:
	 *   If the vectors are COLLINEAR, CLOCKWISE or COUNTERCLOCKWISE
	 */
	static int direction(const Cartesian &a, const Cartesian &b, const Cartesian &c) noexcept;

	// Returns the squared distance between two vectors
	static double dist(const Cartesian &a, const Cartesian &b) noexcept;

public:
	Constraint boundingCircle;
	std::vector<Constraint> constraints;
	std::vector<Cartesian> corners;
	Cartesian centroid;

	Geometry() = delete;
	// The region is specified by a bounding circle.
	Geometry(Constraint&& constraint);
	// Constructor from a set of points (v) in the Earth
	Geometry(std::vector<Cartesian>&& v, const GeometryType &type);
	// Move Constructor
	Geometry(Geometry&&) = default;
	// Copy Constructor
	Geometry(const Geometry&) = delete;

	// Move assignment
	Geometry& operator=(Geometry&& c) = default;
	// Copy assignment
	Geometry& operator=(const Geometry& c) = delete;

	/*
	 * Constraints:
	 *  For each side, we have a 0-halfspace (great circle) passing through the 2 corners.
	 *  Since we are in counterclockwise order, the vector product of the two
	 *  successive corners just gives the correct constraint.
	 *
	 * Requirements:
	 *  All points should fit within half of the globe.
	 */
	void convexHull(std::vector<Cartesian>&& v);

	/*
	 * Constraints:
	 *    For each side, we have a 0-halfspace (great circle) passing through the 2 corners.
	 *    Since we are in counterclockwise order, the vector product of the two
	 *    successive corners just gives the correct constraint.
	 *
	 * Requirements:
	 *    Polygons should be counterclockwise.
	 *    Polygons should be convex.
	 */
	void convexPolygon(std::vector<Cartesian>&& v);
};
