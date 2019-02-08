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

#include "polygon.h"

#include <algorithm>
#include <cmath>


Polygon::ConvexPolygon::Direction
Polygon::ConvexPolygon::get_direction(const Cartesian& a, const Cartesian& b, const Cartesian& c) noexcept
{
	double angle = (a ^ b) * c;
	if (angle > DBL_TOLERANCE) {
		return Direction::CLOCKWISE;
	}
	if (angle < -DBL_TOLERANCE) {
		return Direction::COUNTERCLOCKWISE;
	}
	return Direction::COLLINEAR;
}


double
Polygon::ConvexPolygon::dist(const Cartesian& a, const Cartesian& b) noexcept
{
	Cartesian p = a - b;
	return p.x * p.x + p.y * p.y + p.z * p.z;
}


std::vector<Cartesian>
Polygon::ConvexPolygon::graham_scan(std::vector<Cartesian>&& points)
{
	if (points.size() < 3) {
		THROW(GeometryError, "Polygon must have at least three corners");
	}

	// Find the min 'y', min 'x' and min 'z'.
	auto it = points.begin();
	auto it_e = points.end();
	it->normalize(); // Normalize the point.
	auto it_swap = it;
	for (++it; it != it_e; ++it) {
		it->normalize(); // Normalize the point.
		if (*it < *it_swap) {
			it_swap = it;
		}
	}

	// Swap positions.
	std::iter_swap(it_swap, points.begin());

	// Sort the n - 1 elements in ascending angle with respect to the first point P0.
	it = points.begin();
	std::sort(it + 1, points.end(), [P0 = *it](const Cartesian &a, const Cartesian &b) {
		// Calculate direction.
		const auto dir = get_direction(P0, a, b);
		switch (dir) {
			case Direction::COLLINEAR:
				return (dist(P0, b) > dist(P0, a));
			case Direction::COUNTERCLOCKWISE:
				return true;
			case Direction::CLOCKWISE:
				return false;
			default:
				return false;
		}
	});

	// Deleting duplicates points.
	while (it != points.end() - 1) {
		*it == *(it + 1) ? it = points.erase(it) : ++it;
	}

	if (points.size() < 3) {
		THROW(GeometryError, "Polygon should have at least three corners");
	}

	// Points convex polygon
	std::vector<Cartesian> convex_points;
	convex_points.reserve(points.size() + 1);

	it = points.begin();
	it_e = points.end();
	convex_points.push_back(*it++);
	convex_points.push_back(*it++);
	convex_points.push_back(*it++);
	for ( ; it != it_e; ++it) {
		while (true) {
			// Not found the convex polygon.
			if (convex_points.size() == 1) {
				THROW(GeometryError, "Convex Hull not found");
			}
			const auto it_last = convex_points.end() - 1;
			const auto dir = get_direction(*(it_last - 1), *it_last, *it);
			if (dir == Direction::COUNTERCLOCKWISE) {
				break;
			}
			convex_points.pop_back();
		}
		convex_points.push_back(std::move(*it));
	}
	// Duplicate first point.
	convex_points.push_back(convex_points.front());

	return convex_points;
}


void
Polygon::ConvexPolygon::init()
{
	/*
	 * Calculate the bounding circle for Polygon.
	 * Take it as the bounding circle of the triangle with the widest opening angle.
	 */
	boundingCircle.distance = 1.0;
	const auto it_last = corners.end() - 1;
	for (auto it_i = corners.begin(); it_i != it_last; ++it_i) {
		for (auto it_j = it_i + 1; it_j != it_last; ++it_j) {
			for (auto it_k = it_j + 1; it_k != it_last; ++it_k) {
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

		centroid += *it_i;
	}

	centroid.normalize();
	centroid.scale = M_PER_RADIUS_EARTH;

	double max = 1.0;
	for (auto it = corners.begin(); it != it_last; ++it) {
		double d = *it * centroid;
		if (d < max) {
			max = d;
		}
	}

	radius = std::acos(max) * M_PER_RADIUS_EARTH;
}


void
Polygon::ConvexPolygon::process_chull(std::vector<Cartesian>&& points)
{
	// The convex is formed in counterclockwise.
	auto convex_points = graham_scan(std::move(points));

	if (convex_points.size() < 3) {
		THROW(GeometryError, "Convex Hull not found");
	}

	// The corners are in clockwise but we need the corners in counterclockwise order and normalize.
	corners.reserve(convex_points.size());

	auto it_last = convex_points.rend() - 1;
	for (auto it = convex_points.rbegin(); it != it_last; ++it) {
		auto center = *it ^ *(it + 1);
		center.normalize();
		constraints.emplace_back(std::move(center));
		corners.push_back(std::move(*it));
	}
	corners.push_back(std::move(*it_last));

	init();
}


void
Polygon::ConvexPolygon::process_polygon(std::vector<Cartesian>&& points)
{
	// Repeats the first corner at the end if it does not repeat.
	if (!points.empty() && points.front() != points.back()) {
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
		// Direction must be the same for all.
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
			constraints.emplace_back(std::move(constraint));
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
			constraints.emplace_back(std::move(constraint));
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
Polygon::ConvexPolygon::intersectEdges(Cartesian aux, double length_v0_v1, const Cartesian& v0, const Cartesian& v1, double length_corners, const Cartesian& corner, const Cartesian& n_corner) const
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
Polygon::ConvexPolygon::intersectTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
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
Polygon::ConvexPolygon::insideVertex(const Cartesian& v) const noexcept
{
	for (const auto& constraint : constraints) {
		if (!HTM::insideVertex_Constraint(v, constraint)) {
			return 0;
		}
	}
	return 1;
}


TypeTrixel
Polygon::ConvexPolygon::verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
{
	int sum = (
		(insideVertex(v0) != 0 ? 1 : 0) +
		(insideVertex(v1) != 0 ? 1 : 0) +
		(insideVertex(v2) != 0 ? 1 : 0)
	);

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
Polygon::ConvexPolygon::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const
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
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

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
Polygon::ConvexPolygon::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const
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
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

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
Polygon::ConvexPolygon::toWKT() const
{
	if (corners.empty()) {
		return std::string("POLYGON EMPTY");
	}

	std::string wkt;
	const auto str_polygon = to_string();
	wkt.reserve(str_polygon.length() + 9);
	wkt.assign("POLYGON(").append(str_polygon).push_back(')');
	return wkt;
}


std::string
Polygon::ConvexPolygon::to_string() const
{
	if (corners.empty()) {
		return std::string(" EMPTY");
	}

	std::string str;
	const size_t size_corner = 40;
	str.reserve(size_corner * corners.size());
	str.push_back('(');
	char result[size_corner];
	for (const auto& corner : corners) {
		const auto geodetic = corner.toGeodetic();
		snprintf(result, size_corner, "%.7f %.7f %.7f, ",
			std::get<1>(geodetic),
			std::get<0>(geodetic),
			std::get<2>(geodetic)
		);
		str.append(result);
	}
	str.pop_back();
	str.back() = ')';
	return str;
}


std::vector<std::string>
Polygon::ConvexPolygon::getTrixels(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	trixel_data data(partials, HTM_MAX_LEVEL);
	error = error * radius;
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
Polygon::ConvexPolygon::getRanges(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	range_data data(partials, HTM_MAX_LEVEL);
	error = error * radius;
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


std::vector<Cartesian>
Polygon::ConvexPolygon::getCentroids() const
{
	return std::vector<Cartesian>({ centroid });
}


void
Polygon::simplify()
{
	if (!simplified && convexpolygons.size() > 1) {
		// Sort convexpolygons.
		std::sort(convexpolygons.begin(), convexpolygons.end(), std::less<>());

		// Deleting redundant convexpolygons.
		for (auto it = convexpolygons.begin(); it != convexpolygons.end(); ) {
			auto n_it = it + 1;
			if (n_it != convexpolygons.end() && *it == *n_it) {
				n_it = convexpolygons.erase(n_it);
				int cont = 1;
				while (n_it != convexpolygons.end() && *it == *n_it) {
					n_it = convexpolygons.erase(n_it);
					++cont;
				}
				if ((cont % 2) != 0) {
					it = convexpolygons.erase(it);
				} else {
					++it;
				}
			} else {
				++it;
			}
		}

		simplified = true;
	}
}


std::string
Polygon::toWKT() const
{
	std::string wkt("POLYGON");
	wkt.append(to_string());
	return wkt;
}


std::string
Polygon::to_string() const
{
	if (convexpolygons.empty()) {
		return " EMPTY";
	}

	std::string str("(");
	for (const auto& convexpolygon : convexpolygons) {
		const auto str_convexpolygon = convexpolygon.to_string();
		str.reserve(str.length() + str_convexpolygon.length() + 2);
		str.append(str_convexpolygon).append(", ");
	}
	str.pop_back();
	str.back() = ')';
	return str;
}


std::vector<std::string>
Polygon::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	for (const auto& convexpolygon : convexpolygons) {
		trixels = HTM::trixel_exclusive_disjunction(std::move(trixels), convexpolygon.getTrixels(partials, error));
	}

	return trixels;
}


std::vector<range_t>
Polygon::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& convexpolygon : convexpolygons) {
		ranges = HTM::range_exclusive_disjunction(std::move(ranges), convexpolygon.getRanges(partials, error));
	}

	return ranges;
}


std::vector<Cartesian>
Polygon::getCentroids() const
{
	if (convexpolygons.size() == 1) {
		return convexpolygons.back().getCentroids();
	}
	// FIXME: Efficient way for calculate centroids for a Polygon with holes.
	return std::vector<Cartesian>();
}
