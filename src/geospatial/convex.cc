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

#include "convex.h"

#include <algorithm>
#include <cmath>


void
Convex::simplify()
{
	if (!simplified) {
		// Sort circles.
		std::sort(circles.begin(), circles.end(), std::less<>());

		sign = Sign::ZERO;
		for (auto it = circles.begin(); it != circles.end(); ++it) {
			sign = static_cast<Convex::Sign>(static_cast<uint8_t>(sign) & static_cast<uint8_t>(it->constraint.sign));
			for (auto it_n = it + 1; it_n != circles.end(); ) {
				auto gamma = std::acos(it->constraint.center * it_n->constraint.center);
				if (gamma >= (it->constraint.arcangle + it_n->constraint.arcangle)) {
					// Empty intersection.
					circles.clear();
					sign = Sign::ZERO;
					simplified = true;
					return;
				}
				// Deleting redundants circles.
				if ((it_n->constraint.arcangle - it->constraint.arcangle) >= gamma) {
					it_n = circles.erase(it_n);
				} else {
					++it_n;
				}
			}
		}

		simplified = true;
	}
}


int
Convex::insideVertex(const Cartesian& v) const noexcept
{
	for (const auto& circle : circles) {
		if (!HTM::insideVertex_Constraint(v, circle.constraint)) {
			return 0;
		}
	}
	return 1;
}


bool
Convex::intersectCircles(const Constraint& bounding_circle) const
{
	for (const auto& circle : circles) {
		if (!HTM::intersectConstraints(circle.constraint, bounding_circle)) {
			return false;
		}
	}
	return true;
}


TypeTrixel
Convex::verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
{
	int F = (
		(insideVertex(v0) != 0 ? 1 : 0) +
		(insideVertex(v1) != 0 ? 1 : 0) +
		(insideVertex(v2) != 0 ? 1 : 0)
	);

	switch (F) {
		case 0: {
			// If bounding circle doesnot intersect all circles, the trixel is considered OUTSIDE.
			if (!intersectCircles(HTM::getBoundingCircle(v0, v1, v2))) {
				return TypeTrixel::OUTSIDE;
			}

			if (sign == Sign::NEG) {
				// In this point might have very complicated patterns inside the triangle, so we just assume partial to be certain.
				return TypeTrixel::PARTIAL;
			}

			// For positive, zero and mixed convex.
			auto it = circles.begin();
			const auto& smallest_c = it->constraint;
			// The smallest constraint intersect with an edge of trixel (v0, v1, v2).
			if (HTM::intersectConstraint_EdgeTrixel(smallest_c, v0, v1, v2)) {
				const auto it_e = circles.end();
				// Any other positive constraint not intersect with an enge of trixel (v0, v1, v2).
				for (++it; it != it_e && it->constraint.sign == Constraint::Sign::POS; ++it) {
					if (!HTM::intersectConstraint_EdgeTrixel(it->constraint, v0, v1, v2)) {
						// Constraint center inside trixel (v0, v1, v2).
						if (HTM::insideVertex_Trixel(it->constraint.center, v0, v1, v2)) {
							return TypeTrixel::PARTIAL;
						}
						// The triangle is inside of constraint.
						if (HTM::insideVertex_Constraint(v0, it->constraint)) {
							return TypeTrixel::PARTIAL;
						}
						return TypeTrixel::OUTSIDE;
					}
				}
				// In this point for mixed convex might have very complicated patterns inside the triangle, so we just assume partial to be certain.
				return TypeTrixel::PARTIAL;
			}
			if (sign == Sign::POS || sign == Sign::ZERO) {
				// Constraint center inside trixel (v0, v1, v2).
				if (HTM::insideVertex_Trixel(smallest_c.center, v0, v1, v2)) {
					return TypeTrixel::PARTIAL;
				}
				// The triangle is inside of constraint.
				if (HTM::insideVertex_Constraint(v0, smallest_c)) {
					return TypeTrixel::PARTIAL;
				}
				return TypeTrixel::OUTSIDE;
			}
			// In this point might have very complicated patterns inside the triangle, so we just assume partial to be certain.
			return TypeTrixel::PARTIAL;
		}
		case 1:
		case 2:
			return TypeTrixel::PARTIAL;
		case 3: {
			// For negative or mixed convex we need to test further.
			if (sign == Sign::NEG || sign == Sign::MIXED) {
				for (const auto& circle : circles) {
					if (circle.constraint.sign == Constraint::Sign::NEG) {
						// Constraint center inside trixel (there is a hole).
						if (HTM::insideVertex_Trixel(circle.constraint.center, v0, v1, v2)) {
							return TypeTrixel::PARTIAL;
						}
						// Negative constraint intersect with a side.
						if (HTM::intersectConstraint_EdgeTrixel(circle.constraint, v0, v1, v2)) {
							return TypeTrixel::PARTIAL;
						}
					}
				}
			}
			return TypeTrixel::FULL;
		}
		default:
			return TypeTrixel::OUTSIDE;
	}
}


void
Convex::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const
{
	// Finish the recursion.
	if (level == data.max_level) {
		data.aux_trixels.push_back(std::move(name));
		return;
	}

	auto w2 = HTM::midPoint(v0, v1);
	auto w0 = HTM::midPoint(v1, v2);
	auto w1 = HTM::midPoint(v2, v0);

	TypeTrixel type_trixels[4] = {
		verifyTrixel(v0, w2, w1),
		verifyTrixel(v1, w0, w2),
		verifyTrixel(v2, w1, w0),
		verifyTrixel(w0, w1, w2)
	};

	// Number of full subtrixels.
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

	// Finish the recursion.
	if (F == 4) {
		data.trixels.push_back(std::move(name));
		return;
	}

	++level;

	switch (type_trixels[0]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "0");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v0, w2, w1, name + "0", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[1]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "1");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v1, w0, w2, name + "1", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[2]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "2");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v2, w1, w0, name + "2", data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[3]) {
		case TypeTrixel::FULL:
			data.trixels.push_back(name + "3");
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(w0, w1, w2, name + "3", data, level);
			break;
		default:
			break;
	}
}


void
Convex::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const
{
	// Finish the recursion.
	if (level == data.max_level) {
		HTM::insertGreaterRange(data.aux_ranges, HTM::getRange(id, level));
		return;
	}

	auto w2 = HTM::midPoint(v0, v1);
	auto w0 = HTM::midPoint(v1, v2);
	auto w1 = HTM::midPoint(v2, v0);

	TypeTrixel type_trixels[4] = {
		verifyTrixel(v0, w2, w1),
		verifyTrixel(v1, w0, w2),
		verifyTrixel(v2, w1, w0),
		verifyTrixel(w0, w1, w2)
	};

	// Number of full subtrixels.
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

	// Finish the recursion.
	if (F == 4) {
		HTM::insertGreaterRange(data.ranges, HTM::getRange(id, level));
		return;
	}

	++level;
	id <<= 2;

	switch (type_trixels[0]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v0, w2, w1, id, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[1]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 1, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v1, w0, w2, id + 1, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[2]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 2, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(v2, w1, w0, id + 2, data, level);
			break;
		default:
			break;
	}

	switch (type_trixels[3]) {
		case TypeTrixel::FULL:
			HTM::insertGreaterRange(data.ranges, HTM::getRange(id + 3, level));
			break;
		case TypeTrixel::PARTIAL:
			lookupTrixel(w0, w1, w2, id + 3, data, level);
			break;
		default:
			break;
	}
}


std::string
Convex::toWKT() const
{
	std::string wkt("CONVEX");
	wkt.append(to_string());
	return wkt;
}


std::string
Convex::to_string() const
{
	if (circles.empty()) {
		return std::string(" EMPTY");
	}

	std::string str("(");
	for (const auto& constraint : circles) {
		auto str_constraint = constraint.to_string();
		str.reserve(str.length() + str_constraint.length() + 1);
		str.append(str_constraint).push_back(',');
	}
	str.back() = ')';
	return str;
}


std::vector<std::string>
Convex::getTrixels(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	trixel_data data(partials, HTM_MAX_LEVEL);
	error = error * circles.begin()->constraint.radius;
	for (size_t i = 0; i < HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < error) {
			data.max_level = i;
			break;
		}
	}

	if (verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2], start_trixels[0].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2], start_trixels[1].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2], start_trixels[2].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2], start_trixels[3].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2], start_trixels[4].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2], start_trixels[5].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2], start_trixels[6].name, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2], start_trixels[7].name, data, 0);
	}

	return data.getTrixels();
}


std::vector<range_t>
Convex::getRanges(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	range_data data(partials, HTM_MAX_LEVEL);
	error = error * circles.begin()->constraint.radius;
	for (size_t i = 0; i < HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < error) {
			data.max_level = i;
			break;
		}
	}

	if (verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2], start_trixels[0].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2], start_trixels[1].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2], start_trixels[2].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2], start_trixels[3].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2], start_trixels[4].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2], start_trixels[5].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2], start_trixels[6].id, data, 0);
	}

	if (verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]) != TypeTrixel::OUTSIDE) {
		lookupTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2], start_trixels[7].id, data, 0);
	}

	return data.getRanges();
}


std::vector<Cartesian>
Convex::getCentroids() const
{
	// FIXME: Efficient way for calculate centroids for a Convex.
	return std::vector<Cartesian>();
}
