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

#include "circle.h"

class Polygon : public Geometry {
public:
	class ConvexPolygon : public Geometry {
		enum class Direction : uint8_t {
			COLLINEAR,
			CLOCKWISE,
			COUNTERCLOCKWISE,
		};

		std::vector<Cartesian> corners;
		std::vector<Constraint> constraints;
		Constraint boundingCircle;
		Cartesian centroid;
		double radius;

		// Gets the direction of the three points.
		static Direction get_direction(const Cartesian& a, const Cartesian& b, const Cartesian& c) noexcept;

		// Returns the squared distance between two points
		static double dist(const Cartesian& a, const Cartesian& b) noexcept;

		void init();

		void process_chull(std::vector<Cartesian>&& points);
		void process_polygon(std::vector<Cartesian>&& points);

		bool intersectEdges(Cartesian aux, double length_v0_v1, const Cartesian& v0, const Cartesian& v1, double length_corners, const Cartesian& corner, const Cartesian& n_corner) const;
		bool intersectTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;

		int insideVertex(const Cartesian& v) const noexcept;
		TypeTrixel verifyTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) const;
		void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, std::string name, trixel_data& data, uint8_t level) const;
		void lookupTrixel(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id, range_data& data, uint8_t level) const;

	public:
		ConvexPolygon(Geometry::Type type, std::vector<Cartesian>&& points)
			: Geometry(type),
			  centroid(0, 0, 0)
		{
			switch (type) {
				case Geometry::Type::POLYGON:
					process_polygon(std::move(points));
					return;
				case Geometry::Type::CHULL:
					process_chull(std::move(points));
					return;
				default:
					THROW(GeometryError, "Invalid type for Polygon");
			}
		}

		ConvexPolygon(ConvexPolygon&& polygon) noexcept
			: Geometry(std::move(polygon)),
			  corners(std::move(polygon.corners)),
			  constraints(std::move(polygon.constraints)),
			  boundingCircle(std::move(polygon.boundingCircle)),
			  centroid(std::move(polygon.centroid)),
			  radius(std::move(polygon.radius)) { }

		ConvexPolygon(const ConvexPolygon& polygon)
			: Geometry(polygon),
			  corners(polygon.corners),
			  constraints(polygon.constraints),
			  boundingCircle(polygon.boundingCircle),
			  centroid(polygon.centroid),
			  radius(polygon.radius) { }

		ConvexPolygon& operator=(ConvexPolygon&& polygon) noexcept {
			Geometry::operator=(std::move(polygon));
			corners = std::move(polygon.corners);
			constraints = std::move(polygon.constraints);
			boundingCircle = std::move(polygon.boundingCircle);
			centroid = std::move(polygon.centroid);
			radius = std::move(polygon.radius);
			return *this;
		}

		ConvexPolygon& operator=(const ConvexPolygon& polygon) {
			Geometry::operator=(polygon);
			corners = polygon.corners;
			constraints = polygon.constraints;
			boundingCircle = polygon.boundingCircle;
			centroid = polygon.centroid;
			radius = polygon.radius;
			return *this;
		}

		bool operator<(const ConvexPolygon& polygon) const noexcept {
			return corners < polygon.corners;
		}

		bool operator==(const ConvexPolygon& polygon) const noexcept {
			return corners == polygon.corners;
		}

		const std::vector<Cartesian>& getCorners() const noexcept {
			return corners;
		}

		const Cartesian& getCentroid() const noexcept {
			return centroid;
		}

		double getRadius() const noexcept {
			return radius;
		}

		// Calculates the convex hull of vector of points using Graham Scan Algorithm.
		static std::vector<Cartesian> graham_scan(std::vector<Cartesian>&& points);

		std::string toWKT() const override;
		std::string to_string() const override;
		std::vector<std::string> getTrixels(bool, double) const override;
		std::vector<range_t> getRanges(bool, double) const override;
		std::vector<Cartesian> getCentroids() const override;
	};

private:
	std::vector<ConvexPolygon> convexpolygons;
	bool simplified;

public:
	explicit Polygon(Geometry::Type type)
		: Geometry(type),
		  simplified(true) { }

	Polygon(Geometry::Type type, std::vector<Cartesian>&& points)
		: Geometry(type),
		  convexpolygons({ ConvexPolygon(type, std::move(points)) }),
		  simplified(true) { }

	Polygon(Polygon&& polygon) noexcept
		: Geometry(std::move(polygon)),
		  convexpolygons(std::move(polygon.convexpolygons)),
		  simplified(std::move(polygon.simplified)) { }

	Polygon(const Polygon& polygon)
		: Geometry(polygon),
		  convexpolygons(polygon.convexpolygons),
		  simplified(polygon.simplified) { }

	~Polygon() = default;

	Polygon& operator=(Polygon&& polygon) noexcept {
		Geometry::operator=(std::move(polygon));
		convexpolygons = std::move(polygon.convexpolygons);
		simplified = std::move(polygon.simplified);
		return *this;
	}

	Polygon& operator=(const Polygon& polygon) {
		Geometry::operator=(polygon);
		convexpolygons = polygon.convexpolygons;
		simplified = polygon.simplified;
		return *this;
	}

	bool operator<(const Polygon& polygon) const noexcept {
		return convexpolygons < polygon.convexpolygons;
	}

	bool operator==(const Polygon& polygon) const noexcept {
		return convexpolygons == polygon.convexpolygons;
	}

	void add(std::vector<Cartesian>&& points) {
		convexpolygons.emplace_back(type, std::move(points));
		simplified = false;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Polygon, std::decay_t<T>>::value>>
	void add(T&& polygon) {
		convexpolygons.push_back(std::forward<T>(polygon));
		simplified = false;
	}

	void reserve(size_t new_cap) {
		convexpolygons.reserve(new_cap);
	}

	bool empty() const noexcept {
		return convexpolygons.empty();
	}

	const std::vector<ConvexPolygon>& getConvexPolygons() const noexcept {
		return convexpolygons;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
	std::vector<Cartesian> getCentroids() const override;
};
