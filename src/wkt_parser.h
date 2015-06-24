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

#ifndef XAPIAND_INCLUDED_WKT_PARSER_H
#define XAPIAND_INCLUDED_WKT_PARSER_H

#include "htm.h"
#include "utils.h"


class EWKT_Parser {
	public:
		std::vector<Geometry> G;
		double error;
		bool partials;
		int SRID;
		std::vector<Geometry> gv;
		std::vector<std::string> trixels;

		static pcre *compiled_find_geometry_re;
		static pcre *compiled_find_circle_re;
		static pcre *compiled_find_subpolygon_re;
		static pcre *compiled_find_polygon_re;
		static pcre *compiled_find_collection_re;

		EWKT_Parser(std::string &EWKT, bool partials, double error);

		std::vector<std::string> stringSplit(const std::string &str, const std::string &delimiter);
		std::vector<std::string> get_trixels(std::string &father, int depth, std::string &son);
		std::vector<std::string> xor_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2);
		std::vector<std::string> or_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2);
		std::vector<std::string> and_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2);

		std::vector<std::string> parse_circle(std::string &specification);
		std::vector<std::string> parse_polygon(std::string &specification);
		std::vector<std::string> parse_multipolygon(std::string &specification);
		std::vector<std::string> parse_point(std::string &specification);
		std::vector<std::string> parse_multipoint(std::string &specification);
		std::vector<std::string> parse_geometry_collection(std::string &data);
		std::vector<std::string> parse_geometry_intersection(std::string &data);
};


#endif /* XAPIAND_INCLUDED_WKT_PARSER_H */