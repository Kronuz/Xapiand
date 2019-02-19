/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <cinttypes>        // for PRIu64
#include <cmath>            // for M_PI
#include <cstdint>          // for uint64_t, int8_t
#include <cstdio>           // for snprintf
#include <memory>           // for shared_ptr
#include <vector>           // for vector

#include "cartesian.h"


#define __STDC_FORMAT_MACROS


// Maximum level allowed (In this level the accuracy is 30 centimeters).
constexpr size_t HTM_MAX_LEVEL = 25;


// Error for generating the trixels
constexpr double HTM_MIN_ERROR = 0.05;
constexpr double HTM_MAX_ERROR = 1.0;


// Constants.
constexpr size_t HTM_MAX_LENGTH_NAME = HTM_MAX_LEVEL + 2;  // 25 + 2 = 27
constexpr size_t HTM_BYTES_ID        = 7;
constexpr size_t HTM_BITS_ID         = 2 * HTM_MAX_LENGTH_NAME;  // 27 * 2 = 54
constexpr size_t HTM_START_POS       = HTM_BITS_ID - 4;  // 54 - 4 = 50


// Radians in a circumference (2pi).
constexpr double RAD_PER_CIRCUMFERENCE = 2.0 * M_PI;


// error = 0.30*2^(25-level) (depth 25 is about 10 milli-arcseconds for astronomers or 0.30 meters on the earth’s surface)
constexpr double ERROR_NIVEL[] = {
	10066329.6, 5033164.8, 2516582.4, 1258291.2, 629145.6, 314572.8, 157286.4,
	78643.2,    39321.6,   19660.8,   9830.4,    4915.2,   2457.6,   1228.8,
	614.4,      307.2,     153.6,     76.8,      38.4,     19.2,     9.6,
	4.8,        2.4,       1.2,       0.6,       0.3
};


struct trixel_t {
	uint64_t id;
	std::string name;
	int v0, v1, v2;
};


struct index_t {
	int v0, v1, v2;
};


enum class TypeTrixel : uint8_t {
	FULL,
	PARTIAL,
	OUTSIDE,
};


struct range_t {
	uint64_t start;
	uint64_t end;

	range_t()
		: start(0),
		  end(0) { }

	range_t(uint64_t _start, uint64_t _end)
		: start(_start),
		  end(_end) { }

	range_t(range_t&& o) noexcept = default;
	range_t(const range_t& o) = default;

	range_t& operator=(range_t&& o) noexcept = default;
	range_t& operator=(const range_t& o) = default;

	bool operator==(const range_t& r) const noexcept {
		return start == r.start && end == r.end;
	}

	bool operator!=(const range_t& r) const noexcept {
		return !operator==(r);
	}

	bool operator>(const range_t& r) const noexcept {
		return start > r.start;
	}

	bool operator<(const range_t& r) const noexcept {
		return start < r.start;
	}

	std::string to_string() const {
		char result[36];
		snprintf(result, 36, "%" PRIu64 "-%" PRIu64, start, end);
		return std::string(result);
	}
};


namespace std {
	template<>
	struct hash<range_t> {
		inline size_t operator()(const range_t& p) const {
			std::hash<std::string> hash_fn;
			return hash_fn(p.to_string());
		}
	};
}


const Cartesian start_vertices[6] = {
	Cartesian(0.0,  0.0,  1.0),
	Cartesian(1.0,  0.0,  0.0),
	Cartesian(0.0,  1.0,  0.0),
	Cartesian(-1.0, 0.0,  0.0),
	Cartesian(0.0,  -1.0, 0.0),
	Cartesian(0.0,  0.0,  -1.0),
};


const trixel_t start_trixels[8] = {
	{ 8,  "N0", 1, 0, 4 },
	{ 9,  "N1", 4, 0, 3 },
	{ 10, "N2", 3, 0, 2 },
	{ 11, "N3", 2, 0, 1 },
	{ 12, "S0", 1, 5, 2 },
	{ 13, "S1", 2, 5, 3 },
	{ 14, "S2", 3, 5, 4 },
	{ 15, "S3", 4, 5, 1 },
};


class Constraint;
class Geometry;


/*
 * All the Geometry was obtained in the next papers:
 * - Alex Szalay, Jim Gray, Gyorgy Fekete, Peter Kunszt, Peter Kukol and Ani Thakar (August 2005).
 *   "Indexing the Sphere with the Hierarchical Triangular Mesh".
 *    http://research.microsoft.com/apps/pubs/default.aspx?id=64531
 * - P. Z. Kunszt, A. S. Szalay, A. R. Thakar (631-637 2001). "The Hierarchical Triangular Mesh".
 *   Dept. of Physics and Astronomy, Johns Hopkins University, Baltimore
 *   http://www.noao.edu/noao/staff/yao/sdss_papers/kunszt.pdf
 */
namespace HTM {
	// Union and intersection and exclusive disjunction of two sort vectors of ranges.
	std::vector<std::string> trixel_union(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2);
	std::vector<std::string> trixel_intersection(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2);
	std::vector<std::string> trixel_exclusive_disjunction(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2);

	// Union, intersection and exclusive disjunction of two sort vectors of ranges.
	std::vector<range_t> range_union(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2);
	std::vector<range_t> range_intersection(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2);
	std::vector<range_t> range_exclusive_disjunction(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2);

	// Finds the start trixel containing the coord.
	const trixel_t& startTrixel(const Cartesian& coord) noexcept;

	// Finds the midpoint of two edges.
	Cartesian midPoint(const Cartesian& v0, const Cartesian& v1);

	// Returns if there is a Hole between c and the trixel (v0, v1, v2).
	bool thereisHole(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2);

	// Returns the Constraint defined by the bounding circle of the trixel (v0, v1, v2).
	Constraint getBoundingCircle(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2);

	// Returns if the constraints intersect.
	bool intersectConstraints(const Constraint& c1, const Constraint& c2);

	// Returns if vertex v is inside trixel (v0,v1,v2).
	bool insideVertex_Trixel(const Cartesian& v, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2);

	// Returns if vertex v is inside Constraint c.
	bool insideVertex_Constraint(const Cartesian& v, const Constraint& c);

	// Returns if Constraint c intersects with an edge of the trixel (v0,v1,v2).
	bool intersectConstraint_EdgeTrixel(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2);

	// Returns if there is a intersection between Constraint c and the edge (v1, v2).
	bool intersection(const Constraint& c, const Cartesian& v1, const Cartesian& v2);

	// Simplify a sort vector of trixels.
	void simplifyTrixels(std::vector<std::string>& trixels);

	// Simplify a sort vector of ranges.
	void simplifyRanges(std::vector<range_t>& ranges);

	// Calculates its trixel name.
	std::string getTrixelName(const Cartesian& coord);
	std::string getTrixelName(uint64_t id);

	// Calculates its HTM id.
	uint64_t getId(const Cartesian& coord);
	uint64_t getId(const std::string& name);

	// Get range of given data.
	range_t getRange(uint64_t id, uint8_t level);
	range_t getRange(const std::string& name);

	// Get trixels of ranges.
	std::vector<std::string> getTrixels(const std::vector<range_t>& ranges);

	// Get id trixels of ranges.
	std::vector<uint64_t> getIdTrixels(const std::vector<range_t>& ranges);

	// Insert a range into ranges, range must be greater than or equal to last value in ranges.
	template <typename T, typename = std::enable_if_t<std::is_same<range_t, std::decay_t<T>>::value>>
	void insertGreaterRange(std::vector<range_t>& ranges, T&& range) {
		if (ranges.empty()) {
			ranges.push_back(std::forward<T>(range));
		} else {
			auto& prev = ranges.back();
			if (prev.end < range.start - 1) {     // (start - 1) for join adjacent integer ranges.
				ranges.push_back(std::forward<T>(range));
			} else if (prev.end < range.end) {    // if ranges overlap, last range end is updated.
				prev.end = range.end;
			}
		}
	}

	std::tuple<Cartesian, Cartesian, Cartesian> getCorners(const std::string& name);

	// Functions to test HTM trixels.
	void writeGoogleMap(const std::string& file, const std::string& output_file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels);
	void writePython3D(const std::string& file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels);
	// Functions to test Graham scan algorithm.
	void writeGrahamScanMap(const std::string& file, const std::string& output_file, const std::vector<Cartesian>& points, const std::vector<Cartesian>& convex_points);
	void writeGrahamScan3D(const std::string& file, const std::vector<Cartesian>& points, const std::vector<Cartesian>& convex_points);
}
