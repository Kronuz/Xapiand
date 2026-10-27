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
#include <memory>                                 // for std::unique_ptr
#include <string>                                 // for std::string
#include <string_view>                            // for std::string_view
#include <vector>                                 // for std::vector

#include "geospatial/geometry.h"                  // for M_PER_RADIUS_EARTH
#include "msgpack.h"                              // for MsgPack
#include "engine/callbacks.h"                    // for engine::PostingSource
#include "engine/query.h"                        // for engine::Query
#include "engine/types.h"                        // for engine::valueno


struct required_spc_t;


static inline constexpr double geo_weight_from_angle(double angle) {
	return M_PI - angle;
}


// A value source for GeoSpatial value range: some of the document's ranges overlap the search ranges.
class GeoSpatialRange : public engine::PostingSource {
	engine::valueno slot_ = 0;

	// Ranges for the search.
	std::vector<range_t> _ranges;
	std::vector<Cartesian> _centroids;

	/*
	 * Calculates the smallest angle between its centroids and search centroids.
	 */
	double calculateWeight(std::string_view value) const;

	/*
	 * Calculates if some their values is inside ranges.
	 */
	bool insideRanges(std::string_view value) const;

public:
	GeoSpatialRange() = default;

	/* Construct a new source which returns only documents with a
	 *  some of their values inside of ranges.
	 *
	 *  @param slot The value slot to read values from.
	 *  @param ranges
	*/
	template <typename R, typename C, typename = std::enable_if_t<std::is_same<std::vector<range_t>, std::decay_t<R>>::value && std::is_same<std::vector<Cartesian>, std::decay_t<C>>::value>>
	GeoSpatialRange(engine::valueno slot, R&& ranges, C&& centroids) :
		slot_(slot),
		_ranges(std::forward<R>(ranges)),
		_centroids(std::forward<C>(centroids)) { }

	// Call this function for create a new Geo Spatial Query.
	static engine::Query getQuery(const required_spc_t& field_spc, const MsgPack& obj);

	engine::valueno slot() const override { return slot_; }
	bool accept(std::string_view value) const override { return insideRanges(value); }
	double weight(std::string_view value) const override;
	double max_weight() const override { return geo_weight_from_angle(0.0); }
	std::unique_ptr<engine::PostingSource> clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view serialised) const override;
	std::string description() const override;
};
