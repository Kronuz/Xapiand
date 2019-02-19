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

#include "htm.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "collection.h"
#include "string.hh"


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


/*
 * Fills result with the trixels that conform to the father except trixel's son.
 *   Father      Son			 Trixels back:
 *     /\	     /\
 *    /__\      /__\	   =>	     __
 *   /\  /\					       /\  /\
 *  /__\/__\					  /__\/__\
 *
 * result is sort.
 */
static inline void exclusive_disjunction(std::vector<std::string>& result, const std::string& father, const std::string& son, size_t depth) {
	if (depth != 0u) {
		switch (son.at(father.length())) {
			case '0':
				exclusive_disjunction(result, father + std::string(1, '0'), son, --depth);
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '1':
				result.push_back(father + std::string(1, '0'));
				exclusive_disjunction(result, father + std::string(1, '1'), son, --depth);
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '2':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				exclusive_disjunction(result, father + std::string(1, '2'), son, --depth);
				result.push_back(father + std::string(1, '3'));
				return;
			case '3':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				exclusive_disjunction(result, father + std::string(1, '3'), son, --depth);
				return;
		}
	}
}


std::vector<std::string>
HTM::trixel_exclusive_disjunction(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != txs1.end() && it2 != txs2.end()) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				size_t depth = it1->length() - it2->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it2, *it1, depth);
				it2 = txs2.erase(it2);
				it2 = txs2.insert(it2, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it1 = txs1.erase(it1);
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				size_t depth = it2->length() - it1->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it1, *it2, depth);
				it1 = txs1.erase(it1);
				it1 = txs1.insert(it1, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it2 = txs2.erase(it2);
			} else {
				++it1;
			}
		}
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

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
		}
		if (prev.end < it1->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it1->end;
		}
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start - 1) {  // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		if (prev.end < it2->end) {  // if ranges overlap, previous range end is updated.
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
				if (it1->end <= it2->end) {
					insertGreaterRange(res, range_t(it2->start, it1->end));
					++it1;
				} else {
					insertGreaterRange(res, *it2);
					++it2;
				}
			} else {
				++it1;
			}
		} else {
			if (it2->end >= it1->start) {
				if (it2->end <= it1->end) {
					insertGreaterRange(res, range_t(it1->start, it2->end));
					++it2;
				} else {
					insertGreaterRange(res, *it1);
					++it1;
				}
			} else {
				++it2;
			}
		}
	}

	return res;
}


static inline void exclusive_disjunction(std::vector<range_t>& res, range_t& range) {
	if (res.empty()) {
		res.push_back(std::move(range));
		return;
	}

	auto& prev = res.back();
	if (prev.start == range.start) {
		if (prev.end > range.end) {
			prev.start = range.end + 1;
		} else if (prev.end < range.end) {
			prev.start = prev.end + 1;
			prev.end = range.end;
		} else {
			res.pop_back();
		}
	} else {
		if (range.start < prev.start) {
			std::swap(prev, range);
		}
		if (prev.end < range.start) {
			HTM::insertGreaterRange(res, std::move(range));
		} else {
			if (prev.end == range.end) {
				prev.end = --range.start;
			} else if (prev.end < range.end) {
				std::swap(++prev.end, --range.start);
				HTM::insertGreaterRange(res, std::move(range));
			} else {
				std::swap(prev.end, --range.start);
				std::swap(range.start, ++range.end);
				HTM::insertGreaterRange(res, std::move(range));
			}
		}
	}
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
		if (it1->start < it2->start) {
			exclusive_disjunction(res, *it1);
			++it1;
		} else {
			exclusive_disjunction(res, *it2);
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start) {
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		}
		exclusive_disjunction(res, *it1);
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start) {
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		exclusive_disjunction(res, *it2);
		++it2;
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
	return (
		intersection(c, v0, v1) ||
		intersection(c, v1, v2) ||
		intersection(c, v2, v0)
	);
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

	if (aux < 0.0 || (_a > -DBL_TOLERANCE && _a < DBL_TOLERANCE)) {
		return false;
	}

	aux = std::sqrt(aux);
	_a = 2 * _a;
	_b = -_b;
	double r1 = (_b + aux) / _a;
	double r2 = (_b - aux) / _a;

	return ((r1 >= 0.0 && r1 <= 1.0) || (r2 >= 0.0 && r2 <= 1.0));
}


void
HTM::simplifyTrixels(std::vector<std::string>& trixels)
{
	if (trixels.empty()) {
		return;
	}

	// Delete duplicates and redundants.
	for (auto it = trixels.begin(); it != trixels.end() - 1; ) {
		auto n_it = it + 1;
		if (n_it->find(*it) == 0) {
			trixels.erase(n_it);
		} else {
			++it;
		}
	}

	// To compact.
	for (auto it = trixels.begin(); trixels.size() > 3 && it < trixels.end() - 3; ) {
		auto tlen = it->length();
		if (tlen > 2) {
			auto it_j = it + 1;
			auto it_k = it + 2;
			auto it_l = it + 3;
			if (it_j->length() == tlen && it_k->length() == tlen && it_l->length() == tlen) {
				--tlen;
				bool equals = true;
				for (size_t p = 0; p < tlen; ++p) {
					const auto& c = (*it)[p];
					if ((*it_j)[p] != c || (*it_k)[p] != c || (*it_l)[p] != c) {
						equals = false;
						break;
					}
				}
				if (equals) {
					it_j = trixels.erase(it_j);
					it_j = trixels.erase(it_j);
					trixels.erase(it_j);
					it->pop_back();
					it_j = trixels.begin();
					it < it_j + 3 ? it = it_j : it -= 3;
					continue;
				}
			}
		}
		++it;
	}
}


void
HTM::simplifyRanges(std::vector<range_t>& ranges)
{
	if (!ranges.empty()) {
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


std::string
HTM::getTrixelName(uint64_t id)
{
	uint8_t last_pos = std::ceil(std::log2(id));
	++last_pos;
	last_pos &= ~1;  // Must be multiple of two.
	std::string trixel;
	trixel.reserve(last_pos / 2);
	last_pos -= 2;
	uint64_t mask = static_cast<uint64_t>(3) << last_pos;
	trixel.push_back(((id & mask) >> last_pos) == 3 ? 'S' : 'N');
	while ((mask >>= 2) != 0u) {
		last_pos -= 2;
		trixel.push_back('0' + ((id & mask) >> last_pos));
	}

	return trixel;
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
		return {start, start + ((uint64_t) 1 << mask) - 1};
	}
	return {id, id};
}


range_t
HTM::getRange(const std::string& name)
{
	return getRange(getId(name), name.length() - 2);
}


static inline void get_trixels(std::vector<std::string>& trixels, uint64_t start, uint64_t end) {
	if (start == end) {
		trixels.push_back(HTM::getTrixelName(start));
		return;
	}

	uint8_t log_inc = std::ceil(std::log2(end - start));
	log_inc &= ~1;  // Must be multiple of two.
	uint64_t max_inc = std::pow(2, log_inc);

	uint64_t mod = start % max_inc;
	uint64_t _start = mod != 0u ? start + max_inc - mod : start;

	while (end < (_start + max_inc - 1) || log_inc > HTM_START_POS) {
		log_inc -= 2;
		max_inc = std::pow(2, log_inc);
		mod = start % max_inc;
		_start = mod != 0u ? start + max_inc - mod : start;
	}

	if (_start > start) {
		get_trixels(trixels, start, _start - 1);
	}

	uint64_t _end = end - max_inc + 2;
	while (_start < _end) {
		trixels.push_back(HTM::getTrixelName(_start >> log_inc));
		_start += max_inc;
	}

	if (_start <= end) {
		get_trixels(trixels, _start, end);
	}
}


std::vector<std::string>
HTM::getTrixels(const std::vector<range_t>& ranges)
{
	std::vector<std::string> trixels;
	trixels.reserve(ranges.size());
	for (const auto& range : ranges) {
		get_trixels(trixels, range.start, range.end);
	}

	return trixels;
}


static inline void get_id_trixels(std::vector<uint64_t>& id_trixels, uint64_t start, uint64_t end) {
	if (start == end) {
		id_trixels.push_back(start);
		return;
	}

	uint8_t log_inc = std::ceil(std::log2(end - start));
	log_inc &= ~1;  // Must be multiple of two.
	uint64_t max_inc = std::pow(2, log_inc);

	uint64_t mod = start % max_inc;
	uint64_t _start = mod != 0u ? start + max_inc - mod : start;

	while (end < (_start + max_inc - 1) || log_inc > HTM_START_POS) {
		log_inc -= 2;
		max_inc = std::pow(2, log_inc);
		mod = start % max_inc;
		_start = mod != 0u ? start + max_inc - mod : start;
	}

	if (_start > start) {
		get_id_trixels(id_trixels, start, _start - 1);
	}

	uint64_t _end = end - max_inc + 2;
	while (_start < _end) {
		id_trixels.push_back(_start >> log_inc);
		_start += max_inc;
	}

	if (_start <= end) {
		get_id_trixels(id_trixels, _start, end);
	}
}


std::vector<uint64_t>
HTM::getIdTrixels(const std::vector<range_t>& ranges)
{
	std::vector<uint64_t> id_trixels;
	id_trixels.reserve(ranges.size());
	for (const auto& range : ranges) {
		get_id_trixels(id_trixels, range.start, range.end);
	}

	return id_trixels;
}


std::tuple<Cartesian, Cartesian, Cartesian>
HTM::getCorners(const std::string& name)
{
	const auto& start_trixel = start_trixels[name[1] - '0' + (name[0] == 'S' ? 4 : 0)];
	Cartesian v0 = start_vertices[start_trixel.v0];
	Cartesian v1 = start_vertices[start_trixel.v1];
	Cartesian v2 = start_vertices[start_trixel.v2];

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
			default:
				THROW(HTMError, "Invalid trixel's name: {}", name);
		}
		++i;
	}

	return std::make_tuple(std::move(v0), std::move(v1), std::move(v2));
}


/*
 *  _____         _     _   _ _____ __  __
 * |_   _|__  ___| |_  | | | |_   _|  \/  |
 *   | |/ _ \/ __| __| | |_| | | | | |\/| |
 *   | |  __/\__ \ |_  |  _  | | | | |  | |
 *   |_|\___||___/\__| |_| |_| |_| |_|  |_|
 *
 */


// Number of decimal places to print the file python.
constexpr int HTM_DIGITS          = 50;
constexpr double HTM_INC_CIRCLE   = RAD_PER_CIRCUMFERENCE / 50.0;
constexpr int HTM_LINE_POINTS     = 25;


static std::string getConstraint3D(const Constraint& bCircle, char color) {
	std::string xs = "x = [" + string::format("{}", bCircle.center.x) + "]\n";
	xs += "y = [" + string::format("{}", bCircle.center.y) + "]\n";
	xs += "z = [" + string::format("{}", bCircle.center.z) + "]\n";
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
		auto vcx = string::format("{}", vc.x);
		auto vcy = string::format("{}", vc.y);
		auto vcz = string::format("{}", vc.z);
		xs += vcx + ", ";
		ys += vcy + ", ";
		zs += vcz + ", ";
		if (t == 0.0) {
			x0 = vcx;
			y0 = vcy;
			z0 = vcz;
		}
		++i;
	}
	xs += x0 + "]\n";
	ys += y0 + "]\n";
	zs += z0 + "]\n";

	return xs + ys + zs;
}


static void writeGoogleMap(std::ofstream& fs, const Point& point) {
	const auto latlon = point.getCartesian().toLatLon();
	fs << "mymap.marker(" << latlon.first << ", " << latlon.second << ",  'red')\n";
}


static void writeGoogleMap(std::ofstream& fs, const MultiPoint& multipoint) {
	for (const auto& point : multipoint.getPoints()) {
		writeGoogleMap(fs, point);
	}
}


static void writeGoogleMap(std::ofstream& fs, const Circle& circle) {
	const auto& constraint = circle.getConstraint();
	const auto latlon = constraint.center.toLatLon();
	fs << "mymap.marker(" << latlon.first << ", " << latlon.second << ",  'red')\n";
	if (constraint.sign == Constraint::Sign::NEG) {
		fs << "mymap.circle(" << latlon.first << ", " << latlon.second << ", " << constraint.arcangle << ", '#FF0000', ew=2)\n";
	} else {
		fs << "mymap.circle(" << latlon.first << ", " << latlon.second << ", " << constraint.arcangle << ", '#0000FF', ew=2)\n";
	}
}


static void writeGoogleMap(std::ofstream& fs, const Convex& convex) {
	for (const auto& circle : convex.getCircles()) {
		writeGoogleMap(fs, circle);
	}
}


static void writeGoogleMap(std::ofstream& fs, const Polygon& polygon) {
	std::string lat;
	std::string lon;
	for (const auto& convexpolygon : polygon.getConvexPolygons()) {
		const auto& corners = convexpolygon.getCorners();
		lat.assign(1, '[');
		lon.assign(1, '[');
		for (const auto& corner : corners) {
			const auto latlon = corner.toLatLon();
			lat.append(std::to_string(latlon.first)).push_back(',');
			lon.append(std::to_string(latlon.second)).push_back(',');
		}
		lat.back() = ']';
		lon.back() = ']';
		const auto latlon = convexpolygon.getCentroid().toLatLon();
		fs << "mymap.marker(" << latlon.first << ", " << latlon.second << ",  'red')\n";
		fs << "mymap.polygon(" << lat << ',' << lon << ',' << "edge_color='blue', edge_width=2, face_color='blue', face_alpha=0.2)\n";
	}
}


static void writeGoogleMap(std::ofstream& fs, const MultiCircle& multicircle) {
	for (const auto& circle : multicircle.getCircles()) {
		writeGoogleMap(fs, circle);
	}
}


static void writeGoogleMap(std::ofstream& fs, const MultiConvex& multiconvex) {
	for (const auto& convex : multiconvex.getConvexs()) {
		writeGoogleMap(fs, convex);
	}
}


static void writeGoogleMap(std::ofstream& fs, const MultiPolygon& multipolygon) {
	for (const auto& polygon : multipolygon.getPolygons()) {
		writeGoogleMap(fs, polygon);
	}
}


static void writeGoogleMap(std::ofstream& fs, const Intersection& intersection);


static void writeGoogleMap(std::ofstream& fs, const Collection& collection) {
	writeGoogleMap(fs, collection.getMultiPoint());
	writeGoogleMap(fs, collection.getMultiCircle());
	writeGoogleMap(fs, collection.getMultiConvex());
	writeGoogleMap(fs, collection.getMultiPolygon());
	for (const auto& intersection : collection.getIntersections()) {
		writeGoogleMap(fs, intersection);
	}
}


static void writeGoogleMap(std::ofstream& fs, const Intersection& intersection) {
	for (const auto& geometry : intersection.getGeometries()) {
		switch (geometry->getType()) {
			case Geometry::Type::POINT: {
				writeGoogleMap(fs, *std::static_pointer_cast<Point>(geometry));
				break;
			}
			case Geometry::Type::MULTIPOINT: {
				writeGoogleMap(fs, *std::static_pointer_cast<MultiPoint>(geometry));
				break;
			}
			case Geometry::Type::CIRCLE: {
				writeGoogleMap(fs, *std::static_pointer_cast<Circle>(geometry));
				break;
			}
			case Geometry::Type::CONVEX: {
				writeGoogleMap(fs, *std::static_pointer_cast<Convex>(geometry));
				break;
			}
			case Geometry::Type::CHULL:
			case Geometry::Type::POLYGON: {
				writeGoogleMap(fs, *std::static_pointer_cast<Polygon>(geometry));
				break;
			}
			case Geometry::Type::MULTICIRCLE: {
				writeGoogleMap(fs, *std::static_pointer_cast<MultiCircle>(geometry));
				break;
			}
			case Geometry::Type::MULTICONVEX: {
				writeGoogleMap(fs, *std::static_pointer_cast<MultiConvex>(geometry));
				break;
			}
			case Geometry::Type::MULTICHULL:
			case Geometry::Type::MULTIPOLYGON: {
				writeGoogleMap(fs, *std::static_pointer_cast<MultiPolygon>(geometry));
				break;
			}
			case Geometry::Type::COLLECTION: {
				writeGoogleMap(fs, *std::static_pointer_cast<Collection>(geometry));
				break;
			}
			case Geometry::Type::INTERSECTION: {
				writeGoogleMap(fs, *std::static_pointer_cast<Intersection>(geometry));
				break;
			}
			default:
				break;
		}
	}
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Point& point) {
	const auto latlon = point.getCartesian().toLatLon();
	fs << "mymap = gmplot.GoogleMapPlotter(" << latlon.first << ", " << latlon.second << ",  18)\n";
}


static void writeGoogleMapPlotter(std::ofstream& fs, const MultiPoint& multipoint) {
	if (!multipoint.empty()) {
		// Check distance between points.
		double distance = 1.0;
		const auto points = multipoint.getPoints();
		const auto it_e = points.end();
		const auto it_last = it_e - 1;
		for (auto it = points.begin(); it != it_last; ++it) {
			for (auto it_n = it + 1; it_n != it_e; ++it_n) {
				double d = it->getCartesian() * it_n->getCartesian();
				if (d < distance) {
					distance = d;
				}
			}
		}
		const auto latlon = points.back().getCartesian().toLatLon();
		fs << "mymap = gmplot.GoogleMapPlotter(" << latlon.first << ", " << latlon.second << ",  " << (20 - 2 * std::log10(std::acos(distance) * M_PER_RADIUS_EARTH)) << ")\n";
	}
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Circle& circle) {
	const auto& constraint = circle.getConstraint();
	auto latlon = constraint.center.toLatLon();
	fs << "mymap = gmplot.GoogleMapPlotter(" << latlon.first << ", " << latlon.second << ",  " << (20 - 2 * std::log10(constraint.radius)) << ")\n";
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Convex& convex) {
	writeGoogleMapPlotter(fs, convex.getCircles().back());
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Polygon& polygon) {
	const auto& convexpolygon = polygon.getConvexPolygons().back();
	const auto latlon = convexpolygon.getCentroid().toLatLon();
	fs << "mymap = gmplot.GoogleMapPlotter(" << latlon.first << ", " << latlon.second << ",  " << (20 - 2 * std::log10(convexpolygon.getRadius())) << ")\n";
}


static void writeGoogleMapPlotter(std::ofstream& fs, const MultiCircle& multicircle) {
	writeGoogleMapPlotter(fs, multicircle.getCircles().front());
}


static void writeGoogleMapPlotter(std::ofstream& fs, const MultiConvex& multiconvex) {
	writeGoogleMapPlotter(fs, multiconvex.getConvexs().back());
}


static void writeGoogleMapPlotter(std::ofstream& fs, const MultiPolygon& multipolygon) {
	writeGoogleMapPlotter(fs, multipolygon.getPolygons().back());
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Intersection& intersection);


static void writeGoogleMapPlotter(std::ofstream& fs, const Collection& collection) {
	const auto& multicircle = collection.getMultiCircle();
	if (multicircle.empty()) {
		const auto& multipolygon = collection.getMultiPolygon();
		if (multipolygon.empty()) {
			const auto& multiconvex = collection.getMultiConvex();
			if (multiconvex.empty()) {
				const auto& intersections = collection.getIntersections();
				if (intersections.empty()) {
					const auto& multipoint = collection.getMultiPoint();
					if (multipoint.empty()) {
						THROW(NullConvex, "Empty Collection");
					} else {
						writeGoogleMapPlotter(fs, collection.getMultiPoint());
					}
				} else {
					for (const auto& intersection : intersections) {
						writeGoogleMapPlotter(fs, intersection);
					}
				}
			} else {
				writeGoogleMapPlotter(fs, multiconvex);
			}
		} else {
			writeGoogleMapPlotter(fs, multipolygon);
		}
	} else {
		writeGoogleMapPlotter(fs, multicircle);
	}
}


static void writeGoogleMapPlotter(std::ofstream& fs, const Intersection& intersection) {
	const auto& geometries = intersection.getGeometries();
	const auto it_e = geometries.rend();
	for (auto it = geometries.rbegin(); it != it_e; ++it) {
		switch ((*it)->getType()) {
			case Geometry::Type::POINT: {
				writeGoogleMapPlotter(fs, *std::static_pointer_cast<Point>(*it));
				return;
			}
			case Geometry::Type::MULTIPOINT: {
				const auto& multipoint = *std::static_pointer_cast<MultiPoint>(*it);
				if (!multipoint.empty()) {
					writeGoogleMapPlotter(fs, multipoint);
					return;
				}
				break;
			}
			case Geometry::Type::CIRCLE: {
				writeGoogleMapPlotter(fs, *std::static_pointer_cast<Circle>(*it));
				return;
			}
			case Geometry::Type::CONVEX: {
				const auto& convex = *std::static_pointer_cast<Convex>(*it);
				if (!convex.empty()) {
					writeGoogleMapPlotter(fs, convex);
					return;
				}
				break;
			}
			case Geometry::Type::CHULL:
			case Geometry::Type::POLYGON: {
				const auto& polygon = *std::static_pointer_cast<Polygon>(*it);
				if (!polygon.empty()) {
					writeGoogleMapPlotter(fs, polygon);
					return;
				}
				return;
			}
			case Geometry::Type::MULTICIRCLE: {
				const auto& multicircle = *std::static_pointer_cast<MultiCircle>(*it);
				if (!multicircle.empty()) {
					writeGoogleMapPlotter(fs, multicircle);
					return;
				}
				break;
			}
			case Geometry::Type::MULTICONVEX: {
				const auto& multiconvex = *std::static_pointer_cast<MultiConvex>(*it);
				if (!multiconvex.empty()) {
					writeGoogleMapPlotter(fs, multiconvex);
					return;
				}
				break;
			}
			case Geometry::Type::MULTICHULL:
			case Geometry::Type::MULTIPOLYGON: {
				const auto& multipolygon = *std::static_pointer_cast<MultiPolygon>(*it);
				if (!multipolygon.empty()) {
					writeGoogleMapPlotter(fs, multipolygon);
					return;
				}
				break;
			}
			case Geometry::Type::COLLECTION: {
				const auto& collection = *std::static_pointer_cast<Collection>(*it);
				if (!collection.empty()) {
					writeGoogleMapPlotter(fs, collection);
					return;
				}
				break;
			}
			case Geometry::Type::INTERSECTION:  {
				const auto& intersertion = *std::static_pointer_cast<Intersection>(*it);
				if (!intersertion.empty()) {
					writeGoogleMapPlotter(fs, intersertion);
					return;
				}
				break;
			}
			default:
				break;
		}
	}

	THROW(NullConvex, "Empty Intersection");
}


static void writeGoogleMapPlotter(std::ofstream& fs, const std::vector<std::string>& trixels) {
	// Default center (0, 0)
	double lat = 0.0;
	double lng = 0.0;
	double alt = 1.0;

	size_t min_level = HTM_MAX_LEVEL;
	for (const auto& trixel : trixels) {
		auto corners = HTM::getCorners(trixel);

		std::get<0>(corners).scale = M_PER_RADIUS_EARTH;
		const auto latlon0 = std::get<0>(corners).toLatLon();
		lat += latlon0.first;
		lng += latlon0.second;

		std::get<1>(corners).scale = M_PER_RADIUS_EARTH;
		const auto latlon1 = std::get<1>(corners).toLatLon();
		lat += latlon1.first;
		lng += latlon1.second;

		std::get<2>(corners).scale = M_PER_RADIUS_EARTH;
		const auto latlon2 = std::get<2>(corners).toLatLon();
		lat += latlon2.first;
		lng += latlon2.second;

		auto level = trixel.length() - 2;
		if (min_level > level) {
			min_level = level;
		}
	}

	auto size = trixels.size();
	if (size) {
		lat /= size * 3;
		lng /= size * 3;
		alt = (20 - 2 * std::log10(ERROR_NIVEL[min_level]));
	}

	fs << "mymap = gmplot.GoogleMapPlotter(" << lat << ", " << lng << ",  " << alt << ")\n";
}


void
HTM::writeGoogleMap(const std::string& file, const std::string& output_file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels)
{
	std::ofstream fs(file);
	fs.precision(HTM_DIGITS);

	fs << "import sys\n";
	fs << "import os\n\n";
	fs << "from gmplot import gmplot\n";

	// Draw Geometry.
	if (g) {
		switch (g->getType()) {
			case Geometry::Type::POINT: {
				const auto& point = *std::static_pointer_cast<Point>(g);
				writeGoogleMapPlotter(fs, point);
				writeGoogleMap(fs, point);
				break;
			}
			case Geometry::Type::MULTIPOINT: {
				const auto& multipoint = *std::static_pointer_cast<MultiPoint>(g);
				if (!multipoint.empty()) {
					writeGoogleMapPlotter(fs, multipoint);
					writeGoogleMap(fs, multipoint);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::CIRCLE: {
				const auto& circle = *std::static_pointer_cast<Circle>(g);
				writeGoogleMapPlotter(fs, circle);
				writeGoogleMap(fs, circle);
				break;
			}
			case Geometry::Type::CONVEX: {
				const auto& convex = *std::static_pointer_cast<Convex>(g);
				if (!convex.empty()) {
					writeGoogleMapPlotter(fs, convex);
					writeGoogleMap(fs, convex);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::POLYGON:
			case Geometry::Type::CHULL: {
				const auto& polygon = *std::static_pointer_cast<Polygon>(g);
				if (!polygon.empty()) {
					writeGoogleMapPlotter(fs, polygon);
					writeGoogleMap(fs, polygon);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::MULTICIRCLE: {
				const auto& multicircle = *std::static_pointer_cast<MultiCircle>(g);
				if (!multicircle.empty()) {
					writeGoogleMapPlotter(fs, multicircle);
					writeGoogleMap(fs, multicircle);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::MULTICONVEX: {
				const auto& multiconvex = *std::static_pointer_cast<MultiConvex>(g);
				if (!multiconvex.empty()) {
					writeGoogleMapPlotter(fs, multiconvex);
					writeGoogleMap(fs, multiconvex);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::MULTICHULL:
			case Geometry::Type::MULTIPOLYGON: {
				const auto& multipolygon = *std::static_pointer_cast<MultiPolygon>(g);
				if (!multipolygon.empty()) {
					writeGoogleMapPlotter(fs, multipolygon);
					writeGoogleMap(fs, multipolygon);
				} else {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::COLLECTION: {
				const auto& collection = *std::static_pointer_cast<Collection>(g);
				try {
					writeGoogleMapPlotter(fs, collection);
					writeGoogleMap(fs, collection);
				} catch (const NullConvex&) {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
			case Geometry::Type::INTERSECTION: {
				const auto& intersection = *std::static_pointer_cast<Intersection>(g);
				try {
					writeGoogleMapPlotter(fs, intersection);
					writeGoogleMap(fs, intersection);
				} catch (const NullConvex&) {
					writeGoogleMapPlotter(fs, trixels);
				}
				break;
			}
		}
	} else {
		writeGoogleMapPlotter(fs, trixels);
	}

	// Draw trixels.
	if (!trixels.empty()) {
		for (const auto& trixel : trixels) {
			auto corners = getCorners(trixel);
			std::get<0>(corners).scale = M_PER_RADIUS_EARTH;
			std::get<1>(corners).scale = M_PER_RADIUS_EARTH;
			std::get<2>(corners).scale = M_PER_RADIUS_EARTH;

			const auto latlon0 = std::get<0>(corners).toLatLon();
			const auto latlon1 = std::get<1>(corners).toLatLon();
			const auto latlon2 = std::get<2>(corners).toLatLon();

			fs << "mymap.polygon(";
			fs << "[" << latlon0.first << ", " << latlon1.first << ", " << latlon2.first << "],";
			fs << "[" << latlon0.second << ", " << latlon1.second << ", " << latlon2.second << "],";
			fs << "edge_color='cyan', edge_width=2, face_color='blue', face_alpha=0.2)\n";
		}
	}
	fs << "mymap.draw('" << output_file << "')";
	fs.close();
}


static void writePython3D(std::ofstream& fs, const Point& point) {
	const auto& c = point.getCartesian();
	fs << "ax.plot3D([" << c.x << "], [" << c.y << "], [" << c.z << "], 'ko', linewidth = 2.0)\n\n";
}


static void writePython3D(std::ofstream& fs, const MultiPoint& multipoint, bool& sphere, double umbral) {
	if (!sphere) {
		// Check distance between points.
		const auto it_e = multipoint.getPoints().end();
		for (auto it = multipoint.getPoints().begin(); it != it_e; ++it) {
			for (auto it_n = it + 1; it_n != it_e; ++it_n) {
				if ((it->getCartesian() * it_n->getCartesian()) < umbral) {
					sphere = true;
					it = it_e;
					break;
				}
			}
		}
	}

	for (const auto& point : multipoint.getPoints()) {
		writePython3D(fs, point);
	}
}


static void writePython3D(std::ofstream& fs, const Circle& circle, bool& sphere, double umbral) {
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


static void writePython3D(std::ofstream& fs, const Convex& convex, bool& sphere, double umbral) {
	for (const auto& circle : convex.getCircles()) {
		writePython3D(fs, circle, sphere, umbral);
	}
}


static void writePython3D(std::ofstream& fs, const Polygon& polygon, bool& sphere, double umbral) {
	std::string x, y, z;
	for (const auto& convexpolygon : polygon.getConvexPolygons()) {
		if (!sphere && std::acos(convexpolygon.getRadius() / M_PER_RADIUS_EARTH) < umbral) {
			sphere = true;
		}
		x = "x = [";
		y = "y = [";
		z = "z = [";
		const auto& corners = convexpolygon.getCorners();
		const auto it_last = corners.end() - 1;
		auto it = corners.begin();
		for ( ; it != it_last; ++it) {
			const auto& v0 = *it;
			const auto& v1 = *(it + 1);
			for (double i = 0; i < HTM_LINE_POINTS; ++i) {
				const auto inc = i / HTM_LINE_POINTS;
				const auto mp = ((1.0 - inc) * v0 + inc * v1).normalize();
				x.append(string::format("{}", mp.x)).append(", ");
				y.append(string::format("{}", mp.y)).append(", ");
				z.append(string::format("{}", mp.z)).append(", ");
			}
		}
		// Close polygon.
		x.append(string::format("{}", it->x)).append("]\n");
		y.append(string::format("{}", it->y)).append("]\n");
		z.append(string::format("{}", it->z)).append("]\n");
		fs << x << y << z;
		fs << "ax.plot3D(x, y, z, 'b-', linewidth = 2.0)\n";
		const auto& c = convexpolygon.getCentroid();
		fs << "ax.plot3D([" << c.x << "], [" << c.y << "], [" << c.z << "], 'ko', linewidth = 2.0)\n\n";
	}
}


static void writePython3D(std::ofstream& fs, const MultiCircle& multicircle, bool& sphere, double umbral) {
	for (const auto& circle : multicircle.getCircles()) {
		writePython3D(fs, circle, sphere, umbral);
	}
}


static void writePython3D(std::ofstream& fs, const MultiConvex& multiconvex, bool& sphere, double umbral) {
	for (const auto& convex : multiconvex.getConvexs()) {
		writePython3D(fs, convex, sphere, umbral);
	}
}


static void writePython3D(std::ofstream& fs, const MultiPolygon& multipolygon, bool& sphere, double umbral) {
	for (const auto& polygon : multipolygon.getPolygons()) {
		writePython3D(fs, polygon, sphere, umbral);
	}
}


static void writePython3D(std::ofstream& fs, const Intersection& intersection, bool& sphere, double umbral);


static void writePython3D(std::ofstream& fs, const Collection& collection, bool& sphere, double umbral) {
	writePython3D(fs, collection.getMultiPoint(), sphere, umbral);
	writePython3D(fs, collection.getMultiCircle(), sphere, umbral);
	writePython3D(fs, collection.getMultiConvex(), sphere, umbral);
	writePython3D(fs, collection.getMultiPolygon(), sphere, umbral);
	for (const auto& intersection : collection.getIntersections()) {
		writePython3D(fs, intersection, sphere, umbral);
	}
}


static void writePython3D(std::ofstream& fs, const Intersection& intersection, bool& sphere, double umbral) {
	for (const auto& geometry : intersection.getGeometries()) {
		switch (geometry->getType()) {
			case Geometry::Type::POINT: {
				writePython3D(fs, *std::static_pointer_cast<Point>(geometry));
				break;
			}
			case Geometry::Type::MULTIPOINT: {
				writePython3D(fs, *std::static_pointer_cast<MultiPoint>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::CIRCLE: {
				writePython3D(fs, *std::static_pointer_cast<Circle>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::CONVEX: {
				writePython3D(fs, *std::static_pointer_cast<Convex>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::CHULL:
			case Geometry::Type::POLYGON: {
				writePython3D(fs, *std::static_pointer_cast<Polygon>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::MULTICIRCLE: {
				writePython3D(fs, *std::static_pointer_cast<MultiCircle>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::MULTICONVEX: {
				writePython3D(fs, *std::static_pointer_cast<MultiConvex>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::MULTICHULL:
			case Geometry::Type::MULTIPOLYGON: {
				writePython3D(fs, *std::static_pointer_cast<MultiPolygon>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::COLLECTION: {
				writePython3D(fs, *std::static_pointer_cast<Collection>(geometry), sphere, umbral);
				break;
			}
			case Geometry::Type::INTERSECTION: {
				writePython3D(fs, *std::static_pointer_cast<Intersection>(geometry), sphere, umbral);
				break;
			}
			default:
				break;
		}
	}
}


void
HTM::writePython3D(const std::string& file, const std::shared_ptr<Geometry>& g, const std::vector<std::string>& trixels)
{
	std::ofstream fs(file);
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
			writePython3D(fs, *std::static_pointer_cast<Point>(g));
			break;
		}
		case Geometry::Type::MULTIPOINT: {
			writePython3D(fs, *std::static_pointer_cast<MultiPoint>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::CIRCLE: {
			writePython3D(fs, *std::static_pointer_cast<Circle>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::CONVEX: {
			writePython3D(fs, *std::static_pointer_cast<Convex>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::POLYGON:
		case Geometry::Type::CHULL: {
			writePython3D(fs, *std::static_pointer_cast<Polygon>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::MULTICIRCLE: {
			writePython3D(fs, *std::static_pointer_cast<MultiCircle>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::MULTICONVEX: {
			writePython3D(fs, *std::static_pointer_cast<MultiConvex>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::MULTIPOLYGON:
		case Geometry::Type::MULTICHULL: {
			writePython3D(fs, *std::static_pointer_cast<MultiPolygon>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::COLLECTION: {
			writePython3D(fs, *std::static_pointer_cast<Collection>(g), sphere, umbral);
			break;
		}
		case Geometry::Type::INTERSECTION: {
			writePython3D(fs, *std::static_pointer_cast<Intersection>(g), sphere, umbral);
			break;
		}
		default:
			break;
	}

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

	// Draw trixels.
	std::string x, y, z;
	for (const auto& trixel : trixels) {
		const auto corners = getCorners(trixel);
		const auto& v0 = std::get<0>(corners);
		const auto& v1 = std::get<1>(corners);
		const auto& v2 = std::get<2>(corners);
		x = "x = [";
		y = "y = [";
		z = "z = [";
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v0 + inc * v1).normalize();
			x.append(string::format("{}", mp.x)).append(", ");
			y.append(string::format("{}", mp.y)).append(", ");
			z.append(string::format("{}", mp.z)).append(", ");
		}
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v1 + inc * v2).normalize();
			x.append(string::format("{}", mp.x)).append(", ");
			y.append(string::format("{}", mp.y)).append(", ");
			z.append(string::format("{}", mp.z)).append(", ");
		}
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v2 + inc * v0).normalize();
			x.append(string::format("{}", mp.x)).append(", ");
			y.append(string::format("{}", mp.y)).append(", ");
			z.append(string::format("{}", mp.z)).append(", ");
		}
		// Close the trixel.
		x.append(string::format("{}", v0.x)).append("]\n");
		y.append(string::format("{}", v0.y)).append("]\n");
		z.append(string::format("{}", v0.z)).append("]\n");
		fs << (x + y + z);
		fs << rule_trixel;
	}
	fs << show_graphics;
	fs.close();
}


void
HTM::writeGrahamScanMap(const std::string& file, const std::string& output_file, const std::vector<Cartesian>& points, const std::vector<Cartesian>& convex_points)
{
	std::ofstream fs(file);
	fs.precision(HTM_DIGITS);

	fs << "import sys\n";
	fs << "import os\n\n";
	fs << "from gmplot import gmplot\n";

	auto latlon = convex_points.back().toLatLon();
	fs << "mymap = gmplot.GoogleMapPlotter(" << latlon.first << ", " << latlon.second << ", 6)\n";

	// Original Points.
	for (const auto& point : points) {
		latlon = point.toLatLon();
		fs << "mymap.circle(" << latlon.first << ", " << latlon.second << ", " << MIN_RADIUS_RADIANS << ", '#FF0000', ew=2)\n";
	}

	// Polygon formed by convex points
	std::string lat(1, '[');
	std::string lon(1, '[');
	for (const auto& point : convex_points) {
		latlon = point.toLatLon();
		lat.append(std::to_string(latlon.first)).push_back(',');
		lon.append(std::to_string(latlon.second)).push_back(',');
	}
	lat.back() = ']';
	lon.back() = ']';
	fs << "mymap.polygon(" << lat << ',' << lon << ',' << "edge_color='blue', edge_width=2, face_color='blue', face_alpha=0.2)\n";
	fs << "mymap.draw('" << output_file << "')";
	fs.close();
}


void
HTM::writeGrahamScan3D(const std::string& file, const std::vector<Cartesian>& points, const std::vector<Cartesian>& convex_points)
{
	std::ofstream fs(file);
	fs.precision(HTM_DIGITS);

	fs << "import mpl_toolkits.mplot3d as a3\n";
	fs << "import matplotlib.pyplot as plt\n";
	fs << "import numpy as np\n\n\n";
	fs << "ax = a3.Axes3D(plt.figure())\n";

	// Original Points.
	for (auto point : points) {
		point.normalize();
		fs << "x = [" << point.x << "];\ny = [" << point.y << "];\nz = [" << point.z << "]\n";
		fs << "ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);\n";
	}

	// Polygon formed by convex points
	std::string x = "x = [";
	std::string y = "y = [";
	std::string z = "z = [";
	const auto it_last = convex_points.end() - 1;
	auto it = convex_points.begin();
	for ( ; it != it_last; ++it) {
		const auto& v0 = *it;
		const auto& v1 = *(it + 1);
		for (double i = 0; i < HTM_LINE_POINTS; ++i) {
			const auto inc = i / HTM_LINE_POINTS;
			const auto mp = ((1.0 - inc) * v0 + inc * v1).normalize();
			x.append(string::format("{}", mp.x)).append(", ");
			y.append(string::format("{}", mp.y)).append(", ");
			z.append(string::format("{}", mp.z)).append(", ");
		}
	}
	// Close polygon.
	x.append(string::format("{}", it->x)).append("]\n");
	y.append(string::format("{}", it->y)).append("]\n");
	z.append(string::format("{}", it->z)).append("]\n");
	fs << x << y << z;
	fs << "ax.plot3D(x, y, z, 'b-', linewidth = 2.0)\n";
	fs << "plt.ion()\nplt.grid()\nplt.show()";
	fs.close();
}
