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

#include "test_geospatial.h"

#include "gtest/gtest.h"

#include "utils.h"


TEST(testCartesianTransforms, GeoCartesian) {
	EXPECT_EQ(testCartesianTransforms(), 0);
}


TEST(testGrahamScanAlgorithm, GeoGraham) {
	EXPECT_EQ(testGrahamScanAlgorithm(), 0);
}


TEST(testPoint, GeoPoint) {
	EXPECT_EQ(testPoint(), 0);
}


TEST(testMultiPoint, GeoMultiPoint) {
	EXPECT_EQ(testMultiPoint(), 0);
}


TEST(testCircle, GeoCircle) {
	EXPECT_EQ(testCircle(), 0);
}


TEST(testConvex, GeoConvex) {
	EXPECT_EQ(testConvex(), 0);
}


TEST(testPolygon, GeoPolygon) {
	EXPECT_EQ(testPolygon(), 0);
}


TEST(testMultiCircle, GeoMultiCircle) {
	EXPECT_EQ(testMultiCircle(), 0);
}


TEST(testMultiConvex, GeoMultiConvex) {
	EXPECT_EQ(testMultiConvex(), 0);
}


TEST(testMultiPolygon, GeoMultiPolygon) {
	EXPECT_EQ(testMultiPolygon(), 0);
}


TEST(testCollection, GeoCollection) {
	EXPECT_EQ(testCollection(), 0);
}


TEST(testIntersection, GeoIntersection) {
	EXPECT_EQ(testIntersection(), 0);
}


int main(int argc, char **argv) {
	auto initializer = Initializer::create();
	::testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	initializer.destroy();
	return ret;
}
