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

#include "geospatial.h"

#include "cast.h"                                 // for Cast
#include "hashes.hh"                              // for fnv1ah32
#include "repr.hh"                                // for repr
#include "reserved.h"                             // for RESERVED_*


GeoSpatial::GeoSpatial(const MsgPack& obj)
{
	switch (obj.getType()) {
		case MsgPack::Type::STR: {
			EWKT ewkt(obj.str_view());
			geometry = ewkt.getGeometry();
			return;
		}
		case MsgPack::Type::MAP: {
			auto it = obj.begin();
			const auto str_key = it->str();
			switch (Cast::getHash(str_key)) {
				case Cast::Hash::EWKT: {
					try {
						EWKT ewkt(it.value().str_view());
						geometry = ewkt.getGeometry();
						return;
					} catch (const msgpack::type_error&) {
						THROW(GeoSpatialError, "{} must be string", RESERVED_EWKT);
					}
				}
				case Cast::Hash::POINT:
					geometry = std::make_unique<Point>(make_point(it.value()));
					return;
				case Cast::Hash::CIRCLE:
					geometry = std::make_unique<Circle>(make_circle(it.value()));
					return;
				case Cast::Hash::CONVEX:
					geometry = std::make_unique<Convex>(make_convex(it.value()));
					return;
				case Cast::Hash::POLYGON:
					geometry = std::make_unique<Polygon>(make_polygon(it.value(), Geometry::Type::POLYGON));
					return;
				case Cast::Hash::CHULL:
					geometry = std::make_unique<Polygon>(make_polygon(it.value(), Geometry::Type::CHULL));
					return;
				case Cast::Hash::MULTIPOINT:
					geometry = std::make_unique<MultiPoint>(make_multipoint(it.value()));
					return;
				case Cast::Hash::MULTICIRCLE:
					geometry = std::make_unique<MultiCircle>(make_multicircle(it.value()));
					return;
				case Cast::Hash::MULTIPOLYGON:
					geometry = std::make_unique<MultiPolygon>(make_multipolygon(it.value()));
					return;
				case Cast::Hash::GEO_COLLECTION:
					geometry = std::make_unique<Collection>(make_collection(it.value()));
					return;
				case Cast::Hash::GEO_INTERSECTION:
					geometry = std::make_unique<Intersection>(make_intersection(it.value()));
					return;
				default:
					THROW(GeoSpatialError, "Unknown geometry {}", str_key);
			}
		}
		default:
			THROW(GeoSpatialError, "Object must be string or map");
	}
}


inline void
GeoSpatial::process_latitude(data_t& data, const MsgPack& latitude) {
	data.lat = &latitude;
}


inline void
GeoSpatial::process_longitude(data_t& data, const MsgPack& longitude) {
	data.lon = &longitude;
}


inline void
GeoSpatial::process_height(data_t& data, const MsgPack& height) {
	data.height = &height;
}


inline void
GeoSpatial::process_radius(data_t& data, const MsgPack& radius) {
	if (!data.has_radius) {
		THROW(GeoSpatialError, "{} applies only to {} or {}", RESERVED_GEO_RADIUS, RESERVED_CIRCLE, RESERVED_MULTICIRCLE);
	}
	data.radius = &radius;
}


inline void
GeoSpatial::process_units(data_t& data, const MsgPack& units)
{
	try {
		const auto str = units.str_view();
		if (str == "degrees") {
			data.units = Cartesian::Units::DEGREES;
		} else if (str == "radians") {
			data.units = Cartesian::Units::RADIANS;
		} else {
			THROW(GeoSpatialError, "{} must be \"degrees\" or \"radians\"", RESERVED_GEO_UNITS);
		}
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{} must be string (\"degrees\" or \"radians\")", RESERVED_GEO_UNITS);
	}
}


inline void
GeoSpatial::process_srid(data_t& data, const MsgPack& srid) {
	try {
		data.srid = srid.i64();
		if (!Cartesian::is_SRID_supported(data.srid)) {
			THROW(GeoSpatialError, "SRID = {} is not supported", data.srid);
		}
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{} must be integer", RESERVED_GEO_SRID);
	}
}


GeoSpatial::data_t
GeoSpatial::get_data(const MsgPack& o, bool has_radius)
{
	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_GEO_LATITUDE),
		hh(RESERVED_GEO_LAT),
		hh(RESERVED_GEO_LONGITUDE),
		hh(RESERVED_GEO_LNG),
		hh(RESERVED_GEO_HEIGHT),
		hh(RESERVED_GEO_RADIUS),
		hh(RESERVED_GEO_UNITS),
		hh(RESERVED_GEO_SRID),
	});

	data_t data(has_radius);

	const auto it_e = o.end();
	for (auto it = o.begin(); it != it_e; ++it) {
		auto& value = it.value();
		const auto str_key = it->str_view();
		switch (_.fhh(str_key)) {
			case _.fhh(RESERVED_GEO_LATITUDE):
			case _.fhh(RESERVED_GEO_LAT):
				process_latitude(data, value);
				break;
			case _.fhh(RESERVED_GEO_LONGITUDE):
			case _.fhh(RESERVED_GEO_LNG):
				process_longitude(data, value);
				break;
			case _.fhh(RESERVED_GEO_HEIGHT):
				process_height(data, value);
				break;
			case _.fhh(RESERVED_GEO_RADIUS):
				process_radius(data, value);
				break;
			case _.fhh(RESERVED_GEO_UNITS):
				process_units(data, value);
				break;
			case _.fhh(RESERVED_GEO_SRID):
				process_srid(data, value);
				break;
			default:
				THROW(GeoSpatialError, "{} is a invalid word", repr(str_key));
		}
	}
	return data;
}


std::vector<Cartesian>
GeoSpatial::getPoints(const data_t& data, const MsgPack& latitude, const MsgPack& longitude, const MsgPack* height)
{
	try {
		if (data.lat->size() != data.lon->size()) {
			THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
		}
		std::vector<Cartesian> points;
		points.reserve(latitude.size());
		if (height != nullptr) {
			auto it = latitude.begin();
			auto hit = height->begin();
			for (const auto& lon : longitude) {
				points.emplace_back(it->f64(), lon.f64(), hit->f64(), data.units, data.srid);
				++it;
				++hit;
			}
		} else {
			auto it = latitude.begin();
			for (const auto& lon : longitude) {
				points.emplace_back(it->f64(), lon.f64(), 0, data.units, data.srid);
				++it;
			}
		}
		return points;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {} and {} must be array of numbers or nested array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
	}
}


Point
GeoSpatial::make_point(const MsgPack& o)
{

	switch (o.getType()) {
		case MsgPack::Type::MAP: {
			const auto data = get_data(o);
			if ((data.lat == nullptr) || (data.lon == nullptr)) {
				THROW(GeoSpatialError, "{} must contain {} and {}", RESERVED_POINT, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
			}
			try {
				return Point(Cartesian(data.lat->f64(), data.lon->f64(), data.height != nullptr ? data.height->f64() : 0, data.units, data.srid));
			} catch (const msgpack::type_error&) {
				THROW(GeoSpatialError, "{}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
		}
		case MsgPack::Type::ARRAY:
			if (o.size() != 2) {
				THROW(GeoSpatialError, "Expected array of [latitude, longitude]");
			}
			try{
				return Point(Cartesian(o[0].f64(), o[1].f64(), 0, Cartesian::Units::DEGREES));
			} catch (const msgpack::type_error&) {
				THROW(GeoSpatialError, "{}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
		default:
			THROW(GeoSpatialError, "{} must be map", RESERVED_POINT);
	}
}


Circle
GeoSpatial::make_circle(const MsgPack& o)
{
	if (!o.is_map()) {
		THROW(GeoSpatialError, "{} must be map", RESERVED_CIRCLE);
	}
	const auto data = get_data(o, true);
	if ((data.lat == nullptr) || (data.lon == nullptr) || (data.radius == nullptr)) {
		THROW(GeoSpatialError, "{} must contain {}, {} and {}", RESERVED_CIRCLE, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
	}
	try {
		return Circle(Cartesian(data.lat->f64(), data.lon->f64(), data.height != nullptr ? data.height->f64() : 0, data.units, data.srid), data.radius->f64());
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT, RESERVED_GEO_RADIUS);
	}
}


Convex
GeoSpatial::make_convex(const MsgPack& o)
{
	if (!o.is_map()) {
		THROW(GeoSpatialError, "{} must be map", RESERVED_CONVEX);
	}
	const auto data = get_data(o, true);
	if ((data.lat == nullptr) || (data.lon == nullptr) || (data.radius == nullptr)) {
		THROW(GeoSpatialError, "{} must contain {}, {} and {}", RESERVED_CONVEX, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
	}
	if (data.lat->size() != data.lon->size()) {
		THROW(GeoSpatialError, "{} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	try {
		Convex convex;
		if (data.height != nullptr) {
			if (data.lat->size() != data.height->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
			if (data.lat->size() != data.radius->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
			}
			auto it = data.lon->begin();
			auto hit = data.height->begin();
			auto it_radius = data.radius->begin();
			convex.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				convex.add(Circle(Cartesian(latitude.f64(), it->f64(), hit->f64(), data.units, data.srid), it_radius->f64()));
				++it;
				++hit;
				++it_radius;
			}
		} else {
			if (data.lat->size() != data.radius->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
			}
			auto it = data.lon->begin();
			auto it_radius = data.radius->begin();
			convex.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				convex.add(Circle(Cartesian(latitude.f64(), it->f64(), 0, data.units, data.srid), it_radius->f64()));
				++it;
				++it_radius;
			}
		}
		return convex;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT, RESERVED_GEO_RADIUS);
	}
}


Polygon
GeoSpatial::make_polygon(const MsgPack& o, Geometry::Type type)
{
	if (!o.is_map()) {
		THROW(GeoSpatialError, "{} must be map", RESERVED_POLYGON);
	}
	const auto data = get_data(o);
	if ((data.lat == nullptr) || (data.lon == nullptr)) {
		THROW(GeoSpatialError, "{} must contain {} and {}", RESERVED_POLYGON, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	if (data.lat->size() != data.lon->size()) {
		THROW(GeoSpatialError, "{} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	auto it = data.lon->begin();
	if (it->is_array()) {
		Polygon polygon(type);
		if (data.height != nullptr) {
			if (data.lat->size() != data.height->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
			auto hit = data.height->begin();
			polygon.reserve(data.lat->size());
			for (const auto& lat : *data.lat) {
				polygon.add(getPoints(data, lat, *it, &*hit));
				++it;
				++hit;
			}
		} else {
			polygon.reserve(data.lat->size());
			for (const auto& lat : *data.lat) {
				polygon.add(getPoints(data, lat, *it));
				++it;
			}
		}
		return polygon;
	}
	return Polygon(type, getPoints(data, *data.lat, *data.lon, data.height));
}


MultiPoint
GeoSpatial::make_multipoint(const MsgPack& o)
{
	if (!o.is_map()) {
		THROW(GeoSpatialError, "{} must be map", RESERVED_MULTIPOINT);
	}
	const auto data = get_data(o);
	if ((data.lat == nullptr) || (data.lon == nullptr)) {
		THROW(GeoSpatialError, "{} must contain {} and {}", RESERVED_MULTIPOINT, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	if (data.lat->size() != data.lon->size()) {
		THROW(GeoSpatialError, "{} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	try {
		MultiPoint multipoint;
		if (data.height != nullptr) {
			if (data.lat->size() != data.height->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
			auto it = data.lon->begin();
			auto hit = data.height->begin();
			multipoint.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				multipoint.add(Point(Cartesian(latitude.f64(), it->f64(), hit->f64(), data.units, data.srid)));
				++it;
				++hit;
			}
		} else {
			auto it = data.lon->begin();
			multipoint.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				multipoint.add(Point(Cartesian(latitude.f64(), it->f64(), 0, data.units, data.srid)));
				++it;
			}
		}
		return multipoint;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
	}
}


MultiCircle
GeoSpatial::make_multicircle(const MsgPack& o)
{
	if (!o.is_map()) {
		THROW(GeoSpatialError, "{} must be map", RESERVED_MULTICIRCLE);
	}
	const auto data = get_data(o, true);
	if ((data.lat == nullptr) || (data.lon == nullptr) || (data.radius == nullptr)) {
		THROW(GeoSpatialError, "{} must contain {}, {} and {}", RESERVED_MULTICIRCLE, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
	}
	if (data.lat->size() != data.lon->size()) {
		THROW(GeoSpatialError, "{} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
	}
	try {
		MultiCircle multicircle;
		if (data.height != nullptr) {
			if (data.lat->size() != data.height->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
			}
			auto it = data.lon->begin();
			auto hit = data.height->begin();
			multicircle.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				multicircle.add(Circle(Cartesian(latitude.f64(), it->f64(), hit->f64(), data.units, data.srid), data.radius->f64()));
				++it;
				++hit;
			}
		} else {
			auto it = data.lon->begin();
			multicircle.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				multicircle.add(Circle(Cartesian(latitude.f64(), it->f64(), 0, data.units, data.srid), data.radius->f64()));
				++it;
			}
		}
		return multicircle;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT, RESERVED_GEO_RADIUS);
	}
}


MultiPolygon
GeoSpatial::make_multipolygon(const MsgPack& o)
{
	switch (o.getType()) {
		case MsgPack::Type::MAP: {
			MultiPolygon multipolygon;
			multipolygon.reserve(o.size());
			const auto it_e = o.end();
			for (auto it = o.begin(); it != it_e; ++it) {
				const auto str_key = it->str();
				switch (Cast::getHash(str_key)) {
					case Cast::Hash::POLYGON:
						multipolygon.add(make_polygon(it.value(), Geometry::Type::POLYGON));
						break;
					case Cast::Hash::CHULL:
						multipolygon.add(make_polygon(it.value(), Geometry::Type::CHULL));
						break;
					default:
						THROW(GeoSpatialError, "{} must be a map only with {} and {}", RESERVED_MULTIPOLYGON, RESERVED_POLYGON, RESERVED_CHULL);
				}
			}
			return multipolygon;
		}
		case MsgPack::Type::ARRAY: {
			const auto data = get_data(o);
			if ((data.lat == nullptr) || (data.lon == nullptr)) {
				THROW(GeoSpatialError, "{} must contain {} and {}", RESERVED_MULTIPOLYGON, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
			}
			if (data.lat->size() != data.lon->size()) {
				THROW(GeoSpatialError, "{} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
			}
			MultiPolygon multipolygon;
			multipolygon.reserve(data.lat->size());
			if (data.height != nullptr) {
				if (data.lat->size() != data.height->size()) {
					THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_HEIGHT);
				}
				auto m_it = data.lon->begin();
				auto m_hit = data.height->begin();
				for (const auto& m_lat : *data.lat) {
					if (m_lat.is_array()) {
						Polygon polygon(Geometry::Type::POLYGON);
						polygon.reserve(m_lat.size());
						auto it = m_it->begin();
						auto hit = m_hit->begin();
						for (const auto& lat : m_lat) {
							polygon.add(getPoints(data, lat, *it, &*hit));
							++it;
							++hit;
						}
						multipolygon.add(std::move(polygon));
					} else {
						multipolygon.add(Polygon(Geometry::Type::POLYGON, getPoints(data, m_lat, *m_it, &*m_hit)));
					}
					++m_it;
					++m_hit;
				}
			} else {
				auto m_it = data.lon->begin();
				for (const auto& m_lat : *data.lat) {
					if (m_lat.is_array()) {
						Polygon polygon(Geometry::Type::POLYGON);
						polygon.reserve(m_lat.size());
						auto it = m_it->begin();
						for (const auto& lat : m_lat) {
							polygon.add(getPoints(data, lat, *it));
							++it;
						}
						multipolygon.add(std::move(polygon));
					} else {
						multipolygon.add(Polygon(Geometry::Type::POLYGON, getPoints(data, m_lat, *m_it)));
					}
					++m_it;
				}
			}
			return multipolygon;
		}
		default: {
			THROW(GeoSpatialError, "{} must be map or nested array of numbers", RESERVED_MULTIPOLYGON);
		}
	}
}


Collection
GeoSpatial::make_collection(const MsgPack& o)
{
	if (o.is_map()) {
		Collection collection;
		const auto it_e = o.end();
		for (auto it = o.begin(); it != it_e; ++it) {
			const auto str_key = it->str();
			switch (Cast::getHash(str_key)) {
				case Cast::Hash::POINT:
					collection.add_point(make_point(it.value()));
					break;
				case Cast::Hash::CIRCLE:
					collection.add_circle(make_circle(it.value()));
					break;
				case Cast::Hash::CONVEX:
					collection.add_convex(make_convex(it.value()));
					break;
				case Cast::Hash::POLYGON:
					collection.add_polygon(make_polygon(it.value(), Geometry::Type::POLYGON));
					break;
				case Cast::Hash::CHULL:
					collection.add_polygon(make_polygon(it.value(), Geometry::Type::CHULL));
					break;
				case Cast::Hash::MULTIPOINT:
					collection.add_multipoint(make_multipoint(it.value()));
					break;
				case Cast::Hash::MULTICIRCLE:
					collection.add_multicircle(make_multicircle(it.value()));
					break;
				case Cast::Hash::MULTIPOLYGON:
					collection.add_multipolygon(make_multipolygon(it.value()));
					break;
				case Cast::Hash::GEO_COLLECTION:
					collection.add(make_collection(it.value()));
					break;
				case Cast::Hash::GEO_INTERSECTION:
					collection.add_intersection(make_intersection(it.value()));
					break;
				default:
					THROW(GeoSpatialError, "Unknown geometry {}", str_key);
			}
		}
		return collection;
	}
	THROW(GeoSpatialError, "{} must be map", RESERVED_GEO_COLLECTION);
}


Intersection
GeoSpatial::make_intersection(const MsgPack& o)
{
	if (o.is_map()) {
		Intersection intersection;
		intersection.reserve(o.size());
		const auto it_e = o.end();
		for (auto it = o.begin(); it != it_e; ++it) {
			const auto str_key = it->str();
			switch (Cast::getHash(str_key)) {
				case Cast::Hash::POINT:
					intersection.add(std::make_shared<Point>(make_point(it.value())));
					break;
				case Cast::Hash::CIRCLE:
					intersection.add(std::make_shared<Circle>(make_circle(it.value())));
					break;
				case Cast::Hash::CONVEX:
					intersection.add(std::make_shared<Convex>(make_convex(it.value())));
					break;
				case Cast::Hash::POLYGON:
					intersection.add(std::make_shared<Polygon>(make_polygon(it.value(), Geometry::Type::POLYGON)));
					break;
				case Cast::Hash::CHULL:
					intersection.add(std::make_shared<Polygon>(make_polygon(it.value(), Geometry::Type::CHULL)));
					break;
				case Cast::Hash::MULTIPOINT:
					intersection.add(std::make_shared<MultiPoint>(make_multipoint(it.value())));
					break;
				case Cast::Hash::MULTICIRCLE:
					intersection.add(std::make_shared<MultiCircle>(make_multicircle(it.value())));
					break;
				case Cast::Hash::MULTIPOLYGON:
					intersection.add(std::make_shared<MultiPolygon>(make_multipolygon(it.value())));
					break;
				case Cast::Hash::GEO_COLLECTION:
					intersection.add(std::make_shared<Collection>(make_collection(it.value())));
					break;
				case Cast::Hash::GEO_INTERSECTION:
					intersection.add(std::make_shared<Intersection>(make_intersection(it.value())));
					break;
				default:
					THROW(GeoSpatialError, "Unknown geometry {}", str_key);
			}
		}
		return intersection;
	}
	THROW(GeoSpatialError, "{} must be map", RESERVED_GEO_INTERSECTION);
}
