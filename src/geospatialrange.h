/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "stl_serialise.h"

#include <xapian.h>

#include <string>
#include <unordered_set>
#include <vector>


// New Match Decider for GeoSpatial value range.
class GeoSpatialRange : public Xapian::ValuePostingSource {
	// Ranges for the search.
	RangeList ranges;
	CartesianUSet centroids;
	Xapian::valueno slot;
	double angle;

	// Calculates the smallest angle between its centroids  and search centroids.
	void calc_angle(const CartesianUSet& centroids_);
	// Calculates if some their values is inside ranges.
	bool insideRanges();

public:
	/* Construct a new match decider which returns only documents with a
	 *  some of their values inside of ranges.
	 *
	 *  @param slot_ The value slot to read values from.
	 *  @param ranges
	*/
	GeoSpatialRange(Xapian::valueno slot_, const RangeList& ranges_, const CartesianUSet& centroids_);

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

	// Call this function for create a new Query based in ranges.
	static Xapian::Query getQuery(Xapian::valueno slot_, const RangeList& ranges_, const CartesianUSet& centroids_);
};
