/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "polygon.h"


class MultiPolygon : public Geometry {
	std::vector<Polygon> polygons;
	bool simplified;

public:
	MultiPolygon()
		: Geometry(Type::MULTIPOLYGON),
		  simplified(true) { }

	MultiPolygon(MultiPolygon&& multipolygon) noexcept
		: Geometry(std::move(multipolygon)),
		  polygons(std::move(multipolygon.polygons)),
		  simplified(std::move(multipolygon.simplified)) { }

	MultiPolygon(const MultiPolygon& multipolygon)
		: Geometry(multipolygon),
		  polygons(multipolygon.polygons),
		  simplified(multipolygon.simplified) { }

	~MultiPolygon() = default;

	MultiPolygon& operator=(MultiPolygon&& multipolygon) noexcept {
		Geometry::operator=(std::move(multipolygon));
		polygons = std::move(multipolygon.polygons);
		simplified = std::move(multipolygon.simplified);
		return *this;
	}

	MultiPolygon& operator=(const MultiPolygon& multipolygon) {
		Geometry::operator=(multipolygon);
		polygons = multipolygon.polygons;
		simplified = multipolygon.simplified;
		return *this;
	}

	void add(const Polygon& polygon) {
		polygons.push_back(polygon);
		simplified = false;
	}

	void add(Polygon&& polygon) {
		polygons.push_back(std::move(polygon));
		simplified = false;
	}

	void add(const MultiPolygon& multipolygon) {
		polygons.reserve(polygons.size() + multipolygon.polygons.size());
		for (const auto& polygon : multipolygon.polygons) {
			polygons.push_back(polygon);
		}
		simplified = false;
	}

	void add(MultiPolygon&& multipolygon) {
		polygons.reserve(polygons.size() + multipolygon.polygons.size());
		for (auto& polygon : multipolygon.polygons) {
			polygons.push_back(std::move(polygon));
		}
		simplified = false;
	}

	void reserve(size_t new_cap) {
		polygons.reserve(new_cap);
	}

	bool empty() const noexcept {
		return polygons.empty();
	}

	const std::vector<Polygon>& getPolygons() const noexcept {
		return polygons;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
