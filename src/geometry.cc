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

// Constraint in the Earth.
// Radius in meters.
Constraint::Constraint(Cartesian &center_, double radius) : center(center_)
{
	// We normalize the center, because  geometry works around a sphere unitary instead of a ellipsoid.
	center.normalize();
	arcangle = meters2rad(radius);
	distance = cos(arcangle);
	if (distance <= -DBL_TOLERANCE) sign = NEG;
	else if (distance >=  DBL_TOLERANCE) sign = POS;
	else sign = ZERO;
}


// Do a great circle with center in (lat = 0, lon = 0, h = 0, DEGREES) -> (x = 1, y = 0, z = 0).
Constraint::Constraint() : sign(ZERO), distance(0.0), arcangle(PI_HALF) { }


bool
Constraint::operator ==(const Constraint &c) const
{
	return (center == c.center && arcangle == c.arcangle);
}


Constraint&
Constraint::operator=(const Constraint &c)
{
	sign = c.sign;
	center = c.center;
	distance = c.distance;
	arcangle = c.arcangle;
	return *this;
}


// Convert distance in meters to earth's radians.
double
Constraint::meters2rad(double meters)
{
	if (meters < 0.1) meters = 0.1;
	else if (meters > MAX_RADIUS_HALFSPACE_EARTH) return M_PI;
	return meters / M_PER_RADIUS_EARTH;
}


// The region is specified by a bounding circle.
Geometry::Geometry(const Constraint &c)
{
	boundingCircle = c;
	constraints.push_back(c);
}


Geometry::Geometry(std::vector<Cartesian> &v, typePoints type)
{
	(type == Geometry::CONVEX_POLYGON) ? convexPolygon(v) : convexHull(v);
}


// Constructor for a set of points in the Earth.
// typePolygon -> Geometry::CONVEX or Geometry::NO_CONVEX
void
Geometry::convexHull(std::vector<Cartesian> &v)
{
	// Constraints:
	//    For each side, we have a 0-halfspace (great circle) passing through the 2 corners.
	//    Since we are in counterclockwise order, the vector product of the two
	//    successive corners just gives the correct constraint.
	//
	// Requirements:
	//    Set of points, Not necessarily must form a convex Polygono.
	//	  All points should fit within half of the globe.

	// We found the convex hull for the points given, using Graham Scan Algorithm.
	std::vector<Cartesian> points_convex;
	convexHull(v, points_convex);

	// The convex is formed in counterclockwise.
	int i, len = (int)points_convex.size();
	if (len < 3) throw MSG_Error("Convex Hull not found");

	// The corners are in clockwise but we need the corners in counterclockwise order and normalize.
	Cartesian center;
	corners.reserve(len);
	points_convex.insert(points_convex.begin(), *(points_convex.end() - 1));
	std::vector<Cartesian>::reverse_iterator it(points_convex.rbegin()), n_it, e_it(points_convex.rend() - 1);
	for (; it != e_it; it++) {
		n_it = it + 1;
		center = *it ^ *n_it;
		center.normalize();
		Constraint c;
		c.center = center;
		constraints.push_back(c);
		corners.push_back(*it);
	}

	// Calculate the bounding circle for the convex.
	// Take it as the bounding circle of the triangle with the widest opening angle.
	boundingCircle.distance = 1.0;
	std::vector<Cartesian>::iterator it_i(corners.begin()), it_j, it_k;
	for ( ; it_i != corners.end(); it_i++) {
		for (it_j = it_i + 1; it_j != corners.end(); it_j++) {
			for (it_k = it_j + 1; it_k != corners.end(); it_k++) {
				Cartesian v_aux = (*it_j - *it_i) ^ (*it_k - *it_j);
				v_aux.normalize();
				// Calculate the correct opening angle.
				// Can take any corner to calculate the opening angle.
				double d = v_aux * *it_i;
				if (boundingCircle.distance > d) {
					boundingCircle.distance = d;
					boundingCircle.center = v_aux;
					boundingCircle.arcangle = acos(d);
					if (d <= -DBL_TOLERANCE) boundingCircle.sign = NEG;
					else if (d >=  DBL_TOLERANCE) boundingCircle.sign = POS;
					else boundingCircle.sign = ZERO;
				}
			}
		}
	}
}


// Constructor for a convex polygon.
void
Geometry::convexPolygon(std::vector<Cartesian> &v)
{
	// Constraints:
	//    For each side, we have a 0-halfspace (great circle) passing through the 2 corners.
	//    Since we are in counterclockwise order, the vector product of the two
	//    successive corners just gives the correct constraint.
	//
	// Requirements:
	//    Polygons should be counterclockwise.
	//    Polygons should be convex.

	// Repeat the first corner at the end if it does not repeat.
	if (*v.begin() != *(v.end() - 1)) v.push_back(*v.begin());

	int len = v.size();
	if (len < 4) throw "Polygon should have at least three corners";

	bool counterclockwise;
	bool first_counterclockwise;
	std::vector<Cartesian>::iterator it(v.begin()), n_it, it_k, e_it(v.end() - 1);

	//Check for the type of direction.
	Cartesian constraint;
	for ( ; it != e_it; it++) {
		// Direction should be the same for all.
		n_it = it + 1;
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
				throw MSG_Error("Polygon is not convex, You should use the constructor -> Geometry(g, Geometry::NO_CONVEX)");
			}
		}

		// The vector product of the two successive corners just gives the correct constraint.
		constraint = *it ^ *n_it;
		if (constraint.norm() <= DBL_TOLERANCE) {
			throw MSG_Error("Repeating corners, edge error, You should use the constructor -> Geometry(g, Geometry::NO_CONVEX)");
		}
	}

	// Building convex always in counterclockwise.
	if (counterclockwise) {
		corners.reserve(v.size() - 1);
		for (it = v.begin(); it != e_it; it++) {
			n_it = it + 1;
			constraint = *it ^ *n_it;
			constraint.normalize();
			// Convex hulls for a set of points on the surface of a sphere are only well defined
			// if the points all fit within half of the globe. This is a 0-halfspace, or a halfspace
			// with a arcangle pi/2.
			Constraint c;
			c.center = constraint;
			constraints.push_back(c);
			// Normalize the corner.
			it->normalize();
			corners.push_back(*it);
		}
	} else {
		// If direction is clockwise, revert the direction.
		corners.reserve(v.size() - 1);
		std::vector<Cartesian>::reverse_iterator rit(v.rbegin()), rn_it, f_rit(v.rend() - 1);
		for ( ; rit != f_rit; rit++) {
			rn_it = rit + 1;
			constraint = *rit ^ *rn_it;
			constraint.normalize();
			Constraint c;
			c.center = constraint;
			constraints.push_back(c);
			// Normalize the corner.
			rit->normalize();
			corners.push_back(*rit);
		}
	}

	// Calculate the bounding circle for the convex.
	// Take it as the bounding circle of the triangle with the widest opening angle.
	boundingCircle.distance = 1.0;
	for (it = corners.begin(); it != corners.end(); it++) {
		for (n_it = it + 1; n_it != corners.end(); n_it++) {
			for (it_k = n_it + 1; it_k != corners.end(); it_k++) {
				Cartesian v_aux = (*n_it - *it) ^ (*it_k - *n_it);
				v_aux.normalize();
				// Calculate the correct opening angle.
				// Can take any corner to calculate the opening angle.
				double d = v_aux * *it;
				if (boundingCircle.distance > d) {
					boundingCircle.distance = d;
					boundingCircle.center = v_aux;
					boundingCircle.arcangle = acos(d);
					if (d <= -DBL_TOLERANCE) boundingCircle.sign = NEG;
					else if (d >=  DBL_TOLERANCE) boundingCircle.sign = POS;
					else boundingCircle.sign = ZERO;
				}
			}
		}
	}
}


// Obtain the direction of the vectors.
// Return:
// 		If the vectors are collinear, clockwise or counterclockwise.
int
Geometry::direction(const Cartesian &a, const Cartesian &b, const Cartesian &c)
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


// Return the distance^2 between two vectors.
double
Geometry::dist(const Cartesian &a, const Cartesian &b)
{
	Cartesian p = a - b;
	return (p.x * p.x + p.y * p.y + p.z * p.z);
}


// Function used by quickSort to sort an vector of
// points with respect to the first point P0.
bool
Geometry::compare(const Cartesian &P0, const Cartesian &a, const Cartesian &b)
{
	// Calculate direction.
	int dir = direction(P0, a, b);
	if (dir == COLLINEAR) {
		return ((dist(P0, b) > dist(P0, a)) ? true : false);
	}
	return (dir == COUNTERCLOCKWISE) ? true : false;
}


// Return a convex hull of a set of points using Graham Scan Algorithm.
void
Geometry::convexHull(std::vector<Cartesian> &points, std::vector<Cartesian> &points_convex)
{
	int len = (int)points.size();
	if (len < 3) throw MSG_Error("Polygon should have at least three corners");

	// Find the min 'y', min 'x' and min 'z'.
	std::vector<Cartesian>::iterator it(points.begin());
	it->normalize(); // Normalize the points.
	std::vector<Cartesian>::iterator it_swap(it);
	for ( ; it != points.end(); it++) {
		it->normalize(); // Normalize the point.
		if (it->y < it_swap->y ||
		  	(it_swap->y == it->y && it->x < it_swap->x) ||
			(it_swap->y == it->y && it->x == it_swap->x && it->z < it_swap->z)) {
			it_swap = it;
		}
	}

	// Swap positions.
	std::iter_swap(it_swap, points.begin());

	// Sort the n - 1 elements in ascending angle.
	sort(++points.begin(), points.end(), std::bind(compare, *points.begin(), std::placeholders::_1, std::placeholders::_2));

	// Deleting duplicates points.
	it = points.begin();
	while (it != points.end() - 1) {
		(*it == *(it + 1)) ? points.erase(it + 1) : it++;
	}

	if ((len = (int)points.size()) < 3) throw MSG_Error("Polygon should have at least three corners");

	// Points convex
	it = points.begin();
	points_convex.push_back(*it++);
	points_convex.push_back(*it++);
	points_convex.push_back(*it++);

	for (; it != points.end(); it++) {
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


// Found the polygon's area using Shoelace formula.
double
Geometry::areaPolygon()
{
	int len = (int)corners.size();
	double D = 0;
	double I = 0;
	std::vector<Cartesian>::iterator it(corners.begin()), n_i, nn_i;
	for (; it != corners.end(); it++) {
		n_i = (it + 1 == corners.end()) ? corners.begin() : it + 1;
		nn_i = (n_i + 1 == corners.end()) ? corners.begin() : n_i + 1;
		D += it->x * n_i->y * nn_i->z;
		I += it->z * n_i->y * nn_i->x;
	}
	double A = 0.5 * (D - I);
	return (A < 0 ? -A : A);
}


// Found the polygon's centroid.
Cartesian
Geometry::centroidPolygon()
{
	double x = 0, y = 0, z = 0;
	int len = (int)corners.size();
	std::vector<Cartesian>::const_iterator it = corners.begin();
	for ( ; it != corners.end(); it++) {
		x += (*it).x;
		y += (*it).y;
		z += (*it).z;
	}
	return Cartesian(x / len, y / len, z / len);
}


// Average distance from the vertex to the centroid of the polygon.
double
Geometry::vertex2centroid()
{
	Cartesian centroid = centroidPolygon();
	double sum = 0;
	std::vector<Cartesian>::iterator it = corners.begin();
	for ( ; it != corners.end(); it++) {
		sum += centroid.distance(*it);
	}
	return sum / corners.size();
}


// If the region is a bounding circle return the circle's radio.
// Otherwise return the radius equal to vertex2Centroid().
double
Geometry::getRadius()
{
	if (corners.size() > 2) {
		// return sqrt(0.5 * areaPolygon()) * M_PER_RADIUS_EARTH;
		return vertex2centroid() * M_PER_RADIUS_EARTH;
	} else {
		return boundingCircle.arcangle * M_PER_RADIUS_EARTH;
	}
}