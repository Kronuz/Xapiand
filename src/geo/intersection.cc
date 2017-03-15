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

#include "intersection.h"


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
	std::string wkt("GEOMETRYINTERSECTION ");
	wkt.append(to_string());
	return wkt;
}


std::string
Intersection::to_string() const
{
	if (geometries.empty()) {
		return std::string("EMPTY");
	}

	std::string str(1, '(');
	for (const auto& geometry : geometries) {
		const auto str_geometry = geometry->toWKT();
		str.reserve(str.length() + str_geometry.length() + 2);
		str.append(str_geometry).append(", ");
	}

	str.pop_back();
	str.back() = ')';
	return str;
}


std::vector<std::string>
Intersection::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	for (const auto& geometry : geometries) {
		trixels = HTM::trixel_intersection(std::move(trixels), geometry->getTrixels(partials, error));
		if (trixels.empty()) {
			return trixels;
		}
	}

	return trixels;
}


std::vector<range_t>
Intersection::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& geometry : geometries) {
		ranges = HTM::range_intersection(std::move(ranges), geometry->getRanges(partials, error));
		if (ranges.empty()) {
			return ranges;
		}
	}

	return ranges;
}
