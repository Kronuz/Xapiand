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

#pragma once

#include "utils.h"
#include "geospatialrange.h"
#include "serialise.h"

#include <regex>


extern const std::regex find_geometry_re;
extern const std::regex find_circle_re;
extern const std::regex find_subpolygon_re;
extern const std::regex find_collection_re;


class EWKT_Parser {
	double error;
	bool partials;
	int SRID;

	std::vector<std::string> parse_circle(const std::string &specification);
	std::vector<std::string> parse_multicircle(const std::string &specification);
	std::vector<std::string> parse_polygon(const std::string &specification, const GeometryType &type);
	std::vector<std::string> parse_multipolygon(const std::string &specification, const GeometryType &type);
	std::vector<std::string> parse_point(const std::string &specification);
	std::vector<std::string> parse_multipoint(const std::string &specification);
	std::vector<std::string> parse_geometry_collection(const std::string &data);
	std::vector<std::string> parse_geometry_intersection(const std::string &data);

public:
	std::vector<Geometry> gv;
	CartesianList centroids;
	std::vector<std::string> trixels;

	EWKT_Parser() = delete;
	// Move constructor
	EWKT_Parser(EWKT_Parser&&) = default;
	// Copy Constructor
	EWKT_Parser(const EWKT_Parser&) = delete;
	EWKT_Parser(const std::string &EWKT, bool partials, double error);


	static std::vector<std::string> stringSplit(const std::string &str, const std::string &delimiter);
	static std::vector<std::string> get_trixels(const std::string &father, size_t depth, const std::string &son);
	static void xor_trixels(std::vector<std::string> &txs1, std::vector<std::string>&& txs2);
	static void or_trixels(std::vector<std::string> &txs1, std::vector<std::string>&& txs2);
	static void and_trixels(std::vector<std::string> &txs1, std::vector<std::string>&& txs2);
	static bool isEWKT(const std::string &str);
	static void getRanges(const std::string &field_value, bool partials, double error, std::vector<range_t> &ranges, CartesianList &centroids);
	static void getRanges(const std::string &field_value, bool partials, double error, std::vector<range_t> &ranges, CartesianList &centroids, std::string& serialise_val);
};
