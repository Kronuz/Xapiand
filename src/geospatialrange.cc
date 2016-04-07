/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "length.h"
#include "serialise.h"

#include <cmath>


static double geo_weight_from_angle(double angle) {
	return (M_PI - angle) * M_PER_RADIUS_EARTH;
}


GeoSpatialRange::GeoSpatialRange(Xapian::valueno slot_, const RangeList& ranges_, const CartesianUSet& centroids_)
	: ValuePostingSource(slot_),
	  slot(slot_)
{
	ranges.reserve(ranges_.size());
	ranges.insert(ranges.end(), ranges_.begin(), ranges_.end());
	centroids.reserve(centroids_.size());
	centroids.insert(centroids_.begin(), centroids_.end());
	set_maxweight(geo_weight_from_angle(0.0));
}


// Receive start and end did not serialize.
Xapian::Query
GeoSpatialRange::getQuery(Xapian::valueno slot_, const RangeList& ranges_, const CartesianUSet& centroids_)
{
	if (ranges_.empty()){
		return Xapian::Query::MatchNothing;
	}

	// GeoSpatial Range
	GeoSpatialRange gsr(slot_, ranges_, centroids_);
	return Xapian::Query(&gsr);
}


void
GeoSpatialRange::calc_angle(const CartesianUSet& centroids_)
{
	angle = M_PI;
	for (const auto& centroid_ : centroids_) {
		double aux = M_PI;
		for (const auto& centroid : centroids) {
			double rad_angle = acos(centroid_ * centroid);
			if (rad_angle < aux) aux = rad_angle;
		}
		if (aux < angle) angle = aux;
	}
}


bool
GeoSpatialRange::insideRanges()
{
	StringList list;
	list.unserialise(get_value());
	RangeList ranges_;
	CartesianUSet centroids_;
	for (const auto& value : list) {
		auto unser_value = Unserialise::geo(value);
		ranges_.add_unserialise(unser_value.first);
		centroids_.add_unserialise(unser_value.second);
	}

	for (const auto& range_ : ranges_) {
		for (const auto& range : ranges) {
			if (range_.start <= range.end && range_.end >= range.start) {
				calc_angle(centroids_);
				return true;
			}
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
	if (!ValuePostingSource::check(min_docid, min_wt)) {
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
	return geo_weight_from_angle(angle);
}


GeoSpatialRange*
GeoSpatialRange::clone() const
{
	return new GeoSpatialRange(slot, ranges, centroids);
}


std::string
GeoSpatialRange::name() const
{
	return "GeoSpatialRange";
}


std::string
GeoSpatialRange::serialise() const
{
	return serialise_length(slot) + Serialise::geo(ranges, centroids);
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	const char* pos = s.data();
	const char* end = pos + s.size();
	auto slot_ = static_cast<Xapian::valueno>(unserialise_length(&pos, end, false));
	auto unser_geo = Unserialise::geo(std::string(pos, end - pos));
	RangeList ranges_;
	ranges_.unserialise(unser_geo.first);
	CartesianUSet centroids_;
	centroids_.unserialise(unser_geo.second);
	return new GeoSpatialRange(slot_, ranges_, centroids_);
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
	result += std::to_string(slot);
	return result;
}
