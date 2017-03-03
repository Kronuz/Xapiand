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

#include "polygon.h"

#include <cmath>


void
Polygon::init()
{
	/*
	 * Calculate the bounding circle for Polygon.
	 * Take it as the bounding circle of the triangle with the widest opening angle.
	 */
	boundingCircle.distance = 1.0;
	const auto it_e = corners.end();
	for (auto it_i = corners.begin(); it_i != it_e; ++it_i) {
		for (auto it_j = it_i + 1; it_j != it_e; ++it_j) {
			for (auto it_k = it_j + 1; it_k != it_e; ++it_k) {
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
				}
			}
		}

		centroid.x += it_i->x;
		centroid.y += it_i->y;
		centroid.z += it_i->z;
	}

	centroid.normalize();

	double max = 0;
	for (const auto& corner : corners) {
		double d = corner * centroid;
		if (d > max) {
			max = d;
		}
	}

	max_radius = std::acos(max) * M_PER_RADIUS_EARTH;
}


void
Polygon::process(std::vector<Cartesian>&& points)
{
	// Repeats the first corner at the end if it does not repeat.
	if (points.size() && points.front() != points.back()) {
		points.push_back(points.front());
	}

	if (points.size() < 4) {
		THROW(GeometryError, "Polygon should have at least three corners");
	}

	// Check for the type of direction.
	bool counterclockwise = false, first_counterclockwise = false;
	Cartesian constraint;
	auto it_last = points.end() - 1;
	const auto it_b = points.begin();
	for (auto it = it_b; it != it_last; ++it) {
		// Direction should be the same for all.
		const auto& n_point = *(it + 1);
		if (it != it_b) {
			// Calculate the direction of the third corner and restriction formed in the previous iteration.
			if (constraint * n_point < DBL_TOLERANCE) {
				counterclockwise = false;
				if (it == it_b + 1) {
					first_counterclockwise = false;
				}
			} else {
				counterclockwise = true;
				if (it == it_b + 1) {
					first_counterclockwise = true;
				}
			}

			if (it != (it_b + 1) && counterclockwise != first_counterclockwise) {
				THROW(GeometryError, "Polygon is not convex");
			}
		}

		// The vector product of the two successive points just gives the correct constraint.
		constraint = *it ^ n_point;
		if (constraint.norm() < DBL_TOLERANCE) {
			THROW(GeometryError, "Poligon has duplicate points");
		}
	}

	// Building convex polygon always in counterclockwise.
	corners.reserve(points.size());
	if (counterclockwise) {
		for (auto it = points.begin(); it != it_last; ++it) {
			constraint = *it ^ *(it + 1);
			constraint.normalize();
			constraints.push_back(std::move(constraint));
			// Normalizes the corner.
			it->normalize();
			corners.push_back(std::move(*it));
		}
		it_last->normalize();
		corners.push_back(std::move(*it_last));
	} else {
		// If direction is clockwise, revert the direction.
		auto rit_last = points.rend() - 1;
		for (auto rit = points.rbegin(); rit != rit_last; ++rit) {
			constraint = *rit ^ *(rit + 1);
			constraint.normalize();
			constraints.push_back(std::move(constraint));
			// Normalize the corner.
			rit->normalize();
			corners.push_back(std::move(*rit));
		}
		rit_last->normalize();
		corners.push_back(std::move(*rit_last));
	}

	init();
}


inline bool
Polygon::intersectEdges(Cartesian aux, double length_v0_v1, const Cartesian& v0, const Cartesian& v1, double length_corners, const Cartesian& corner, const Cartesian& n_corner) const
{
	/*
	 * If the intersection is inside trixel's edge (v0, v1), its distance to the corners
	 * is smaller than Polygon's side (corner, n_corner). This test has to be done for:
	 * Polygon's edge and trixel's edge.
	 */

	aux.normalize();
	double d1 = std::acos(corner * aux);     // distance to corner
	double d2 = std::acos(n_corner * aux);   // distance to n_corner
	// Test with the Polygon's edge
	if ((d1 - length_corners) < DBL_TOLERANCE && (d2 - length_corners) < DBL_TOLERANCE) {
		d1 = std::acos(v0 * aux);            // distance to vertex v0
		d2 = std::acos(v1 * aux);            // distance to vertex v1
		// Test with trixel's edge.
		if ((d1 - length_v0_v1) < DBL_TOLERANCE && (d2 - length_v0_v1) < DBL_TOLERANCE) {
			return true;
		}
	}

	// Do the same for the other intersection
	aux.inverse();
	d1 = std::acos(corner * aux);     // distance to corner
	d2 = std::acos(n_corner * aux);   // distance to n_corner
	// Test with the Polygon's edge
	if ((d1 - length_corners) < DBL_TOLERANCE && (d2 - length_corners) < DBL_TOLERANCE) {
		d1 = std::acos(v0 * aux);     // distance to vertex v0
		d2 = std::acos(v1 * aux);     // distance to vertex v1
		// Test with trixel's edge.
		if ((d1 - length_v0_v1) < DBL_TOLERANCE && (d2 - length_v0_v1) < DBL_TOLERANCE) {
			return true;
		}
	}

	return false;
}


bool
Polygon::intersectTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
{
	/*
	 * We need to check each polygon's edges against trixel's edges.
	 * If any of trixel's edges has its intersection INSIDE the polygon's side, return true.
	 * Otherwise return if a corner is inside.
	 */

	Cartesian coords[3] = { v0 ^ v1, v1 ^ v2,  v2 ^ v0};
	// Length of trixel's edges in radians.
	double length_trixel_edges[3] = { std::acos(v0 * v1), std::acos(v1 * v2), std::acos(v2 * v0) };

	// Checking each polygon's edge against triangle's edges for intersections.
	const auto it_last = corners.end() - 1;
	for (auto it = corners.begin(); it != it_last; ) {
		const auto& corner = *it;
		const auto& n_corner = *++it;

		const auto aux_coord = corner ^ n_corner;
		double length_polygon_edge = std::acos(corner * n_corner);  // Length of Polygon's edge (corner, n_corner).

		// Intersection with trixel's edges.
		if (intersectEdges(coords[0] ^ aux_coord, length_trixel_edges[0], v0, v1, length_polygon_edge, corner, n_corner) ||
			intersectEdges(coords[1] ^ aux_coord, length_trixel_edges[1], v1, v2, length_polygon_edge, corner, n_corner) ||
			intersectEdges(coords[2] ^ aux_coord, length_trixel_edges[2], v2, v0, length_polygon_edge, corner, n_corner)) {
			return true;
		}
	}

	// If any corner is inside trixel, all corners are inside.
	return HTM::insideVertex_Trixel(*it_last, v0, v1, v2);
}


int
Polygon::insideVertex(const Cartesian& v) const noexcept
{
	for (const auto& constraint : constraints) {
		if (!HTM::insideVertex_Constraint(v, constraint)) {
			return 0;
		}
	}
	return 1;
}


TypeTrixel
Polygon::verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
{
	int sum = insideVertex(v0) + insideVertex(v1) + insideVertex(v2);

	switch (sum) {
		case 0: {
			// If trixel's boundingCircle does not intersect boundingCircle, the trixel is considered OUTSIDE.
			if (HTM::intersectConstraints(boundingCircle, HTM::getBoundingCircle(v0, v1, v2)) && intersectTrixel(v0, v1, v2)) {
				return TypeTrixel::PARTIAL;
			}
			return TypeTrixel::OUTSIDE;
		}
		case 1:
		case 2:
			return TypeTrixel::PARTIAL;
		case 3:
			return TypeTrixel::FULL;
		default:
			return TypeTrixel::OUTSIDE;
	}
}


void
Polygon::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const
{
	// Finish the recursion.
	if (level == data.max_level) {
		data.aux_trixels.push_back(std::move(name));
		return;
	}

	auto w2 = HTM::midPoint(v0, v1);
	auto w0 = HTM::midPoint(v1, v2);
	auto w1 = HTM::midPoint(v2, v0);

	TypeTrixel type_trixels[4] = {
		verifyTrixel(v0, w2, w1),
		verifyTrixel(v1, w0, w2),
		verifyTrixel(v2, w1, w0),
		verifyTrixel(w0, w1, w2)
	};

	// Number of full and partial subtrixels.
	int F = (type_trixels[0] == TypeTrixel::FULL) + (type_trixels[1] == TypeTrixel::FULL) + (type_trixels[2] == TypeTrixel::FULL) + (type_trixels[3] == TypeTrixel::FULL);

	if (F == 4) {
		data.trixels.push_back(std::move(name));
		return;
	}

	++level;

	switch (type_trixels[0]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "0");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v0, w2, w1, name + "0", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[1]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "1");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v1, w0, w2, name + "1", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[2]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "2");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v2, w1, w0, name + "2", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[3]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "3");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(w0, w1, w2, name + "3", data, level);
			break;
		default:
			break;
	}
}


void
Polygon::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const
{
	// Finish the recursion.
	if (level == data.max_level) {
		HTM::insertGreaterRange(data.aux_ranges, HTM::getRange(id, level));
		return;
	}

	auto w2 = HTM::midPoint(v0, v1);
	auto w0 = HTM::midPoint(v1, v2);
	auto w1 = HTM::midPoint(v2, v0);

	TypeTrixel type_trixels[4] = {
		verifyTrixel(v0, w2, w1),
		verifyTrixel(v1, w0, w2),
		verifyTrixel(v2, w1, w0),
		verifyTrixel(w0, w1, w2)
	};

	// Number of full and partial subtrixels.
	int F = (type_trixels[0] == TypeTrixel::FULL) + (type_trixels[1] == TypeTrixel::FULL) + (type_trixels[2] == TypeTrixel::FULL) + (type_trixels[3] == TypeTrixel::FULL);

	if (F == 4) {
		HTM::insertGreaterRange(data.ranges, HTM::getRange(id, level));
		return;
	}

	++level;
	id <<= 2;

	switch (type_trixels[0]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v0, w2, w1, id, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[1]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 1, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v1, w0, w2, id + 1, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[2]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 2, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v2, w1, w0, id + 2, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[3]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 3, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(w0, w1, w2, id + 3, data, level);
			break;
		default:
			break;
	}
}


std::string
Polygon::toWKT() const
{
	std::string wkt;
	const auto str_polygon = to_string();
	wkt.reserve(str_polygon.length() + 12);
	wkt.assign("POLYGON Z (").append(str_polygon).push_back(')');
	return wkt;
}


std::string
Polygon::to_string() const
{
	std::string str;
	const size_t size_corner = 50;
	str.reserve(size_corner * (corners.size() + 1));
	str.push_back('(');
	char result[size_corner];
	for (const auto& corner : corners) {
		snprintf(result, size_corner, "%.6f %.6f %.6f, ",
			corner.x * corner.scale,
			corner.y * corner.scale,
			corner.z * corner.scale
		);
		str.append(result);
	}
	const auto& corner_e = *corners.begin();
	snprintf(result, size_corner, "%.6f %.6f %.6f)",
		corner_e.x * corner_e.scale,
		corner_e.y * corner_e.scale,
		corner_e.z * corner_e.scale
	);
	str.append(result);
	return str;
}


std::vector<std::string>
Polygon::getTrixels(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [%f, %f]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	trixel_data data(partials, HTM_MAX_LEVEL);
	error = error * max_radius;
	for (size_t i = 0; i < HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < error) {
			data.max_level = i;
			break;
		}
	}

	if (verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2], start_trixels[0].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2], start_trixels[1].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2], start_trixels[2].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2], start_trixels[3].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2], start_trixels[4].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2], start_trixels[5].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2], start_trixels[6].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2], start_trixels[7].name, data, 0);
	}

	return data.getTrixels();
}


std::vector<range_t>
Polygon::getRanges(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [%f, %f]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	range_data data(partials, HTM_MAX_LEVEL);
	error = error * max_radius;
	for (size_t i = 0; i < HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < error) {
			data.max_level = i;
			break;
		}
	}

	if (verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2], start_trixels[0].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2], start_trixels[1].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2], start_trixels[2].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2], start_trixels[3].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2], start_trixels[4].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2], start_trixels[5].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2], start_trixels[6].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2], start_trixels[7].id, data, 0);
	}

	return data.getRanges();
}
