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

#pragma once

#include <cmath>                                  // for M_PI
#include <string>                                 // for std::string

#include "geospatial/geometry.h"                  // for M_PER_RADIUS_EARTH
#include "msgpack.h"                              // for MsgPack
#include "xapian.h"                               // for Xapian::*


struct required_spc_t;


static inline constexpr double geo_weight_from_angle(double angle) {
	return (M_PI - angle) * M_PER_RADIUS_EARTH;
}


// New Match Decider for GeoSpatial value range.
class GeoSpatialRange : public Xapian::ValuePostingSource {
	// Ranges for the search.
	std::vector<range_t> ranges;
	std::vector<Cartesian> centroids;

	/*
	 * Calculates the smallest angle between its centroids and search centroids.
	 */
	double calculateWeight() const;

	/*
	 * Calculates if some their values is inside ranges.
	 */
	bool insideRanges() const;

public:
	/* Construct a new match decider which returns only documents with a
	 *  some of their values inside of ranges.
	 *
	 *  @param slot_ The value slot to read values from.
	 *  @param ranges
	*/
	template <typename R, typename C, typename = std::enable_if_t<std::is_same<std::vector<range_t>, std::decay_t<R>>::value && std::is_same<std::vector<Cartesian>, std::decay_t<C>>::value>>
	GeoSpatialRange(Xapian::valueno slot_, R&& ranges_, C&& centroids_)
		: Xapian::ValuePostingSource(slot_),
		  ranges(std::forward<R>(ranges_)),
		  centroids(std::forward<C>(centroids_)) {
		set_maxweight(geo_weight_from_angle(0.0));
	}

	// Call this function for create a new Geo Spatial Query.
	static Xapian::Query getQuery(const required_spc_t& field_spc, const MsgPack& obj);

	void next(double min_wt) override;
	void skip_to(Xapian::docid min_docid, double min_wt) override;
	bool check(Xapian::docid min_docid, double min_wt) override;
	double get_weight() const override;
	GeoSpatialRange* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	GeoSpatialRange* unserialise_with_registry(const std::string& serialised, const Xapian::Registry&) const override;
	void init(const Xapian::Database& db_) override;
	std::string get_description() const override;
};
