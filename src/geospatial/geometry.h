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

#include "htm.h"   // also brings in the Constraint primitive + the earth-radius
                   // constants (via the library's constraint.h); both used to be
                   // defined here, before htm/constraint were extracted.


class Geometry {
public:
	enum class Type : uint8_t {
		POINT,
		MULTIPOINT,
		CIRCLE,
		CONVEX,
		POLYGON,
		CHULL,
		MULTICIRCLE,
		MULTICONVEX,
		MULTIPOLYGON,
		MULTICHULL,
		COLLECTION,
		INTERSECTION,
	};

protected:
	Type type;

public:
	explicit Geometry(Type t)
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

	virtual std::vector<Cartesian> getCentroids() const = 0;
};
