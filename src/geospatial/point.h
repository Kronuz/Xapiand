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


class MultiPoint;


class Point : public Geometry {
	friend class MultiPoint;

	Cartesian p;

public:
	template <typename T, typename = std::enable_if_t<std::is_same<Cartesian, std::decay_t<T>>::value>>
	explicit Point(T&& point)
		: Geometry(Type::POINT),
		  p(std::forward<T>(point))
	{
		p.normalize();
	}

	Point(Point&& point) noexcept
		: Geometry(std::move(point)),
		  p(std::move(point.p)) { }

	Point(const Point& point)
		: Geometry(point),
		  p(point.p) { }

	~Point() = default;

	Point& operator=(Point&& point) noexcept {
		Geometry::operator=(std::move(point));
		p = std::move(point.p);
		return *this;
	}

	Point& operator=(const Point& point) {
		Geometry::operator=(point);
		p = point.p;
		return *this;
	}

	bool operator<(const Point& point) const noexcept {
		return p < point.p;
	}

	bool operator==(const Point& point) const noexcept {
		return p == point.p;
	}

	const Cartesian& getCartesian() const noexcept {
		return p;
	}

	std::string toWKT() const override {
		std::string wkt("POINT");
		wkt.append(to_string());
		return wkt;
	}

	std::string to_string() const override {
		char result[40];
		const auto geodetic = p.toGeodetic();
		snprintf(result, 40, "(%.7f %.7f %.7f)",
			std::get<1>(geodetic),
			std::get<0>(geodetic),
			std::get<2>(geodetic)
		);
		return std::string(result);
	}

	std::vector<std::string> getTrixels(bool, double) const override {
		return { HTM::getTrixelName(p) };
	}

	std::vector<range_t> getRanges(bool, double) const override {
		auto id = HTM::getId(p);
		return { range_t(id, id) };
	}

	std::vector<Cartesian> getCentroids() const override {
		return std::vector<Cartesian>({ p });
	}
};
