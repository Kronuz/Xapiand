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

#include "convex_hull.h"


ConvexHull::Direction
ConvexHull::get_direction(const Cartesian& a, const Cartesian& b, const Cartesian& c) noexcept
{
	double angle = (a ^ b) * c;
	if (angle > DBL_TOLERANCE) {
		return Direction::CLOCKWISE;
	} else if (angle < -DBL_TOLERANCE) {
		return Direction::COUNTERCLOCKWISE;
	} else {
		return Direction::COLLINEAR;
	}
}


double
ConvexHull::dist(const Cartesian& a, const Cartesian& b) noexcept
{
	Cartesian p = a - b;
	return p.x * p.x + p.y * p.y + p.z * p.z;
}


std::vector<Cartesian>
ConvexHull::graham_scan(std::vector<Cartesian>&& points) const
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
				return ((dist(P0, b) > dist(P0, a)) ? true : false);
			case Direction::COUNTERCLOCKWISE:
				return true;
			case Direction::CLOCKWISE:
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
			if (dir != Direction::COUNTERCLOCKWISE) {
				convex_points.pop_back();
				continue;
			} else {
				break;
			}
		}
		convex_points.push_back(std::move(*it));
	}
	// Duplicate first point.
	convex_points.push_back(convex_points.front());

	return convex_points;
}


void
ConvexHull::process(std::vector<Cartesian>&& points)
{
	// The convex is formed in counterclockwise.
	auto convex_points = graham_scan(std::move(points));

	if (convex_points.size() < 3) {
		THROW(GeometryError, "Convex Hull not found");
	}

	// The corners are in clockwise but we need the corners in counterclockwise order and normalize.
	corners.reserve(convex_points.size());

	// Duplicates the last point at the begin.
	convex_points.insert(convex_points.begin(), convex_points.back());

	auto it_last = convex_points.rend() - 1;
	for (auto it = convex_points.rbegin(); it != it_last; ++it) {
		auto center = *it ^ *(it + 1);
		center.normalize();
		constraints.push_back(Constraint(std::move(center)));
		corners.push_back(std::move(*it));
	}
	corners.push_back(std::move(*it_last));

	init();
}
