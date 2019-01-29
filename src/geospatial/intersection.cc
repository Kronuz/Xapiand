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

#include "intersection.h"

#include <algorithm>


void
Intersection::simplify()
{
	std::sort(geometries.begin(), geometries.end(), [](const std::shared_ptr<Geometry>& g1, const std::shared_ptr<Geometry>& g2) {
		return g1->getType() < g2->getType();
	});

	for (auto& geometry : geometries) {
		geometry->simplify();
	}
}


std::string
Intersection::toWKT() const
{
	std::string wkt("GEOMETRYINTERSECTION");
	wkt.append(to_string());
	return wkt;
}


std::string
Intersection::to_string() const
{
	if (geometries.empty()) {
		return std::string(" EMPTY");
	}

	std::string str(1, '(');
	for (const auto& geometry : geometries) {
		const auto str_geometry = geometry->toWKT();
		str.reserve(str.length() + str_geometry.length() + 1);
		str.append(str_geometry).push_back(',');
	}
	str.back() = ')';
	return str;
}


std::vector<std::string>
Intersection::getTrixels(bool partials, double error) const
{
	if (geometries.empty()) {
		return std::vector<std::string>();
	}

	const auto it_e = geometries.end();
	auto it = geometries.begin();
	auto trixels = (*it)->getTrixels(partials, error);
	for (++it; !trixels.empty() && it != it_e; ++it) {
		trixels = HTM::trixel_intersection(std::move(trixels), (*it)->getTrixels(partials, error));
	}

	return trixels;
}


std::vector<range_t>
Intersection::getRanges(bool partials, double error) const
{
	if (geometries.empty()) {
		return std::vector<range_t>();
	}

	const auto it_e = geometries.end();
	auto it = geometries.begin();
	auto ranges = (*it)->getRanges(partials, error);
	for (++it; !ranges.empty() && it != it_e; ++it) {
		ranges = HTM::range_intersection(std::move(ranges), (*it)->getRanges(partials, error));
	}

	return ranges;
}


std::vector<Cartesian>
Intersection::getCentroids() const
{
	// FIXME: Efficient way for calculate centroids for a Intersection.
	return std::vector<Cartesian>();
}
