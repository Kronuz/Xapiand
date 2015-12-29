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

#include "geometry.h"

#include <cmath>
#include <type_traits>


Constraint::Constraint()
	: arcangle(PI_HALF),
	  distance(0.0),
	  radius(RADIUS_GREAT_CIRCLE),
	  sign(ZERO) { }


Constraint::Constraint(Cartesian&& _center)
	: center(std::move(_center)),
	  arcangle(PI_HALF),
	  distance(0.0),
	  radius(RADIUS_GREAT_CIRCLE),
	  sign(ZERO)
{
	/*
	 * We normalize the center, because geometry works around a sphere
	 * unitary instead of a ellipsoid.
	 */
	center.normalize();
}


Constraint::Constraint(Cartesian&& _center, double _radius)
	: center(std::move(_center))
{
	/*
	 * We normalize the center, because geometry works around a sphere
	 * unitary instead of a ellipsoid.
	 */
	center.normalize();

	set_data(_radius);

	if (distance > DBL_TOLERANCE) {
		sign = POS;
	} else if (distance < -DBL_TOLERANCE) {
		sign = NEG;
	} else {
		sign = ZERO;
	}
}


bool
Constraint::operator==(const Constraint &c) const noexcept
{
	return center == c.center && arcangle == c.arcangle;
}


bool
Constraint::operator!=(const Constraint &c) const noexcept
{
	return center != c.center || arcangle != c.arcangle;
}


void
Constraint::set_data(double meters)
{
	if (meters < MIN_RADIUS_METERS) {
		arcangle = MIN_RADIUS_RADIANS;
		distance = 1.0;
		radius = MIN_RADIUS_METERS;
	} else if (meters > MAX_RADIUS_HALFSPACE_EARTH) {
		arcangle = M_PI;
		distance = -1.0;
		radius = MAX_RADIUS_HALFSPACE_EARTH;
	} else {
		arcangle = meters / M_PER_RADIUS_EARTH;
		distance = std::cos(arcangle);
		radius = meters;
	}
}


Geometry::Geometry(Constraint&& constraint)
	: boundingCircle(std::move(constraint)),
	  constraints({ boundingCircle }),
	  centroid(boundingCircle.center) { }


Geometry::Geometry(std::vector<Cartesian>&& v, const GeometryType &type)
{
	type == GeometryType::CONVEX_POLYGON ? convexPolygon(std::move(v)) : convexHull(std::move(v));
}


void
Geometry::convexHull(std::vector<Cartesian>&& v)
{
	std::vector<Cartesian> points_convex;
	points_convex.reserve(v.size());

	// The convex is formed in counterclockwise.
	convexHull(std::move(v), points_convex);

	size_t len = points_convex.size();
	if (len < 3) throw MSG_Error("Convex Hull not found");

	// The corners are in clockwise but we need the corners in counterclockwise order and normalize.
	corners.reserve(len);

	// Duplicates the last point at the begin.
	points_convex.insert(points_convex.begin(), *(points_convex.end() - 1));

	auto e_it = points_convex.rend() - 1;
	for (auto it = points_convex.rbegin(); it != e_it; ++it) {
		auto n_it = it + 1;
		auto center = *it ^ *n_it;
		center.normalize();
		constraints.push_back(Constraint(std::move(center)));
		corners.push_back(*it);
	}

	/*
	 * Calculates the bounding circle for the convex.
	 * Takes it as the bounding circle of the triangle with the widest opening angle.
	 */
	boundingCircle.distance = 1.0;
	for (auto it_i = corners.begin(); it_i != corners.end(); ++it_i) {
		for (auto it_j = it_i + 1; it_j != corners.end(); ++it_j) {
			for (auto it_k = it_j + 1; it_k != corners.end(); ++it_k) {
				auto v_aux = (*it_j - *it_i) ^ (*it_k - *it_j);
				v_aux.normalize();
				/*
				 * Calculates the correct opening angle.
				 * Can take any corner to calculate the opening angle.
				 */
				double d = v_aux * *it_i;
				if (boundingCircle.distance > d) {
					boundingCircle.distance = d;
					boundingCircle.center = std::move(v_aux);
					boundingCircle.arcangle = std::acos(d);
					if (d > DBL_TOLERANCE) boundingCircle.sign = POS;
					else if (d < -DBL_TOLERANCE) boundingCircle.sign = NEG;
					else boundingCircle.sign = ZERO;
				}
			}
		}
	}

	// Sets the Polygon's centroid.
	setCentroid();
}


// Constructor for a convex polygon.
void
Geometry::convexPolygon(std::vector<Cartesian>&& v)
{
	// Repeats the first corner at the end if it does not repeat.
	if (*v.begin() != *(v.end() - 1)) v.push_back(*v.begin());

	auto len = v.size();
	if (len < 4) throw "Polygon should have at least three corners";

	bool counterclockwise = false, first_counterclockwise = false;
	auto e_it = v.end() - 1;

	// Check for the type of direction.
	Cartesian constraint;
	for (auto it = v.begin(); it != e_it; ++it) {
		// Direction should be the same for all.
		auto n_it = it + 1;
		if (it != v.begin()) {
			// Calculate the direction of the third corner and restriction formed in the previous iteration.
			if (constraint * *n_it < DBL_TOLERANCE) {
				counterclockwise = false;
				if (it == v.begin() + 1) {
					first_counterclockwise = false;
				}
			} else {
				counterclockwise = true;
				if (it == v.begin() + 1) {
					first_counterclockwise = true;
				}
			}

			if (it != (v.begin() + 1) && counterclockwise != first_counterclockwise) {
				throw MSG_Error("Polygon is not convex, You should use the constructor -> Geometry(g, Geometry::Type::CONVEX_HULL)");
			}
		}

		// The vector product of the two successive corners just gives the correct constraint.
		constraint = *it ^ *n_it;
		if (constraint.norm() < DBL_TOLERANCE) {
			throw MSG_Error("Repeating corners, edge error, You should use the constructor -> Geometry(g, Geometry::Type::CONVEX_HULL)");
		}
	}

	// Building convex always in counterclockwise.
	corners.reserve(len - 1);
	if (counterclockwise) {
		for (auto it = v.begin(); it != e_it; ++it) {
			auto n_it = it + 1;
			constraint = *it ^ *n_it;
			constraint.normalize();
			constraints.push_back(std::move(constraint));
			// Normalizes the corner.
			it->normalize();
			corners.push_back(*it);
		}
	} else {
		// If direction is clockwise, revert the direction.
		auto f_rit = v.rend() - 1;
		for (auto rit = v.rbegin(); rit != f_rit; ++rit) {
			auto rn_it = rit + 1;
			constraint = *rit ^ *rn_it;
			constraint.normalize();
			constraints.push_back(std::move(constraint));
			// Normalize the corner.
			rit->normalize();
			corners.push_back(*rit);
		}
	}

	// Calculate the bounding circle for the convex.
	// Take it as the bounding circle of the triangle with the widest opening angle.
	boundingCircle.distance = 1.0;
	for (auto it = corners.begin(); it != corners.end(); ++it) {
		for (auto n_it = it + 1; n_it != corners.end(); ++n_it) {
			for (auto it_k = n_it + 1; it_k != corners.end(); ++it_k) {
				auto v_aux = (*n_it - *it) ^ (*it_k - *n_it);
				v_aux.normalize();
				/*
				 * Calculate the correct opening angle.
				 * Can take any corner to calculate the opening angle.
				 */
				double d = v_aux * *it;
				if (boundingCircle.distance > d) {
					boundingCircle.distance = d;
					boundingCircle.center = std::move(v_aux);
					boundingCircle.arcangle = std::acos(d);
					if (d > DBL_TOLERANCE) boundingCircle.sign = POS;
					else if (d < -DBL_TOLERANCE) boundingCircle.sign = NEG;
					else boundingCircle.sign = ZERO;
				}
			}
		}
	}

	// Sets the Polygon's centroid.
	setCentroid();
}


int
Geometry::direction(const Cartesian &a, const Cartesian &b, const Cartesian &c) noexcept
{
	Cartesian aux = a ^ b;
	double angle = aux * c;
	if (angle > DBL_TOLERANCE) {
		return CLOCKWISE;
	} else if (angle < -DBL_TOLERANCE) {
		return COUNTERCLOCKWISE;
	} else {
		return COLLINEAR;
	}
}


double
Geometry::dist(const Cartesian &a, const Cartesian &b) noexcept
{
	Cartesian p = a - b;
	return p.x * p.x + p.y * p.y + p.z * p.z;
}


void
Geometry::convexHull(std::vector<Cartesian>&& points, std::vector<Cartesian> &points_convex) const
{
	auto len = points.size();
	if (len < 3) throw MSG_Error("Polygon should have at least three corners");

	// Find the min 'y', min 'x' and min 'z'.
	auto it = points.begin();
	it->normalize(); // Normalize the point.
	auto it_swap = it;
	for (++it; it != points.end(); ++it) {
		it->normalize(); // Normalize the point.
		if (it->y < it_swap->y ||
			(it_swap->y == it->y && it->x < it_swap->x) ||
			(it_swap->y == it->y && it->x == it_swap->x && it->z < it_swap->z)) {
			it_swap = it;
		}
	}

	// Swap positions.
	std::iter_swap(it_swap, points.begin());

	// Sort the n - 1 elements in ascending angle with respect to the first point P0.
	std::sort(++points.begin(), points.end(), [P0 = *points.begin()](const Cartesian &a, const Cartesian &b) {
		// Calculate direction.
		int dir = direction(P0, a, b);
		if (dir == COLLINEAR) {
			return ((dist(P0, b) > dist(P0, a)) ? true : false);
		}
		return (dir == COUNTERCLOCKWISE) ? true : false;
	});

	// Deleting duplicates points.
	it = points.begin();
	while (it != points.end() - 1) {
		*it == *(it + 1) ? it = points.erase(it) : ++it;
	}

	if ((len = points.size()) < 3) throw MSG_Error("Polygon should have at least three corners");

	// Points convex
	it = points.begin();
	points_convex.push_back(*it++);
	points_convex.push_back(*it++);
	points_convex.push_back(*it++);

	for ( ; it != points.end(); ++it) {
		while (true) {
			// Not found the convex.
			if (points_convex.size() == 1) throw MSG_Error("Convex Hull not found");

			Cartesian last = points_convex.back();
			points_convex.pop_back();

			Cartesian n_last = points_convex.back();

			int direct = direction(n_last, last, *it);
			if (direct != COUNTERCLOCKWISE) {
				continue;
			} else {
				points_convex.push_back(last);
				break;
			}
		}
		points_convex.push_back(*it);
	}
}


void
Geometry::setCentroid()
{
	double x = 0, y = 0, z = 0;
	for (auto it = corners.begin(); it != corners.end(); ++it) {
		x += it->x;
		y += it->y;
		z += it->z;
	}
	centroid = Cartesian(x, y, z);
	centroid.normalize();

	boundingCircle.radius = SCALE_RADIUS * meanAngle2centroid() * M_PER_RADIUS_EARTH;
}


double
Geometry::meanAngle2centroid() const
{
	double sum = 0;
	for (auto it = corners.begin(); it != corners.end(); ++it) {
		sum += std::acos(*it * centroid);
	}

	return sum / corners.size();
}
