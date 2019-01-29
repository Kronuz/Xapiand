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

#include "multiconvex.h"

#include <algorithm>
#include <cmath>


void
MultiConvex::simplify()
{
	if (!simplified) {
		// Simplify and sort convexs.
		std::sort(convexs.begin(), convexs.end(), [](Convex& c1, Convex& c2) {
			c1.simplify();
			c2.simplify();
			return c1 < c2;
		});

		// Deleting redundant and empty convexs.
		for (auto it = convexs.begin(); it != convexs.end(); ) {
			if (it->empty()) {
				it = convexs.erase(it);
			} else {
				auto n_it = it + 1;
				if (n_it != convexs.end() && *it == *n_it) {
					it = convexs.erase(it);
				} else {
					++it;
				}
			}
		}

		simplified = true;
	}
}


std::string
MultiConvex::toWKT() const
{
	std::string wkt("MULTICONVEX");
	wkt.append(to_string());
	return wkt;
}


std::string
MultiConvex::to_string() const
{
	if (convexs.empty()) {
		return std::string(" EMPTY");
	}

	std::string str("(");
	for (const auto& convex : convexs) {
		auto str_convex = convex.to_string();
		str.reserve(str.length() + str_convex.length() + 1);
		str.append(str_convex).push_back(',');
	}
	str.back() = ')';
	return str;
}


std::vector<std::string>
MultiConvex::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	for (const auto& convex : convexs) {
		trixels = HTM::trixel_union(std::move(trixels), convex.getTrixels(partials, error));
	}

	return trixels;
}


std::vector<range_t>
MultiConvex::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& convex : convexs) {
		ranges = HTM::range_union(std::move(ranges), convex.getRanges(partials, error));
	}

	return ranges;
}


std::vector<Cartesian>
MultiConvex::getCentroids() const
{
	// FIXME: Efficient way for calculate centroids for a MultiConvex.
	return std::vector<Cartesian>();
}
