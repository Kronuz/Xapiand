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


Geometry::~Geometry()
{
	constraints.clear();
}


// Constructor for a set of points in the Earth.
Geometry::Geometry(std::vector<Cartesian> &v)
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
	int i, next_i, len = points_convex.size();
	Cartesian constraint;

	if (len < 3) throw Error("Convex Hull not found");

	// The corners are in clockwise but we need the corners in counterclockwise order and normalize.
	for (i = len - 1; i >= 0; i--) {
		next_i = (i == 0) ? len - 1 : i - 1;
		constraint = points_convex.at(i) ^ points_convex.at(next_i);
		Constraint c = Constraint();
		constraint.normalize();
		c.center = constraint;
		constraints.push_back(c);
		corners.push_back(points_convex.at(i));
	}

	// Calculate the bounding circle for the convex.
	// Take it as the bounding circle of the triangle with the widest opening angle.
	boundingCircle.distance = 1.0;
	for (i = 0; i < corners.size(); i++) {
		for (int j = i + 1; j < corners.size(); j++) {
			for (int k = j + 1; k < corners.size(); k++) {
				Cartesian v_aux = (corners.at(j) - corners.at(i)) ^ (corners.at(k) - corners.at(j));
				v_aux.normalize();
				// Calculate the correct opening angle.
				// Can take any corner to calculate the opening angle.
				double d = v_aux * corners.at(i);
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
Geometry::compare(Cartesian &a, Cartesian &b)
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
Geometry::convexHull(std::vector<Cartesian> &points, std::vector<Cartesian> &points_convex) {
	int len = points.size();

	if (len < 3) throw Error("Polygon should have al least three corners");

	// Find the min 'y' and the min 'x'
	// Normalize the points.
	points.at(0).normalize();
	double x_min = points.at(0).x, y_min = points.at(0).y, z_min = points.at(0).z;
	int i, swap_pos = 0;

	for (i = 1; i < len; i++) {
		// Normalize the points.
		points.at(i).normalize();
		if (points.at(i).y < y_min ||
			(y_min == points.at(i).y && points.at(i).x < x_min) ||
			(y_min == points.at(i).y && points.at(i).x == x_min && points.at(i).z < z_min)) {
			x_min = points.at(i).x;
			y_min = points.at(i).y;
			z_min = points.at(i).z;
			swap_pos = i;
		}
	}

	// Swap positions.
	std::swap(points.at(swap_pos), points.at(0));

	// Sort the n - 1 elements in ascending angle.
	P0 = points.at(0);
	quickSort(points, 1, points.size() - 1);

	// Deleting duplicates.
	std::vector<Cartesian>::iterator it(points.begin());
	for ( ; it != points.end() - 1; ) {
		if (*it == *(it + 1)) {
			points.erase(it + 1);
			continue;
		}
		it++;
	}

	len = points.size();

	// Points convex
	points_convex.push_back(P0);
	points_convex.push_back(points.at(1));
	points_convex.push_back(points.at(2));

	for (i = 3; i < len; i++) {
		while (true) {

			// Not found the convex.
			if (points_convex.size() == 1) throw Error("Convex Hull not found");

			Cartesian last = points_convex.back();
			points_convex.pop_back();

			Cartesian n_last = points_convex.back();

			int direct = direction(n_last, last, points.at(i));
			if (direct != COUNTERCLOCKWISE) {
				continue;
			} else {
				points_convex.push_back(last);
				break;
			}
		}
		points_convex.push_back(points.at(i));
	}
}


// Sort an vector of points with respect to the first point P0 and their angle using quick sort.
void
Geometry::quickSort(std::vector<Cartesian> &pts, int first, int last)
{
	int r;
	if (first < last) {
		r = partition(pts, first, last);
		quickSort(pts, first, r);
		quickSort(pts, r + 1, last);
	}
}


// Partition for QuickSort
int
Geometry::partition(std::vector<Cartesian> &pts, int first, int last)
{
	int pivote =  first + (last - first) / 2;
	Cartesian c = pts.at(pivote);
	std::swap(pts.at(pivote), pts.at(last));
	int i = first;
	int j;

	for (j = first; j < last; j++) {
		if (compare(pts.at(j), c)) {
			std::swap(pts.at(j), pts.at(i));
			i = i + 1;
		}
	}

	std::swap(pts.at(i), pts.at(last));
	return i;
}


// Found the polygon's area using Shoelace formula.
double
Geometry::areaPolygon()
{
	int len = corners.size();
	double D = 0;
	double I = 0;
	for (int i = 0; i < len; i++) {
		int n_i = (i == len - 1) ? 0 : i + 1;
		int nn_i = (n_i == len - 1) ? 0 : n_i + 1;
		D += corners.at(i).x * corners.at(n_i).y * corners.at(nn_i).z;
		I += corners.at(i).z * corners.at(n_i).y * corners.at(nn_i).x;
	}
	double A = 0.5 * (D - I);
	return (A < 0 ? -A : A);
}


// Found the polygon's centroid.
Cartesian
Geometry::centroidPolygon()
{
	double x = 0, y = 0, z = 0;
	int len = corners.size();
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