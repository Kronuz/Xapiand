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

#include "multicircle.h"

#include <algorithm>
#include <cmath>


void
MultiCircle::simplify()
{
	if (!simplified) {
		// Sort circles.
		std::sort(circles.begin(), circles.end(), std::greater<>());

		// Deleting redundant circles.
		for (auto it = circles.begin(); it != circles.end(); ++it) {
			for (auto it_n = it + 1; it_n != circles.end(); ) {
				auto gamma = std::acos(it->constraint.center * it_n->constraint.center);
				if ((it->constraint.arcangle - it_n->constraint.arcangle) >= gamma) {
					it_n = circles.erase(it_n);
				} else {
					++it_n;
				}
			}
		}

		simplified = true;
	}
}


std::string
MultiCircle::toWKT() const
{
	std::string wkt("MULTICIRCLE");
	wkt.append(to_string());
	return wkt;
}


std::string
MultiCircle::to_string() const
{
	if (circles.empty()) {
		return std::string(" EMPTY");
	}

	std::string str("(");
	for (const auto& circle : circles) {
		auto str_circle = circle.to_string();
		str.reserve(str.length() + str_circle.length() + 1);
		str.append(str_circle).push_back(',');
	}
	str.back() = ')';
	return str;
}


std::vector<std::string>
MultiCircle::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	for (const auto& circle : circles) {
		trixels = HTM::trixel_union(std::move(trixels), circle.getTrixels(partials, error));
	}

	return trixels;
}


std::vector<range_t>
MultiCircle::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& circle : circles) {
		ranges = HTM::range_union(std::move(ranges), circle.getRanges(partials, error));
	}

	return ranges;
}


std::vector<Cartesian>
MultiCircle::getCentroids() const
{
	std::vector<Cartesian> centroids;
	centroids.reserve(circles.size());
	for (const auto& circle : circles) {
		centroids.push_back(circle.getConstraint().center);
	}

	return centroids;
}
