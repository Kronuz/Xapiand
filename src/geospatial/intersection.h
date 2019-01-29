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

#include "geometry.h"


class Intersection : public Geometry {
	std::vector<std::shared_ptr<Geometry>> geometries;

public:
	Intersection()
		: Geometry(Type::INTERSECTION) { }

	Intersection(Intersection&& intersection) noexcept
		: Geometry(std::move(intersection)),
		  geometries(std::move(intersection.geometries)) { }

	Intersection(const Intersection& intersection)
		: Geometry(intersection),
		  geometries(intersection.geometries) { }

	~Intersection() = default;

	Intersection& operator=(Intersection&& intersection) noexcept {
		Geometry::operator=(std::move(intersection));
		geometries = std::move(intersection.geometries);
		return *this;
	}

	Intersection& operator=(const Intersection& intersection) {
		Geometry::operator=(intersection);
		geometries = intersection.geometries;
		return *this;
	}

	void add(const std::shared_ptr<Geometry>& geometry) {
		geometries.push_back(geometry);
	}

	bool empty() const noexcept {
		return geometries.empty();
	}

	void reserve(size_t new_cap) {
		geometries.reserve(new_cap);
	}

	const std::vector<std::shared_ptr<Geometry>>& getGeometries() const noexcept {
		return geometries;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
