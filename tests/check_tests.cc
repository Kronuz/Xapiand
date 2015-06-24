/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "test_serialise.h"
#include "test_htm.h"
#include "test_wkt_parser.h"
#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <check.h>


START_TEST(test_datetotimestamp)
{
	ck_assert_int_eq(test_datetotimestamp(), 0);
}
END_TEST



START_TEST(test_distanceLatLong)
{
	ck_assert_int_eq(test_distanceLatLong(), 0);
}
END_TEST


START_TEST(test_unserialise_date)
{
	ck_assert_int_eq(test_unserialise_date(), 0);
}
END_TEST


START_TEST(test_unserialise_geo)
{
	ck_assert_int_eq(test_unserialise_geo(), 0);
}
END_TEST


START_TEST(test_cartesian_transforms)
{
	ck_assert_int_eq(test_cartesian_transforms(), 0);
}
END_TEST


START_TEST(test_hullConvex)
{
	ck_assert_int_eq(test_hullConvex(), 0);
}
END_TEST


START_TEST(test_HTM_chull)
{
	ck_assert_int_eq(test_HTM_chull(), 0);
}
END_TEST


START_TEST(test_HTM_circle)
{
	ck_assert_int_eq(test_HTM_circle(), 0);
}
END_TEST


START_TEST(test_wkt_parser)
{
	ck_assert_int_eq(test_wkt_parser(), 0);
}
END_TEST


START_TEST(test_wkt_speed)
{
	ck_assert_int_eq(test_wkt_speed(), 0);
}
END_TEST


Suite* test_suite_serialise(void)
{
	Suite *s;
	TCase *tc_distanceLatLong;
	TCase *tc_datetotimestamp;

	s = suite_create("Tests of serialise");

	tc_datetotimestamp = tcase_create("Date to timestamp");
	tcase_add_test(tc_datetotimestamp, test_datetotimestamp);
	suite_add_tcase(s, tc_datetotimestamp);

	tc_distanceLatLong = tcase_create("Distance Latitud Longitude");
	tcase_add_test(tc_distanceLatLong, test_distanceLatLong);
	suite_add_tcase(s, tc_distanceLatLong);

	return s;
}


Suite* test_suite_unserialise(void)
{
	Suite *s;
	TCase *tc_unserialise_date;
	TCase *tc_unserialise_geo;

	s = suite_create("Tests of unserialise");

	tc_unserialise_date = tcase_create("Unserialise date");
	tcase_add_test(tc_unserialise_date, test_unserialise_date);
	suite_add_tcase(s, tc_unserialise_date);

	tc_unserialise_geo = tcase_create("Unserialise GeoSpatial");
	tcase_add_test(tc_unserialise_geo, test_unserialise_geo);
	suite_add_tcase(s, tc_unserialise_geo);

	return s;
}


Suite* test_suite_cartesian(void)
{
	Suite *s;
	TCase *tc_cartesian_transforms;

	s = suite_create("Test of transformation of coordinates between CRS");

	tc_cartesian_transforms = tcase_create("Transformation of coordinates between CRS");
	tcase_add_test(tc_cartesian_transforms, test_cartesian_transforms);
	suite_add_tcase(s, tc_cartesian_transforms);

	return s;
}


Suite* test_suite_convex_hull(void)
{
	Suite *s;
	TCase *tc_hull_convex;

	s = suite_create("Test of Geometry Hull Convex");

	tc_hull_convex = tcase_create("Convex hull from a set point");
	tcase_add_test(tc_hull_convex, test_hullConvex);
	suite_add_tcase(s, tc_hull_convex);

	return s;
}


Suite* test_suite_HTM(void)
{
	Suite *s;
	TCase *tc_htm_chull;
	TCase *tc_htm_circle;

	s = suite_create("Test of HTM");

	tc_htm_chull = tcase_create("HTM for Polygons");
	tcase_add_test(tc_htm_chull, test_HTM_chull);
	suite_add_tcase(s, tc_htm_chull);

	tc_htm_circle = tcase_create("HTM for Bounding Circles");
	tcase_add_test(tc_htm_circle, test_HTM_circle);
	suite_add_tcase(s, tc_htm_circle);

	return s;
}


Suite* test_suite_wkt_parser(void)
{
	Suite *s;
	TCase *tc_wkt_parser;

	s = suite_create("Test of WKT parser");

	tc_wkt_parser = tcase_create("WKT parser");
	tcase_add_test(tc_wkt_parser, test_wkt_parser);
	suite_add_tcase(s, tc_wkt_parser);

	return s;
}


Suite* test_suite_wkt_speed(void)
{
	Suite *s;
	TCase *tc_wkt_speed;

	s = suite_create("Test WKT speed");

	tc_wkt_speed = tcase_create("WKT speed");
	tcase_add_test(tc_wkt_speed, test_wkt_speed);
	suite_add_tcase(s, tc_wkt_speed);

	return s;
}


int main(void)
{
	int number_failed = 0;
	Suite *serialise, *unserialise, *cartesian, *chull, *HTM, *WKT, *WKTspeed;
	SRunner *sr;

	serialise = test_suite_serialise();
	sr = srunner_create(serialise);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	unserialise = test_suite_unserialise();
	sr = srunner_create(unserialise);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	cartesian = test_suite_cartesian();
	sr = srunner_create(cartesian);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	chull = test_suite_convex_hull();
	sr = srunner_create(chull);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	HTM = test_suite_HTM();
	sr = srunner_create(HTM);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	WKT = test_suite_wkt_parser();
	sr = srunner_create(WKT);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	WKTspeed = test_suite_wkt_speed();
	sr = srunner_create(WKTspeed);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
