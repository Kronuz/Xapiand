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

#include "collection.h"


void
Collection::simplify()
{
	multipoint.simplify();
	multicircle.simplify();
	multiconvex.simplify();
	multipolygon.simplify();
}


std::string
Collection::toWKT() const
{
	std::string wkt("GEOMETRYCOLLECTION Z ");
	wkt.append(to_string());
	return wkt;
}


std::string
Collection::to_string() const
{
	std::string str;
	if (!multipoint.empty()) {
		const auto str_geometry = multipoint.toWKT();
		str.reserve(str_geometry.length() + 2);
		str.append(str_geometry).append(", ");
	}
	if (!multicircle.empty()) {
		const auto str_geometry = multicircle.toWKT();
		str.reserve(str.length() + str_geometry.length() + 2);
		str.append(str_geometry).append(", ");
	}
	if (!multiconvex.empty()) {
		const auto str_geometry = multiconvex.toWKT();
		str.reserve(str.length() + str_geometry.length() + 2);
		str.append(str_geometry).append(", ");
	}
	if (!multipolygon.empty()) {
		const auto str_geometry = multipolygon.toWKT();
		str.reserve(str.length() + str_geometry.length() + 2);
		str.append(str_geometry).append(", ");
	}

	if (str.empty()) {
		return std::string("EMPTY");
	}

	str.pop_back();
	str.back() = ')';
	return str;
}


std::vector<std::string>
Collection::getTrixels(bool partials, double error) const
{
	std::vector<std::string> trixels;
	trixels = HTM::trixel_union(std::move(trixels), multipoint.getTrixels(partials, error));
	trixels = HTM::trixel_union(std::move(trixels), multicircle.getTrixels(partials, error));
	trixels = HTM::trixel_union(std::move(trixels), multiconvex.getTrixels(partials, error));
	trixels = HTM::trixel_union(std::move(trixels), multipolygon.getTrixels(partials, error));

	return trixels;
}


std::vector<range_t>
Collection::getRanges(bool partials, double error) const
{
	std::vector<range_t> ranges;
	ranges = HTM::range_union(std::move(ranges), multipoint.getRanges(partials, error));
	ranges = HTM::range_union(std::move(ranges), multicircle.getRanges(partials, error));
	ranges = HTM::range_union(std::move(ranges), multiconvex.getRanges(partials, error));
	ranges = HTM::range_union(std::move(ranges), multipolygon.getRanges(partials, error));

	return ranges;
}
