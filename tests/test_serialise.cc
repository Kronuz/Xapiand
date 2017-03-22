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

#include "test_serialise.h"

#include "datetime.h"
#include "serialise.h"
#include "utils.h"


const std::vector<test_date_t> test_timestamp_date({
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
});


const std::vector<test_date_t> test_unserialisedate({
	// Date to be serialised.				 Expected date after unserialise.
	{ "2010-10-10T23:05:24.800",             "2010-10-10T23:05:24.800" },
	{ "2010101023:05:24",                    "2010-10-10T23:05:24.000" },
	{ "2010/10/10",                          "2010-10-10T00:00:00.000" },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M", "2020-10-31T23:59:59.999" },
	{ "9115/01/0115:10:50.897-6:40",         "9115-01-01T21:50:50.897" },
	{ "9999/12/20T08:10-03:00||/y",          "9999-12-31T23:59:59.999" },
	{ "0001-01-01T00:00:00.000",             "0001-01-01T00:00:00.000" },
	{ "9999-12-31T23:59:59.000",             "9999-12-31T23:59:59.000" },
});


const std::vector<test_cartesian_t> test_seri_cartesian({
	// Cartesian.                                             Expected serialise Cartesian.
	{ Cartesian( 0.925602814,  0.336891873,  0.172520422),    "\\xaea\\'\\xfe\\x8bJ#\\xe1\\x81~\\x07\\xe6"                },
	{ Cartesian( 0.837915107,  0.224518676,  0.497483301),    "\\xa9\\'%\\xe3\\x84\\x97v\\x14\\x94\\xdc\\x92%"            },
	{ Cartesian( 0.665250371,  0.384082481,  0.640251974),    "\\x9e\\xdc~C\\x8e\\x1a61\\x9d_\\fF"                        },
	{ Cartesian( 0.765933665,  0.407254153,  0.497483341),    "\\xa4\\xdc\\xcca\\x8f{\\xc8\\x89\\x94\\xdc\\x92M"          },
	{ Cartesian( 0.925602814, -0.336891873, -0.172520422),    "\\xaea\\'\\xfec!\\x04\\x1fl\\xed \\x1a"                    },
	{ Cartesian( 0.837915107,  0.224518676, -0.497483301),    "\\xa9\\'%\\xe3\\x84\\x97v\\x14Y\\x8e\\x95\\xdb"            },
	{ Cartesian( 0.665250371, -0.384082481,  0.640251974),    "\\x9e\\xdc~C`P\\xf1\\xcf\\x9d_\\fF"                        },
	{ Cartesian( 0.765933705,  0.407254175,  0.497483262),    "\\xa4\\xdc\\xcc\\x89\\x8f{\\xc8\\x9f\\x94\\xdc\\x91\\xfe"  },
	{ Cartesian(-0.765933705, -0.407254175, -0.497483262),    "I\\x8e[w^\\xef_aY\\x8e\\x96\\x02",                         },
	{ Cartesian(-1.000000000,  0.000000000,  0.000000000),    ";\\x9a\\xca\\x00w5\\x94\\x00w5\\x94\\x00",                 },
	{ Cartesian( 1.000000000,  0.000000000,  0.000000000),    "\\xb2\\xd0^\\x00w5\\x94\\x00w5\\x94\\x00",                 },
});


const std::vector<test_range_t> test_seri_ranges({
	// Range                                          Expected serialise range.
	{ range_t(15061110277275648, 15061247716229119),  "5\\x82\\x00\\x00\\x00\\x00\\x005\\x82\\x1f\\xff\\xff\\xff\\xff" },
	{ range_t(15628458277208064, 15628526996684799),  "7\\x86\\x00\\x00\\x00\\x00\\x007\\x86\\x0f\\xff\\xff\\xff\\xff" },
	{ range_t(15635605102788608, 15635673822265343),  "7\\x8c\\x80\\x00\\x00\\x00\\x007\\x8c\\x8f\\xff\\xff\\xff\\xff" },
	{ range_t(15638628759764992, 15638697479241727),  "7\\x8f@\\x00\\x00\\x00\\x007\\x8fO\\xff\\xff\\xff\\xff"         },
	{ range_t(9007199254740992,   9007199321849855),  " \\x00\\x00\\x00\\x00\\x00\\x00 \\x00\\x00\\x03\\xff\\xff\\xff" },
});


int test_datetotimestamp() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_timestamp_date) {
		std::string timestamp;
		try {
			timestamp = std::to_string(Datetime::timestamp(test.date));
		} catch (const std::exception &exc) {
			timestamp = "";
		}
		if (timestamp != test.serialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Serialise::date is not working. Result: %s Expect: %s", timestamp.c_str(), test.serialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing the transformation between date string and timestamp is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing the transformation between date string and timestamp has mistakes.");
		RETURN(1);
	}
}


int test_unserialise_date() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_unserialisedate) {
		const auto serialised = Serialise::date(test.date);
		const auto date = Unserialise::date(serialised);
		if (date != test.serialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Unserialise::date is not working. Result: %s Expect: %s", date.c_str(), test.serialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing unserialise date is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing unserialise date has mistakes.");
		RETURN(1);
	}
}


int test_serialise_cartesian() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_cartesian) {
		const auto serialised = repr(Serialise::cartesian(test.cartesian), true, false);
		if (serialised != test.serialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Serialise::cartesian is not working. Result: %s Expect: %s", serialised.c_str(), test.serialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing serialise Cartesian is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing serialise Cartesian has mistakes.");
		RETURN(1);
	}
}


int test_unserialise_cartesian() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_cartesian) {
		const auto serialised = Serialise::cartesian(test.cartesian);
		const auto cartesian = Unserialise::cartesian(serialised);
		if (cartesian != test.cartesian) {
			++cont;
			L_ERR(nullptr, "ERROR: Unserialise::cartesian is not working. Result: %s Expect: %s", cartesian.to_string().c_str(), test.cartesian.to_string().c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing unserialise Cartesian is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing unserialise Cartesian has mistakes.");
		RETURN(1);
	}
}


int test_serialise_range() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_ranges) {
		const auto serialised = repr(Serialise::range(test.range), true, false);
		if (serialised != test.serialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Serialise::range is not working. Result: %s Expect: %s", serialised.c_str(), test.serialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing serialise range_t is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing serialise range_t has mistakes.");
		RETURN(1);
	}
}


int test_unserialise_range() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_ranges) {
		const auto serialised = Serialise::range(test.range);
		const auto range = Unserialise::range(serialised);
		if (range != test.range) {
			++cont;
			L_ERR(nullptr, "ERROR: Unserialise::range is not working. Result: %s Expect: %s", range.to_string().c_str(), test.range.to_string().c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing unserialise range_t is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing unserialise range_t has mistakes.");
		RETURN(1);
	}
}
