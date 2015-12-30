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
CartesianList::unserialise(const std::string& serialised)
{
	for (size_t i = 0; i < serialised.size(); i += SIZE_SERIALISE_CARTESIAN) {
		push_back(Unserialise::cartesian(serialised.substr(i, SIZE_SERIALISE_CARTESIAN)));
	}
}


std::string
CartesianList::serialise() const
{
	std::string serialised;
	for (auto i = begin(); i != end(); ++i) {
		serialised.append(Serialise::cartesian(*i));
	}

	return serialised;
}


void
uInt64List::unserialise(const std::string& serialised)
{
	for (size_t i = 0; i < serialised.size(); i += SIZE_BYTES_ID) {
		push_back(Unserialise::trixel_id(serialised.substr(i, SIZE_BYTES_ID)));
	}
}


std::string
uInt64List::serialise() const
{
	std::string serialised;
	for (auto i = begin(); i != end(); ++i) {
		serialised.append(Serialise::trixel_id(*i));
	}

	return serialised;
}


GeoSpatialRange::GeoSpatialRange(Xapian::valueno slot_, const std::vector<range_t>& ranges_, const CartesianList& centroids_)
	: ValuePostingSource(slot_),
	  slot(slot_)
{
	ranges.reserve(ranges_.size());
	ranges.insert(ranges.end(), ranges_.begin(), ranges_.end());
	centroids.reserve(centroids_.size());
	centroids.insert(centroids.end(), centroids_.begin(), centroids_.end());
	set_maxweight(geo_weight_from_angle(0.0));
}


// Receive start and end did not serialize.
Xapian::Query
GeoSpatialRange::getQuery(Xapian::valueno slot_, const std::vector<range_t>& ranges_, const CartesianList& centroids_)
{
	if (ranges_.empty()){
		return Xapian::Query::MatchNothing;
	}

	// GeoSpatial Range
	GeoSpatialRange gsr(slot_, ranges_, centroids_);
	return Xapian::Query(&gsr);
}


void
GeoSpatialRange::calc_angle(const CartesianList& _centroids) noexcept
{
	angle = M_PI;
	for (auto it = _centroids.begin(); it != _centroids.end(); ++it) {
		double aux = M_PI;
		for (auto itl = centroids.begin(); itl != centroids.end(); ++itl) {
			double rad_angle = acos(*it * *itl);
			if (rad_angle < aux) aux = rad_angle;
		}
		if (aux < angle) angle = aux;
	}
}


bool
GeoSpatialRange::insideRanges() noexcept
{
	StringList list;
	list.unserialise(*value_it);
	uInt64List _ranges;
	CartesianList _centroids;
	for (auto it = list.begin(); it != list.end(); ++it) {
		_ranges.unserialise(*it);
		++it;
		_centroids.unserialise(*it);
	}

	for (auto start = _ranges.begin(); start != _ranges.end(); start += 2) {
		auto end = start + 1;
		for (auto it = ranges.begin(); it != ranges.end(); ++it) {
			if (*start <= it->end && *end >= it->start) {
				calc_angle(_centroids);
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
	values.append(encode_length(aux.size()));
	values.append(aux);
	uInt64List s;
	for (auto it = ranges.begin(); it != ranges.end(); ++it) {
		s.push_back(it->start);
		s.push_back(it->end);
	}
	aux = s.serialise();
	values.append(encode_length(aux.size()));
	values.append(aux);
	aux = centroids.serialise();
	values.append(encode_length(aux.size()));
	values.append(aux);
	serialised.append(encode_length(values.size()));
	serialised.append(values);
	return serialised;
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	StringList data;
	data.unserialise(s);
	uInt64List uInt64_;
	uInt64_.unserialise(data[1]);
	std::vector<range_t> ranges_;
	ranges_.reserve(uInt64_.size() / 2);
	for (auto it = uInt64_.begin(); it != uInt64_.end(); ++it) {
		ranges_.push_back({*it, *++it});
	}
	CartesianList centroids_;
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
