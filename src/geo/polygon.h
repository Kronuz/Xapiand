/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include "circle.h"


class Polygon : public Geometry {
protected:
	std::vector<Cartesian> corners;
	std::vector<Constraint> constraints;
	Constraint boundingCircle;
	Cartesian centroid;
	double max_radius;

	void init();

	void process(std::vector<Cartesian>&& points);

	bool intersectEdges(Cartesian aux, double length_v0_v1, const Cartesian& v0, const Cartesian& v1, double length_corners, const Cartesian& corner, const Cartesian& n_corner) const;
	bool intersectTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;

	int insideVertex(const Cartesian& v) const noexcept;
	TypeTrixel verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;
	void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const;
	void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const;

	Polygon(Type type)
		: Geometry(type),
		  centroid(0, 0, 0) { }

public:
	Polygon(std::vector<Cartesian>&& points)
		: Polygon(Type::POLYGON)
	{
		process(std::move(points));
	}

	Polygon(Polygon&& polygon) noexcept
		: Geometry(std::move(polygon)),
		  corners(std::move(polygon.corners)),
		  constraints(std::move(polygon.constraints)),
		  boundingCircle(std::move(polygon.boundingCircle)),
		  centroid(std::move(polygon.centroid)) { }

	Polygon(const Polygon& polygon)
		: Geometry(polygon),
		  corners(polygon.corners),
		  constraints(polygon.constraints),
		  boundingCircle(polygon.boundingCircle),
		  centroid(polygon.centroid) { }

	virtual ~Polygon() = default;

	Polygon& operator=(Polygon&& polygon) noexcept {
		Geometry::operator=(std::move(polygon));
		corners = std::move(polygon.corners);
		constraints = std::move(polygon.constraints);
		boundingCircle = std::move(polygon.boundingCircle);
		centroid = std::move(polygon.centroid);
		return *this;
	}

	Polygon& operator=(const Polygon& polygon) {
		Geometry::operator=(polygon);
		corners = polygon.corners;
		constraints = polygon.constraints;
		boundingCircle = polygon.boundingCircle;
		centroid = polygon.centroid;
		return *this;
	}

	const std::vector<Cartesian>& getCorners() const noexcept {
		return corners;
	}

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool, double) const override;
	std::vector<range_t> getRanges(bool, double) const override;
};
