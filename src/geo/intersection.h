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

#include "multicircle.h"
#include "multiconvex.h"
#include "multipoint.h"
#include "multipolygon.h"


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

	template <typename T, typename = std::enable_if_t<std::is_same<Point, std::decay_t<T>>::value>>
	void add_point(T&& point) {
		geometries.push_back(std::make_shared<Point>(std::forward<T>(point)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Circle, std::decay_t<T>>::value>>
	void add_circle(T&& circle) {
		geometries.push_back(std::make_shared<Circle>(std::forward<T>(circle)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Convex, std::decay_t<T>>::value>>
	void add_convex(T&& convex) {
		geometries.push_back(std::make_shared<Convex>(std::forward<T>(convex)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Polygon, std::decay_t<T>>::value>>
	void add_polygon(T&& polygon) {
		geometries.push_back(std::make_shared<Polygon>(std::forward<T>(polygon)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiPoint, std::decay_t<T>>::value>>
	void add_multipoint(T&& multipoint) {
		geometries.push_back(std::make_shared<MultiPoint>(std::forward<T>(multipoint)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiCircle, std::decay_t<T>>::value>>
	void add_multicircle(T&& multicircle) {
		geometries.push_back(std::make_shared<MultiCircle>(std::forward<T>(multicircle)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiConvex, std::decay_t<T>>::value>>
	void add_multiconvex(T&& multiconvex) {
		geometries.push_back(std::make_shared<MultiConvex>(std::forward<T>(multiconvex)));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiPolygon, std::decay_t<T>>::value>>
	void add_multipolygon(T&& multipolygon) {
		geometries.push_back(std::make_shared<MultiPolygon>(std::forward<T>(multipolygon)));
	}

	const std::vector<std::shared_ptr<Geometry>>& getGeometries() const noexcept {
		return geometries;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
};
