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

#include "geometry.h"

#define HTM_FULL 0
#define HTM_PARTIAL 1
#define HTM_OUTSIDE 2

// number of decimal places to print the file python.
#define DIGITS 50

// Maximum level allowed (In this level the accuracy is 30 centimeters).
#define HTM_MAX_LEVEL 25

// Error for generating the trixels
#define HTM_MIN_ERROR 0.2
#define HTM_MAX_ERROR 0.5


// Constants.
constexpr size_t MAX_SIZE_NAME = HTM_MAX_LEVEL | '\x02';
constexpr size_t SIZE_BYTES_ID = 7;
constexpr size_t SIZE_BYTES_POSITIVE = 8;
constexpr size_t SIZE_BITS_ID = 2 * MAX_SIZE_NAME;


// Radians in a circumference (2pi).
constexpr double RAD_PER_CIRCUMFERENCE = 6.28318530717958647692528677;


// error = 0.30*2^(25-level) (depth 25 is about 10 milli-arcseconds for astronomers or 0.30 meters on the earth’s surface)
constexpr double ERROR_NIVEL[] = {
	10066329.6, 5033164.8, 2516582.4, 1258291.2, 629145.6, 314572.8, 157286.4,
	78643.2,    39321.6,   19660.8,   9830.4,    4915.2,   2457.6,   1228.8,
	614.4,     307.2,     153.6,      76.8,      38.4,     19.2,     9.6,
	4.8,       2.4,       1.2,        0.6,       0.3
};


struct range_t {
	uint64_t start;
	uint64_t end;
};


struct trixel_t {
	uint64_t id;
	std::string name;
	int v0, v1, v2;
};


struct index_t {
	int v0, v1, v2;
};


constexpr uint64_t S0 = 8, S1 = 9, S2 = 10, S3 = 11, N0 = 12, N1 = 13, N2 = 14, N3 = 15;


/*
 * All the Geometry was obtained in the next papers:
 * - Alex Szalay, Jim Gray, Gyorgy Fekete, Peter Kunszt, Peter Kukol and Ani Thakar (August 2005).
 *   "Indexing the Sphere with the Hierarchical Triangular Mesh".
 *    http://research.microsoft.com/apps/pubs/default.aspx?id=64531
 * - P. Z. Kunszt, A. S. Szalay, A. R. Thakar (631-637 2001). "The Hierarchical Triangular Mesh".
 *   Dept. of Physics and Astronomy, Johns Hopkins University, Baltimore
 */
class HTM {
	int8_t max_level;
	bool partials;
	std::vector<std::string> partial_names;

	void lookupTrixels(int8_t level, std::string name, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2);
	// Returns 1 if trixel's vertex are inside, otherwise return 0.
	int insideVertex(const Cartesian& v) const noexcept;
	// Verifies if a trixel is inside, outside or partial of the convex.
	int verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;
	// Returns if a trixel is intersecting or inside of a polygon.
	bool testEdgePolygon(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;
	// Return whether there is a hole inside the triangle.
	bool thereisHole(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;

	/*
	 * Test whether one of the halfspace’s boundary circles intersects with
	 * one of the edges of the triangle.
	 */
	bool intersectEdge(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;

	// Returns if there is an overlap between trixel and convex, calculating the bounding circle of the trixel.
	bool boundingCircle(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;
	std::string getCircle3D(size_t points) const;
	void simplifyTrixels();

	// Finds the start trixel containing the coord.
	static std::string startTrixel(Cartesian& v0, Cartesian& v1, Cartesian& v2, const Cartesian& coord) noexcept;
	// Finds the midpoint of two edges.
	static void midPoint(const Cartesian& v0, const Cartesian& v1, Cartesian& w);
	// Receives a name of a trixel and calculates its id.
	static uint64_t name2id(const std::string& name);
	// Returns if v is inside of a trixel.
	static bool insideVector(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, const Cartesian& v);
	// Returns true if there is a intersection between trixel and convex.
	static bool intersection(const Cartesian& v1, const Cartesian& v2, const Constraint& c);
	static void getCorners(const std::string& name, Cartesian& v0, Cartesian& v1, Cartesian& v2);
	static std::string getCircle3D(const Constraint& bCircle, size_t points);

public:
	Geometry region;
	std::vector<std::string> names;

	HTM() = delete;
	// Move constructor.
	HTM(HTM&&) = default;
	// Copy Constructor.
	HTM(const HTM&) = delete;

	/*
	 * Constructor for HTM.
	 *  If _partials then return triangles partials.
	 *  error should be in [HTM_MIN_VALUE, HTM_MAX_VALUE]; it specifics the error according to
	 *  the diameter of the circle or the circle that adjusts the Polygon's area.
	 */
	HTM(bool _partials, double error, Geometry&& _region);

	// Move assignment.
	HTM& operator=(HTM&&) = default;
	// Copy assignment.
	HTM& operator=(const HTM&) = delete;

	void run();
	void writePython3D(const std::string& file) const;

	// Given a coord, calculates its HTM name.
	static std::string cartesian2name(const Cartesian& coord);
	static Cartesian getCentroid(const std::vector<std::string>& trixel_names);
	static void insertRange(const std::string& name, std::vector<range_t>& ranges, int8_t _max_level);
	static void mergeRanges(std::vector<range_t>& ranges);
	static void writePython3D(const std::string& file, const std::vector<Geometry>& g, const std::vector<std::string>& names_f);
};
