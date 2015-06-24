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

#define FIND_GEOMETRY_RE "(SRID[\\s]*=[\\s]*([0-9]{4})[\\s]*\\;[\\s]*)?(POLYGON|CIRCLE|MULTIPOLYGON|MULTICIRCLE|MULTIPOINT|TRIANGLE)[\\s]*\\(([()0-9.\\s,-]*)\\)|(GEOMETRYCOLLECTION|GEOMETRYINTERSECTION)[\\s]*\\(([()0-9.\\s,A-Z-]*)\\)"
#define FIND_CIRCLE_RE "(\\-?\\d*\\.\\d+|\\-?\\d+)\\s(\\-?\\d*\\.\\d+|\\-?\\d+)(\\s(\\-?\\d*\\.\\d+|\\-?\\d+))?[\\s]*\\,[\\s]*(\\d*\\.\\d+|\\d+)"
#define FIND_SUBPOLYGON_RE "[\\s]*(\\(([\\-?\\d*\\.\\d+|\\-?\\d+\\s,]*)\\))[\\s]*(\\,)?"
#define FIND_POLYGON_RE "[\\s]*[\\s]*\\((.*?\\))\\)[\\s]*(,)?"
#define FIND_COLLECTION_RE "[\\s]*(POLYGON|CIRCLE|MULTIPOLYGON|MULTICIRCLE|MULTIPOINT|TRIANGLE)[\\s]*\\(([()0-9.\\s,-]*)\\)([\\s]*\\,[\\s]*)?"


pcre *EWKT_Parser::compiled_find_geometry_re = NULL;
pcre *EWKT_Parser::compiled_find_circle_re = NULL;
pcre *EWKT_Parser::compiled_find_subpolygon_re = NULL;
pcre *EWKT_Parser::compiled_find_polygon_re = NULL;
pcre *EWKT_Parser::compiled_find_collection_re = NULL;


/* Parser for EWKT (A PostGIS-specific format that includes the spatial reference system identifier (SRID))
 * Geometric objects WKT supported:
 * MULTIPOINT
 * POLYGON
 * MULTIPOLYGON
 * TRIANGLE
 * GEOMETRYCOLLECTION
 *
 * Geometric objects not defined in wkt supported, but defined here by their relevance:
 *  CIRCLE
 *  GEOMETRYINTERSECTION
 *
 * Coordinates for geometries may be:
 * (lat lon) or (lat lon height)
 *
 * This parser do not accept EMPTY geometries.
*/
EWKT_Parser::EWKT_Parser(std::string &EWKT, bool _partials, double _error) : partials(_partials), error(_error)
{
	group_t *gr = NULL;
	int len = (int)EWKT.size();
	int ret = pcre_search(EWKT.c_str(), len, 0, 0, FIND_GEOMETRY_RE, &compiled_find_geometry_re , &gr);
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
			else if (geometry.compare("POLYGON") == 0) trixels = parse_polygon(specification);
			else if (geometry.compare("MULTIPOLYGON") == 0) trixels = parse_multipolygon(specification);
			else if (geometry.compare("MULTIPOINT") == 0) trixels = parse_multipoint(specification);
			else if (geometry.compare("TRIANGLE") == 0) trixels = parse_polygon(specification);
		}
	} else {
		throw MSG_Error("Syntax error in EWKT format or geometry object not supported");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}
}


// The specification is: lat lon [height], radius(double positive)
// lat and lon in degrees.
// height and radius in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_circle(std::string &specification)
{
	group_t *gr = NULL;
	int len = (int)specification.size();
	int ret = pcre_search(specification.c_str(), len, 0, 0, FIND_CIRCLE_RE, &compiled_find_circle_re, &gr);
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

		// Python for the circle.
		gv.push_back(g);
		_htm.writePython3D("CIRCLE.py", gv, _htm.names);

		return _htm.names;
	} else {
		throw MSG_Error("The specification for CIRCLE is lat lon [height], radius in meters(double positive)");
	}
}


// The specification is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_polygon(std::string &specification)
{
	group_t *gr = NULL;
	int len = (int)specification.size();
	int start = 0;

	// First checking if the format is correct.
	std::vector<std::string> res;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_SUBPOLYGON_RE, &compiled_find_subpolygon_re, &gr) != -1) {
		if (start != gr[0].start) throw MSG_Error("Syntax error in EWKT format");
		res.push_back(std::string(specification.c_str(), gr[2].start, gr[2].end - gr[2].start));
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	// Processing the polygon.
	std::vector<std::string> names_f;
	std::vector<std::string>::iterator it(res.begin());
	bool first = true;
	for ( ; it != res.end(); it++) {
		// Split points.
		std::vector<Cartesian> pts;
		std::vector<std::string> points = stringSplit(*it, ",");
		if (points.size() == 0) throw MSG_Error("Syntax error in EWKT formats");

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

		Geometry g(pts);
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

	// Python for the Polygon.
	Constraint c;
	Geometry g(c);
	HTM _htm(partials, error, g);
	_htm.writePython3D("POLYGON.py", gv, names_f);

	return names_f;
}


// The specification is ((lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...)), ((...))
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_multipolygon(std::string &specification)
{
	group_t *gr = NULL;
	int len = (int)specification.size();
	int start = 0;

	// First checking if the format is correct.
	std::vector<std::string> res;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_POLYGON_RE, &compiled_find_polygon_re, &gr) != -1) {
		if (start != gr[0].start) throw MSG_Error("Syntax error in EWKT format");
		res.push_back(std::string(specification.c_str(), gr[1].start, gr[1].end - gr[1].start));
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	std::vector<std::string>::iterator it(res.begin());
	std::vector<std::string> names_f;
	bool first = true;
	for ( ; it != res.end(); it++) {
		if (first) {
			names_f = parse_polygon(*it);
			first = false;
		} else {
			std::vector<std::string> txs2 = parse_polygon(*it);
			names_f = or_trixels(names_f, txs2);
		}
	}

	// Python for the Polygon.
	Constraint c;
	Geometry g(c);
	HTM _htm(partials, error, g);
	_htm.writePython3D("MULTIPOLYGON.py", gv, names_f);

	return names_f;
}


// The specification is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...
// lat and lon in degrees.
// height in meters.
// Return the trixels that cover the region.
std::vector<std::string>
EWKT_Parser::parse_multipoint(std::string &specification)
{
	group_t *gr = NULL;
	int len = (int)specification.size();
	int start = 0;

	// Checking if the format is (lat lon [height]), (lat lon [height]), ... and save the points
	std::vector<std::string> res;
	std::vector<Cartesian> pts;
	while (pcre_search(specification.c_str(), len, start, 0, FIND_SUBPOLYGON_RE, &compiled_find_subpolygon_re, &gr) != -1) {
		if (start != gr[0].start) throw MSG_Error("Syntax error in EWKT format");
		std::string point(specification.c_str(), gr[2].start, gr[2].end - gr[2].start);
		std::vector<std::string> coords = stringSplit(point, " ");
		if (coords.size() == 3) {
			Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), atof(coords.at(2).c_str()), Cartesian::DEGREES, SRID);
			pts.push_back(c);
		} else if (coords.size() == 2) {
			Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
			pts.push_back(c);
		} else {
			throw MSG_Error("The specification for POLYGON is (lat lon [height], ..., lat lon [height]), (lat lon [height], ..., lat lon [height]), ...");
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
				pts.push_back(c);
			} else if (coords.size() == 2) {
				Cartesian c(atof(coords.at(0).c_str()), atof(coords.at(1).c_str()), 0, Cartesian::DEGREES, SRID);
				pts.push_back(c);
			} else {
				throw MSG_Error("The specification for MULTIPOINT is (lat lon [height], ..., lat lon [height]) or (lat lon [height]), ..., (lat lon [height]), ...");
			}
		}
	} else if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	Geometry g(pts);
	HTM _htm(partials, error, g);
	_htm.run();
	gv.push_back(g);

	// Python for the Polygon.
	_htm.writePython3D("MULTIPOINT.py", gv, _htm.names);

	return _htm.names;
}


// Parse a collection of geometries (join by OR operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_collection(std::string &data)
{
	group_t *gr = NULL;
	int len = (int)data.size();
	int start = 0;

	// First checking if the format is correct.
	std::vector<std::string> specification;
	std::vector<std::string> geometry;
	while (pcre_search(data.c_str(), len, start, 0, FIND_COLLECTION_RE, &compiled_find_collection_re, &gr) != -1) {
		if (start != gr[0].start) throw MSG_Error("Syntax error in EWKT format");
		geometry.push_back(std::string(data.c_str(), gr[1].start, gr[1].end - gr[1].start));
		specification.push_back(std::string(data.c_str(), gr[2].start, gr[2].end - gr[2].start));
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	std::vector<std::string>::iterator it(geometry.begin());
	std::vector<std::string>::iterator it_s(specification.begin());
	std::vector<std::string> names_f;
	bool first = true;
	for ( ; it != geometry.end(); it++, it_s++) {
		std::vector<std::string> txs;
		if ((*it).compare("CIRCLE") == 0) txs = parse_circle(*it_s);
		else if ((*it).compare("POLYGON") == 0) txs = parse_polygon(*it_s);
		else if ((*it).compare("MULTIPOLYGON") == 0) txs = parse_multipolygon(*it_s);
		else if ((*it).compare("MULTIPOINT") == 0) txs = parse_multipoint(*it_s);
		else if ((*it).compare("TRIANGLE") == 0) txs = parse_polygon(*it_s);
		if (first) {
			names_f = txs;
			first = false;
		} else {
			names_f = or_trixels(names_f, txs);
		}
	}

	// Python for the Polygon.
	Constraint c;
	Geometry g(c);
	HTM _htm(partials, error, g);
	_htm.writePython3D("GEOMETRYCOLLECTION.py", gv, names_f);

	return names_f;
}


// Parse a intersection of geomtries (join by AND operation).
std::vector<std::string>
EWKT_Parser::parse_geometry_intersection(std::string &data)
{
	group_t *gr = NULL;
	int len = (int)data.size();
	int start = 0;

	// First checking if the format is correct.
	std::vector<std::string> specification;
	std::vector<std::string> geometry;
	while (pcre_search(data.c_str(), len, start, 0, FIND_COLLECTION_RE, &compiled_find_collection_re, &gr) != -1) {
		if (start != gr[0].start) throw MSG_Error("Syntax error in EWKT format");
		geometry.push_back(std::string(data.c_str(), gr[1].start, gr[1].end - gr[1].start));
		specification.push_back(std::string(data.c_str(), gr[2].start, gr[2].end - gr[2].start));
		start = gr[0].end;
	}

	if (start != len) {
		throw MSG_Error("Syntax error in EWKT format");
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	std::vector<std::string>::iterator it(geometry.begin());
	std::vector<std::string>::iterator it_s(specification.begin());
	std::vector<std::string> names_f;
	bool first = true;
	for ( ; it != geometry.end(); it++, it_s++) {
		std::vector<std::string> txs;
		if ((*it).compare("CIRCLE") == 0) txs = parse_circle(*it_s);
		else if ((*it).compare("POLYGON") == 0) txs = parse_polygon(*it_s);
		else if ((*it).compare("MULTIPOLYGON") == 0) txs = parse_multipolygon(*it_s);
		else if ((*it).compare("MULTIPOINT") == 0) txs = parse_multipoint(*it_s);
		else if ((*it).compare("TRIANGLE") == 0) txs = parse_polygon(*it_s);
		if (first) {
			names_f = txs;
			first = false;
		} else {
			names_f = and_trixels(names_f, txs);
		}
	}

	// Python for the Polygon.
	Constraint c;
	Geometry g(c);
	HTM _htm(partials, error, g);
	_htm.writePython3D("GEOMETRYINTERSECTION.py", gv, names_f);

	return names_f;
}


// String tokenizer.
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
	int pos = 0;
	for ( ;it1 != txs1.end(); ) {
		std::vector<std::string>::iterator it2(txs2.begin());
		bool no_changes = true;
		for ( ;it2 != txs2.end(); ) {
			int s1 = (*it1).size(), s2 = (*it2).size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				if (s1 == s2) {
					txs1.erase(it1);
					txs2.erase(it2);
				} else {
					std::vector<std::string> txs_aux = get_trixels(*it2, s1 - s2, *it1);
					txs1.erase(it1);
					txs2.erase(it2);
					txs2.insert(it2, txs_aux.begin(), txs_aux.end());
				}
				no_changes = false;
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it1, s2 - s1, *it2);
				txs2.erase(it2);
				txs1.erase(it1);
				txs1.insert(it1, txs_aux.begin(), txs_aux.end());
				it1 = txs1.begin() + pos;
				continue;
			}
			it2++;
		}

		if (no_changes) {
			it1++;
			pos++;
		}
	}

	res = txs1;
	res.insert(res.end(), txs2.begin(), txs2.end());

	return res;
}


// Join of two sets of trixels.
std::vector<std::string>
EWKT_Parser::or_trixels(std::vector<std::string> &txs1, std::vector<std::string> &txs2)
{
	std::vector<std::string> res;
	std::vector<std::string>::iterator it1(txs1.begin());
	int pos = 0;
	for ( ;it1 != txs1.end(); ) {
		std::vector<std::string>::iterator it2(txs2.begin());
		bool no_changes = true;
		for ( ;it2 != txs2.end(); ) {
			int s1 = (*it1).size(), s2 = (*it2).size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it2, s1 - s2, *it1);
				txs1.erase(it1);
				no_changes = false;
				break;
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it1, s2 - s1, *it2);
				txs2.erase(it2);
				continue;
			}
			it2++;
		}

		if (no_changes) {
			it1++;
			pos++;
		}
	}

	res = txs1;
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
		for ( ;it2 != txs2.end(); it2++) {
			int s1 = (*it1).size(), s2 = (*it2).size();
			if (s1 >= s2 && (*it1).find(*it2) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it2, s1 - s2, *it1);
				res.push_back(*it1);
			} else if (s2 > s1 && (*it2).find(*it1) == 0) {
				std::vector<std::string> txs_aux = get_trixels(*it1, s2 - s1, *it2);
				res.push_back(*it2);
			}
		}
	}

	return res;
}


std::vector<std::string>
EWKT_Parser::get_trixels(std::string &father, int depth, std::string &son)
{
	std::vector<std::string> resul;
	std::vector<std::string> sonsF;
	resul.push_back(father);

	for (int i = 0; i < depth; i++) {
		std::vector<std::string> sons;
		std::vector<std::string>::const_iterator it(resul.begin());
		for ( ; it != resul.end(); it++) {
			switch (son.at(father.size() + i)) {
				case '0':
					sons.push_back(*it + "0");
					sonsF.push_back(*it + "1");
					sonsF.push_back(*it + "2");
					sonsF.push_back(*it + "3");
					break;
				case '1':
					sonsF.push_back(*it + "0");
					sons.push_back(*it + "1");
					sonsF.push_back(*it + "2");
					sonsF.push_back(*it + "3");
					break;
				case '2':
					sonsF.push_back(*it + "0");
					sonsF.push_back(*it + "1");
					sons.push_back(*it + "2");
					sonsF.push_back(*it + "3");
					break;
				case '3':
					sonsF.push_back(*it + "0");
					sonsF.push_back(*it + "1");
					sonsF.push_back(*it + "2");
					sons.push_back(*it + "3");
					break;
			}
		}
		resul = sons;
	}

	return sonsF;
}


/*void
EWKT_Parser::printVector(const std::vector<std::string> &v)
{
	std::vector<std::string>::const_iterator it(v.begin());
	for ( ; it != v.end(); it++) {
		printf("%s  ", (*it).c_str());
	}
	printf("\n");
}*/