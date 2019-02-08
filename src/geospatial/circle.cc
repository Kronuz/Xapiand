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

#include "circle.h"


bool
Circle::operator==(const Circle& c) const noexcept
{
	return constraint == c.constraint;
}


bool
Circle::operator!=(const Circle& c) const noexcept
{
	return !operator==(c);
}


bool
Circle::operator<(const Circle& c) const noexcept
{
	return constraint < c.constraint;
}


bool
Circle::operator>(const Circle& c) const noexcept
{
	return constraint > c.constraint;
}


TypeTrixel
Circle::verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const
{
	int sum = (
		(HTM::insideVertex_Constraint(v0, constraint) ? 1 : 0) +
		(HTM::insideVertex_Constraint(v1, constraint) ? 1 : 0) +
		(HTM::insideVertex_Constraint(v2, constraint) ? 1 : 0)
	);

	switch (sum) {
		case 0: {
			// Get bounding circle of trixel (v0, v1, v2).
			auto bounding_circle = HTM::getBoundingCircle(v0, v1, v2);
			// If  constraint intersect with the bounding circle.
			if (HTM::intersectConstraints(constraint, bounding_circle)) {
				// The constraint intersect with a edge of trixel (v0, v1, v2).
				if (HTM::intersectConstraint_EdgeTrixel(constraint, v0, v1, v2)) {
					return TypeTrixel::PARTIAL;
				}
				// Center of constraint inside trixel (v0, v1, v2).
				if (HTM::insideVertex_Trixel(constraint.center, v0, v1, v2)) {
					return TypeTrixel::PARTIAL;
				}
			}
			return TypeTrixel::OUTSIDE;
		}
		case 1:
		case 2:
			return TypeTrixel::PARTIAL;
		case 3:
			/*
			 * All corners are inside.
			 *   For Negative constraints we need to test if there is a hole or there are intersections with the trixel.
			 */
			if (constraint.sign == Constraint::Sign::NEG) {
				// If there is a hole the trixel is flag to TypeTrixel::PARTIAL.
				if (HTM::thereisHole(constraint, v0, v1, v2)) {
					return TypeTrixel::PARTIAL;
				}
				// Test whether one of the negative halfspace’s boundary circles intersects with one of the edges of the triangle.
				if (HTM::intersectConstraint_EdgeTrixel(constraint, v0, v1, v2)) {
					return TypeTrixel::PARTIAL;
				}
			}
			return TypeTrixel::FULL;
		default:
			return TypeTrixel::OUTSIDE;
	}
}


void
Circle::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const
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

	// Number of full and partial subtrixels.
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

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
Circle::lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const
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

	// Number of full and partial subtrixels.
	int F = (
		(type_trixels[0] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[1] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[2] == TypeTrixel::FULL ? 1 : 0) +
		(type_trixels[3] == TypeTrixel::FULL ? 1 : 0)
	);

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
Circle::toWKT() const
{
	std::string wkt("CIRCLE");
	wkt.append(to_string());
	return wkt;
}


std::string
Circle::to_string() const
{
	char result[55];
	const auto geodetic = constraint.center.toGeodetic();
	snprintf(result, 55, "(%.7f %.7f %.7f, %.6f)",
		std::get<1>(geodetic),
		std::get<0>(geodetic),
		std::get<2>(geodetic),
		constraint.radius
	);
	return std::string(result);
}


std::vector<std::string>
Circle::getTrixels(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	trixel_data data(partials, HTM_MAX_LEVEL);
	error = error * constraint.radius;
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
Circle::getRanges(bool partials, double error) const
{
	if (error < HTM_MIN_ERROR || error > HTM_MAX_ERROR) {
		THROW(HTMError, "Error must be in [{}, {}]", HTM_MIN_ERROR, HTM_MAX_ERROR);
	}

	range_data data(partials, HTM_MAX_LEVEL);
	error = error * constraint.radius;
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
Circle::getCentroids() const
{
	return std::vector<Cartesian>({ constraint.center });
}
