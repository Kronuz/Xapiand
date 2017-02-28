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

#include "geo.h"

#include "../cast.h"


const std::unordered_map<std::string, Geo::dispatch_func> Geo::map_dispatch({
	{ GEO_LATITUDE,          &Geo::process_latitude    },
	{ GEO_LONGITUDE,         &Geo::process_longitude   },
	{ GEO_HEIGHT,            &Geo::process_height      },
	{ GEO_RADIUS,            &Geo::process_radius      },
	{ GEO_UNITS,             &Geo::process_units       },
	{ GEO_SRID,              &Geo::process_srid        },
});


Geo::Geo(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::STR: {
			EWKT ewkt(obj.as_string());
			geometry = std::move(ewkt.geometry);
			return;
		}
		case MsgPack::Type::MAP: {
			const auto str_key = obj.begin()->as_string();
			switch ((Cast::Hash)xxh64::hash(str_key)) {
				case Cast::Hash::POINT:
					geometry = make_point(obj.at(str_key));
					return;
				case Cast::Hash::CIRCLE:
					geometry = make_circle(obj.at(str_key));
					return;
				case Cast::Hash::CONVEX:
					geometry = make_convex(obj.at(str_key));
					return;
				case Cast::Hash::POLYGON:
					THROW(GeoError, "Not implemented yet");
				case Cast::Hash::CHULL:
					THROW(GeoError, "Not implemented yet");
				case Cast::Hash::MULTIPOINT:
					geometry = make_multipoint(obj.at(str_key));
					return;
				case Cast::Hash::MULTICIRCLE:
					geometry = make_multicircle(obj.at(str_key));
					return;
				case Cast::Hash::MULTIPOLYGON:
				case Cast::Hash::MULTICHULL:
				case Cast::Hash::GEO_COLLECTION:
				case Cast::Hash::GEO_INTERSECTION:
					THROW(GeoError, "Not implemented yet");
				default:
					THROW(GeoError, "Unknown geometry %s", str_key.c_str());
			}
		}
		default:
			THROW(GeoError, "Object must be string or map");
	}
}


inline void
Geo::process_latitude(data_t& data, const MsgPack& latitude) {
	data.lat = &latitude;
}


inline void
Geo::process_longitude(data_t& data, const MsgPack& longitude) {
	data.lon = &longitude;
}


inline void
Geo::process_height(data_t& data, const MsgPack& height) {
	data.height = &height;
}


inline void
Geo::process_radius(data_t& data, const MsgPack& radius) {
	if (!data.has_radius) {
		THROW(GeoError, "%s applies only to %s or %s", GEO_RADIUS, RESERVED_CIRCLE, RESERVED_MULTICIRCLE);
	}
	data.radius = &radius;
}


inline void
Geo::process_units(data_t& data, const MsgPack& units)
{
	try {
		const auto str = units.as_string();
		if (str == "degrees") {
			data.units = Cartesian::Units::DEGREES;
		} else if (str == "radians") {
			data.units = Cartesian::Units::RADIANS;
		} else {
			THROW(GeoError, "%s must be \"degrees\" or \"radians\"", GEO_UNITS);
		}
	} catch (const msgpack::type_error&) {
		THROW(GeoError, "%s must be string (\"degrees\" or \"radians\")", GEO_UNITS);
	}
}


inline void
Geo::process_srid(data_t& data, const MsgPack& srid) {
	try {
		data.srid = srid.as_i64();
		if (!Cartesian::is_SRID_supported(data.srid)) {
			THROW(GeoError, "SRID = %d is not supported", data.srid);
		}
	} catch (const msgpack::type_error&) {
		THROW(GeoError, "%s must be integer", GEO_SRID);
	}
}


Geo::data_t
Geo::get_data(const MsgPack& o, bool has_radius)
{
	data_t data(has_radius);
	static const auto dit_e = map_dispatch.end();
	const auto it_e = o.end();
	for (auto it = o.begin(); it != it_e; ++it) {
		const auto str_key = it->as_string();
		const auto dit = map_dispatch.find(str_key);
		if (dit == dit_e) {
			THROW(GeoError, "%s is a invalid word", str_key.c_str());
		} else {
			(this->*dit->second)(data, it.value());
		}
	}
	return data;
}


std::unique_ptr<Point>
Geo::make_point(const MsgPack& o)
{
	if (o.is_map()) {
		const auto data = get_data(o);
		if (!data.lat || !data.lon) {
			THROW(GeoError, "%s must contain %s and %s", RESERVED_POINT, GEO_LATITUDE, GEO_LONGITUDE);
		}
		try {
			return std::make_unique<Point>(Cartesian(data.lat->as_f64(), data.lon->as_f64(), data.height ? data.height->as_f64() : 0, data.units, data.srid));
		} catch (const msgpack::type_error&) {
			THROW(GeoError, "%s, %s and %s must be numeric", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT);
		}
	} else {
		THROW(GeoError, "%s must be map", RESERVED_POINT);
	}
}


std::unique_ptr<Circle>
Geo::make_circle(const MsgPack& o)
{
	if (o.is_map()) {
		const auto data = get_data(o, true);
		if (!data.lat || !data.lon || !data.radius) {
			THROW(GeoError, "%s must contain %s, %s and %s", RESERVED_CIRCLE, GEO_LATITUDE, GEO_LONGITUDE, GEO_RADIUS);
		}
		try {
			return std::make_unique<Circle>(Cartesian(data.lat->as_f64(), data.lon->as_f64(), data.height ? data.height->as_f64() : 0, data.units, data.srid), data.radius->as_f64());
		} catch (const msgpack::type_error&) {
			THROW(GeoError, "%s, %s, %s and %s must be numeric", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT, GEO_RADIUS);
		}
	} else {
		THROW(GeoError, "%s must be map", RESERVED_CIRCLE);
	}
}


std::unique_ptr<Convex>
Geo::make_convex(const MsgPack& o)
{
	if (o.is_map()) {
		const auto data = get_data(o, true);
		if (!data.lat || !data.lon || !data.radius) {
			THROW(GeoError, "%s must contain %s, %s and %s", RESERVED_CONVEX, GEO_LATITUDE, GEO_LONGITUDE, GEO_RADIUS);
		}
		try {
			if (data.lat->size() == data.lon->size()) {
				if (!data.height) {
					auto convex = std::make_unique<Convex>();
					auto it = data.lon->begin();
					for (const auto& latitude : *data.lat) {
						convex->add(Circle(Cartesian(latitude.as_f64(), it->as_f64(), 0, data.units, data.srid), data.radius->as_f64()));
						++it;
					}
					return convex;
				} else if (data.lat->size() == data.height->size()) {
					auto convex = std::make_unique<Convex>();
					auto it = data.lon->begin();
					auto hit = data.height->begin();
					for (const auto& latitude : *data.lat) {
						convex->add(Circle(Cartesian(latitude.as_f64(), it->as_f64(), hit->as_f64(), data.units, data.srid), data.radius->as_f64()));
						++it;
						++hit;
					}
					return convex;
				}

			}
			THROW(GeoError, "%s, %s and %s must have the same size", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT);
		} catch (const msgpack::type_error&) {
			THROW(GeoError, "%s, %s, %s and %s must be numeric", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT, GEO_RADIUS);
		}
	} else {
		THROW(GeoError, "%s must be map", RESERVED_CONVEX);
	}
}


std::unique_ptr<MultiPoint>
Geo::make_multipoint(const MsgPack& o)
{
	if (o.is_map()) {
		const auto data = get_data(o);
		if (!data.lat || !data.lon) {
			THROW(GeoError, "%s must contain %s and %s", RESERVED_MULTIPOINT, GEO_LATITUDE, GEO_LONGITUDE);
		}
		try {
			if (data.lat->size() == data.lon->size()) {
				if (!data.height) {
					auto multipoint = std::make_unique<MultiPoint>();
					auto it = data.lon->begin();
					for (const auto& latitude : *data.lat) {
						multipoint->add(Point(Cartesian(latitude.as_f64(), it->as_f64(), 0, data.units, data.srid)));
						++it;
					}
					return multipoint;
				} else if (data.lat->size() == data.height->size()) {
					auto multipoint = std::make_unique<MultiPoint>();
					auto it = data.lon->begin();
					auto hit = data.height->begin();
					for (const auto& latitude : *data.lat) {
						multipoint->add(Point(Cartesian(latitude.as_f64(), it->as_f64(), hit->as_f64(), data.units, data.srid)));
						++it;
						++hit;
					}
					return multipoint;
				}

			}
			THROW(GeoError, "%s, %s and %s must have the same size", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT);
		} catch (const msgpack::type_error&) {
			THROW(GeoError, "%s, %s and %s must be numeric", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT);
		}
	} else {
		THROW(GeoError, "%s must be map", RESERVED_MULTIPOINT);
	}
}


std::unique_ptr<MultiCircle>
Geo::make_multicircle(const MsgPack& o)
{
	if (o.is_map()) {
		const auto data = get_data(o, true);
		if (!data.lat || !data.lon || !data.radius) {
			THROW(GeoError, "%s must contain %s, %s and %s", RESERVED_MULTICIRCLE, GEO_LATITUDE, GEO_LONGITUDE, GEO_RADIUS);
		}
		try {
			if (data.lat->size() == data.lon->size()) {
				if (!data.height) {
					auto multicircle = std::make_unique<MultiCircle>();
					auto it = data.lon->begin();
					for (const auto& latitude : *data.lat) {
						multicircle->add(Circle(Cartesian(latitude.as_f64(), it->as_f64(), 0, data.units, data.srid), data.radius->as_f64()));
						++it;
					}
					return multicircle;
				} else if (data.lat->size() == data.height->size()) {
					auto multicircle = std::make_unique<MultiCircle>();
					auto it = data.lon->begin();
					auto hit = data.height->begin();
					for (const auto& latitude : *data.lat) {
						multicircle->add(Circle(Cartesian(latitude.as_f64(), it->as_f64(), hit->as_f64(), data.units, data.srid), data.radius->as_f64()));
						++it;
						++hit;
					}
					return multicircle;
				}

			}
			THROW(GeoError, "%s, %s and %s must have the same size", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT);
		} catch (const msgpack::type_error&) {
			THROW(GeoError, "%s, %s, %s and %s must be numeric", GEO_LATITUDE, GEO_LONGITUDE, GEO_HEIGHT, GEO_RADIUS);
		}
	} else {
		THROW(GeoError, "%s must be map", RESERVED_MULTICIRCLE);
	}
}
