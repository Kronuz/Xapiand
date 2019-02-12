/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "geospatialrange.h"

#include <cmath>                                  // for M_PI

#include "generate_terms.h"                       // for GenerateTerms
#include "geospatial/geospatial.h"                // for GeoSpatial
#include "schema.h"                               // for required_spc_t
#include "serialise_list.h"                       // for StringList


Xapian::Query
GeoSpatialRange::getQuery(const required_spc_t& field_spc, const MsgPack& obj)
{
	GeoSpatial geo(obj);

	auto geometry = geo.getGeometry();
	auto ranges = geometry->getRanges(field_spc.flags.partials, field_spc.error);
	auto centroids = geometry->getCentroids();

	if (ranges.empty()) {
		return Xapian::Query();
	}

	auto query = GenerateTerms::geo(ranges, field_spc.accuracy, field_spc.acc_prefix);
	auto gsr = new GeoSpatialRange(field_spc.slot, std::move(ranges), std::move(centroids));
	if (query.empty()) {
		return Xapian::Query(gsr->release());
	}
	return Xapian::Query(Xapian::Query::OP_FILTER, Xapian::Query(gsr->release()), query);
}


double
GeoSpatialRange::calculateWeight() const
{
	const auto centroids = Unserialise::centroids(get_value());

	if (_centroids.empty()) {
		return geo_weight_from_angle(M_PI);
	}

	double min_angle = M_PI;
	for (const auto& centroid : centroids) {
		for (const auto& _centroid : _centroids) {
			double rad_angle = centroid.distance(_centroid);
			if (rad_angle < min_angle) {
				min_angle = rad_angle;
			}
		}
	}
	return geo_weight_from_angle(min_angle);
}


bool
GeoSpatialRange::insideRanges() const
{
	const auto ranges = Unserialise::ranges(get_value());

	if (ranges.empty() || ranges.front().start > _ranges.back().end || ranges.back().end < _ranges.front().start) {
		return false;
	}

	auto it1 = _ranges.begin();
	auto it2 = ranges.begin();
	const auto it1_e = _ranges.end();
	const auto it2_e = ranges.end();

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
		if (insideRanges()) { break; }
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
GeoSpatialRange::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		if (insideRanges()) { break; }
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
	return calculateWeight();
}


GeoSpatialRange*
GeoSpatialRange::clone() const
{
	return new GeoSpatialRange(get_slot(), _ranges, _centroids);
}


std::string
GeoSpatialRange::name() const
{
	return "GeoSpatialRange";
}


std::string
GeoSpatialRange::serialise() const
{
	std::vector<std::string> data = { serialise_length(get_slot()), Serialise::ranges(_ranges) };
	return StringList::serialise(data.begin(), data.end());
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string& serialised, const Xapian::Registry& /*registry*/) const
{
	try {
		StringList data(serialised);

		if (data.size() != 2) {
			throw Xapian::NetworkError("Bad serialised GeoSpatialRange");
		}

		auto it = data.begin();
		const auto slot_ = static_cast<Xapian::valueno>(unserialise_length(*it));
		auto ranges_centroids = Unserialise::ranges_centroids(*++it);
		return new GeoSpatialRange(slot_,
			std::vector<range_t>(std::make_move_iterator(ranges_centroids.first.begin()), std::make_move_iterator(ranges_centroids.first.end())),
			std::vector<Cartesian>(std::make_move_iterator(ranges_centroids.second.begin()), std::make_move_iterator(ranges_centroids.second.end())));
	} catch (const SerialisationError&) {
		throw Xapian::NetworkError("Bad serialised GeoSpatialRange");
	}
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
