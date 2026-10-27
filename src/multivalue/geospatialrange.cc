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

#include "database/schema.h"                      // for required_spc_t
#include "engine/error.h"                         // for engine::Error
#include "generate_terms.h"                       // for GenerateTerms
#include "geospatial/geospatial.h"                // for GeoSpatial
#include "serialise_list.h"                       // for StringList


engine::Query
GeoSpatialRange::getQuery(const required_spc_t& field_spc, const MsgPack& obj)
{
	GeoSpatial geo(obj);

	auto geometry = geo.getGeometry();
	auto ranges = geometry->getRanges(field_spc.flags.partials, field_spc.error);
	auto centroids = geometry->getCentroids();

	if (ranges.empty()) {
		return engine::Query();
	}

	auto query = GenerateTerms::geo(ranges, field_spc.accuracy, field_spc.acc_prefix);

	if (field_spc.slot != engine::BAD_VALUENO) {
		auto gsr = std::make_unique<GeoSpatialRange>(field_spc.slot, std::move(ranges), std::move(centroids));
		if (query.empty()) {
			return engine::Query::source(std::move(gsr));
		}
		return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(gsr)), query);
	}

	return query;
}


double
GeoSpatialRange::calculateWeight(std::string_view value) const
{
	// A value that does not decode weighs like one with no centroid.
	try {
		const auto centroids = Unserialise::centroids(value);

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
	} catch (const SerialisationError&) {
		return geo_weight_from_angle(M_PI);
	}
}


bool
GeoSpatialRange::insideRanges(std::string_view value) const
{
	// A value that does not decode is not accepted.
	try {
		const auto ranges = Unserialise::ranges(value);

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
	} catch (const SerialisationError&) {
		return false;
	}
}


double
GeoSpatialRange::weight(std::string_view value) const
{
	return calculateWeight(value);
}


std::unique_ptr<engine::PostingSource>
GeoSpatialRange::clone() const
{
	return std::make_unique<GeoSpatialRange>(slot_, _ranges, _centroids);
}


std::string
GeoSpatialRange::name() const
{
	return "GeoSpatialRange";
}


std::string
GeoSpatialRange::serialise() const
{
	std::vector<std::string> data = { serialise_length(slot_), Serialise::ranges_centroids(_ranges, _centroids) };
	return StringList::serialise(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
GeoSpatialRange::unserialise(std::string_view serialised) const
{
	try {
		StringList data(serialised);

		if (data.size() != 2) {
			throw engine::Error(engine::errc::invalid, "Bad serialised GeoSpatialRange");
		}

		auto it = data.begin();
		const auto slot = static_cast<engine::valueno>(unserialise_length(*it));
		auto ranges_centroids = Unserialise::ranges_centroids(*++it);
		return std::make_unique<GeoSpatialRange>(slot,
			std::vector<range_t>(std::make_move_iterator(ranges_centroids.first.begin()), std::make_move_iterator(ranges_centroids.first.end())),
			std::vector<Cartesian>(std::make_move_iterator(ranges_centroids.second.begin()), std::make_move_iterator(ranges_centroids.second.end())));
	} catch (const SerialisationError&) {
		throw engine::Error(engine::errc::invalid, "Bad serialised GeoSpatialRange");
	}
}


std::string
GeoSpatialRange::description() const
{
	std::string result("GeoSpatialRange ");
	result += std::to_string(slot_);
	return result;
}
