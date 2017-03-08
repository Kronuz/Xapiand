/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include <memory>              // for default_delete, unique_ptr
#include <regex>               // for regex
#include <unordered_map>       // for regex

#include "collection.h"
#include "intersection.h"


extern const std::regex find_geometry_re;
extern const std::regex find_circle_re;
extern const std::regex parenthesis_list_split_re;
extern const std::regex find_collection_re;


class GeoSpatial;


class EWKT {
	using dispatch_func = void (EWKT::*)(int, const std::string&);

	static const std::unordered_map<std::string, dispatch_func> map_dispatch;

	std::unique_ptr<Geometry> geometry;

	Point _parse_point(int SRID, const std::string& specification);
	Circle _parse_circle(int SRID, const std::string& specification);
	Convex _parse_convex(int SRID, const std::string& specification);
	MultiPoint _parse_multipoint(int SRID, const std::string& specification);
	MultiCircle _parse_multicircle(int SRID, const std::string& specification);

	void parse_point(int SRID, const std::string& specification);
	void parse_circle(int SRID, const std::string& specification);
	void parse_convex(int SRID, const std::string& specification);
	void parse_multipoint(int SRID, const std::string& specification);
	void parse_multicircle(int SRID, const std::string& specification);

	friend class GeoSpatial;

public:
	EWKT(const std::string& str);

	EWKT(EWKT&& ewkt) noexcept
		: geometry(std::move(ewkt.geometry)) { }

	EWKT(const EWKT&) = delete;

	~EWKT() = default;

	EWKT& operator=(EWKT&& ewkt) noexcept {
		geometry = std::move(ewkt.geometry);
		return *this;
	}

	EWKT& operator=(const EWKT&) = delete;

	static bool isEWKT(const std::string& str);

	const std::unique_ptr<Geometry>& getGeometry() const {
		geometry->simplify();
		return geometry;
	}
};
