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

#include "multipolygon.h"

#include <algorithm>


void
MultiPolygon::simplify()
{
	if (!simplified) {
		// Simplify xorpolygons.
		for (auto& polygon : polygons) {
			polygon.simplify();
		}

		// Sort polygons.
		std::sort(polygons.begin(), polygons.end(), std::less<>());

		// Deleting duplicated and empty polygons.
		for (auto it = polygons.begin(); it != polygons.end(); ) {
			if (it->empty()) {
				it = polygons.erase(it);
			} else {
				auto n_it = it + 1;
				if (n_it != polygons.end() && *it == *n_it) {
					it = polygons.erase(it);
				} else {
					++it;
				}
			}
		}

		simplified = true;
	}
}


std::string
MultiPolygon::toWKT() const
{
	std::string wkt("MULTIPOLYGON");
	wkt.append(to_string());
	return wkt;
}


std::string
MultiPolygon::to_string() const
{
	if (polygons.empty()) {
		return " EMPTY";
	}

	std::string str("(");
	for (const auto& polygon : polygons) {
		const auto str_polygon = polygon.to_string();
		str.reserve(str.length() + str_polygon.length() + 1);
		str.append(str_polygon).push_back(',');
	}
	str.back() = ')';
	return str;
}



std::vector<std::string>
MultiPolygon::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	for (const auto& polygon : polygons) {
		trixels = HTM::trixel_union(std::move(trixels), polygon.getTrixels(partials, error));
	}

	return trixels;
}


std::vector<range_t>
MultiPolygon::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& polygon : polygons) {
		ranges = HTM::range_union(std::move(ranges), polygon.getRanges(partials, error));
	}

	return ranges;
}


std::vector<Cartesian>
MultiPolygon::getCentroids() const
{
	std::vector<Cartesian> centroids;
	for (const auto& polygon : polygons) {
		const auto& convex_polygons = polygon.getConvexPolygons();
		if (convex_polygons.size() == 1) {
			centroids.push_back(convex_polygons.back().getCentroid());
		} else {
			// FIXME: Efficient way for calculate centroids for a Polygon with holes.
		}
	}

	return centroids;
}
