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

#include "htm.h"

#include <cmath>
#include <set>
#include <fstream>


static const Cartesian start_vertices[6] = {
	Cartesian(0.0,  0.0,  1.0),
	Cartesian(1.0,  0.0,  0.0),
	Cartesian(0.0,  1.0,  0.0),
	Cartesian(-1.0, 0.0,  0.0),
	Cartesian(0.0,  -1.0, 0.0),
	Cartesian(0.0,  0.0,  -1.0)
};


static const trixel_t start_trixels[8] = {
	{ S2, "s2", 3, 5, 4 },
	{ N1, "n1", 4, 0, 3 },
	{ S1, "s1", 2, 5, 3 },
	{ N2, "n2", 3, 0, 2 },
	{ S3, "s3", 4, 5, 1 },
	{ N0, "n0", 1, 0, 4 },
	{ S0, "s0", 1, 5, 2 },
	{ N3, "n3", 2, 0, 1 }
};


static const index_t S[4] = {
	{ 1, 5, 2 },
	{ 2, 5, 3 },
	{ 3, 5, 4 },
	{ 4, 5, 1 }
};


static const index_t N[4] = {
	{ 1, 0, 4 },
	{ 4, 0, 3 },
	{ 3, 0, 2 },
	{ 2, 0, 1 }
};


HTM::HTM(bool _partials, double error, Geometry&& _region)
	: partials(_partials),
	  region(std::move(_region))
{
	// Get the error with respect to the radius.
	error = error > 0.5 ? 2 * HTM_MAX_ERROR : error < 0.2 ? 2 * HTM_MIN_ERROR : 2 * error;
	double errorD = error * region.boundingCircle.radius;
	max_level = HTM_MAX_LEVEL;
	for (int i = 0; i <= HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < errorD || i == HTM_MAX_LEVEL) {
			max_level = i;
			break;
		}
	}
}


std::string
HTM::startTrixel(Cartesian &v0, Cartesian &v1, Cartesian &v2, const Cartesian &coord) noexcept
{
	size_t num = (coord.x > 0 ? 4 : 0) + (coord.y > 0 ? 2 : 0) + (coord.z > 0 ? 1 : 0);
	v0 = start_vertices[start_trixels[num].v0];
	v1 = start_vertices[start_trixels[num].v1];
	v2 = start_vertices[start_trixels[num].v2];
	return start_trixels[num].name;
}


void
HTM::midPoint(const Cartesian &v0, const Cartesian &v1, Cartesian &w)
{
	w = v0 + v1;
	w.normalize();
}


std::string
HTM::cartesian2name(const Cartesian &coord)
{
	Cartesian v0, v1, v2;
	Cartesian w0, w1, w2;

	std::string name(startTrixel(v0, v1, v2, coord));

	// Search in children's trixel
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		midPoint(v0, v1, w2);
		midPoint(v1, v2, w0);
		midPoint(v2, v0, w1);
		if (insideVector(v0, w2, w1, coord)) {
			name += "0";
			v1 = w2;
			v2 = w1;
		} else if(insideVector(v1, w0, w2, coord)) {
			name += "1";
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if(insideVector(v2, w1, w0, coord)) {
			name += "2";
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else {
			name += "3";
			v0 = w0;
			v1 = w1;
			v2 = w2;
		}
	}

	return name;
}


uint64_t
HTM::name2id(const std::string &name)
{
	size_t _size = name.size();

	uint64_t id = name[0] == 'n' ? 3 : 2;

	for (size_t i = 1; i < _size; ++i) {
		id <<= 2;
		id |= name[i] - '0';
	}

	return id;
}


int
HTM::insideVertex(const Cartesian &v) const noexcept
{
	for (auto it = region.constraints.begin(); it != region.constraints.end(); ++it) {
		if ((*it).center * v <= (*it).distance) return 0;
	}

	return 1;
}


int
HTM::verifyTrixel(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2) const
{
	int sum = insideVertex(v0) + insideVertex(v1) + insideVertex(v2);

	if (sum == 1 || sum == 2) return HTM_PARTIAL;

	/*
	 * If corners are inside.
	 *   Case 1: the sign is NEG and there is a hole, the trixel will be mark as HTM_PARTIAL.
	 *   Case 2: the sign is NEG and there are intersections, the trixel will be mark as HTM_PARTIAL.
	 *   Other case: HTM_FULL.
	 */
	if (sum == 3) {
		// We need to test whether there is a hole inside the triangle for negative halfspace’s boundary circle.
		// If there is a hole the trixel is flag to PARTIAL.
		if (region.constraints[0].sign == NEG) {
			if (thereisHole(v0, v1, v2)) return HTM_PARTIAL;
			// Test whether one of the negative halfspace’s boundary circles intersects with one of the edges of the triangle.
			if (intersectEdge(v0, v1, v2)) return HTM_PARTIAL;
		}
		return HTM_FULL;
	}

	if (region.constraints[0].sign == NEG || !boundingCircle(v0, v1, v2)) {
		return HTM_OUTSIDE;
	}

	// If region is a bounding circle
	if (region.constraints[0].sign == POS || region.constraints.size() == 1) {
		// The constraint intersect with a side.
		if (intersectEdge(v0, v1, v2)) {
			return HTM_PARTIAL;
			// Constraint in triangle.
		} else if (insideVector(v0, v1, v2, region.boundingCircle.center)) {
			return HTM_PARTIAL;
		} else {
			return HTM_OUTSIDE;
		}
	// If region it is a Polygon
	} else if (testEdgePolygon(v0, v1, v2)) {
		return HTM_PARTIAL;
	} else {
		return HTM_OUTSIDE;
	}
}


bool
HTM::testEdgePolygon(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2) const
{
	/*
	 * We need to check each polygon's side against the 3 triangle edges.
	 * If any of the triangle's edges has its intersection INSIDE the polygon's side, return true.
	 * Otherwise return if a corner is inside.
	 */

	Cartesian coords[3];
	double length[3];  // length of edge in radians.
	Cartesian start_e[3]; // Start edge.
	Cartesian end_e[3]; // End edge.

	// For the first edge
	coords[0] = v0 ^ v1;
	length[0] = std::acos(v0 * v1);
	start_e[0] = v0;
	end_e[0] = v1;

	// For the second edge
	coords[1] = v1 ^ v2;
	length[1] = std::acos(v1 * v2);
	start_e[1] = v1;
	end_e[1] = v2;

	// For the third edge
	coords[2] = v2 ^ v0;
	length[2] = std::acos(v2 * v0);
	start_e[2] = v2;
	end_e[2] = v0;

	// Checking each polygon's side against the 3 triangle edges for intersections.
	auto numCorners = region.corners.size();
	for (size_t i = 0; i < numCorners; ++i) {
		size_t j = (i == numCorners - 1) ? 0 : i + 1;
		double d1, d2;
		double dij = std::acos(region.corners[i] * region.corners[j]);  // Distance between points i and j.

		// Calculate the intersection with the 3 triangle's edges.
		for (size_t k = 0; k < 3; ++k) {
			auto aux = coords[k] ^ (region.corners[i] ^ region.corners[j]);
			aux.normalize();
			// If the intersection is inside the edge of the convex, its distance to the corners
			// is smaller than the side of Polygon. This test has to be done for:
			//     convex's edge and
			//     triangle's edge.
			for (size_t kk = 0; kk < 2; ++kk) {
				d1 = std::acos(region.corners[i] * aux); // distance to the corner i
				d2 = std::acos(region.corners[j] * aux); // distance to the corner j
				// Test with the convex's edge
				if (d1 - dij < DBL_TOLERANCE && d2 - dij < DBL_TOLERANCE) {
					d1 = std::acos(start_e[k] * aux);
					d2 = std::acos(end_e[k] * aux);
					// Test with triangle's edge.
					if ((d1 - length[k]) < DBL_TOLERANCE && (d2 - length[k]) < DBL_TOLERANCE) {
						return true;
					}
				}
				aux.inverse(); // Do the same for the other intersection
			}
		}
	}

	return insideVector(v0, v1, v2, region.corners[0]);
}


bool
HTM::thereisHole(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2) const noexcept
{
	if (((v0 ^ v1) * region.boundingCircle.center) >= 0.0 ||
		((v1 ^ v2) * region.boundingCircle.center) >= 0.0 ||
		((v2 ^ v0) * region.boundingCircle.center) >= 0.0)
		return false;
	return true;
}


bool
HTM::insideVector(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2, const Cartesian &v) noexcept
{
	if (((v0 ^ v1) * v) < 0 ||
		((v1 ^ v2) * v) < 0 ||
		((v2 ^ v0) * v) < 0)
		return false;
	return true;
}


bool
HTM::intersectEdge(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2) const
{
	if (intersection(v0, v1, region.boundingCircle) ||
		intersection(v1, v2, region.boundingCircle) ||
		intersection(v2, v0, region.boundingCircle)) return true;
	return false;
}


bool
HTM::intersection(const Cartesian &v1, const Cartesian &v2, const Constraint &c)
{
	double gamma1 = v1 * c.center;
	double gamma2 = v2 * c.center;
	double cos_t = v1 * v2;
	double square_u = (1 - cos_t) / (1 + cos_t);

	double _a = - square_u * (gamma1 + c.distance);
	double _b = gamma1 * (square_u - 1) + gamma2 * (square_u + 1);
	double _c = gamma1 - c.distance;
	double aux = (_b * _b) - (4 * _a * _c);

	if (aux < 0.0 || (_a > -DBL_TOLERANCE && _a < DBL_TOLERANCE)) return false;


	aux = std::sqrt(aux);
	_a = 2 * _a;
	_b = -_b;
	double r1 = (_b + aux) / _a;
	double r2 = (_b - aux) / _a;

	if ((r1 >= 0.0 && r1 <= 1.0) || (r2 >= 0.0 && r2 <= 1.0)) return true;

	return false;
}


bool
HTM::boundingCircle(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2) const
{
	Cartesian vb = (v1 - v0) ^ (v2 - v1);
	vb.normalize();
	double phi_d = std::acos(v0 * vb);

	double tetha = std::acos(vb * region.boundingCircle.center);
	if (tetha >= (phi_d + region.boundingCircle.arcangle)) return false;
	return true;
}


void
HTM::lookupTrixels(int8_t level, std::string name, const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	// Finish the recursion.
	if (--level < 0) {
		partials ? names.push_back(std::move(name)) : partial_names.push_back(std::move(name));
		return;
	}

	int P, F, type_trixels[4];
	Cartesian w0, w1, w2;
	midPoint(v0, v1, w2);
	midPoint(v1, v2, w0);
	midPoint(v2, v0, w1);

	type_trixels[0] = verifyTrixel(v0, w2, w1);
	type_trixels[1] = verifyTrixel(v1, w0, w2);
	type_trixels[2] = verifyTrixel(v2, w1, w0);
	type_trixels[3] = verifyTrixel(w0, w1, w2);

	// Number of full and partial subtrixels.
	F = (type_trixels[0] == HTM_FULL)  + (type_trixels[1] == HTM_FULL)  + (type_trixels[2] == HTM_FULL)  + (type_trixels[3] == HTM_FULL);
	P = (type_trixels[0] == HTM_PARTIAL) + (type_trixels[1] == HTM_PARTIAL) + (type_trixels[2] == HTM_PARTIAL) + (type_trixels[3] == HTM_PARTIAL);

	if (F == 4) {
		names.push_back(std::move(name));
		return;
	}

	// Save the full subtrixels' id
	if (type_trixels[0] == HTM_FULL) {
		names.push_back(name + "0");
	}
	if (type_trixels[1] == HTM_FULL) {
		names.push_back(name + "1");
	}
	if (type_trixels[2] == HTM_FULL) {
		names.push_back(name + "2");
	}
	if (type_trixels[3] == HTM_FULL) {
		names.push_back(name + "3");
	}

	// Recursion
	if (type_trixels[0] == HTM_PARTIAL) lookupTrixels(level, name + "0", v0, w2, w1);
	if (type_trixels[1] == HTM_PARTIAL) lookupTrixels(level, name + "1", v1, w0, w2);
	if (type_trixels[2] == HTM_PARTIAL) lookupTrixels(level, name + "2", v2, w1, w0);
	if (type_trixels[3] == HTM_PARTIAL) lookupTrixels(level, name + "3", w0, w1, w2);
}


void
HTM::run()
{
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]))
		lookupTrixels(max_level, start_trixels[0].name, start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]))
		lookupTrixels(max_level, start_trixels[1].name, start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]))
		lookupTrixels(max_level, start_trixels[2].name, start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]))
		lookupTrixels(max_level, start_trixels[3].name, start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]))
		lookupTrixels(max_level, start_trixels[4].name, start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]))
		lookupTrixels(max_level, start_trixels[5].name, start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]))
		lookupTrixels(max_level, start_trixels[6].name, start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]))
		lookupTrixels(max_level, start_trixels[7].name, start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]);

	// If there is not full trixels, return the partial trixels although the partials is false.
	if (names.size() == 0) {
		names.reserve(partial_names.size());
		names.insert(names.begin(), partial_names.begin(), partial_names.end());
	}

	simplifyTrixels();
}


void
HTM::simplifyTrixels()
{
	for (auto i = 0u;  names.size() - i > 3; ) {
		auto l = i + 1;
		auto j = l++;
		auto k = l++;
		auto tlen = names[i].size();
		auto flen = tlen - 1;
		std::string father(names[i].substr(0, flen));
		if (names[j].size() == tlen && names[j].compare(0, flen, father) == 0 && \
			names[k].size() == tlen && names[k].compare(0, flen, father) == 0 && \
			names[l].size() == tlen && names[l].compare(0, flen, father) == 0) {
			auto it = names.erase(names.begin() + j);
			it = names.erase(it);
			names.erase(it);
			names[i] = father;
			i < 3 ? i = 0 : i -= 3;
			continue;
		}
		++i;
	}
}


void
HTM::insertRange(const std::string &name, std::vector<range_t> &ranges, int8_t _max_level)
{
	int8_t mask;
	uint64_t start, end;
	uint64_t id = name2id(name);

	int8_t level = name.size() - 2;
	if (level < _max_level) {
		mask = (_max_level - level) << 1;
		start = id << mask;
		end = start + ((uint64_t) 1 << mask) - 1;
	} else {
		start = end = id;
	}

	ranges.push_back({ start, end });
}


void
HTM::mergeRanges(std::vector<range_t> &ranges)
{
	if (ranges.size() == 0) return;

	// Vector sorted Low to High according to start.
	std::sort(ranges.begin(), ranges.end(), [](const range_t &r1, const range_t &r2) {
		return r1.start < r2.start;
	});

	for (auto it = ranges.begin() + 1; it != ranges.end(); ) {
		auto tmp = it - 1;	 // Get previous range.
		// If current range is not overlapping with previous.
		if (tmp->end < it->start - 1) {  // (start-1) for join adjacent integer ranges).
			++it;
			continue;
		} else if (tmp->end < it->end) {  // range, continue. Otherwise update the end of
			tmp->end = it->end;			  // previous range, if ending of current range is more.
			it = ranges.erase(it);
			continue;
		}

		it = ranges.erase(it);  // If ranges overlapping.
	}
}


void
HTM::getCorners(const std::string &name, Cartesian &v0, Cartesian &v1, Cartesian &v2) noexcept
{
	Cartesian w0, w1, w2;
	size_t trixel = name[1] - '0';

	if (name[0] == 's') {
		v0 = start_vertices[S[trixel].v0];
		v1 = start_vertices[S[trixel].v1];
		v2 = start_vertices[S[trixel].v2];
	} else {
		v0 = start_vertices[N[trixel].v0];
		v1 = start_vertices[N[trixel].v1];
		v2 = start_vertices[N[trixel].v2];
	}

	size_t i = 2, len = name.size();
	while (i < len) {
		midPoint(v0, v1, w2);
		midPoint(v1, v2, w0);
		midPoint(v2, v0, w1);
		switch (name[i]) {
			case '0':
				v1 = w2;
				v2 = w1;
				break;
			case '1':
				v0 = v1;
				v1 = w0;
				v2 = w2;
				break;
			case '2':
				v0 = v2;
				v1 = w1;
				v2 = w0;
				break;
			case '3':
				v0 = w0;
				v1 = w1;
				v2 = w2;
				break;
		}
		++i;
	}

}


std::string
HTM::getCircle3D(size_t points) const
{
	double inc = RAD_PER_CIRCUMFERENCE / points;
	char x0s[DIGITS];
	char y0s[DIGITS];
	char z0s[DIGITS];
	snprintf(x0s, DIGITS, "%.50f", region.boundingCircle.center.x);
	snprintf(y0s, DIGITS, "%.50f", region.boundingCircle.center.y);
	snprintf(z0s, DIGITS, "%.50f", region.boundingCircle.center.z);
	std::string xs = "x = [" + std::string(x0s) + "]\n";
	xs += "y = [" + std::string(y0s) + "]\n";
	xs += "z = [" + std::string(z0s) + "]\n";
	xs += "ax.plot3D(x, y, z, 'ko', linewidth = 2.0)\n\n";
	std::string ys, zs;
	if (region.boundingCircle.sign == POS) {
		std::string x0, y0, z0;
		xs += "x = [";
		ys = "y = [";
		zs = "z = [";
		Cartesian a = (region.boundingCircle.center.y == 0) ? Cartesian(0, 1, 0) : Cartesian(1.0, - (region.boundingCircle.center.x + region.boundingCircle.center.z) / region.boundingCircle.center.y, 1.0);
		a.normalize();
		Cartesian b = a ^ region.boundingCircle.center;

		Cartesian vc;
		for (double t = 0; t <= RAD_PER_CIRCUMFERENCE; t += inc) {
			double rc = region.boundingCircle.arcangle * std::cos(t);
			double rs = region.boundingCircle.arcangle * std::sin(t);
			vc.x = region.boundingCircle.center.x + rc * a.x + rs * b.x;
			vc.y = region.boundingCircle.center.y + rc * a.y + rs * b.y;
			vc.z = region.boundingCircle.center.z + rc * a.z + rs * b.z;
			char vcx[DIGITS];
			char vcy[DIGITS];
			char vcz[DIGITS];
			snprintf(vcx, DIGITS, "%.50f", vc.x);
			snprintf(vcy, DIGITS, "%.50f", vc.y);
			snprintf(vcz, DIGITS, "%.50f", vc.z);
			xs += std::string(vcx) + ", ";
			ys += std::string(vcy) + ", ";
			zs += std::string(vcz) + ", ";
			if (t == 0.0) {
				x0 = std::string(vcx) + ", ";
				y0 = std::string(vcy) + ", ";
				z0 = std::string(vcz) + ", ";
			}
		}
		xs += x0 + "]\n";
		ys += y0 + "]\n";
		zs += z0 + "]\n";
	}

	return xs + ys + zs;
}


std::string
HTM::getCircle3D(const Constraint &bCircle, size_t points)
{
	double inc = RAD_PER_CIRCUMFERENCE / points;
	char x0s[DIGITS];
	char y0s[DIGITS];
	char z0s[DIGITS];
	snprintf(x0s, DIGITS, "%.50f", bCircle.center.x);
	snprintf(y0s, DIGITS, "%.50f", bCircle.center.y);
	snprintf(z0s, DIGITS, "%.50f", bCircle.center.z);
	std::string xs = "x = [" + std::string(x0s) + "]\n";
	xs += "y = [" + std::string(y0s) + "]\n";
	xs += "z = [" + std::string(z0s) + "]\n";
	xs += "ax.plot3D(x, y, z, 'ko', linewidth = 2.0)\n\n";
	std::string ys, zs;
	if (bCircle.sign == POS) {
		std::string x0, y0, z0;
		xs += "x = [";
		ys = "y = [";
		zs = "z = [";
		Cartesian a = (bCircle.center.y == 0) ? Cartesian(0, 1, 0) : Cartesian(1.0, - (bCircle.center.x + bCircle.center.z) / bCircle.center.y, 1.0);
		a.normalize();
		Cartesian b = a ^ bCircle.center;

		Cartesian vc;
		for (double t = 0; t <= RAD_PER_CIRCUMFERENCE; t += inc) {
			double rc = bCircle.arcangle * std::cos(t);
			double rs = bCircle.arcangle * std::sin(t);
			vc.x = bCircle.center.x + rc * a.x + rs * b.x;
			vc.y = bCircle.center.y + rc * a.y + rs * b.y;
			vc.z = bCircle.center.z + rc * a.z + rs * b.z;
			char vcx[DIGITS];
			char vcy[DIGITS];
			char vcz[DIGITS];
			snprintf(vcx, DIGITS, "%.50f", vc.x);
			snprintf(vcy, DIGITS, "%.50f", vc.y);
			snprintf(vcz, DIGITS, "%.50f", vc.z);
			xs += std::string(vcx) + ", ";
			ys += std::string(vcy) + ", ";
			zs += std::string(vcz) + ", ";
			if (t == 0.0) {
				x0 = std::string(vcx) + ", ";
				y0 = std::string(vcy) + ", ";
				z0 = std::string(vcz) + ", ";
			}
		}
		xs += x0 + "]\n";
		ys += y0 + "]\n";
		zs += z0 + "]\n";
	}

	return xs + ys + zs;
}


void
HTM::writePython3D(const std::string &file) const
{
	std::ofstream fs(file);
	std::string x("x = ["), y("y = ["), z("z = [");
	int numCorners = (int)region.corners.size() - 1;

	fs << "from mpl_toolkits.mplot3d import Axes3D\n";
	fs << "from mpl_toolkits.mplot3d.art3d import Poly3DCollection\n";
	fs << "import matplotlib.pyplot as plt\n\n\n";
	fs << "ax = Axes3D(plt.figure())\n";
	if (numCorners > 1) {
		for (int i = 0; i < numCorners; ++i) {
			char vx[DIGITS];
			char vy[DIGITS];
			char vz[DIGITS];
			auto aux = region.corners[i];
			snprintf(vx, DIGITS, "%.50f", aux.x);
			snprintf(vy, DIGITS, "%.50f", aux.y);
			snprintf(vz, DIGITS, "%.50f", aux.z);
			x += std::string(vx) + ", ";
			y += std::string(vy) + ", ";
			z += std::string(vz) + ", ";
		}
		char v0x[DIGITS];
		char v0y[DIGITS];
		char v0z[DIGITS];
		auto aux = region.corners[numCorners];
		snprintf(v0x, DIGITS, "%.50f", aux.x);
		snprintf(v0y, DIGITS, "%.50f", aux.y);
		snprintf(v0z, DIGITS, "%.50f", aux.z);
		char v1x[DIGITS];
		char v1y[DIGITS];
		char v1z[DIGITS];
		aux = region.corners[0];
		snprintf(v1x, DIGITS, "%.50f", aux.x);
		snprintf(v1y, DIGITS, "%.50f", aux.y);
		snprintf(v1z, DIGITS, "%.50f", aux.z);
		x += std::string(v0x) + ", " + std::string(v1x) + "]\n";
		y += std::string(v0y) + ", " + std::string(v1y) + "]\n";
		z += std::string(v0z) + ", " + std::string(v1z) + "]\n";
		fs << (x + y + z) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
	} else {
		fs << getCircle3D(100) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
	}

	for (auto itn = names.begin(); itn != names.end(); ++itn) {
		Cartesian v0, v1, v2;
		getCorners((*itn), v0, v1, v2);
		char v0x[DIGITS];
		char v0y[DIGITS];
		char v0z[DIGITS];
		snprintf(v0x, DIGITS, "%.50f", v0.x);
		snprintf(v0y, DIGITS, "%.50f", v0.y);
		snprintf(v0z, DIGITS, "%.50f", v0.z);
		char v1x[DIGITS];
		char v1y[DIGITS];
		char v1z[DIGITS];
		snprintf(v1x, DIGITS, "%.50f", v1.x);
		snprintf(v1y, DIGITS, "%.50f", v1.y);
		snprintf(v1z, DIGITS, "%.50f", v1.z);
		char v2x[DIGITS];
		char v2y[DIGITS];
		char v2z[DIGITS];
		snprintf(v2x, DIGITS, "%.50f", v2.x);
		snprintf(v2y, DIGITS, "%.50f", v2.y);
		snprintf(v2z, DIGITS, "%.50f", v2.z);
		x = "x = [" + std::string(v0x) + ", " + std::string(v1x) + ", " + std::string(v2x) + ", " + std::string(v0x) + "]\n";
		y = "y = [" + std::string(v0y) + ", " + std::string(v1y) + ", " + std::string(v2y) + ", " + std::string(v0y) + "]\n";
		z = "z = [" + std::string(v0z) + ", " + std::string(v1z) + ", " + std::string(v2z) + ", " + std::string(v0z) + "]\n";
		fs << (x + y + z) << "ax.plot3D(x, y, z, 'r-')\n\n";
	}

	fs << "plt.ion()\nplt.grid()\nplt.show()";
	fs.close();
}


void
HTM::writePython3D(const std::string &file, const std::vector<Geometry> &g, const std::vector<std::string> &names_f)
{
	std::ofstream fs(file);

	fs << "from mpl_toolkits.mplot3d import Axes3D\n";
	fs << "from mpl_toolkits.mplot3d.art3d import Poly3DCollection\n";
	fs << "import matplotlib.pyplot as plt\n\n\n";
	fs << "ax = Axes3D(plt.figure())\n";

	for (auto it_g = g.begin(); it_g != g.end(); ++it_g) {
		std::string x("x = ["), y("y = ["), z("z = [");
		int numCorners = (int)(it_g->corners.size()) - 1;
		if (numCorners > 1) {
			for (int i = 0; i < numCorners; ++i) {
				char vx[DIGITS];
				char vy[DIGITS];
				char vz[DIGITS];
				auto aux = (*it_g).corners[i];
				snprintf(vx, DIGITS, "%.50f", aux.x);
				snprintf(vy, DIGITS, "%.50f", aux.y);
				snprintf(vz, DIGITS, "%.50f", aux.z);
				x += std::string(vx) + ", ";
				y += std::string(vy) + ", ";
				z += std::string(vz) + ", ";
			}
			char v0x[DIGITS];
			char v0y[DIGITS];
			char v0z[DIGITS];
			auto aux = (*it_g).corners[numCorners];
			snprintf(v0x, DIGITS, "%.50f", aux.x);
			snprintf(v0y, DIGITS, "%.50f", aux.y);
			snprintf(v0z, DIGITS, "%.50f", aux.z);
			char v1x[DIGITS];
			char v1y[DIGITS];
			char v1z[DIGITS];
			aux = (*it_g).corners[0];
			snprintf(v1x, DIGITS, "%.50f", aux.x);
			snprintf(v1y, DIGITS, "%.50f", aux.y);
			snprintf(v1z, DIGITS, "%.50f", aux.z);
			x += std::string(v0x) + ", " + std::string(v1x) + "]\n";
			y += std::string(v0y) + ", " + std::string(v1y) + "]\n";
			z += std::string(v0z) + ", " + std::string(v1z) + "]\n";
			fs << (x + y + z) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
		} else {
			fs << getCircle3D((*it_g).boundingCircle, 100) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
		}
	}

	std::string x, y, z;
	for (auto itn = names_f.begin(); itn != names_f.end(); ++itn) {
		Cartesian v0, v1, v2;
		getCorners((*itn), v0, v1, v2);
		char v0x[DIGITS];
		char v0y[DIGITS];
		char v0z[DIGITS];
		snprintf(v0x, DIGITS, "%.50f", v0.x);
		snprintf(v0y, DIGITS, "%.50f", v0.y);
		snprintf(v0z, DIGITS, "%.50f", v0.z);
		char v1x[DIGITS];
		char v1y[DIGITS];
		char v1z[DIGITS];
		snprintf(v1x, DIGITS, "%.50f", v1.x);
		snprintf(v1y, DIGITS, "%.50f", v1.y);
		snprintf(v1z, DIGITS, "%.50f", v1.z);
		char v2x[DIGITS];
		char v2y[DIGITS];
		char v2z[DIGITS];
		snprintf(v2x, DIGITS, "%.50f", v2.x);
		snprintf(v2y, DIGITS, "%.50f", v2.y);
		snprintf(v2z, DIGITS, "%.50f", v2.z);
		x = "x = [" + std::string(v0x) + ", " + std::string(v1x) + ", " + std::string(v2x) + ", " + std::string(v0x) + "]\n";
		y = "y = [" + std::string(v0y) + ", " + std::string(v1y) + ", " + std::string(v2y) + ", " + std::string(v0y) + "]\n";
		z = "z = [" + std::string(v0z) + ", " + std::string(v1z) + ", " + std::string(v2z) + ", " + std::string(v0z) + "]\n";
		fs << (x + y + z) << "ax.plot3D(x, y, z, 'r-')\n\n";
	}
	fs << "plt.ion()\nplt.grid()\nplt.show()";
	fs.close();
}


Cartesian
HTM::getCentroid(const std::vector<std::string> &trixel_names) noexcept
{
	Cartesian w0, w1, w2, v0, v1, v2, centroid(0, 0, 0);

	for (auto it = trixel_names.begin(); it != trixel_names.end(); ++it) {
		size_t trixel = it->at(1) - '0';
		if (it->at(0) == 's') {
			v0 = start_vertices[S[trixel].v0];
			v1 = start_vertices[S[trixel].v1];
			v2 = start_vertices[S[trixel].v2];
		} else {
			v0 = start_vertices[N[trixel].v0];
			v1 = start_vertices[N[trixel].v1];
			v2 = start_vertices[N[trixel].v2];
		}

		size_t i = 2, len = it->size();
		while (i < len) {
			midPoint(v0, v1, w2);
			midPoint(v1, v2, w0);
			midPoint(v2, v0, w1);
			switch (it->at(i)) {
				case '0':
					v1 = w2;
					v2 = w1;
					break;
				case '1':
					v0 = v1;
					v1 = w0;
					v2 = w2;
					break;
				case '2':
					v0 = v2;
					v1 = w1;
					v2 = w0;
					break;
				case '3':
					v0 = w0;
					v1 = w1;
					v2 = w2;
					break;
			}
			++i;
		}

		centroid += v0 + v1 + v2;
	}

	centroid.normalize();

	return centroid;
}
