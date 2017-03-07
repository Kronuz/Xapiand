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

#include "convex_hull.h"


class XorPolygon : public Geometry {
	std::vector<std::shared_ptr<Polygon>> polygons;
	bool simplified;

public:
	XorPolygon()
		: Geometry(Type::XOR_POLYGON),
		  simplified(true) { }

	XorPolygon(XorPolygon&& xor_polygon) noexcept
		: Geometry(std::move(xor_polygon)),
		  polygons(std::move(xor_polygon.polygons)),
		  simplified(std::move(xor_polygon.simplified)) { }

	XorPolygon(const XorPolygon& xor_polygon)
		: Geometry(xor_polygon),
		  polygons(xor_polygon.polygons),
		  simplified(xor_polygon.simplified) { }

	~XorPolygon() = default;

	XorPolygon& operator=(XorPolygon&& xor_polygon) noexcept {
		Geometry::operator=(std::move(xor_polygon));
		polygons = std::move(xor_polygon.polygons);
		simplified = std::move(xor_polygon.simplified);
		return *this;
	}

	XorPolygon& operator=(const XorPolygon& xor_polygon) {
		Geometry::operator=(xor_polygon);
		polygons = xor_polygon.polygons;
		simplified = xor_polygon.simplified;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Polygon, std::decay_t<T>>::value>>
	void add_polygon(T&& polygon) {
		polygons.push_back(std::make_shared<T>(std::forward<T>(polygon)));
		simplified = false;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<ConvexHull, std::decay_t<T>>::value>>
	void add_chull(T&& chull) {
		polygons.push_back(std::make_shared<T>(std::forward<T>(chull)));
		simplified = false;
	}

	const std::vector<std::shared_ptr<Polygon>>& getPolygons() const noexcept {
		return polygons;
	}

	void simplify();

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
};
