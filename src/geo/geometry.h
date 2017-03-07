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

#pragma once

#include "htm.h"


// Earth radius in meters
constexpr double M_PER_RADIUS_EARTH = 6367444.7;

// Radius maximum in meters allowed in a constraint (all the earth)
constexpr double MAX_RADIUS_HALFSPACE_EARTH = 20003917.491659265;

// Min radius in meters allowed
constexpr double MIN_RADIUS_METERS = 0.1;

// Min radius in radians allowed, MIN_RADIUS_METERS / M_PER_RADIUS_EARTH.
constexpr double MIN_RADIUS_RADIANS = 0.00000001570488707974173690;

// Radius in meters of a great circle
constexpr double RADIUS_GREAT_CIRCLE = 10001958.7458296325;


/*
 * A circular area, given by the plane slicing it off sphere.
 *
 * All Cartesians are normalized because Geometry and HTM works around a unary sphere
 * instead of a spheroid.
 */
class Constraint {
	void set_data(double _radius);

public:
	// Constants used for specify the sign of the bounding circle or a convex.
	enum class Sign : uint8_t {
		POS       = 0b0001,
		NEG       = 0b0010,
		ZERO      = 0b0011,
	};

	Cartesian center;
	double arcangle;
	double distance;
	double radius;         // Radius in meters
	Sign sign;

	/*
	 * Does a great circle with center in
	 * lat = 0, lon = 0, h = 0 (Default Cartesian)
	 */
	Constraint();

	// Does a great circle with the defined center.
	template <typename T, typename = std::enable_if_t<std::is_same<Cartesian, std::decay_t<T>>::value>>
	Constraint(T&& _center)
		: center(std::forward<T>(_center)),
		  arcangle(PI_HALF),
		  distance(0.0),
		  radius(RADIUS_GREAT_CIRCLE),
		  sign(Sign::ZERO)
	{
		center.normalize();
	}

	// Constraint in the Earth. Radius in meters.
	template <typename T, typename = std::enable_if_t<std::is_same<Cartesian, std::decay_t<T>>::value>>
	Constraint(T&& _center, double _radius)
		: center(std::forward<T>(_center))
	{
		center.normalize();
		set_data(_radius);
	}

	// Move and copy constructor.
	Constraint(Constraint&& c) = default;
	Constraint(const Constraint& c) = default;

	// Move and copy assignment.
	Constraint& operator=(Constraint&& c) = default;
	Constraint& operator=(const Constraint& c) = default;

	bool operator==(const Constraint& c) const noexcept;
	bool operator!=(const Constraint& c) const noexcept;
	bool operator<(const Constraint& c) const noexcept;
	bool operator>(const Constraint& c) const noexcept;

	std::string to_string() const;
};


class Geometry {
public:
	enum class Type : uint8_t {
		POINT,
		MULTIPOINT,
		CIRCLE,
		CONVEX,
		POLYGON,
		CONVEX_HULL,
		XOR_POLYGON,
		MULTICIRCLE,
		MULTICONVEX,
		MULTIPOLYGON,
		COLLECTION,
		INTERSECTION,
	};

protected:
	Type type;

public:
	Geometry(Type t)
		: type(t) { }

	Geometry(Geometry&& g) noexcept
		: type(std::move(g.type)) { }

	Geometry(const Geometry& g)
		: type(g.type) { }

	virtual ~Geometry() = default;

	Geometry& operator=(Geometry&& g) noexcept {
		type = std::move(g.type);
		return *this;
	}

	Geometry& operator=(const Geometry& g) {
		type = g.type;
		return *this;
	}

	Type getType() const noexcept {
		return type;
	}

	std::string toEWKT() const {
		std::string ewkt(DEFAULT_CRS);
		ewkt.append(toWKT());
		return ewkt;
	}

	virtual void simplify() { }

	virtual std::string toWKT() const = 0;
	virtual std::string to_string() const = 0;
	virtual std::vector<std::string> getTrixels(bool partials, double error) const = 0;
	virtual std::vector<range_t> getRanges(bool partials, double error) const = 0;

	virtual std::vector<Cartesian> getCentroids() const {
		return std::vector<Cartesian>();
	}
};
