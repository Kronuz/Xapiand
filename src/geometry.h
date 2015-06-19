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

#ifndef XAPIAND_INCLUDED_GEOMETRY_H
#define XAPIAND_INCLUDED_GEOMETRY_H

#include "Cartesian.h"
#include <vector>
#include <fstream>

// Constants used for specify the sign of the bounding circle.
#define NEG -1
#define ZERO 0
#define POS 1
#define COLLINEAR 0
#define CLOCKWISE 1
#define COUNTERCLOCKWISE 2

// Earth radius in meters.
const double M_PER_RADIUS_EARTH = 6367444.7;

// Radius maximum allowed in a constraints (all the earth).
const double MAX_RADIUS_HALFSPACE_EARTH = 20003917.491659265; // meters


class Constraint {
	public:
		int sign;
		Cartesian center;
		double distance, arcangle;

		Constraint(Cartesian &center_, double radius);
		Constraint();
		bool operator==(const Constraint &c) const;
		Constraint& operator=(const Constraint &c);
		double meters2rad(double meters);
};


class Geometry {
	public:
		std::vector<Constraint> constraints;
		Constraint boundingCircle;
		std::vector<Cartesian> corners;

		Geometry(const Constraint &c);
		Geometry();
		Geometry(std::vector<Cartesian> &v);
		~Geometry();
		double getRadius();

	private:
		Cartesian P0;
		void convexHull(std::vector<Cartesian> &points, std::vector<Cartesian> &points_convex);
		int direction(const Cartesian &a, const Cartesian &b, const Cartesian &c);
		double dist(const Cartesian &a, const Cartesian &b);
		void quickSort(std::vector<Cartesian> &pts, int first, int last);
		int partition(std::vector<Cartesian> &pts, int first, int last);
		bool compare(Cartesian &a, Cartesian &b);
		double areaPolygon();
};


#endif /* XAPIAND_INCLUDED_GEOMETRY_H */