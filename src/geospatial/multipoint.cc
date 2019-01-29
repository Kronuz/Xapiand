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

#include "multipoint.h"

#include <algorithm>


void
MultiPoint::simplify()
{
	if (!simplified) {
		// Sort points.
		std::sort(points.begin(), points.end(), std::less<>());
		// Delete duplicate points
		points.erase(std::unique(points.begin(), points.end()), points.end());

		simplified = true;
	}
}


std::string
MultiPoint::toWKT() const
{
	std::string wkt("MULTIPOINT");
	wkt.append(to_string());
	return wkt;
}


std::string
MultiPoint::to_string() const
{
	if (points.empty()) {
		return " EMPTY";
	}

	std::string str("(");
	for (const auto& point : points) {
		auto str_point = point.to_string();
		str.reserve(str.length() + str_point.length() + 1);
		str.append(str_point).push_back(',');
	}
	str.back() = ')';
	return str;
}


std::vector<std::string>
MultiPoint::getTrixels(bool /*partials*/, double /*error*/) const
{
	std::vector<std::string> trixels;
	trixels.reserve(points.size());
	for (const auto& point : points) {
		trixels.push_back(HTM::getTrixelName(point.p));
	}

	std::sort(trixels.begin(), trixels.end());

	return trixels;
}


std::vector<range_t>
MultiPoint::getRanges(bool /*partials*/, double /*error*/) const
{
	std::vector<range_t> ranges;
	ranges.reserve(points.size());
	for (const auto& point : points) {
		const auto id = HTM::getId(point.p);
		ranges.emplace_back(id, id);
	}

	std::sort(ranges.begin(), ranges.end(), std::less<>());
	HTM::simplifyRanges(ranges);

	return ranges;
}


std::vector<Cartesian>
MultiPoint::getCentroids() const
{
	std::vector<Cartesian> centroids;
	centroids.reserve(points.size());
	for (const auto& point : points) {
		centroids.push_back(point.getCartesian());
	}

	return centroids;
}
