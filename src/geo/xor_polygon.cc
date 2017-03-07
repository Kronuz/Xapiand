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

#include "xor_polygon.h"


void
XorPolygon::simplify()
{
	// Sort polygons.
	std::sort(polygons.begin(), polygons.end(), [](const std::shared_ptr<Polygon>& p1, const std::shared_ptr<Polygon>& p2) {
		return p1->getCorners() < p2->getCorners();
	});

	// Deleting redundant polygons.
	for (auto it = polygons.begin(); it != polygons.end(); ) {
		auto n_it = it + 1;
		if (n_it != polygons.end() && (*it)->getCorners() == (*n_it)->getCorners()) {
			n_it = polygons.erase(n_it);
			int cont = 1;
			while (n_it != polygons.end() && (*it)->getCorners() == (*n_it)->getCorners()) {
				n_it = polygons.erase(n_it);
				++cont;
			}
			if (cont % 2) {
				it = polygons.erase(it);
			} else {
				++it;
			}
		} else {
			++it;
		}
	}
}


std::string
XorPolygon::toWKT() const
{
	std::string wkt("POLYGON Z ");
	wkt.append(to_string());
	return wkt;
}


std::string
XorPolygon::to_string() const
{
	if (polygons.empty()) {
		return "()";
	}

	std::string str("(");
	for (const auto& polygon : polygons) {
		const auto str_polygon = polygon->to_string();
		str.reserve(str.length() + str_polygon.length() + 2);
		str.append(str_polygon).append(", ");
	}
	str.pop_back();
	str.back() = ')';
	return str;
}


std::vector<std::string>
XorPolygon::getTrixels(bool partials, double error) const
{
	return HTM::getTrixels(getRanges(partials, error));
}


std::vector<range_t>
XorPolygon::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	for (const auto& polygon : polygons) {
		ranges = HTM::range_exclusive_disjunction(std::move(ranges), polygon->getRanges(partials, error));
	}

	return ranges;
}
