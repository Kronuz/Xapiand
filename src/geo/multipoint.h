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

#include "point.h"


class MultiPoint : public Geometry {
	std::vector<Point> points;

public:
	MultiPoint()
		: Geometry(Type::MULTIPOINT) { }

	MultiPoint(MultiPoint&& multipoint) noexcept
		: Geometry(std::move(multipoint)),
		  points(std::move(multipoint.points)) { }

	MultiPoint(const MultiPoint& multipoint)
		: Geometry(multipoint),
		  points(multipoint.points) { }

	~MultiPoint() = default;

	MultiPoint& operator=(MultiPoint&& multipoint) noexcept {
		Geometry::operator=(std::move(multipoint));
		points = std::move(multipoint.points);
		return *this;
	}

	MultiPoint& operator=(const MultiPoint& multipoint) {
		Geometry::operator=(multipoint);
		points = multipoint.points;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Point, std::decay_t<T>>::value>>
	void add(T&& point) {
		points.push_back(std::forward<T>(point));
	}

	const std::vector<Point>& getPoints() const noexcept {
		return points;
	}

	void simplify();

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
};
