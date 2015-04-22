/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include "tests.h"


const test test_timestamp_date[] = {
	{ "2014-10-10",                             "1412899200.000000" },
	{ "20141010",                               "1412899200.000000" },
	{ "2014/10/10",                             "1412899200.000000" },
	{ "2012/10/10T0:00:00",                     "1349827200.000000" },
	{ "2012-10-10T23:59:59",                    "1349913599.000000" },
	{ "2010-10-10T10:10:10 +06:30",             "1286682010.000000" },
	{ "2010-10-10T03:40:10Z",                   "1286682010.000000" },
	{ "2010/10/1003:40:10+00:00",               "1286682010.000000" },
	{ "2010 10 10 3:40:10.000-00:00",           "1286682010.000000" },
	{ "2015-10-10T23:55:58.765-07:50",          "1444549558.765000" },
	{ "201012208:10-3:00||-1y",                 "1261307400.000000" },
	{ "2010 12 20 08:10-03:00||+1y",            "1324379400.000000" },
	{ "2010 12 20 08:10-03:00||+1M",            "1295521800.000000" },
	{ "2010/12/20T08:10-03:00||-1M",            "1290251400.000000" },
	{ "2010 12 20 08:10-03:00||+12d",           "1293880200.000000" },
	{ "2010/12/20T08:10-03:00||-22d",           "1290942600.000000" },
	{ "2010 12 20 08:10-03:00||+20h",           "1292915400.000000" },
	{ "2010/12/20T08:10-03:00||-6h",            "1292821800.000000" },
	{ "2010 12 20 08:10-03:00||+55m",           "1292846700.000000" },
	{ "2010/12/20T08:10-03:00||-14m",           "1292842560.000000" },
	{ "2010 12 20 08:10-03:00||+69s",           "1292843469.000000" },
	{ "2010/12/20T08:10-03:00||-9s",            "1292843391.000000" },
	{ "2015 04 20 08:10-03:00||+2w",            "1430737800.000000" },
	{ "2015/04/20T08:10-03:00||-3w",            "1427713800.000000" },
	{ "2010/12/20T08:10-03:00||/y",             "1293839999.999000" },
	{ "2010/12/20T08:10-03:00 || //y",          "1262304000.000000" },
	{ "2010/12/20T08:10-03:00||/M",             "1293839999.999000" },
	{ "2010/12/20T08:10-03:00||//M",            "1291161600.000000" },
	{ "2010/12/20T08:10-03:00||/d",             "1292889599.999000" },
	{ "2010/12/20T08:10-03:00||//d",            "1292803200.000000" },
	{ "2010/12/20T08:10-03:00  ||  /h",         "1292846399.999000" },
	{ "2010/12/20 08:10-03:00||//h",            "1292842800.000000" },
	{ "2010/12/20T08:10-03:00||/m",             "1292843459.999000" },
	{ "2010/12/20T08:10-03:00||//m",            "1292843400.000000" },
	{ "2010 12 20 8:10:00.000 -03:00 || /s",    "1292843400.999000" },
	{ "2010/12/20 08:10:00-03:00||//s",         "1292843400.000000" },
	{ "2015 04 23 8:10:00.000 -03:00 || /w",    "1430006399.999000" },
	{ "2015/04/23 08:10:00-03:00||//w",         "1429401600.000000" },
	{ "2015-10-10T23:55:58.765-06:40||+5y",     "1602398158.765000" },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M",    "1604188799.999000" },
	{ "2010 07 21 8:10||+3d-12h+56m/d",         "1279929599.999000" },
	{ "2010 07 21 8:10||+3d-12h+56m//d",        "1279843200.000000" },
	{ "2010/12/12||+10M-3h//y",                 "1293840000.000000" },
	{ "2010 12 10 0:00:00 || +2M/M",            "1298937599.999000" },
	{ "20100202||/w+3w/M+3M/M-3M+2M/M-2M//M",   "1264982400.000000" },
	{ "2010/12/12||+10M-3h//y4",                ""                  },
	{ "2010-10/10",                             ""                  },
	{ "201010-10",                              ""                  },
	{ "2010-10-10T 4:55",                       ""                  },
	{ "2010-10-10Z",                            ""                  },
	{ "2010-10-10 09:10:10 - 6:56",             ""                  },
	{ "2010-10-10 09:10:10 -656",               ""                  },
	{ NULL,                                     NULL                },
};


const test_distance test_distanceLatLong_fields[] = {
	{"23.56, 48.76 ; 40mi",  64373.76 },
	{"23.56, 48.76 ; 40km",  40000.00 },
	{"23.56, 48.76 ; 40m",   40       },
	{"23.56, 48.76 ; 40",    40       },
	{"23.56,48.76;40yd",     36.57600 },
	{"23.56, 48.76; 40ft",   12.19200 },
	{"23.56, 48.76 ;40in",   1.01600  },
	{"23.56,48.76 ; 40cm",   0.4      },
	{"23.56, 48.76 ; 40mm",  0.04     },
	{"23.56, 48.76 ; 40mmi", -1.0     },
	{"23.56, 48.76k ; 40mm", -1.0     },
	{NULL,                   -1.0     },
};


bool test_datetotimestamp()
{
	int cont = 0;
	for (const test *p = test_timestamp_date; p->str; ++p) {
		std::string date = std::string(p->str);
		LOG(NULL, "Date orig: %s\n", date.c_str());
		std::string timestamp(timestamp_date(date));
		if (timestamp.compare(p->expect) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", timestamp.c_str(), p->expect);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Test is correct.\n");
		return true;
	} else {
		LOG_ERR(NULL, "ERROR: Test has mistakes.\n");
		return false;
	}
}


bool test_distanceLatLong()
{
	int cont = 0;
	for (const test_distance *p = test_distanceLatLong_fields; p->str; ++p) {
		double coords_[3];
		LOG(NULL, "Distance LatLong: %s\n", p->str);
		if (get_coords(p->str, coords_) == 0) {
			if (coords_[2] != p->val) {
				cont++;
				LOG_ERR(NULL, "ERROR: Resul: %f Expect: %f\n", coords_[2], p->val);
			}
		} else {
			if (p->val != -1.0) {
				cont++;
				LOG_ERR(NULL, "ERROR: Resul: Error en format(-1) Expect: %f\n", p->val);
			}
		}
	}

	if (cont == 0) {
		LOG(NULL, "Test is correct.\n");
		return true;
	} else {
		LOG_ERR(NULL, "ERROR: Test has mistakes.\n");
		return false;
	}
}