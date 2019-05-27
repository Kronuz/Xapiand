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
#include "reserved/geo.h"                         // for RESERVED_*
#include "reserved/types.h"                       // for RESERVED_*
#include "strict_stox.hh"                         // for strict_stoull


static inline double
distance(std::string_view str)
{
	auto size = str.size();
	std::string_view s1, s2, s3;
	if (size > 3) {
		s1 = str.substr(size - 1, 1);
		s2 = str.substr(size - 2, 2);
		s3 = str.substr(size - 3, 3);
	} else if (size > 2) {
		s1 = str.substr(size - 1, 1);
		s2 = str.substr(size - 2, 2);
	} else if (size > 1) {
		s1 = str.substr(size - 1, 1);
	}

	double mul = 1;
	if (s3 == "nmi") {
		str.remove_suffix(3);
		mul = 1852.0;
	} else if (s2 == "mm") {
		str.remove_suffix(2);
		mul = 1.0 / 1000.0;
	} else if (s2 == "cm") {
		str.remove_suffix(2);
		mul = 1.0 / 100.0;
	} else if (s2 == "km") {
		str.remove_suffix(2);
		mul = 1000.0;
	} else if (s2 == "in") {
		str.remove_suffix(2);
		mul = 1.0 / 39.37;
	} else if (s2 == "ft") {
		str.remove_suffix(2);
		mul = 1.0 / 3.2808;
	} else if (s2 == "yd") {
		str.remove_suffix(2);
		mul = 1.0 / 1.0936;
	} else if (s2 == "mi") {
		str.remove_suffix(2);
		mul = 1609.344;
	} else if (s2 == "NM") {
		str.remove_suffix(2);
		mul = 1852.0;
	} else if (s1 == "m") {
		str.remove_suffix(1);
	}

	int errno_save;
	auto d = strict_stoi(&errno_save, str);
	if (errno_save != 0) {
		THROW(GeoSpatialError, "Object must be a valid metric or imperial distance string");
	}
	return d * mul;
}


static inline double
distance(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::STR:
			return distance(obj.str_view());
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
		case MsgPack::Type::FLOAT:
			return obj.f64();
		default:
			THROW(GeoSpatialError, "Distance object must be string, numeric {}", enum_name(obj.get_type()));
	}
}


GeoSpatial::GeoSpatial(const MsgPack& obj)
{
	switch (obj.get_type()) {
		case MsgPack::Type::STR: {
			EWKT ewkt(obj.str_view());
			geometry = ewkt.getGeometry();
			return;
		}

		case MsgPack::Type::MAP: {
			auto it = obj.begin();
			const auto str_key = it->str();
			switch (Cast::get_hash_type(str_key)) {
				case Cast::HashType::EWKT: {
					try {
						EWKT ewkt(it.value().str_view());
						geometry = ewkt.getGeometry();
						return;
					} catch (const msgpack::type_error&) {
						THROW(GeoSpatialError, "{} must be string", RESERVED_EWKT);
					}
				}
				case Cast::HashType::POINT:
					geometry = std::make_unique<Point>(make_point(it.value()));
					return;
				case Cast::HashType::CIRCLE:
					geometry = std::make_unique<Circle>(make_circle(it.value()));
					return;
				case Cast::HashType::CONVEX:
					geometry = std::make_unique<Convex>(make_convex(it.value()));
					return;
				case Cast::HashType::POLYGON:
					geometry = std::make_unique<Polygon>(make_polygon(it.value(), Geometry::Type::POLYGON));
					return;
				case Cast::HashType::CHULL:
					geometry = std::make_unique<Polygon>(make_polygon(it.value(), Geometry::Type::CHULL));
					return;
				case Cast::HashType::MULTIPOINT:
					geometry = std::make_unique<MultiPoint>(make_multipoint(it.value()));
					return;
				case Cast::HashType::MULTICIRCLE:
					geometry = std::make_unique<MultiCircle>(make_multicircle(it.value()));
					return;
				case Cast::HashType::MULTIPOLYGON:
					geometry = std::make_unique<MultiPolygon>(make_multipolygon(it.value()));
					return;
				case Cast::HashType::GEO_COLLECTION:
					geometry = std::make_unique<Collection>(make_collection(it.value()));
					return;
				case Cast::HashType::GEO_INTERSECTION:
					geometry = std::make_unique<Intersection>(make_intersection(it.value()));
					return;
				default:
					THROW(GeoSpatialError, "Unknown geometry {}", str_key);
			}
			return;
		}

		case MsgPack::Type::ARRAY: {
			geometry = std::make_unique<Point>(make_point(obj));
			return;
		}

		default:
			THROW(GeoSpatialError, "Object must be string, map or array not {}", enum_name(obj.get_type()));
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
GeoSpatial::process_altitude(data_t& data, const MsgPack& altitude) {
	data.alt = &altitude;
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
		hh(RESERVED_GEO_LON),
		hh(RESERVED_GEO_ALTITUDE),
		hh(RESERVED_GEO_ALT),
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
			case _.fhh(RESERVED_GEO_LON):
				process_longitude(data, value);
				break;
			case _.fhh(RESERVED_GEO_ALTITUDE):
			case _.fhh(RESERVED_GEO_ALT):
				process_altitude(data, value);
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
GeoSpatial::getPoints(const data_t& data, const MsgPack& latitude, const MsgPack& longitude, const MsgPack* altitude)
{
	try {
		if (data.lat->size() != data.lon->size()) {
			THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
		}
		std::vector<Cartesian> points;
		points.reserve(latitude.size());
		if (altitude != nullptr) {
			auto it = latitude.begin();
			auto hit = altitude->begin();
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
		THROW(GeoSpatialError, "{}, {} and {} must be array of numbers or nested array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
	}
}


Point
GeoSpatial::make_point(const MsgPack& o)
{

	switch (o.get_type()) {
		case MsgPack::Type::MAP: {
			const auto data = get_data(o);
			if ((data.lat == nullptr) || (data.lon == nullptr)) {
				THROW(GeoSpatialError, "{} must contain {} and {}", RESERVED_POINT, RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE);
			}
			try {
				return Point(Cartesian(data.lat->f64(), data.lon->f64(), data.alt != nullptr ? distance(*data.alt) : 0, data.units, data.srid));
			} catch (const msgpack::type_error&) {
				THROW(GeoSpatialError, "{}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
		}

		case MsgPack::Type::ARRAY: {
			auto items = o.size();
			if ((items == 2 && o[0].is_number() && o[1].is_number()) ||
				(items == 3 && o[0].is_number() && o[1].is_number() && o[2].is_number())) {
				// GeoJSON requires longitude first, latutude second
				auto longitude = o[0].f64();
				auto latitude = o[1].f64();
				auto altitude = items == 3 ? o[2].f64() : 0.0;
				if (longitude >= -180.0 && longitude <= 180.0 && latitude >= -90.0 && latitude <= 90.0) {
					return Point(Cartesian(latitude, longitude, altitude, Cartesian::Units::DEGREES));
				} else {
					THROW(GeoSpatialError, "{}, {} and {} must in range", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
				}
			} else {
				if (items == 2 && items != 3) {
					THROW(GeoSpatialError, "Expected array of [longitude, latitude] or [longitude, latitude, altitude]");
				}
				THROW(GeoSpatialError, "{}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
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
		return Circle(Cartesian(data.lat->f64(), data.lon->f64(), data.alt != nullptr ? distance(*data.alt) : 0, data.units, data.srid), distance(*data.radius));
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be numeric", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE, RESERVED_GEO_RADIUS);
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
		if (data.alt != nullptr) {
			if (data.lat->size() != data.alt->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
			if (data.lat->size() != data.radius->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_RADIUS);
			}
			auto it = data.lon->begin();
			auto hit = data.alt->begin();
			auto it_radius = data.radius->begin();
			convex.reserve(data.lat->size());
			for (const auto& latitude : *data.lat) {
				convex.add(Circle(Cartesian(latitude.f64(), it->f64(), hit->f64(), data.units, data.srid), distance(*it_radius)));
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
				convex.add(Circle(Cartesian(latitude.f64(), it->f64(), 0, data.units, data.srid), distance(*it_radius)));
				++it;
				++it_radius;
			}
		}
		return convex;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE, RESERVED_GEO_RADIUS);
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
		if (data.alt != nullptr) {
			if (data.lat->size() != data.alt->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
			auto hit = data.alt->begin();
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
	return Polygon(type, getPoints(data, *data.lat, *data.lon, data.alt));
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
		if (data.alt != nullptr) {
			if (data.lat->size() != data.alt->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
			auto it = data.lon->begin();
			auto hit = data.alt->begin();
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
		THROW(GeoSpatialError, "{}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
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
		if (data.alt != nullptr) {
			if (data.lat->size() != data.alt->size()) {
				THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
			}
			auto it = data.lon->begin();
			auto hit = data.alt->begin();
			multicircle.reserve(data.lat->size());
			auto radius = distance(*data.radius);
			for (const auto& latitude : *data.lat) {
				multicircle.add(Circle(Cartesian(latitude.f64(), it->f64(), hit->f64(), data.units, data.srid), radius));
				++it;
				++hit;
			}
		} else {
			auto it = data.lon->begin();
			multicircle.reserve(data.lat->size());
			auto radius = distance(*data.radius);
			for (const auto& latitude : *data.lat) {
				multicircle.add(Circle(Cartesian(latitude.f64(), it->f64(), 0, data.units, data.srid), radius));
				++it;
			}
		}
		return multicircle;
	} catch (const msgpack::type_error&) {
		THROW(GeoSpatialError, "{}, {}, {} and {} must be array of numbers", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE, RESERVED_GEO_RADIUS);
	}
}


MultiPolygon
GeoSpatial::make_multipolygon(const MsgPack& o)
{
	switch (o.get_type()) {
		case MsgPack::Type::MAP: {
			MultiPolygon multipolygon;
			multipolygon.reserve(o.size());
			const auto it_e = o.end();
			for (auto it = o.begin(); it != it_e; ++it) {
				const auto str_key = it->str();
				switch (Cast::get_hash_type(str_key)) {
					case Cast::HashType::POLYGON:
						multipolygon.add(make_polygon(it.value(), Geometry::Type::POLYGON));
						break;
					case Cast::HashType::CHULL:
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
			if (data.alt != nullptr) {
				if (data.lat->size() != data.alt->size()) {
					THROW(GeoSpatialError, "{}, {} and {} must have the same size", RESERVED_GEO_LATITUDE, RESERVED_GEO_LONGITUDE, RESERVED_GEO_ALTITUDE);
				}
				auto m_it = data.lon->begin();
				auto m_hit = data.alt->begin();
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
			switch (Cast::get_hash_type(str_key)) {
				case Cast::HashType::POINT:
					collection.add_point(make_point(it.value()));
					break;
				case Cast::HashType::CIRCLE:
					collection.add_circle(make_circle(it.value()));
					break;
				case Cast::HashType::CONVEX:
					collection.add_convex(make_convex(it.value()));
					break;
				case Cast::HashType::POLYGON:
					collection.add_polygon(make_polygon(it.value(), Geometry::Type::POLYGON));
					break;
				case Cast::HashType::CHULL:
					collection.add_polygon(make_polygon(it.value(), Geometry::Type::CHULL));
					break;
				case Cast::HashType::MULTIPOINT:
					collection.add_multipoint(make_multipoint(it.value()));
					break;
				case Cast::HashType::MULTICIRCLE:
					collection.add_multicircle(make_multicircle(it.value()));
					break;
				case Cast::HashType::MULTIPOLYGON:
					collection.add_multipolygon(make_multipolygon(it.value()));
					break;
				case Cast::HashType::GEO_COLLECTION:
					collection.add(make_collection(it.value()));
					break;
				case Cast::HashType::GEO_INTERSECTION:
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
			switch (Cast::get_hash_type(str_key)) {
				case Cast::HashType::POINT:
					intersection.add(std::make_shared<Point>(make_point(it.value())));
					break;
				case Cast::HashType::CIRCLE:
					intersection.add(std::make_shared<Circle>(make_circle(it.value())));
					break;
				case Cast::HashType::CONVEX:
					intersection.add(std::make_shared<Convex>(make_convex(it.value())));
					break;
				case Cast::HashType::POLYGON:
					intersection.add(std::make_shared<Polygon>(make_polygon(it.value(), Geometry::Type::POLYGON)));
					break;
				case Cast::HashType::CHULL:
					intersection.add(std::make_shared<Polygon>(make_polygon(it.value(), Geometry::Type::CHULL)));
					break;
				case Cast::HashType::MULTIPOINT:
					intersection.add(std::make_shared<MultiPoint>(make_multipoint(it.value())));
					break;
				case Cast::HashType::MULTICIRCLE:
					intersection.add(std::make_shared<MultiCircle>(make_multicircle(it.value())));
					break;
				case Cast::HashType::MULTIPOLYGON:
					intersection.add(std::make_shared<MultiPolygon>(make_multipolygon(it.value())));
					break;
				case Cast::HashType::GEO_COLLECTION:
					intersection.add(std::make_shared<Collection>(make_collection(it.value())));
					break;
				case Cast::HashType::GEO_INTERSECTION:
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
