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

#ifndef XAPIAND_INCLUDED_HTM_H
#define XAPIAND_INCLUDED_HTM_H

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <set>
#include "geometry.h"

#define uInt64 unsigned long long
#define HTM_FULL 0
#define HTM_PARTIAL 1
#define HTM_OUTSIDE 2

// number of decimal places to print the file python.
#define DIGITS 50

// Maximum level allowed (In this level the accuracy is 30 centimeters).
#define HTM_MAX_LEVEL 25

// Radians in a circumference (2pi).
const double RAD_PER_CIRCUMFERENCE = 6.28318530717958647692528677;

// error = 0.30*2^(25-level) (depth 25 is about 10 milli-arcseconds for astronomers or 0.30 meters on the earth’s surface)
const double ERROR_NIVEL[] = {10066329.6, 5033164.8, 2516582.4, 1258291.2, 629145.6, 314572.8, 157286.4,
					   78643.2,    39321.6,   19660.8,   9830.4,    4915.2,   2457.6,   1228.8,
					   614.4,     307.2,     153.6,      76.8,      38.4,     19.2,     9.6,
					   4.8,       2.4,       1.2,        0.6,       0.3}; // meters.

typedef struct range_s {
	uInt64 start;
	uInt64 end;
} range_t;

typedef struct trixel_s {
	uInt64 id;
	std::string name;
	int v0, v1, v2;
} trixel_t;

const uInt64 S0 = 8, S1 = 9, S2 = 10, S3 = 11, N0 = 12, N1 = 13, N2 = 14, N3 = 15;

const Cartesian start_vertices[6] = {
	Cartesian(0.0,  0.0,  1.0),
	Cartesian(1.0,  0.0,  0.0),
	Cartesian(0.0,  1.0,  0.0),
	Cartesian(-1.0, 0.0,  0.0),
	Cartesian(0.0,  -1.0, 0.0),
	Cartesian(0.0,  0.0,  -1.0)
};

const trixel_t start_trixels[8] = {
	{ S0, "s0", 1, 5, 2 },
	{ S1, "s1", 2, 5, 3 },
	{ S2, "s2", 3, 5, 4 },
	{ S3, "s3", 4, 5, 1 },
	{ N0, "n0", 1, 0, 4 },
	{ N1, "n1", 4, 0, 3 },
	{ N2, "n2", 3, 0, 2 },
	{ N3, "n3", 2, 0, 1 }
};


// All the Geometry was obtained in the next papers:
//   * Alex Szalay, Jim Gray, Gyorgy Fekete, Peter Kunszt, Peter Kukol and Ani Thakar (August 2005).
//     "Indexing the Sphere with the Hierarchical Triangular Mesh".
//     http://research.microsoft.com/apps/pubs/default.aspx?id=64531
//   * P. Z. Kunszt, A. S. Szalay, A. R. Thakar (631-637 2001). "The Hierarchical Triangular Mesh".
//     Dept. of Physics and Astronomy, Johns Hopkins University, Baltimore
class HTM {

	public:
		short max_level;
		Geometry region;
		std::vector<std::string> names;
		bool partials;

		HTM(bool partials_, double error, Geometry &region_);

		void run();
		void lookupTrixels(int level, std::string name, const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		int insideVertex(const Cartesian &v);
		int verifyTrixel(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		bool thereisHole(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		bool intersectEdge(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		bool boundingCircle(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		bool testEdgePolygon(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2);
		void writePython3D(const std::string &file);
		std::string getCircle3D(int points);

		static void startTrixel(Cartesian &v0, Cartesian &v1, Cartesian &v2, const Cartesian &coord, std::string &name);
		static void cartesian2name(Cartesian &coord, std::string &name);
		static void name2id(const std::string &name, uInt64 &id);
		static bool insideVector(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2, const Cartesian &v);
		static bool intersection(const Cartesian &v1, const Cartesian &v2, const Constraint &c);
		static void midPoint(const Cartesian &v0, const Cartesian &v1, Cartesian &w);
		static void insertRange(const std::string &name, std::vector<range_t> &ranges, int _max_level);
		static void mergeRanges(std::vector<range_t> &_ranges);
		static void getCorners(const std::string &name, Cartesian &v0, Cartesian &v1, Cartesian &v2);
		static void writePython3D(const std::string &file, std::vector<Geometry> &g, std::vector<std::string> &names_f);
		static std::string getCircle3D(const Constraint &bCircle, int points);
		static bool compareRanges(const range_t &r1, const range_t &r2);
};


#endif /* XAPIAND_INCLUDED_HTM_H */