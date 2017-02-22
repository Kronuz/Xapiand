/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "geospatialrange.h"

#include <cmath>                          // for M_PI, acos
#include <utility>                        // for pair

#include "serialise.h"                    // for STLRanges, STLString


bool
GeoSpatialRange::insideRanges()
{
	const auto _ranges = Unserialise::ranges(get_value());

	if (_ranges.empty() || ranges.front().start > _ranges.back().end || ranges.back().end < _ranges.front().start) {
		return false;
	}

	auto it1 = ranges.begin();
	auto it2 = _ranges.begin();
	const auto it1_e = ranges.end();
	const auto it2_e = _ranges.end();

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			if (it1->end >= it2->start) {
				return true;
			}
			++it1;
		} else {
			if (it2->end >= it1->start) {
				return true;
			}
			++it2;
		}
	}

	return false;
}


void
GeoSpatialRange::next(double min_wt)
{
	Xapian::ValuePostingSource::next(min_wt);
	while (!at_end()) {
		if (insideRanges()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
GeoSpatialRange::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		if (insideRanges()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


bool
GeoSpatialRange::check(Xapian::docid min_docid, double min_wt)
{
	if (!Xapian::ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (at_end()) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRanges();
}


double
GeoSpatialRange::get_weight() const
{
	return 1.0;
}


GeoSpatialRange*
GeoSpatialRange::clone() const
{
	return new GeoSpatialRange(get_slot(), ranges);
}


std::string
GeoSpatialRange::name() const
{
	return "GeoSpatialRange";
}


std::string
GeoSpatialRange::serialise() const
{
	std::vector<std::string> data = { serialise_length(get_slot()), Serialise::ranges(ranges) };
	return Serialise::STLString(data.begin(), data.end());
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	std::vector<std::string> data;
	Unserialise::STLString(s, std::back_inserter(data));

	const auto slot_ = static_cast<Xapian::valueno>(unserialise_length(data.at(0)));
	return new GeoSpatialRange(slot_, Unserialise::ranges(data.at(1)));
}


void
GeoSpatialRange::init(const Xapian::Database& db_)
{
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	set_termfreq_min(0);
}


std::string
GeoSpatialRange::get_description() const
{
	std::string result("GeoSpatialRange ");
	result += std::to_string(get_slot());
	return result;
}
