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

#include "convex.h"


class MultiConvex : public Geometry {
	std::vector<Convex> convexs;
	bool simplified;

public:
	MultiConvex()
		: Geometry(Type::MULTICONVEX),
		  simplified(true) { }

	MultiConvex(MultiConvex&& multiconvex) noexcept
		: Geometry(std::move(multiconvex)),
		  convexs(std::move(multiconvex.convexs)),
		  simplified(std::move(multiconvex.simplified)) { }

	MultiConvex(const MultiConvex& multiconvex)
		: Geometry(multiconvex),
		  convexs(multiconvex.convexs),
		  simplified(multiconvex.simplified) { }

	~MultiConvex() = default;

	MultiConvex& operator=(MultiConvex&& multiconvex) noexcept {
		Geometry::operator=(std::move(multiconvex));
		convexs = std::move(multiconvex.convexs);
		simplified = std::move(multiconvex.simplified);
		return *this;
	}

	MultiConvex& operator=(const MultiConvex& multiconvex) {
		Geometry::operator=(multiconvex);
		convexs = multiconvex.convexs;
		simplified = multiconvex.simplified;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Convex, std::decay_t<T>>::value>>
	void add(T&& convex) {
		convexs.push_back(std::forward<T>(convex));
		simplified = false;
	}

	void add(const MultiConvex& multiconvex) {
		convexs.reserve(convexs.size() + multiconvex.convexs.size());
		for (const auto& convex : multiconvex.convexs) {
			convexs.push_back(convex);
		}
		simplified = false;
	}

	void add(MultiConvex&& multiconvex) {
		convexs.reserve(convexs.size() + multiconvex.convexs.size());
		for (auto& convex : multiconvex.convexs) {
			convexs.push_back(std::move(convex));
		}
		simplified = false;
	}

	bool empty() const noexcept {
		return convexs.empty();
	}

	const std::vector<Convex>& getConvexs() const noexcept {
		return convexs;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
