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

#pragma once

#include "polygon.h"


class ConvexHull : public Polygon {
	enum class Direction : uint8_t {
		COLLINEAR,
		CLOCKWISE,
		COUNTERCLOCKWISE,
	};

	// Gets the direction of the three points.
	static Direction get_direction(const Cartesian& a, const Cartesian& b, const Cartesian& c) noexcept;

	// Returns the squared distance between two points
	static double dist(const Cartesian& a, const Cartesian& b) noexcept;

	// Calculates the convex hull of vector of points using Graham Scan Algorithm.
	std::vector<Cartesian> graham_scan(std::vector<Cartesian>&& points) const;

	void process(std::vector<Cartesian>&& points);

public:
	ConvexHull(std::vector<Cartesian>&& points)
		: Polygon(Type::CONVEX_HULL)
	{
		process(std::move(points));
	}

	ConvexHull(ConvexHull&& chull) noexcept
		: Polygon(std::move(chull)) { }

	ConvexHull(const ConvexHull& chull)
		: Polygon(chull) { }

	~ConvexHull() = default;

	ConvexHull& operator=(ConvexHull&& chull) noexcept {
		Polygon::operator=(std::move(chull));
		return *this;
	}

	ConvexHull& operator=(const ConvexHull& chull) {
		Polygon::operator=(chull);
		return *this;
	}

	std::string toWKT() const override;
};
