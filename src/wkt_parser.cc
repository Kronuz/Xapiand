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

#define FIND_GEOMETRY_RE "(SRID[\\s]*=[\\s]*([0-9]{4})[\\s]*\\;[\\s]*)?(POLYGON|MULTIPOLYGON|CIRCLE|MULTICIRCLE|POINT|MULTIPOINT|CHULL|MULTICHULL)[\\s]*\\(([()0-9.\\s,-]*)\\)|(GEOMETRYCOLLECTION|GEOMETRYINTERSECTION)[\\s]*\\(([()0-9.\\s,A-Z-]*)\\)"
#define FIND_CIRCLE_RE "(\\-?\\d*\\.\\d+|\\-?\\d+)\\s(\\-?\\d*\\.\\d+|\\-?\\d+)(\\s(\\-?\\d*\\.\\d+|\\-?\\d+))?[\\s]*\\,[\\s]*(\\d*\\.\\d+|\\d+)"
#define FIND_SUBPOLYGON_RE "[\\s]*(\\(([\\-?\\d*\\.\\d+|\\-?\\d+\\s,]*)\\))[\\s]*(\\,)?"
#define FIND_MULTI_RE "[\\s]*[\\s]*\\((.*?\\))\\)[\\s]*(,)?"
#define FIND_COLLECTION_RE "[\\s]*(POLYGON|MULTIPOLYGON|CIRCLE|MULTICIRCLE|POINT|MULTIPOINT|CHULL|MULTICHULL)[\\s]*\\(([()0-9.\\s,-]*)\\)([\\s]*\\,[\\s]*)?"


pcre *compiled_find_geometry_re = NULL;
pcre *compiled_find_circle_re = NULL;
pcre *compiled_find_subpolygon_re = NULL;
pcre *compiled_find_multi_re = NULL;
pcre *compiled_find_collection_re = NULL;


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
EWKT_Parser::EWKT_Parser(const std::string &EWKT, bool _partials, double _error) : partials(_partials), error(_error)
{
	unique_group unique_gr;
	int len = (int)EWKT.size();
	int ret = pcre_search(EWKT.c_str(), len, 0, 0, FIND_GEOMETRY_RE, &compiled_find_geometry_re, unique_gr);
	group_t *gr = unique_gr.get();
	if (ret != -1 && len == gr[0].end - gr[0].start) {
		if (gr[2].end - gr[2].start != 0) {
			SRID = atoi(std::string(EWKT.c_str(), gr[2].start, gr[2].end - gr[2].start).c_str());
			if (!Cartesian().is_SRID_supported(SRID)) {
				throw MSG_Error("SRID not supported");
			}
		} else {
			SRID = 4326; // WGS 84 default.
		}
		if (gr[5].end - gr[5].start != 0) {
			std::string data(EWKT.c_str(), gr[6].start, gr[6].end - gr[6].start);
			std::string geometry = std::string(EWKT.c_str(), gr[5].start, gr[5].end - gr[5].start);
			if (geometry.compare("GEOMETRYCOLLECTION") == 0) trixels = parse_geometry_collection(data);
			else if (geometry.compare("GEOMETRYINTERSECTION") == 0) trixels = parse_geometry_intersection(data);
		} else {
			std::string geometry = std::string(EWKT.c_str(), gr[3].start, gr[3].end - gr[3].start);
			std::string specification(EWKT.c_str(), gr[4].start, gr[4].end - gr[4].start);
			if (geometry.compare("CIRCLE") == 0) trixels = parse_circle(specification);
			else if (geometry.compare("MULTICIRCLE") == 0) trixels = parse_multicircle(specification);
			else if (geometry.compare("POLYGON") == 0) trixels = parse_polygon(specification, Geometry::CONVEX_POLYGON);
			else if (geometry.compare("MULTIPOLYGON") == 0) trixels = parse_multipolygon(specification, Geometry::CONVEX_POLYGON);
			else if (geometry.compare("CHULL") == 0) trixels = parse_polygon(specification, Geometry::CONVEX_HULL);
			else if (geometry.compare("MULTICHULL") == 0) trixels = parse_multipolygon(specification, Geometry::CONVEX_HULL);
			else if (geometry.compare("POINT") == 0) trixels = parse_point(specification);
			else if (geometry.compare("MULTIPOINT") == 0) trixels = parse_multipoint(specification);
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
EWKT_Parser::parse_circle(std::string &specification)
{
	unique_group unique_gr;
	int len = (int)specification.size();
	int ret = pcre_search(specification.c_str(), len, 0, 0, FIND_CIRCLE_RE, &compiled_find_circle_re, unique_gr);
	group_t *gr = unique_gr.get();
	if (ret != -1 && len == gr[0].end - gr[0].start) {
		double lat = atof(std::string(specification.c_str(), gr[1].start, gr[1].end - gr[1].start).c_str());
		double lon = atof(std::string(specification.c_str(), gr[2].start, gr[2].end - gr[2].start).c_str());
		double h;
		if (gr[4].end - gr[4].start > 0) {
			h = atof(std::string(specification.c_str(), gr[4].start, gr[4].end - gr[4].start).c_str());
		} else {
			h = 0;
		}
		double radius = atof(std::string(specification.c_str(), gr[5].start, gr[5].end - gr[5].start).c_str());
		Cartesian center(lat, lon, h, Cartesian::DEGREES, SRID);
		Constraint c(center, radius);
		Geometry g(c);
		HTM _htm(partials, error, g);
		_htm.run();
		gv.push_back(g);

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
EWKT_Parser::parse_multicircle(std::string &specification)
{
	unique_group unique_gr;
	int len = (int)specification.size();
	int start = 0;

	// Checking if the format is correct and and processing the circles.
	std::vector<std::string> names_f;
	bool first = true;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_MULTI_RE, &compiled_find_multi_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (MULTICIRCLE)");
		}
		std::string circle(specification.c_str(), gr[1].start, gr[1].end - gr[1].start);
		if (first) {
			names_f = parse_circle(circle);
			first = false;
		} else {
			std::vector<std::string> txs = parse_circle(circle);
			names_f = or_trixels(names_f, txs);
		}
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format (MULTICIRCLE)");
	}

	return names_f;
}


// The specification is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_polygon(std::string &specification, Geometry::typePoints type)
{
	unique_group unique_gr;
	int len = (int)specification.size();
	int start = 0;

	// Checking if the format is correct and processing the subpolygons.
	std::vector<std::string> names_f;
	bool first = true;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_SUBPOLYGON_RE, &compiled_find_subpolygon_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (POLYGON)");
		}

		std::string subpolygon(specification.c_str(), gr[2].start, gr[2].end - gr[2].start);
		// split points
		std::vector<Cartesian> pts;
		std::vector<std::string> points = stringSplit(subpolygon, ",");
		if (points.size() == 0) {
			throw MSG_Error("Syntax error in EWKT format (POLYGON)");
		}

		std::vector<std::string>::iterator it_p(points.begin());
		for ( ; it_p != points.end(); it_p++) {
			// Get lat, lon and height.
			std::vector<std::string> coords = stringSplit(*it_p, " ");
			if (coords.size() == 3) {
				Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), atof(coords.at(2).c_str()), Cartesian::DEGREES, SRID);
				pts.push_back(c);
			} else if (coords.size() == 2) {
				Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
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

		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	return names_f;
}


// The specification is ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...)), ((...))
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_multipolygon(std::string &specification, Geometry::typePoints type)
{
	unique_group unique_gr;
	int len = (int)specification.size();
	int start = 0;

	// Checking if the format is correct and and processing the polygons.
	std::vector<std::string> names_f;
	bool first = true;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_MULTI_RE, &compiled_find_multi_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (MULTIPOLYGON)");
		}
		std::string polygon(specification.c_str(), gr[1].start, gr[1].end - gr[1].start);
		if (first) {
			names_f = parse_polygon(polygon, type);
			first = false;
		} else {
			std::vector<std::string> txs = parse_polygon(polygon, type);
			names_f = or_trixels(names_f, txs);
		}
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format (MULTIPOLYGON)");
	}

	return names_f;
}


// The specification is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the points' trixels.
std::vector<std::string>
EWKT_Parser::parse_point(std::string &specification)
{
	std::vector<std::string> res;
	std::string name;

	std::vector<std::string> coords = stringSplit(specification, " (,");
	if (coords.size() == 3) {
		Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), atof(coords.at(2).c_str()), Cartesian::DEGREES, SRID);
		HTM::cartesian2name(c, name);
		res.push_back(name);
	} else if (coords.size() == 2) {
		Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
		HTM::cartesian2name(c, name);
		res.push_back(name);
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
EWKT_Parser::parse_multipoint(std::string &specification)
{
	unique_group unique_gr;
	int len = (int)specification.size();
	int start = 0;

	// Checking if the format is (lat lon [height]), (lat lon [height]), ... and save the points.
	std::vector<std::string> res;
	std::string name;

	while (pcre_search(specification.c_str(), len, start, 0, FIND_SUBPOLYGON_RE, &compiled_find_subpolygon_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (MULTIPOINT)");
		}
		std::string point(specification.c_str(), gr[2].start, gr[2].end - gr[2].start);
		std::vector<std::string> coords = stringSplit(point, " ");
		if (coords.size() == 3) {
			Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), atof(coords.at(2).c_str()), Cartesian::DEGREES, SRID);
			HTM::cartesian2name(c, name);
			res.push_back(name);
		} else if (coords.size() == 2) {
			Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
			HTM::cartesian2name(c, name);
			res.push_back(name);
		} else {
			throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...");
		}
		start = gr[0].end;
	}

	if (start == 0) {
		std::vector<std::string> points = stringSplit(specification, ",");
		std::vector<std::string>::const_iterator it(points.begin());
		for ( ; it != points.end(); it++) {
			std::vector<std::string> coords = stringSplit(*it, " ");
			if (coords.size() == 3) {
				Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), atof(coords.at(2).c_str()), Cartesian::DEGREES, SRID);
				HTM::cartesian2name(c, name);
				res.push_back(name);
			} else if (coords.size() == 2) {
				Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
				HTM::cartesian2name(c, name);
				res.push_back(name);
			} else {
				throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...");
			}
		}
	} else if (start != len) {
		throw MSG_Error("Syntax error in EWKT format (MULTIPOINT)");
	}

	return res;
}


// Parse a collection of geometries (join by OR operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_collection(std::string &data)
{
	unique_group unique_gr;
	int len = (int)data.size();
	int start = 0;

	// Checking if the format is correct and processing the geometries.
	std::vector<std::string> specification;
	std::vector<std::string> geometry;
	std::vector<std::string> names_f;
	bool first = true;
	while (pcre_search(data.c_str(), len, start, 0, FIND_COLLECTION_RE, &compiled_find_collection_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (GEOMETRYCOLLECTION)");
		}
		std::string geometry(data.c_str(), gr[1].start, gr[1].end - gr[1].start);
		std::string specification(data.c_str(), gr[2].start, gr[2].end - gr[2].start);
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

		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	return names_f;
}


// Parse a intersection of geomtries (join by AND operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_intersection(std::string &data)
{
	unique_group unique_gr;
	int len = (int)data.size();
	int start = 0;

	// Checking if the format is correct and processing the geometries.
	std::vector<std::string> specification;
	std::vector<std::string> geometry;
	std::vector<std::string> names_f;
	bool first = true;
	while (pcre_search(data.c_str(), len, start, 0, FIND_COLLECTION_RE, &compiled_find_collection_re, unique_gr) != -1) {
		group_t *gr = unique_gr.get();
		if (start != gr[0].start) {
			throw MSG_Error("Syntax error in EWKT format (GEOMETRYINTERSECTION)");
		}
		std::string geometry(data.c_str(), gr[1].start, gr[1].end - gr[1].start);
		std::string specification(data.c_str(), gr[2].start, gr[2].end - gr[2].start);
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
		}

		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

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
	while (it1 != txs1.end()) {
		bool inc = true;
		std::vector<std::string>::iterator it2(txs2.begin());
		while (it2 != txs2.end()) {
			int s1 = (int)(*it1).size(), s2 = (int)(*it2).size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				if (s1 == s2) {
					it1 = txs1.erase(it1);
					it2 = txs2.erase(it2);
				} else {
					std::vector<std::string> txs_aux = get_trixels(*it2, s1 - s2, *it1);
					it1 = txs1.erase(it1);
					it2 = txs2.erase(it2);
					it2 = txs2.insert(it2, txs_aux.begin(), txs_aux.end());
				}
				inc = false;
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it1, s2 - s1, *it2);
				it2 = txs2.erase(it2);
				it1 = txs1.erase(it1);
				it1 = txs1.insert(it1, txs_aux.begin(), txs_aux.end());
				inc = false;
				break;
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
			int s1 = (int)(*it1).size(), s2 = (int)(*it2).size();
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
	for ( ;it1 != txs1.end(); it1++) {
		std::vector<std::string>::iterator it2(txs2.begin());
		while (it2 != txs2.end()) {
			int s1 = (int)(*it1).size(), s2 = (int)(*it2).size();
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
EWKT_Parser::get_trixels(std::string &father, int depth, std::string &son)
{
	std::vector<std::string> sonsF;
	std::string p_son(father);

	for (int i = 0; i < depth; i++) {
		switch (son.at(father.size() + i)) {
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


bool is_like_EWKT(const char *str)
{
	unique_group unique_gr;
	int len = (int)strlen(str);
	int ret = pcre_search(str, len, 0, 0, FIND_GEOMETRY_RE, &compiled_find_geometry_re, unique_gr);
	group_t *gr = unique_gr.get();

	return (ret != -1 && len == gr[0].end - gr[0].start) ? true : false;
}