/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "htm.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

#include "circle.h"
#include "convex.h"
#include "multicircle.h"
#include "point.h"


// Number of decimal places to print the file python.
constexpr int HTM_DIGITS          = 50;
constexpr double HTM_INC_CIRCLE   = RAD_PER_CIRCUMFERENCE / 50.0;
constexpr int HTM_LINE_POINTS     = 25;


std::vector<std::string>
HTM::trixel_union(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

	return res;
}


std::vector<std::string>
HTM::trixel_intersection(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty() || txs2.empty()) {
		return std::vector<std::string>();
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() < txs2.size() ? txs1.size() : txs2.size());

	const auto it1_e = txs1.end();
	const auto it2_e = txs2.end();
	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				res.push_back(std::move(*it1));
				++it1;
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				res.push_back(std::move(*it2));
				++it2;
			} else {
				++it1;
			}
		}
	}

	return res;
}


std::vector<range_t>
HTM::range_union(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? std::move(*it1++) : std::move(*it2++));

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			insertGreaterRange(res, std::move(*it1));
			++it1;
		} else {
			insertGreaterRange(res, std::move(*it2));
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start - 1) {   // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		} else if (prev.end < it1->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it1->end;
		}
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start - 1) {  // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		} else if (prev.end < it2->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it2->end;
		}
		++it2;
	}

	return res;
}


std::vector<range_t>
HTM::range_intersection(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty() || rs2.empty() || rs1.front().start > rs2.back().end || rs1.back().end < rs2.front().start) {
		return std::vector<range_t>();
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() < rs2.size() ? rs1.size() : rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			if (it1->end >= it2->start) {
				if (it1->end < it2->end) {
					insertGreaterRange(res, range_t(it2->start, it1->end));
				} else {
					insertGreaterRange(res, *it2);
				}
			}
			++it1;
		} else {
			if (it2->end >= it1->start) {
				if (it2->end < it1->end) {
					insertGreaterRange(res, range_t(it1->start, it2->end));
				} else {
					insertGreaterRange(res, *it1);
				}
			}
			++it2;
		}
	}

	return res;
}


std::vector<range_t>
HTM::range_exclusive_disjunction(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? *it1++ : *it2++);

	while (it1 != it1_e && it2 != it2_e) {
		auto& prev = res.back();
		if (it1->start < it2->start) {
			if (prev.end > it1->start) {
				if (prev.end > it1->end) {
					std::swap(prev.end, it1->start);
					std::swap(it1->start, it1->end);
					insertGreaterRange(res, std::move(*it1));
				} else {
					std::swap(prev.end, it1->start);
					insertGreaterRange(res, std::move(*it1));
				}
			} else {
				insertGreaterRange(res, std::move(*it1));
			}
			++it1;
		} else {
			if (prev.end > it2->start) {
				if (prev.end > it2->end) {
					std::swap(prev.end, it2->start);
					std::swap(it2->start, it2->end);
					insertGreaterRange(res, std::move(*it2));
				} else {
					std::swap(prev.end, it2->start);
					insertGreaterRange(res, std::move(*it2));
				}
			} else {
				insertGreaterRange(res, std::move(*it2));
			}
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start - 1) {   // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		} else {
			if (prev.end > it1->start) {
				if (prev.end > it1->end) {
					std::swap(prev.end, it1->start);
					std::swap(it1->start, it1->end);
					insertGreaterRange(res, std::move(*it1));
				} else {
					std::swap(prev.end, it1->start);
					insertGreaterRange(res, std::move(*it1));
				}
			} else {
				insertGreaterRange(res, std::move(*it1));
			}
			++it1;
		}
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start - 1) {   // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		} else {
			if (prev.end > it2->start) {
				if (prev.end > it2->end) {
					std::swap(prev.end, it2->start);
					std::swap(it2->start, it2->end);
					insertGreaterRange(res, std::move(*it2));
				} else {
					std::swap(prev.end, it2->start);
					insertGreaterRange(res, std::move(*it2));
				}
			} else {
				insertGreaterRange(res, std::move(*it2));
			}
			++it2;
		}
	}

	return res;
}


const trixel_t&
HTM::startTrixel(const Cartesian& coord) noexcept
{
	int num;
	if (coord.x > 0 && coord.y >= 0) {
		num = (coord.z >= 0) ? 3 : 4; // N3  S0
	} else if (coord.x <= 0 && coord.y > 0) {
		num = (coord.z >= 0) ? 2 : 5; // N2  S1
	} else if (coord.x < 0 && coord.y <= 0) {
		num = (coord.z >= 0) ? 1 : 6; // N1  S2
	} else if (coord.x >= 0 && coord.y < 0) {
		num = (coord.z >= 0) ? 0 : 7; // N0  S3
	} else {
		num = (coord.z >= 0) ? 3 : 4; // N3  S0
	}
	return start_trixels[num];
}


Cartesian
HTM::midPoint(const Cartesian& v0, const Cartesian& v1)
{
	auto w = v0 + v1;
	w.normalize();
	return w;
}


bool
HTM::thereisHole(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (v0 ^ v1) * c.center < 0.0 && (v1 ^ v2) * c.center < 0.0 && (v2 ^ v0) * c.center < 0.0;
}


Constraint
HTM::getBoundingCircle(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	// Normal vector to the triangle plane.
	Constraint boundingCircle((v1 - v0) ^ (v2 - v1));
	boundingCircle.arcangle = std::acos(v0 * boundingCircle.center);
	return boundingCircle;
}


bool
HTM::intersectConstraints(const Constraint& c1, const Constraint& c2)
{
	return std::acos(c1.center * c2.center) < (c1.arcangle + c2.arcangle);
}


bool
HTM::insideVertex_Trixel( const Cartesian& v, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (v0 ^ v1) * v > 0 && (v1 ^ v2) * v > 0 && (v2 ^ v0) * v > 0;
}


bool
HTM::insideVertex_Constraint(const Cartesian& v, const Constraint& c)
{
	return c.center * v > c.distance;
}


bool
HTM::intersectConstraint_EdgeTrixel(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	if (intersection(c, v0, v1) ||
		intersection(c, v1, v2) ||
		intersection(c, v2, v0)) return true;
	return false;
}


bool
HTM::intersection(const Constraint& c, const Cartesian& v1, const Cartesian& v2)
{
	double gamma1 = v1 * c.center;
	double gamma2 = v2 * c.center;
	double cos_t = v1 * v2;
	double square_u = (1 - cos_t) / (1 + cos_t);

	double _a = - square_u * (gamma1 + c.distance);
	double _b = gamma1 * (square_u - 1) + gamma2 * (square_u + 1);
	double _c = gamma1 - c.distance;
	double aux = (_b * _b) - (4 * _a * _c);

	if (aux < 0.0 || (_a > -DBL_TOLERANCE && _a < DBL_TOLERANCE)) return false;


	aux = std::sqrt(aux);
	_a = 2 * _a;
	_b = -_b;
	double r1 = (_b + aux) / _a;
	double r2 = (_b - aux) / _a;

	if ((r1 >= 0.0 && r1 <= 1.0) || (r2 >= 0.0 && r2 <= 1.0)) return true;

	return false;
}


void
HTM::simplifyTrixels(std::vector<std::string>& trixels)
{
	for (size_t i = 0;  trixels.size() - i > 3; ) {
		auto tlen = trixels[i].length();
		if (tlen > 2) {
			auto j = i + 1;
			auto k = i + 2;
			auto l = i + 3;
			if (trixels[j].length() == tlen && trixels[k].length() == tlen && trixels[l].length() == tlen) {
				--tlen;
				bool equals = true;
				for (size_t p = 0; p < tlen; ++p) {
					const auto& c = trixels[i][p];
					if (!(trixels[j][p] == c && trixels[k][p] == c && trixels[l][p] == c)) {
						equals = false;
						break;
					}
				}
				if (equals) {
					auto it = trixels.erase(trixels.begin() + j);
					it = trixels.erase(it);
					trixels.erase(it);
					trixels[i].pop_back();
					i < 3 ? i = 0 : i -= 3;
					continue;
				}
			}
		}
		++i;
	}
}


void
HTM::simplifyRanges(std::vector<range_t>& ranges)
{
	if (ranges.empty()) return;

	for (auto it = ranges.begin() + 1; it != ranges.end(); ) {
		// Get previous range.
		auto tmp = it - 1;
		// Test if current range is not overlapping with previous.
		if (tmp->end < it->start - 1) {   // (start - 1) for join adjacent integer ranges.
			++it;
		} else if (tmp->end < it->end) {  // if ranges overlap, previous range end is updated.
			tmp->end = it->end;
			it = ranges.erase(it);
		} else {
			// Ranges overlapping.
			it = ranges.erase(it);
		}
	}
}


std::string
HTM::getTrixelName(const Cartesian& coord)
{
	const auto& start_trixel = startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto name = start_trixel.name;

	// Search in children's trixel
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		if (insideVertex_Trixel(coord, v0, w2, w1)) {
			name.push_back('0');
			v1 = w2;
			v2 = w1;
		} else if (insideVertex_Trixel(coord, v1, w0, w2)) {
			name.push_back('1');
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if (insideVertex_Trixel(coord, v2, w1, w0)) {
			name.push_back('2');
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else {
			name.push_back('3');
			v0 = w0;
			v1 = w1;
			v2 = w2;
		}
	}

	return name;
}


uint64_t
HTM::getId(const Cartesian& coord)
{
	const auto& start_trixel = startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto id = start_trixel.id;

	// Search in children's trixel
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		id <<= 2;
		if (insideVertex_Trixel(coord, v0, w2, w1)) {
			v1 = w2;
			v2 = w1;
		} else if (insideVertex_Trixel(coord, v1, w0, w2)) {
			id += 1;
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if (insideVertex_Trixel(coord, v2, w1, w0)) {
			id += 2;
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else {
			id += 3;
			v0 = w0;
			v1 = w1;
			v2 = w2;
		}
	}

	return id;
}


uint64_t
HTM::getId(const std::string& name)
{
	const auto len = name.length();

	uint64_t id = name[0] == 'N' ? 2 : 3;

	for (size_t i = 1; i < len; ++i) {
		id <<= 2;
		id |= name[i] - '0';
	}

	return id;
}


range_t
HTM::getRange(uint64_t id, uint8_t level)
{
	if (level < HTM_MAX_LEVEL) {
		int8_t mask = (HTM_MAX_LEVEL - level) << 1;
		uint64_t start = id << mask;
		return range_t(start, start + ((uint64_t) 1 << mask) - 1);
	} else {
		return range_t(id, id);
	}
}


range_t
HTM::getRange(const std::string& name)
{
	return getRange(getId(name), name.length() - 2);
}


std::vector<std::string>
HTM::getTrixels(const std::vector<range_t>& ranges)
{
	std::vector<std::string> trixels;
	trixels.reserve(ranges.size());
	std::bitset<HTM_BITS_ID> s, e;
	for (const auto& range : ranges) {
		s = range.start;
		e = range.end;
		size_t idx = 0;
		while (idx < s.size() - 4 && s.test(idx) == 0 && e.test(idx) == 1 && s.test(idx + 1) == 0 && e.test(idx + 1) == 1) {
			idx += 2;
		}
		uint64_t inc = std::pow(2, idx), start = range.start;
		size_t len = (HTM_BITS_ID - idx) / 2;
		while (start < range.end) {
			std::string trixel;
			trixel.reserve(len);
			s = start;
			size_t i = HTM_BITS_ID - 2;
			if (s.test(i)) {
				trixel.push_back('S');
			} else {
				trixel.push_back('N');
			}
			while (--i >= idx) {
				trixel.push_back('0' + 2 * s.test(i) + s.test(i - 1));
				--i;
			}
			start += inc;
			trixels.push_back(std::move(trixel));
		}
	}
	HTM::simplifyTrixels(trixels);
	return trixels;
}


void
HTM::getCorners(const std::string& name, Cartesian& v0, Cartesian& v1, Cartesian& v2)
{
	const auto& start_trixel = start_trixels[name[1] - '0' + (name[0] == 'S' ? 4 : 0)];
	v0 = start_vertices[start_trixel.v0];
	v1 = start_vertices[start_trixel.v1];
	v2 = start_vertices[start_trixel.v2];

	size_t i = 2, len = name.length();
	while (i < len) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		switch (name[i]) {
			case '0':
				v1 = w2;
				v2 = w1;
				break;
			case '1':
				v0 = v1;
				v1 = w0;
				v2 = w2;
				break;
			case '2':
				v0 = v2;
				v1 = w1;
				v2 = w0;
				break;
			case '3':
				v0 = w0;
				v1 = w1;
				v2 = w2;
				break;
		}
		++i;
	}
}


std::string
HTM::getConstraint3D(const Constraint& bCircle, char color)
{
	char x0s[HTM_DIGITS];
	char y0s[HTM_DIGITS];
	char z0s[HTM_DIGITS];
	snprintf(x0s, HTM_DIGITS, "%.50f", bCircle.center.x);
	snprintf(y0s, HTM_DIGITS, "%.50f", bCircle.center.y);
	snprintf(z0s, HTM_DIGITS, "%.50f", bCircle.center.z);

	std::string xs = "x = [" + std::string(x0s) + "]\n";
	xs += "y = [" + std::string(y0s) + "]\n";
	xs += "z = [" + std::string(z0s) + "]\n";
	xs += "ax.plot3D(x, y, z, '" + std::string(1, color) + "o', linewidth = 2.0)\n\n"; // 211

	auto c_inv = bCircle.center;
	c_inv.inverse();

	std::string ys, zs;
	std::string x0, y0, z0;
	xs += "x = [";
	ys = "y = [";
	zs = "z = [";
	auto a = bCircle.center.y == 0 ? Cartesian(0.0, 1.0, 0.0) : Cartesian(1.0, - (1.0 * (bCircle.center.x + bCircle.center.z) / bCircle.center.y), 1.0);
	a.normalize();
	auto b = a ^ bCircle.center;

	Cartesian vc;
	auto f = std::sin(bCircle.arcangle);
	int i = 0;
	for (double t = 0; t <= RAD_PER_CIRCUMFERENCE; t += HTM_INC_CIRCLE) {
		double rc = f * std::cos(t);
		double rs = f * std::sin(t);
		vc.x = bCircle.distance * bCircle.center.x + rc * a.x + rs * b.x;
		vc.y = bCircle.distance * bCircle.center.y + rc * a.y + rs * b.y;
		vc.z = bCircle.distance * bCircle.center.z + rc * a.z + rs * b.z;
		char vcx[HTM_DIGITS];
		char vcy[HTM_DIGITS];
		char vcz[HTM_DIGITS];
		snprintf(vcx, HTM_DIGITS, "%.50f", vc.x);
		snprintf(vcy, HTM_DIGITS, "%.50f", vc.y);
		snprintf(vcz, HTM_DIGITS, "%.50f", vc.z);
		xs += std::string(vcx) + ", ";
		ys += std::string(vcy) + ", ";
		zs += std::string(vcz) + ", ";
		if (t == 0.0) {
			x0 = std::string(vcx);
			y0 = std::string(vcy);
			z0 = std::string(vcz);
		}
		++i;
	}
	xs += x0 + "]\n";
	ys += y0 + "]\n";
	zs += z0 + "]\n";

	return xs + ys + zs;
}


void
HTM::writeGoogleMap(const std::string& file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels, const std::string& path_google_map)
{
	std::ofstream fs(file + ".py");
	fs.precision(HTM_DIGITS);

	fs << "import sys\n";
	fs << "import os\n\n";
	fs << "sys.path.append(os.path.abspath('" << path_google_map << "'))\n\n";
	fs << "from google_map_plotter import GoogleMapPlotter\n";

	// Draw Geometry.
	switch (g->getType()) {
		case Geometry::Type::POINT: {
			double lat, lon, height;
			std::static_pointer_cast<Point>(g)->getCartesian().toGeodetic(lat, lon, height);
			fs << "mymap = GoogleMapPlotter(" << lat << ", " << lon << ",  20)\n";
			fs << "mymap.marker(" << lat << ", " << lon << ",  'red')\n";
			break;
		}
		case Geometry::Type::CIRCLE: {
			double lat, lon, height;
			const auto& constraint = std::static_pointer_cast<Circle>(g)->getConstraint();
			constraint.center.toGeodetic(lat, lon, height);
			fs << "mymap = GoogleMapPlotter(" << lat << ", " << lon << ",  " << (20 - 2 * std::log10(constraint.radius)) << ")\n";
			fs << "mymap.marker(" << lat << ", " << lon << ",  'red')\n";
			if (constraint.sign == Constraint::Sign::NEG) {
				fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#FF0000', ew=2)\n";
			} else {
				fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#0000FF', ew=2)\n";
			}
			break;
		}
		case Geometry::Type::MULTICIRCLE: {
			double lat, lon, height;
			const auto& circles = std::static_pointer_cast<MultiCircle>(g)->getCircles();
			bool first = true;
			for (const auto& circle : circles) {
				const auto& constraint = circle.getConstraint();
				constraint.center.toGeodetic(lat, lon, height);
				if (first) {
					fs << "mymap = GoogleMapPlotter(" << lat << ", " << lon << ",  " << (20 - 2 * std::log10(constraint.radius)) << ")\n";
					first = false;
				}
				fs << "mymap.marker(" << lat << ", " << lon << ",  'red')\n";
				if (constraint.sign == Constraint::Sign::NEG) {
					fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#FF0000', ew=2)\n";
				} else {
					fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#0000FF', ew=2)\n";
				}
			}
			break;
		}
		case Geometry::Type::CONVEX: {
			double lat, lon, height;
			const auto& circles = std::static_pointer_cast<Convex>(g)->getCircles();
			bool first = true;
			for (const auto& circle : circles) {
				const auto& constraint = circle.getConstraint();
				constraint.center.toGeodetic(lat, lon, height);
				if (first) {
					fs << "mymap = GoogleMapPlotter(" << lat << ", " << lon << ",  " << (20 - 2 * std::log10(constraint.radius)) << ")\n";
					first = false;
				}
				fs << "mymap.marker(" << lat << ", " << lon << ",  'red')\n";
				if (constraint.sign == Constraint::Sign::NEG) {
					fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#FF0000', ew=2)\n";
				} else {
					fs << "mymap.circle(" << lat << ", " << lon << ", " << constraint.arcangle << ", '#0000FF', ew=2)\n";
				}
			}
			break;
		}
	}

	// Draw trixels.
	Cartesian v0, v1, v2;
	for (const auto& trixel : trixels) {
		getCorners(trixel, v0, v1, v2);
		v0.scale = M_PER_RADIUS_EARTH;
		v1.scale = M_PER_RADIUS_EARTH;
		v2.scale = M_PER_RADIUS_EARTH;

		double lat[3], lon[3], height[3];
		v0.toGeodetic(lat[0], lon[0], height[0]);
		v1.toGeodetic(lat[1], lon[1], height[1]);
		v2.toGeodetic(lat[2], lon[2], height[2]);

		fs << "mymap.polygon(";
		fs << "[" << lat[0] << ", " << lat[1] << ", " << lat[2] << "],";
		fs << "[" << lon[0] << ", " << lon[1] << ", " << lon[2] << "],";
		fs << "edge_color='cyan', edge_width=2, face_color='blue', face_alpha=0.2)\n";
	}
	fs << "mymap.draw('" << file << ".html')";
	fs.close();
}


void
HTM::writePython3D(const std::string& file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels)
{
	std::ofstream fs(file + ".py");
	fs.precision(HTM_DIGITS);

	fs << "import mpl_toolkits.mplot3d as a3\n";
	fs << "import matplotlib.pyplot as plt\n";
	fs << "import numpy as np\n\n\n";
	fs << "ax = a3.Axes3D(plt.figure())\n";

	// Draw Geometry.
	bool sphere =  false;
	double umbral = 0.95;
	switch (g->getType()) {
		case Geometry::Type::POINT: {
			const auto& c = std::static_pointer_cast<Point>(g)->getCartesian();
			fs << "x = [" << c.x << "]\n";
			fs << "y = [" << c.y << "]\n";
			fs << "z = [" << c.z << "]\n";
			fs << "ax.plot3D(x, y, z, 'ko', linewidth = 2.0)\n\n";
			break;
		}
		case Geometry::Type::CIRCLE: {
			const auto& constraint = std::static_pointer_cast<Circle>(g)->getConstraint();
			char color;
			if (constraint.sign == Constraint::Sign::NEG) {
				color = 'r';
				sphere = true;
			} else {
				color = 'b';
				if (constraint.distance < umbral) {
					sphere = true;
				}
			}
			fs << getConstraint3D(constraint, color) << "ax.plot3D(x, y, z, '" << color << "-', linewidth = 2.0)\n\n";
			break;
		}
		case Geometry::Type::MULTICIRCLE: {
			const auto& multicircle = std::static_pointer_cast<MultiCircle>(g);
			for (const auto& circle : multicircle->getCircles()) {
				const auto& constraint = circle.getConstraint();
				char color;
				if (constraint.sign == Constraint::Sign::NEG) {
					color = 'r';
					sphere = true;
				} else {
					color = 'b';
					if (constraint.distance < umbral) {
						sphere = true;
					}
				}
				fs << getConstraint3D(constraint, color) << "ax.plot3D(x, y, z, '" << color << "-', linewidth = 2.0)\n\n";
			}
			break;
		}
		case Geometry::Type::CONVEX: {
			const auto& convex = std::static_pointer_cast<Convex>(g);
			for (const auto& circle : convex->getCircles()) {
				char color;
				const auto& constraint = circle.getConstraint();
				if (constraint.sign == Constraint::Sign::NEG) {
					color = 'r';
					sphere = true;
				} else {
					color = 'b';
					if (constraint.distance < umbral) {
						sphere = true;
					}
				}
				fs << getConstraint3D(constraint, color) << "ax.plot3D(x, y, z, '" << color << "-', linewidth = 2.0)\n\n";
			}
			break;
		}
	}

	std::string x, y, z;
	Cartesian v0, v1, v2;
	// Draw trixels.
	std::string rule_trixel;
	std::string show_graphics;
	if (sphere) {
		rule_trixel = "ax.plot3D(x, y, z, 'c-', linewidth = 2.0)\n";
		// Draw unary sphere.
		show_graphics = "phi, theta = np.mgrid[0.0:np.pi:50j, 0.0:2.0*np.pi:50j];\n" \
			"x = np.sin(phi) * np.cos(theta);\n" \
			"y = np.sin(phi) * np.sin(theta);\n" \
			"z = np.cos(phi);\n" \
			"ax.plot_surface(x, y, z,  rstride=1, cstride=1, color='g', alpha=0.03, linewidth=1)\n" \
			"plt.ion()\nplt.grid()\nplt.show()";
	} else {
		rule_trixel = "vtx = [zip(x, y, z)];\n" \
			"tri = a3.art3d.Poly3DCollection(vtx, alpha=0.3);\n" \
			"tri.set_color('cyan')\n" \
			"tri.set_edgecolor('c')\n" \
			"ax.add_collection3d(tri)\n";
		show_graphics = "plt.ion()\nplt.grid()\nplt.show()";
	}
	for (const auto& trixel : trixels) {
		getCorners(trixel, v0, v1, v2);
		char vx[HTM_DIGITS];
		char vy[HTM_DIGITS];
		char vz[HTM_DIGITS];
		x = "x = [";
		y = "y = [";
		z = "z = [";
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v0 + inc * v1).normalize();
			snprintf(vx, HTM_DIGITS, "%.50f", mp.x);
			snprintf(vy, HTM_DIGITS, "%.50f", mp.y);
			snprintf(vz, HTM_DIGITS, "%.50f", mp.z);
			x += vx + std::string(", ");
			y += vy + std::string(", ");
			z += vz + std::string(", ");
		}
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v1 + inc * v2).normalize();
			snprintf(vx, HTM_DIGITS, "%.50f", mp.x);
			snprintf(vy, HTM_DIGITS, "%.50f", mp.y);
			snprintf(vz, HTM_DIGITS, "%.50f", mp.z);
			x += vx + std::string(", ");
			y += vy + std::string(", ");
			z += vz + std::string(", ");
		}
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v2 + inc * v0).normalize();
			snprintf(vx, HTM_DIGITS, "%.50f", mp.x);
			snprintf(vy, HTM_DIGITS, "%.50f", mp.y);
			snprintf(vz, HTM_DIGITS, "%.50f", mp.z);
			x += vx + std::string(", ");
			y += vy + std::string(", ");
			z += vz + std::string(", ");
		}
		// Close the trixel.
		snprintf(vx, HTM_DIGITS, "%.50f", v0.x);
		snprintf(vy, HTM_DIGITS, "%.50f", v0.y);
		snprintf(vz, HTM_DIGITS, "%.50f", v0.z);
		x += vx + std::string("]\n");
		y += vy + std::string("]\n");
		z += vz + std::string("]\n");
		fs << (x + y + z);
		fs << rule_trixel;
	}
	fs << show_graphics;
	fs.close();
}
