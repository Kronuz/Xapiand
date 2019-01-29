/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#pragma once

#include "geometry.h"


class Convex;
class MultiCircle;


struct range_data {
	bool partials;
	uint8_t max_level;
	std::vector<range_t> ranges;
	std::vector<range_t> partial_ranges;
	std::vector<range_t>& aux_ranges;

	range_data(bool partials_, uint8_t max_level_)
		: partials(partials_),
		  max_level(max_level_),
		  ranges(),
		  partial_ranges(),
		  aux_ranges(partials ? ranges : partial_ranges) { }

	std::vector<range_t> getRanges() {
		if (!partials && ranges.empty()) {
			ranges.reserve(partial_ranges.size());
			ranges.insert(ranges.begin(), std::make_move_iterator(partial_ranges.begin()), std::make_move_iterator(partial_ranges.end()));
		}
		return std::move(ranges);
	}
};


struct trixel_data {
	bool partials;
	uint8_t max_level;
	std::vector<std::string> trixels;
	std::vector<std::string> partial_trixels;
	std::vector<std::string>& aux_trixels;

	trixel_data(bool partials_, uint8_t max_level_)
		: partials(partials_),
		  max_level(max_level_),
		  trixels(),
		  partial_trixels(),
		  aux_trixels(partials ? trixels : partial_trixels) { }

	std::vector<std::string> getTrixels() {
		if (!partials && trixels.empty()) {
			trixels.reserve(partial_trixels.size());
			trixels.insert(trixels.begin(), std::make_move_iterator(partial_trixels.begin()), std::make_move_iterator(partial_trixels.end()));
		}
		return std::move(trixels);
	}
};


class Circle : public Geometry {
	friend class Convex;
	friend class MultiCircle;

	Constraint constraint;

	TypeTrixel verifyTrixel(const Cartesian&, const Cartesian&, const Cartesian&) const;
	void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const;
	void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const;

public:
	template <typename T, typename = std::enable_if_t<std::is_same<Cartesian, std::decay_t<T>>::value>>
	Circle(T&& _center, double radius)
		: Geometry(Type::CIRCLE),
		  constraint(std::forward<T>(_center), radius) { }

	Circle(Circle&& circle) noexcept
		: Geometry(std::move(circle)),
		  constraint(std::move(circle.constraint)) { }

	Circle(const Circle& circle)
		: Geometry(circle),
		  constraint(circle.constraint) { }

	~Circle() = default;

	Circle& operator=(Circle&& circle) noexcept {
		Geometry::operator=(std::move(circle));
		constraint = std::move(circle.constraint);
		return *this;
	}

	Circle& operator=(const Circle& circle) {
		Geometry::operator=(circle);
		constraint = circle.constraint;
		return *this;
	}

	bool operator==(const Circle& c) const noexcept;
	bool operator!=(const Circle& c) const noexcept;
	bool operator<(const Circle& c) const noexcept;
	bool operator>(const Circle& c) const noexcept;

	const Constraint& getConstraint() const noexcept {
		return constraint;
	}

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
