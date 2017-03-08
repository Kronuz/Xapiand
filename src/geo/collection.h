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

#include "multicircle.h"
#include "multiconvex.h"
#include "multipoint.h"
#include "multipolygon.h"


class Collection : public Geometry {
	MultiPoint multipoint;
	MultiCircle multicircle;
	MultiConvex multiconvex;
	MultiPolygon multipolygon;

public:
	Collection()
		: Geometry(Type::COLLECTION) { }

	Collection(Collection&& collection) noexcept
		: Geometry(std::move(collection)),
		  multipoint(std::move(collection.multipoint)),
		  multicircle(std::move(collection.multicircle)),
		  multiconvex(std::move(collection.multiconvex)),
		  multipolygon(std::move(collection.multipolygon)) { }

	Collection(const Collection& collection)
		: Geometry(collection),
		  multipoint(collection.multipoint),
		  multicircle(collection.multicircle),
		  multiconvex(collection.multiconvex),
		  multipolygon(collection.multipolygon) { }

	~Collection() = default;

	Collection& operator=(Collection&& collection) noexcept {
		Geometry::operator=(std::move(collection));
		multipoint = std::move(collection.multipoint);
		multicircle = std::move(collection.multicircle);
		multiconvex = std::move(collection.multiconvex);
		multipolygon = std::move(collection.multipolygon);
		return *this;
	}

	Collection& operator=(const Collection& collection) {
		Geometry::operator=(collection);
		multipoint = collection.multipoint;
		multicircle = collection.multicircle;
		multiconvex = collection.multiconvex;
		multipolygon = collection.multipolygon;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Point, std::decay_t<T>>::value>>
	void add_point(T&& point) {
		multipoint.add(std::forward<T>(point));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Circle, std::decay_t<T>>::value>>
	void add_circle(T&& circle) {
		multicircle.add(std::forward<T>(circle));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Convex, std::decay_t<T>>::value>>
	void add_convex(T&& convex) {
		multiconvex.add(std::forward<T>(convex));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<Polygon, std::decay_t<T>>::value>>
	void add_polygon(T&& polygon) {
		multipolygon.add(std::forward<T>(polygon));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiPoint, std::decay_t<T>>::value>>
	void add_multipoint(T&& multipoint_) {
		multipoint.add(std::forward<T>(multipoint_));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiCircle, std::decay_t<T>>::value>>
	void add_multicircle(T&& multicircle_) {
		multicircle.add(std::forward<T>(multicircle_));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiConvex, std::decay_t<T>>::value>>
	void add_multiconvex(T&& multiconvex_) {
		multiconvex.add(std::forward<T>(multiconvex_));
	}

	template <typename T, typename = std::enable_if_t<std::is_same<MultiPolygon, std::decay_t<T>>::value>>
	void add_multipolygon(T&& multipolygon_) {
		multipolygon.add(std::forward<T>(multipolygon_));
	}

	const MultiPoint& getMultiPoint() const noexcept {
		return multipoint;
	}

	const MultiCircle& getMultiCirlce() const noexcept {
		return multicircle;
	}

	const MultiConvex& getMultiConvex() const noexcept {
		return multiconvex;
	}

	const MultiPolygon& getMultiPolygon() const noexcept {
		return multipolygon;
	}

	void simplify() override;

	std::string toWKT() const override;
	std::string to_string() const override;
	std::vector<std::string> getTrixels(bool partials, double error) const override;
	std::vector<range_t> getRanges(bool partials, double error) const override;
};
