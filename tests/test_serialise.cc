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
#include "../src/datetime.h"
#include "../src/serialise.h"


const test test_timestamp_date[] = {
	// Date 									Expected timestamp.
	{ "2014-01-01||-1M/y",                      "1388534399.999000"   },
	{ "2014-10-10||-12M",                       "1381363200.000000"   },
	{ "2014-10-10||-42M",                       "1302393600.000000"   },
	{ "2014-10-10||+2M",                        "1418169600.000000"   },
	{ "2014-10-10||+47M",                       "1536537600.000000"   },
	{ "2014-10-10||+200d",                      "1430179200.000000"   },
	{ "2014-10-10||-200d",                      "1395619200.000000"   },
	{ "2014-10-10||+5d",                        "1413331200.000000"   },
	{ "2014-10-10||-5d",                        "1412467200.000000"   },
	{ "2010 12 20 08:10-03:00||-10y",           "977310600.000000"    },
	{ "2010 12 20 08:10-03:00||+10y",           "1608462600.000000"   },
	{ "2010 12 20 08:10-03:00||-100w",          "1232363400.000000"   },
	{ "2010 12 20 08:10-03:00||+100w",          "1353323400.000000"   },
	{ "2010/12/20T08:10-03:00||-17616360h",     "-62126052600.000000" },
	{ "2010/12/20T08:10-03:00||+17616360h",     "64711739400.000000"  },
	{ "0001/12/20T08:10-03:00||//y",            "-62135596800.000000" },
	{ "9999/12/20T08:10-03:00||/y",             "253402300799.999000" },
	{ "2014-10-10",                             "1412899200.000000"   },
	{ "20141010T00:00:00",                      "1412899200.000000"   },
	{ "2014/10/10",                             "1412899200.000000"   },
	{ "2012/10/10T0:00:00",                     "1349827200.000000"   },
	{ "2012-10-10T23:59:59",                    "1349913599.000000"   },
	{ "2010-10-10T10:10:10 +06:30",             "1286682010.000000"   },
	{ "2010-10-10T03:40:10Z",                   "1286682010.000000"   },
	{ "2010/10/1003:40:10+00:00",               "1286682010.000000"   },
	{ "2010 10 10 3:40:10.000-00:00",           "1286682010.000000"   },
	{ "2015-10-10T23:55:58.765-07:50",          "1444549558.765000"   },
	{ "201012208:10-3:00||-1y",                 "1261307400.000000"   },
	{ "2010 12 20 08:10-03:00||+1y",            "1324379400.000000"   },
	{ "2010 12 20 08:10-03:00||+1M",            "1295521800.000000"   },
	{ "2010/12/20T08:10-03:00||-1M",            "1290251400.000000"   },
	{ "2010 12 20 08:10-03:00||+12d",           "1293880200.000000"   },
	{ "2010/12/20T08:10-03:00||-22d",           "1290942600.000000"   },
	{ "2010 12 20 08:10-03:00||+20h",           "1292915400.000000"   },
	{ "2010/12/20T08:10-03:00||-6h",            "1292821800.000000"   },
	{ "2010 12 20 08:10-03:00||+55m",           "1292846700.000000"   },
	{ "2010/12/20T08:10-03:00||-14m",           "1292842560.000000"   },
	{ "2010 12 20 08:10-03:00||+69s",           "1292843469.000000"   },
	{ "2010/12/20T08:10-03:00||-9s",            "1292843391.000000"   },
	{ "2015 04 20 08:10-03:00||+2w",            "1430737800.000000"   },
	{ "2015/04/20T08:10-03:00||-3w",            "1427713800.000000"   },
	{ "2010/12/20T08:10-03:00||/y",             "1293839999.999000"   },
	{ "2010/12/20T08:10-03:00 || //y",          "1262304000.000000"   },
	{ "2010/12/20T08:10-03:00||/M",             "1293839999.999000"   },
	{ "2010/12/20T08:10-03:00||//M",            "1291161600.000000"   },
	{ "2010/12/20T08:10-03:00||/d",             "1292889599.999000"   },
	{ "2010/12/20T08:10-03:00||//d",            "1292803200.000000"   },
	{ "2010/12/20T08:10-03:00  ||  /h",         "1292846399.999000"   },
	{ "2010/12/20 08:10-03:00||//h",            "1292842800.000000"   },
	{ "2010/12/20T08:10-03:00||/m",             "1292843459.999000"   },
	{ "2010/12/20T08:10-03:00||//m",            "1292843400.000000"   },
	{ "2010 12 20 8:10:00.000 -03:00 || /s",    "1292843400.999000"   },
	{ "2010/12/20 08:10:00-03:00||//s",         "1292843400.000000"   },
	{ "2015 04 23 8:10:00.000 -03:00 || /w",    "1430006399.999000"   },
	{ "2015/04/23 08:10:00-03:00||//w",         "1429401600.000000"   },
	{ "2015-10-10T23:55:58.765-06:40||+5y",     "1602398158.765000"   },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M",    "1604188799.999000"   },
	{ "2010 07 21 8:10||+3d-12h+56m/d",         "1279929599.999000"   },
	{ "2010 07 21 8:10||+3d-12h+56m//d",        "1279843200.000000"   },
	{ "2010/12/12||+10M-3h//y",                 "1293840000.000000"   },
	{ "2010 12 10 0:00:00 || +2M/M",            "1298937599.999000"   },
	{ "20100202||/w+3w/M+3M/M-3M+2M/M-2M//M",   "1264982400.000000"   },
	{ "2010/12/12||+10M-3h//y4",                ""                    },
	{ "2010-10/10",                             ""                    },
	{ "201010-10",                              ""                    },
	{ "2010-10-10T 4:55",                       ""                    },
	{ "2010-10-10Z",                            ""                    },
	{ "2010-10-10 09:10:10 - 6:56",             ""                    },
	{ "2010-10-10 09:10:10 -656",               ""                    },
	{ NULL,                                     NULL                  },
	};


const test test_unserialisedate[] {
	// Date to be serialised.				 Expected date after unserialise.
	{ "2010-10-10T23:05:24.800",             "2010-10-10T23:05:24.800" },
	{ "2010101023:05:24",                    "2010-10-10T23:05:24.000" },
	{ "2010/10/10",                          "2010-10-10T00:00:00.000" },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M", "2020-10-31T23:59:59.999" },
	{ "9115/01/0115:10:50.897-6:40",         "9115-01-01T21:50:50.897" },
	{ "9999/12/20T08:10-03:00||/y",          "9999-12-31T23:59:59.999" },
	{ "-62135596800.000",                    "0001-01-01T00:00:00.000" },
	{ "253402300799.999000",                 "9999-12-31T23:59:59.999" },
	{ NULL,                                  NULL                      },
};


const test_cartesian test_seri_cartesian[] {
	// Cartesian.		                                    Expected serialise Cartesian.                       Expected Cartesian after of unserialise.
	{ Cartesian(10.0, 20.0, 0.0, Cartesian::DEGREES),       "r\\xc6]\\xfdO\\xafY\\xe0E\\xe3=\\xe5",             "0.925602814 0.336891873 0.172520422"    },
	{ Cartesian(30.0, 15.0, 0.0, Cartesian::DEGREES),       "m\\x8c[\\xe2H\\xfc\\xac\\x13YA\\xc8$",             "0.837915107 0.224518676 0.497483301"    },
	{ Cartesian(40.0, 30.0, 21.0, Cartesian::DEGREES),      "cA\\xb4BR\\x7fl0a\\xc4BE",                         "0.665250371 0.384082481 0.640251974"    },
	{ Cartesian(30.0, 28.0, 100.0, Cartesian::DEGREES),     "iB\\x02`S\\xe0\\xfe\\x88YA\\xc8L",                 "0.765933665 0.407254153 0.497483341"    },
	{ Cartesian(-10.0, -20.0, 0.0, Cartesian::DEGREES),     "r\\xc6]\\xfd\\'\\x86:\\x1e1RV\\x19",               "0.925602814 -0.336891873 -0.172520422"  },
	{ Cartesian(-30.0, 15.0, 0.0, Cartesian::DEGREES),      "m\\x8c[\\xe2H\\xfc\\xac\\x13\\x1d\\xf3\\xcb\\xda", "0.837915107 0.224518676 -0.497483301"   },
	{ Cartesian(40.0, -30.0, 21.0, Cartesian::DEGREES),     "cA\\xb4B$\\xb6\\'\\xcea\\xc4BE",                   "0.665250371 -0.384082481 0.640251974"   },
	{ Cartesian(30.0, 28.0, -100.0, Cartesian::DEGREES),    "iB\\x02\\x88S\\xe0\\xfe\\x9eYA\\xc7\\xfd",         "0.765933705 0.407254175 0.497483262"    },
	{ Cartesian(-0.765933705, -0.407254175, -0.497483262),  "\\r\\xf3\\x91v#T\\x95`\\x1d\\xf3\\xcc\\x01",       "-0.765933705 -0.407254175 -0.497483262" },
	{ Cartesian(),                                           NULL,                                               NULL                                    },
};


const test_trixel_id test_seri_trixels[] {
	// Trixel's id       Expected serialise id.         Expected id after of unserialise.
	{ 13200083375642939, ".\\xe5g\\xe8\\x9cY;",         13200083375642939 },
	{ 9106317391687190,  " Z%\\xbdW\\xee\\x16",         9106317391687190  },
	{ 14549284226108186, "3\\xb0\\x7f6\\x08\\x8b\\x1a", 14549284226108186 },
	{ 17752546963481661, "?\\x11\\xd8\\xef\\x9d\\xe4=", 17752546963481661 },
	{ 0,                  NULL,                         0                 },
};


int test_datetotimestamp()
{
	int cont = 0;
	for (const test *p = test_timestamp_date; p->str; ++p) {
		std::string date = std::string(p->str);
		std::string timestamp;
		try {
			timestamp = std::to_string(Datetime::timestamp(date));
		} catch (const std::exception &ex) {
			LOG_ERR(NULL, "ERROR: %s\n", ex.what());
			timestamp = "";
		}
		if (timestamp.compare(p->expect) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", timestamp.c_str(), p->expect);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing the transformation between date string and timestamp is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing the transformation between date string and timestamp has mistakes.\n");
		return 1;
	}
}


int test_unserialise_date()
{
	int cont = 0;
	for (const test *p = test_unserialisedate; p->str; ++p) {
		std::string date_s = Serialise::date(p->str);
		std::string date = Unserialise::date(date_s);
		if (date.compare(p->expect) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", date.c_str(), p->expect);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing unserialise date is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing unserialise date has mistakes.\n");
		return 1;
	}
}


int test_serialise_cartesian()
{
	int cont = 0;
	for (const test_cartesian *p = test_seri_cartesian; p->expect_serialise; ++p) {
		Cartesian c = p->cartesian;
		c.normalize();
		std::string res(repr(Serialise::cartesian(c)));
		if (res.compare(p->expect_serialise) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", res.c_str(), p->expect_serialise);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing serialise Cartesian is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing serialise Cartesian has mistakes.\n");
		return 1;
	}
}


int test_unserialise_cartesian()
{
	int cont = 0;
	for (const test_cartesian *p = test_seri_cartesian; p->expect_unserialise; ++p) {
		Cartesian c = p->cartesian;
		c.normalize();
		std::string serialise(Serialise::cartesian(c));
		c = Unserialise::cartesian(serialise);
		char res[40];
		snprintf(res, sizeof(res), "%1.9f %1.9f %1.9f", c.x, c.y, c.z);
		if (strcmp(res, p->expect_unserialise) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", res, p->expect_unserialise);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing unserialise Cartesian is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing unserialise Cartesian has mistakes.\n");
		return 1;
	}
}


int test_serialise_trixel_id()
{
	int cont = 0;
	for (const test_trixel_id *p = test_seri_trixels; p->expect_serialise; ++p) {
		uInt64 trixel_id = p->trixel_id;
		std::string res(repr(Serialise::trixel_id(trixel_id)));
		if (res.compare(p->expect_serialise) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", res.c_str(), p->expect_serialise);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing serialise HTM Trixel's id is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing serialise HTM Trixel's id has mistakes.\n");
		return 1;
	}
}


int test_unserialise_trixel_id()
{
	int cont = 0;
	for (const test_trixel_id *p = test_seri_trixels; p->expect_serialise; ++p) {
		uInt64 trixel_id = p->trixel_id;
		std::string serialise(Serialise::trixel_id(trixel_id));
		trixel_id = Unserialise::trixel_id(serialise);
		if (p->trixel_id != trixel_id) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %llu Expect: %llu\n", trixel_id, p->trixel_id);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing unserialise HTM Trixel's id is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing unserialise HTM Trixel's id has mistakes.\n");
		return 1;
	}
}