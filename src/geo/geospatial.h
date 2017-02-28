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

#include "../msgpack.h"
#include "ewkt.h"


constexpr const char GEO_LATITUDE[]   = "_latitude";
constexpr const char GEO_LONGITUDE[]  = "_longitude";
constexpr const char GEO_HEIGHT[]     = "_height";
constexpr const char GEO_RADIUS[]     = "_radius";
constexpr const char GEO_UNITS[]      = "_units";
constexpr const char GEO_SRID[]       = "_srid";


class GeoSpatial {
	struct data_t {
		const MsgPack* lat;
		const MsgPack* lon;
		const MsgPack* height;
		bool has_radius;
		const MsgPack* radius;
		Cartesian::Units units;
		int srid;

		data_t(bool has_radius_)
			: has_radius(has_radius_),
			  units(Cartesian::Units::DEGREES),
			  srid(WGS84) { }
	};

	using dispatch_func = void (GeoSpatial::*)(data_t&, const MsgPack&);

	static const std::unordered_map<std::string, dispatch_func> map_dispatch;

	void process_latitude(data_t& data, const MsgPack& latitude);
	void process_longitude(data_t& data, const MsgPack& longitude);
	void process_height(data_t& data, const MsgPack& height);
	void process_radius(data_t& data, const MsgPack& radius);
	void process_units(data_t& data, const MsgPack& units);
	void process_srid(data_t& data, const MsgPack& srid);

	data_t get_data(const MsgPack& o, bool has_radius=false);

	std::unique_ptr<Point> make_point(const MsgPack& o);
	std::unique_ptr<Circle> make_circle(const MsgPack& o);
	std::unique_ptr<Convex> make_convex(const MsgPack& o);
	std::unique_ptr<MultiPoint> make_multipoint(const MsgPack& o);
	std::unique_ptr<MultiCircle> make_multicircle(const MsgPack& o);

public:
	std::unique_ptr<Geometry> geometry;

	GeoSpatial(const MsgPack& obj);
};
