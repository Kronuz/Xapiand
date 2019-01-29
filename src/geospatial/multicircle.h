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

#include "circle.h"


class MultiCircle : public Geometry {
	std::vector<Circle> circles;
	bool simplified;

public:
	MultiCircle()
		: Geometry(Type::MULTICIRCLE),
		  simplified(true) { }

	MultiCircle(MultiCircle&& multicircle) noexcept
		: Geometry(std::move(multicircle)),
		  circles(std::move(multicircle.circles)),
		  simplified(std::move(multicircle.simplified)) { }

	MultiCircle(const MultiCircle& multicircle)
		: Geometry(multicircle),
		  circles(multicircle.circles),
		  simplified(multicircle.simplified) { }

	~MultiCircle() = default;

	MultiCircle& operator=(MultiCircle&& multicircle) noexcept {
		Geometry::operator=(std::move(multicircle));
		circles = std::move(multicircle.circles);
		simplified = std::move(multicircle.simplified);
		return *this;
	}

	MultiCircle& operator=(const MultiCircle& multicircle) {
		Geometry::operator=(multicircle);
		circles = multicircle.circles;
		simplified = multicircle.simplified;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Circle, std::decay_t<T>>::value>>
	void add(T&& circle) {
		circles.push_back(std::forward<T>(circle));
		simplified = false;
	}

	void add(const MultiCircle& multicircle) {
		circles.reserve(circles.size() + multicircle.circles.size());
		for (const auto& circle : multicircle.circles) {
			circles.push_back(circle);
		}
		simplified = false;
	}

	void add(MultiCircle&& multicircle) {
		circles.reserve(circles.size() + multicircle.circles.size());
		for (auto& circle : multicircle.circles) {
			circles.push_back(std::move(circle));
		}
		simplified = false;
	}

	void reserve(size_t new_cap) {
		circles.reserve(new_cap);
	}

	bool empty() const noexcept {
		return circles.empty();
	}

	const std::vector<Circle>& getCircles() const noexcept {
		return circles;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
