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

#include "string_view.hh"                         // for std::string_view

#include "msgpack.h"                              // for MsgPack
#include "ewkt.h"


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
			: lat(nullptr),
			  lon(nullptr),
			  height(nullptr),
			  has_radius(has_radius_),
			  radius(nullptr),
			  units(Cartesian::Units::DEGREES),
			  srid(WGS84) { }
	};

	std::shared_ptr<Geometry> geometry;

	void process_latitude(data_t& data, const MsgPack& latitude);
	void process_longitude(data_t& data, const MsgPack& longitude);
	void process_height(data_t& data, const MsgPack& height);
	void process_radius(data_t& data, const MsgPack& radius);
	void process_units(data_t& data, const MsgPack& units);
	void process_srid(data_t& data, const MsgPack& srid);

	data_t get_data(const MsgPack& o, bool has_radius=false);
	std::vector<Cartesian> getPoints(const data_t& data, const MsgPack& latitude, const MsgPack& longitude, const MsgPack* height=nullptr);

	Point make_point(const MsgPack& o);
	Circle make_circle(const MsgPack& o);
	Convex make_convex(const MsgPack& o);
	Polygon make_polygon(const MsgPack& o, Geometry::Type type);
	MultiPoint make_multipoint(const MsgPack& o);
	MultiCircle make_multicircle(const MsgPack& o);
	MultiPolygon make_multipolygon(const MsgPack& o);
	Collection make_collection(const MsgPack& o);
	Intersection make_intersection(const MsgPack& o);

public:
	GeoSpatial(const MsgPack& obj);

	std::shared_ptr<Geometry> getGeometry() const {
		geometry->simplify();
		return geometry;
	}
};
