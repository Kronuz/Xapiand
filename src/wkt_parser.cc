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

#include "wkt_parser.h"
#include "serialise.h"


std::regex find_geometry_re("(SRID[\\s]*=[\\s]*([0-9]{4})[\\s]*\\;[\\s]*)?(POLYGON|MULTIPOLYGON|CIRCLE|MULTICIRCLE|POINT|MULTIPOINT|CHULL|MULTICHULL)[\\s]*\\(([()0-9.\\s,-]*)\\)|(GEOMETRYCOLLECTION|GEOMETRYINTERSECTION)[\\s]*\\(([()0-9.\\s,A-Z-]*)\\)", std::regex::icase | std::regex::optimize);
std::regex find_circle_re("(\\-?\\d*\\.\\d+|\\-?\\d+)\\s(\\-?\\d*\\.\\d+|\\-?\\d+)(\\s(\\-?\\d*\\.\\d+|\\-?\\d+))?[\\s]*\\,[\\s]*(\\d*\\.\\d+|\\d+)", std::regex::icase | std::regex::optimize);
std::regex find_subpolygon_re("[\\s]*(\\(([\\-?\\d*\\.\\d+|\\-?\\d+\\s,]*)\\))[\\s]*(\\,)?", std::regex::icase | std::regex::optimize);
std::regex find_multi_poly_re("[\\s]*[\\s]*\\((.*?\\))\\)[\\s]*(,)?", std::regex::icase | std::regex::optimize);
std::regex find_multi_circle_re("[\\s]*[\\s]*\\((.*?)\\)[\\s]*(,)?", std::regex::icase | std::regex::optimize);
std::regex find_collection_re("[\\s]*(POLYGON|MULTIPOLYGON|CIRCLE|MULTICIRCLE|POINT|MULTIPOINT|CHULL|MULTICHULL)[\\s]*\\(([()0-9.\\s,-]*)\\)([\\s]*\\,[\\s]*)?", std::regex::icase | std::regex::optimize);


/* Parser for EWKT (A PostGIS-specific format that includes the spatial reference system identifier (SRID))
 * Geometric objects WKT supported:
 *  POINT
 *  MULTIPOINT
 *  POLYGON       // Polygon should be convex. Otherwise it should be used CHULL.
 *  MULTIPOLYGON
 *  GEOMETRYCOLLECTION
 *
 * Geometric objects not defined in wkt supported, but defined here by their relevance:
 *  GEOMETRYINTERSECTION
 *  CIRCLE
 *  MULTICIRCLE
 *  CHULL  			// Convex Hull from a points' set.
 *  MULTICHULL
 *
 * Coordinates for geometries may be:
 * (lat lon) or (lat lon height)
 *
 * This parser do not accept EMPTY geometries and
 * The polygons are not required to be repeated first coordinate to end like EWKT.
*/
EWKT_Parser::EWKT_Parser(const std::string &EWKT, bool _partials, double _error) : error(_error), partials(_partials)
{
	std::smatch m;
	if (std::regex_match(EWKT, m, find_geometry_re) && static_cast<size_t>(m.length(0)) == EWKT.size()) {
		if (m.length(2) != 0) {
			SRID = std::stoi(m.str(2));
			if (!Cartesian().is_SRID_supported(SRID)) {
				throw MSG_Error("SRID not supported");
			}
		} else {
			SRID = 4326; // WGS 84 default.
		}
		if (m.length(5) != 0) {
			std::string geometry(m.str(5));
			if (geometry.compare("GEOMETRYCOLLECTION") == 0) {
				trixels = parse_geometry_collection(m.str(6));
			} else if (geometry.compare("GEOMETRYINTERSECTION") == 0) {
				trixels = parse_geometry_intersection(m.str(6));
				return;
			}
		} else {
			std::string geometry(m.str(3));
			if (geometry.compare("CIRCLE") == 0) {
				trixels = parse_circle(m.str(4));
				return;
			} else if (geometry.compare("MULTICIRCLE") == 0) {
				trixels = parse_multicircle(m.str(4));
			} else if (geometry.compare("POLYGON") == 0) {
				trixels = parse_polygon(m.str(4), Geometry::CONVEX_POLYGON);
				return;
			} else if (geometry.compare("MULTIPOLYGON") == 0) {
				trixels = parse_multipolygon(m.str(4), Geometry::CONVEX_POLYGON);
			} else if (geometry.compare("CHULL") == 0) {
				trixels = parse_polygon(m.str(4), Geometry::CONVEX_HULL);
				return;
			} else if (geometry.compare("MULTICHULL") == 0) {
				trixels = parse_multipolygon(m.str(4), Geometry::CONVEX_HULL);
			} else if (geometry.compare("POINT") == 0) {
				trixels = parse_point(m.str(4));
				return;
			} else if (geometry.compare("MULTIPOINT") == 0) {
				trixels = parse_multipoint(m.str(4));
			}
		}
		// Deleting duplicate centroids.
		for (CartesianList::iterator it(centroids.begin()), del; it != centroids.end(); it++) {
			del = it + 1;
			while ((del = std::find(del, centroids.end(), *it)) != centroids.end()) {
				del = centroids.erase(del);
			}
		}
	} else {
		throw MSG_Error("Syntax error in EWKT format or geometry object not supported");
	}
}


// The specification is: lat lon [height], radius(double positive)
// lat and lon in degrees.
// height and radius in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_circle(const std::string &specification)
{
	std::smatch m;
	if (std::regex_match(specification, m, find_circle_re) && static_cast<size_t>(m.length(0)) == specification.size()) {
		double lat = std::stod(m.str(1));
		double lon = std::stod(m.str(2));
		double h = m.length(4) > 0 ? std::stod(m.str(4)) : 0;
		double radius = std::stod(m.str(5));
		Cartesian center(lat, lon, h, Cartesian::DEGREES, SRID);
		Constraint c(center, radius);
		Geometry g(c);
		HTM _htm(partials, error, g);
		_htm.run();
		gv.push_back(g);

		centroids.push_back(g.centroid);

		return _htm.names;
	} else {
		throw MSG_Error("The specification for CIRCLE is lat lon [height], radius in meters(double positive)");
	}
}


// The specification is: (lat lon [height], radius), ... (lat lon [height], radius)
// lat and lon in degrees.
// height and radius in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_multicircle(const std::string &specification)
{
	// Checking if the format is correct and circles are procesed.
	std::vector<std::string> names_f;
	bool first = true;
	size_t match_size = 0;

	std::sregex_iterator next(specification.begin(), specification.end(), find_multi_circle_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		if (first) {
			names_f = parse_circle(next->str(1));
			first = false;
		} else {
			std::vector<std::string> txs = parse_circle(next->str(1));
			names_f = or_trixels(names_f, txs);
		}
	}

	if (match_size != specification.size()) {
		throw MSG_Error("Syntax error in EWKT format (MULTICIRCLE)");
	}

	return names_f;
}


// The specification is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_polygon(const std::string &specification, const Geometry::typePoints &type)
{
	// Checking if the format is correct and subpolygons are procesed.
	std::vector<std::string> names_f;
	bool first = true;
	size_t match_size = 0;

	std::sregex_iterator next(specification.begin(), specification.end(), find_subpolygon_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		// split points
		std::vector<Cartesian> pts;
		std::vector<std::string> points = stringSplit(next->str(2), ",");
		if (points.size() == 0) throw MSG_Error("Syntax error in EWKT format (POLYGON)");

		std::vector<std::string>::iterator it_p(points.begin());
		for ( ; it_p != points.end(); it_p++) {
			// Get lat, lon and height.
			std::vector<std::string> coords = stringSplit(*it_p, " ");
			if (coords.size() == 3) {
				Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), std::stod(coords.at(2)), Cartesian::DEGREES, SRID);
				pts.push_back(c);
			} else if (coords.size() == 2) {
				Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), 0, Cartesian::DEGREES, SRID);
				pts.push_back(c);
			} else {
				throw MSG_Error("The specification for POLYGON is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...");
			}
		}

		Geometry g(pts, type);
		HTM _htm(partials, error, g);
		_htm.run();
		gv.push_back(g);

		if (first) {
			names_f = _htm.names;
			first = false;
		} else {
			names_f = xor_trixels(names_f, _htm.names);
		}
	}

	centroids.push_back(HTM::getCentroid(names_f));

	if (match_size != specification.size()) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	return names_f;
}


// The specification is ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...)), ((...))
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_multipolygon(const std::string &specification, const Geometry::typePoints &type)
{
	std::vector<std::string> names_f;
	bool first = true;
	size_t match_size = 0;

	std::sregex_iterator next(specification.begin(), specification.end(), find_multi_poly_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		if (first) {
			names_f = parse_polygon(next->str(1), type);
			first = false;
		} else {
			std::vector<std::string> txs = parse_polygon(next->str(1), type);
			names_f = or_trixels(names_f, txs);
		}
	}

	if (match_size != specification.size()) {
		throw MSG_Error("Syntax error in EWKT format (MULTIPOLYGON)");
	}

	return names_f;
}


// The specification is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the points' trixels.
std::vector<std::string>
EWKT_Parser::parse_point(const std::string &specification)
{
	std::vector<std::string> res;
	std::string name;

	std::vector<std::string> coords = stringSplit(specification, " (,");
	if (coords.size() == 3) {
		Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), std::stod(coords.at(2)), Cartesian::DEGREES, SRID);
		c.normalize();
		HTM::cartesian2name(c, name);
		res.push_back(name);
		centroids.push_back(c);
	} else if (coords.size() == 2) {
		Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), 0, Cartesian::DEGREES, SRID);
		c.normalize();
		HTM::cartesian2name(c, name);
		res.push_back(name);
		centroids.push_back(c);
	} else {
		throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...");
	}

	return res;
}


// The specification is lat lon [height]
// lat and lon in degrees.
// height in meters.
// Return the point's trixels.
std::vector<std::string>
EWKT_Parser::parse_multipoint(const std::string &specification)
{
	// Checking if the format is (lat lon [height]), (lat lon [height]), ... and save the points.
	std::vector<std::string> res;
	std::string name;
	size_t match_size = 0;

	std::sregex_iterator next(specification.begin(), specification.end(), find_subpolygon_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		std::vector<std::string> coords = stringSplit(next->str(2), " ");
		if (coords.size() == 3) {
			Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), std::stod(coords.at(2)), Cartesian::DEGREES, SRID);
			c.normalize();
			HTM::cartesian2name(c, name);
			res.push_back(name);
			centroids.push_back(c);
		} else if (coords.size() == 2) {
			Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), 0, Cartesian::DEGREES, SRID);
			c.normalize();
			HTM::cartesian2name(c, name);
			res.push_back(name);
			centroids.push_back(c);
		} else {
			throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...");
		}
	}

	if (match_size == 0) {
		std::vector<std::string> points = stringSplit(specification, ",");
		std::vector<std::string>::const_iterator it(points.begin());
		for ( ; it != points.end(); it++) {
			std::vector<std::string> coords = stringSplit(*it, " ");
			if (coords.size() == 3) {
				Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), std::stod(coords.at(2)), Cartesian::DEGREES, SRID);
				c.normalize();
				HTM::cartesian2name(c, name);
				res.push_back(name);
				centroids.push_back(c);
			} else if (coords.size() == 2) {
				Cartesian c(std::stod(coords.at(0)), std::stod(coords.at(1)), 0, Cartesian::DEGREES, SRID);
				c.normalize();
				HTM::cartesian2name(c, name);
				res.push_back(name);
				centroids.push_back(c);
			} else {
				throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...");
			}
		}
	} else if (match_size != specification.size()) {
		throw MSG_Error("Syntax error in EWKT format (MULTIPOINT)");
	}

	return res;
}


// Parse a collection of geometries (join by OR operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_collection(const std::string &data)
{
	// Checking if the format is correct and processing the geometries.
	std::vector<std::string> names_f;
	bool first = true;
	size_t match_size = 0;

	std::sregex_iterator next(data.begin(), data.end(), find_collection_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		std::string geometry(next->str(1));
		std::string specification(next->str(2));
		std::vector<std::string> txs;
		if (geometry.compare("CIRCLE") == 0) txs = parse_circle(specification);
		else if (geometry.compare("MULTICIRCLE") == 0) txs = parse_multicircle(specification);
		else if (geometry.compare("POLYGON") == 0) txs = parse_polygon(specification, Geometry::CONVEX_POLYGON);
		else if (geometry.compare("MULTIPOLYGON") == 0) txs = parse_multipolygon(specification, Geometry::CONVEX_POLYGON);
		else if (geometry.compare("POINT") == 0) txs = parse_point(specification);
		else if (geometry.compare("MULTIPOINT") == 0) txs = parse_multipoint(specification);
		else if (geometry.compare("CHULL") == 0) txs = parse_polygon(specification, Geometry::CONVEX_HULL);
		else if (geometry.compare("MULTICHULL") == 0) txs = parse_polygon(specification, Geometry::CONVEX_HULL);

		if (first) {
			names_f = txs;
			first = false;
		} else {
			names_f = or_trixels(names_f, txs);
		}
	}

	if (match_size != data.size()) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	return names_f;
}


// Parse a intersection of geometries (join by AND operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_intersection(const std::string &data)
{
	unique_group unique_gr;
	int len = (int)data.size(), start = 0;

	// Checking if the format is correct and processing the geometries.
	std::vector<std::string> names_f;
	bool first = true;
	size_t match_size = 0;

	std::sregex_iterator next(data.begin(), data.end(), find_collection_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	for ( ; next != end; ++next) {
		match_size += next->length(0);
		std::string geometry(next->str(1));
		std::string specification(next->str(2));
		std::vector<std::string> txs;
		if (geometry.compare("CIRCLE") == 0) txs = parse_circle(specification);
		else if (geometry.compare("MULTICIRCLE") == 0) txs = parse_multicircle(specification);
		else if (geometry.compare("POLYGON") == 0) txs = parse_polygon(specification, Geometry::CONVEX_POLYGON);
		else if (geometry.compare("MULTIPOLYGON") == 0) txs = parse_multipolygon(specification, Geometry::CONVEX_POLYGON);
		else if (geometry.compare("POINT") == 0) txs = parse_point(specification);
		else if (geometry.compare("MULTIPOINT") == 0) txs = parse_multipoint(specification);
		else if (geometry.compare("CHULL") == 0) txs = parse_polygon(specification, Geometry::CONVEX_HULL);
		else if (geometry.compare("MULTICHULL") == 0) txs = parse_polygon(specification, Geometry::CONVEX_HULL);

		if (first) {
			names_f = txs;
			first = false;
		} else {
			names_f = and_trixels(names_f, txs);
			if (names_f.empty()) return names_f;
		}
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	centroids.clear();
	centroids.push_back(HTM::getCentroid(names_f));
	return names_f;
}


// String tokenizer by characters in delimiter.
std::vector<std::string>
EWKT_Parser::stringSplit(const std::string &str, const std::string &delimiter)
{
	std::vector<std::string> results;
	size_t prev = 0, next = 0, len;

	while ((next = str.find_first_of(delimiter, prev)) != std::string::npos) {
		len = next - prev;
		if (len > 0) {
			results.push_back(str.substr(prev, len));
		}
		prev = next + 1;
	}

	if (prev < str.size()) {
		results.push_back(str.substr(prev));
	}

	return results;
}


// Exclusive or of two sets of trixels.
std::vector<std::string>
EWKT_Parser::xor_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2)
{
	std::vector<std::string> res;
	std::vector<std::string>::iterator it1(txs1.begin());
	size_t current_pos = 0;
	while (it1 != txs1.end()) {
		bool inc = true;
		std::vector<std::string>::iterator it2(txs2.begin());
		while (it2 != txs2.end()) {
			size_t s1 = it1->size(), s2 = it2->size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				if (s1 == s2) {
					it1 = txs1.erase(it1);
					it2 = txs2.erase(it2);
				} else {
					std::vector<std::string> txs_aux = get_trixels(*it2, s1 - s2, *it1);
					it1 = txs1.erase(it1);
					it2 = txs2.erase(it2);
					txs2.insert(it2, txs_aux.begin(), txs_aux.end());
				}
				inc = false;
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it1, s2 - s1, *it2);
				it2 = txs2.erase(it2);
				it1 = txs1.erase(it1);
				txs1.insert(it1, txs_aux.begin(), txs_aux.end());
				it1 = txs1.begin() + current_pos;
				inc = false;
				break;
			}
			it2++;
		}
		if (inc) {
			it1++;
			current_pos++;
		}
	}

	res.reserve(txs1.size() + txs2.size());
	res.insert(res.end(), txs1.begin(), txs1.end());
	res.insert(res.end(), txs2.begin(), txs2.end());
	return res;
}


// Join of two sets of trixels.
std::vector<std::string>
EWKT_Parser::or_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2)
{
	std::vector<std::string> res;
	std::vector<std::string>::iterator it1(txs1.begin());
	while (it1 != txs1.end()) {
		bool inc = true;
		std::vector<std::string>::iterator it2(txs2.begin());
		while (it2 != txs2.end()) {
			size_t s1 = it1->size(), s2 = it2->size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				it1 = txs1.erase(it1);
				inc = false;
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				it2 = txs2.erase(it2);
				continue;
			}
			it2++;
		}
		if (inc) it1++;
	}

	res.reserve(txs1.size() + txs2.size());
	res.insert(res.end(), txs1.begin(), txs1.end());
	res.insert(res.end(), txs2.begin(), txs2.end());
	return res;
}


// Intersection of two sets of trixels.
std::vector<std::string>
EWKT_Parser::and_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2)
{
	std::vector<std::string> res;
	std::vector<std::string>::iterator it1(txs1.begin());
	for ( ; it1 != txs1.end(); it1++) {
		std::vector<std::string>::iterator it2(txs2.begin());
		while (it2 != txs2.end()) {
			size_t s1 = it1->size(), s2 = it2->size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				res.push_back(*it1);
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				res.push_back(*it2);
				it2 = txs2.erase(it2);
				continue;
			}
			it2++;
		}
	}

	return res;
}


/*
 * Returns the trixels that conform to the father except trixel's son.
 *   Father      Son			 Trixels back:
 *     /\	     /\
 *    /__\      /__\	   =>	     __
 *   /\  /\					       /\  /\
 *  /__\/__\					  /__\/__\
 */
std::vector<std::string>
EWKT_Parser::get_trixels(const std::string &father, size_t depth, const std::string &son)
{
	std::vector<std::string> sonsF;
	std::string p_son(father);
	size_t m_size = father.size() + depth;

	for (size_t i = father.size(); i < m_size; i++) {
		switch (son.at(i)) {
			case '0':
				sonsF.push_back(p_son + "1");
				sonsF.push_back(p_son + "2");
				sonsF.push_back(p_son + "3");
				p_son += "0";
				break;
			case '1':
				sonsF.push_back(p_son + "0");
				sonsF.push_back(p_son + "2");
				sonsF.push_back(p_son + "3");
				p_son += "1";
				break;
			case '2':
				sonsF.push_back(p_son + "0");
				sonsF.push_back(p_son + "1");
				sonsF.push_back(p_son + "3");
				p_son += "2";
				break;
			case '3':
				sonsF.push_back(p_son + "0");
				sonsF.push_back(p_son + "1");
				sonsF.push_back(p_son + "2");
				p_son += "3";
				break;
		}
	}

	return sonsF;
}


bool
EWKT_Parser::isEWKT(const std::string &str)
{
	std::smatch m;
	return std::regex_match(str, m, find_geometry_re) && static_cast<size_t>(m.length(0)) == str.size();
}


void
EWKT_Parser::getRanges(const std::string &field_value, bool partials, double error, std::vector<range_t> &ranges, CartesianList &centroids)
{
	EWKT_Parser ewkt(field_value, partials, error);

	if (ewkt.trixels.empty()) return;

	std::vector<std::string>::const_iterator it(ewkt.trixels.begin());
	for ( ; it != ewkt.trixels.end(); it++) {
		HTM::insertRange(*it, ranges, HTM_MAX_LEVEL);
	}
	HTM::mergeRanges(ranges);
	centroids = ewkt.centroids;
}


void
EWKT_Parser::getIndexTerms(const std::string &field_value, bool partials, double error, std::vector<std::string> &terms)
{
	EWKT_Parser ewkt(field_value, partials, error);

	if (ewkt.trixels.empty()) return;

	std::vector<range_t> ranges;
	std::vector<std::string>::const_iterator it(ewkt.trixels.begin());
	for ( ; it != ewkt.trixels.end(); it++) {
		HTM::insertRange(*it, ranges, HTM_MAX_LEVEL);
	}
	HTM::mergeRanges(ranges);

	std::vector<range_t>::const_iterator rit(ranges.begin());
	std::string result;
	for (size_t num_ran = 0; rit != ranges.end(); rit++) {
		if (num_ran < RANGES_BY_TERM) {
			result += Serialise::trixel_id(rit->start) + Serialise::trixel_id(rit->end);
			num_ran++;
		} else {
			terms.push_back(result);
			result = Serialise::trixel_id(rit->start) + Serialise::trixel_id(rit->end);
			num_ran = 1;
		}
	}
	terms.push_back(result);
}


void
EWKT_Parser::getSearchTerms(const std::string &field_value, bool partials, double error, std::vector<std::string> &terms)
{
	EWKT_Parser ewkt(field_value, partials, error);

	if (ewkt.trixels.empty()) return;

	std::vector<range_t> ranges;
	std::vector<std::string>::const_iterator it(ewkt.trixels.begin());
	for ( ; it != ewkt.trixels.end(); it++) {
		HTM::insertRange(*it, ranges, HTM_MAX_LEVEL);
	}
	HTM::mergeRanges(ranges);

	std::vector<range_t>::const_iterator rit(ranges.begin());
	std::string result(std::to_string(rit->start) + WKT_SEPARATOR + std::to_string(rit->end));
	size_t num_ran = 1;
	for (rit++; rit != ranges.end(); rit++) {
		if (num_ran < RANGES_BY_TERM) {
			result += WKT_SEPARATOR + std::to_string(rit->start) + WKT_SEPARATOR + std::to_string(rit->end);
			num_ran++;
		} else {
			terms.push_back(result);
			result = std::to_string(rit->start) + WKT_SEPARATOR + std::to_string(rit->end);
			num_ran = 1;
		}
	}
	terms.push_back(result);
}
