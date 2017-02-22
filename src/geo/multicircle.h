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

#include "circle.h"


class MultiCircle : public Geometry {
	std::vector<Circle> circles;

public:
	MultiCircle()
		: Geometry(Type::MULTICIRCLE) { }

	MultiCircle(MultiCircle&& multicircle) noexcept
		: Geometry(std::move(multicircle)),
		  circles(std::move(multicircle.circles)) { }

	MultiCircle(const MultiCircle& multicircle)
		: Geometry(multicircle),
		  circles(multicircle.circles) { }

	~MultiCircle() = default;

	MultiCircle& operator=(MultiCircle&& multicircle) noexcept {
		Geometry::operator=(std::move(multicircle));
		circles = std::move(multicircle.circles);
		return *this;
	}

	MultiCircle& operator=(const MultiCircle& multicircle) {
		Geometry::operator=(multicircle);
		circles = multicircle.circles;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Circle, std::decay_t<T>>::value>>
	void add(T&& circle) {
		circles.push_back(std::forward<T>(circle));
	}

	const std::vector<Circle>& getCircles() const noexcept {
		return circles;
	}

	void simplify();

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
};
