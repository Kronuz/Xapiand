/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "multivalue.h"
#include "length.h"
#include "serialise.h"

#include <cmath>


static double geo_weight_from_angle(double angle) {
	return (M_PI - angle) * M_PER_RADIUS_EARTH;
}


void
CartesianUSet::unserialise(const std::string& serialised)
{
	for (size_t i = 0; i < serialised.size(); i += SIZE_SERIALISE_CARTESIAN) {
		insert(Unserialise::cartesian(serialised.substr(i, SIZE_SERIALISE_CARTESIAN)));
	}
}


std::string
CartesianUSet::serialise() const
{
	std::string serialised;
	for (const auto& p : *this) {
		serialised.append(Serialise::cartesian(p));
	}

	return serialised;
}


void
RangeList::unserialise(const std::string& serialised)
{
	for (size_t i = 0; i < serialised.size(); i += SIZE_BYTES_ID) {
		push_back({ Unserialise::trixel_id(serialised.substr(i, SIZE_BYTES_ID)), Unserialise::trixel_id(serialised.substr(i += SIZE_BYTES_ID, SIZE_BYTES_ID)) });
	}
}


std::string
RangeList::serialise() const
{
	std::string serialised;
	for (const auto& range : *this) {
		serialised.append(Serialise::trixel_id(range.start));
		serialised.append(Serialise::trixel_id(range.end));
	}

	return serialised;
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
	list.unserialise(*value_it);
	RangeList ranges_;
	CartesianUSet centroids_;
	const auto it_e = list.end();
	for (auto it = list.begin(); it != it_e; ++it) {
		ranges_.unserialise(*it);
		++it;
		centroids_.unserialise(*it);
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
	while (value_it != db.valuestream_end(slot)) {
		if (insideRanges()) break;
		++value_it;
	}
}


void
GeoSpatialRange::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRanges()) break;
		++value_it;
	}
}


bool
GeoSpatialRange::check(Xapian::docid min_docid, double min_wt)
{
	if (!ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (value_it == db.valuestream_end(slot)) {
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
	std::string serialised, values, aux(Xapian::sortable_serialise(slot));
	values.append(serialise_length(aux.size()));
	values.append(aux);
	aux = ranges.serialise();
	values.append(serialise_length(aux.size()));
	values.append(aux);
	aux = centroids.serialise();
	values.append(serialise_length(aux.size()));
	values.append(aux);
	serialised.append(serialise_length(values.size()));
	serialised.append(values);
	return serialised;
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	StringList data;
	data.unserialise(s);
	RangeList ranges_;
	ranges_.unserialise(data[1]);
	CartesianUSet centroids_;
	centroids_.unserialise(data[2]);
	return new GeoSpatialRange(Xapian::sortable_unserialise(data[0]), ranges_, centroids_);
}


void
GeoSpatialRange::init(const Xapian::Database& db_)
{
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	termfreq_min = 0;
}


std::string
GeoSpatialRange::get_description() const
{
	std::string result("GeoSpatialRange ");
	result += std::to_string(slot);
	return result;
}
