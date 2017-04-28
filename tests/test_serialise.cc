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
	{ "2014-01-01||-1M/y",                      "1388534399.999999"   },
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
	{ "2010/12/20T08:10-03:00||/y",             "1293839999.999999"   },
	{ "2010/12/20T08:10-03:00 || //y",          "1262304000.000000"   },
	{ "2010/12/20T08:10-03:00||/M",             "1293839999.999999"   },
	{ "2010/12/20T08:10-03:00||//M",            "1291161600.000000"   },
	{ "2010/12/20T08:10-03:00||/d",             "1292889599.999999"   },
	{ "2010/12/20T08:10-03:00||//d",            "1292803200.000000"   },
	{ "2010/12/20T08:10-03:00  ||  /h",         "1292846399.999999"   },
	{ "2010/12/20 08:10-03:00||//h",            "1292842800.000000"   },
	{ "2010/12/20T08:10-03:00||/m",             "1292843459.999999"   },
	{ "2010/12/20T08:10-03:00||//m",            "1292843400.000000"   },
	{ "2010 12 20 8:10:00.000 -03:00 || /s",    "1292843400.999999"   },
	{ "2010/12/20 08:10:00-03:00||//s",         "1292843400.000000"   },
	{ "2015 04 23 8:10:00.000 -03:00 || /w",    "1430006399.999999"   },
	{ "2015/04/23 08:10:00-03:00||//w",         "1429401600.000000"   },
	{ "2015-10-10T23:55:58.765-06:40||+5y",     "1602398158.765000"   },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M",    "1604188799.999999"   },
	{ "2010 07 21 8:10||+3d-12h+56m/d",         "1279929599.999999"   },
	{ "2010 07 21 8:10||+3d-12h+56m//d",        "1279843200.000000"   },
	{ "2010/12/12||+10M-3h//y",                 "1293840000.000000"   },
	{ "2010 12 10 0:00:00 || +2M/M",            "1298937599.999999"   },
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
	{ "2010-10-10T23:05:24.800",             "2010-10-10T23:05:24.8"      },
	{ "2010101023:05:24",                    "2010-10-10T23:05:24"        },
	{ "2010/10/10",                          "2010-10-10T00:00:00"        },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M", "2020-10-31T23:59:59.999999" },
	{ "9115/01/0115:10:50-6:40",             "9115-01-01T21:50:50"        },
	{ "9999/12/20T08:10-03:00||//y",         "9999-01-01T00:00:00"        },
	{ "0001-01-01T00:00:00.000",             "0001-01-01T00:00:00"        },
	{ "9999-12-31T23:59:59.000",             "9999-12-31T23:59:59"        },
	{ "2030-10-10T23:59:59.8979999999",      "2030-10-10T23:59:59.898"    },
	{ "2030-11-11T23:59:59.8979911111",      "2030-11-11T23:59:59.897991" },
	{ "2025-01-21T23:59:59.12",              "2025-01-21T23:59:59.12"     },
	{ "2040-01-21T23:59:59.123",             "2040-01-21T23:59:59.123"    },
	{ "1970-11-29 03:09:09.89756",           "1970-11-29T03:09:09.89756"  },
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


const std::vector<test_uuid_t> test_seri_uuids({
	// Guid                                     Expected serialised uuid.                                            Expected unserialise uuid
	{ "00000000-0000-0000-0000-000000000000",   "\\x10\\x00\\x00",                                                   "00000000-0000-0000-0000-000000000000" },
	{ "00000000-0000-1000-8000-000000000000",   "1\\x00",                                                            "00000000-0000-1000-8000-000000000000" },
	{ "00000000-0000-1000-a000-000000000000",   "(\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\b",                           "00000000-0000-1000-a000-000000000000" },
	{ "00000000-0000-4000-b000-000000000000",   "\\b\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\f",                         "00000000-0000-4000-b000-000000000000" },
	{ "00000000-2000-1000-c000-000000000000",   "\\x80\\x01\\x00\\x00\\x00\\x00`\\x19\\x1e\\x03",                    "00000000-2000-1000-c000-000000000000" },
	{ "00000000-2000-4000-c000-000000000000",   "\\x80\\x01\\x00\\x00\\x00\\x00`\\x19N\\x03",                        "00000000-2000-4000-c000-000000000000" },
	{ "{00000000-2000-2000-0000-000000000000}", "p\\x01\\x00\\x00\\x00\\x00`\\x19.",                                 "00000000-2000-2000-0000-000000000000" },
	{ "4ec97478-c3a9-11e6-bbd0-a46ba9ba5662",   "\\xb8\\x80\\xde\\xf3\\xe8\\x92\\x9dR\\x07",                         "4ec97478-c3a9-11e6-bbd0-a46ba9ba5662" },
	{ "b6e0e797-80fc-11e6-b58a-60f81dc76762",   "\\xaf\\xd8\\xd9q\\x07>\\x98b\\x8dy\\x0en\\xcb\\x0f\\xfc\\xff",      "b6e0e797-80fc-11e6-b58a-60f81dc76762" },
	{ "d095e48f-c64f-4f08-91ec-888e6068dfe0",   "\\x0f\\xf87\\x1a\\x98#\"{\\x04I^\\t\\xfdd \\xd2",                   "d095e48f-c64f-4f08-91ec-888e6068dfe0" },
	{ "c5c52a08-c3b4-11e6-9231-339cb51d7742",   "\\xb8\\x88\\x91\\x12T\\x8a\\x8bi\\x07",                             "c5c52a08-c3b4-11e6-9231-339cb51d7742" },
	{ "{c5c52a08-c3b4-51e6-7231-339cb51d7742}", "\\xf0\\t*\\xc5\\xc5\\xb4\\x03\\x00P\\t\\xddu\\xd4r\\xce\\xc4\\xc8", "c5c52a08-c3b4-51e6-7231-339cb51d7742" },
	// Compound uuids
	{
		"{00000000-0000-0000-0000-000000000000;00000000-0000-1000-8000-000000000000;00000000-0000-1000-a000-000000000000}",
		"\\x10\\x00\\x001\\x00(\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\b",
		"{EAAAMQAoAAAAAAAAAAg}"
	},
	{
		"{00000000-0000-4000-b000-000000000000;00000000-2000-1000-c000-000000000000;00000000-2000-4000-c000-000000000000}",
		"\\b\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\f\\x80\\x01\\x00\\x00\\x00\\x00`\\x19\\x1e\\x03\\x80\\x01\\x00\\x00\\x00\\x00`\\x19N\\x03",
		"{CAAAAAAAAAAMgAEAAAAAYBkeA4ABAAAAAGAZTgM}"
	},
	{
		"{00000000-2000-2000-0000-000000000000;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662;b6e0e797-80fc-11e6-b58a-60f81dc76762}",
		"p\\x01\\x00\\x00\\x00\\x00`\\x19.\\xb8\\x80\\xde\\xf3\\xe8\\x92\\x9dR\\x07\\xaf\\xd8\\xd9q\\x07>\\x98b\\x8dy\\x0en\\xcb\\x0f\\xfc\\xff",
		"{cAEAAAAAYBkuuIDe8-iSnVIHr9jZcQc-mGKNeQ5uyw_8_w}"
	},
	{
		"{d095e48f-c64f-4f08-91ec-888e6068dfe0;c5c52a08-c3b4-11e6-9231-339cb51d7742;c5c52a08-c3b4-51e6-7231-339cb51d7742}",
		"\\x0f\\xf87\\x1a\\x98#\"{\\x04I^\\t\\xfdd \\xd2\\xb8\\x88\\x91\\x12T\\x8a\\x8bi\\x07\\xf0\\t*\\xc5\\xc5\\xb4\\x03\\x00P\\t\\xddu\\xd4r\\xce\\xc4\\xc8",
		"{D_g3GpgjInsESV4J_WQg0riIkRJUiotpB_AJKsXFtAMAUAndddRyzsTI}"
	},
	{
		"{EAAAMQAoAAAAAAAAAAg}",
		"\\x10\\x00\\x001\\x00(\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\b",
		"{EAAAMQAoAAAAAAAAAAg}"
	},
	{
		"{CAAAAAAAAAAMgAEAAAAAYBkeA4ABAAAAAGAZTgM}",
		"\\b\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\f\\x80\\x01\\x00\\x00\\x00\\x00`\\x19\\x1e\\x03\\x80\\x01\\x00\\x00\\x00\\x00`\\x19N\\x03",
		"{CAAAAAAAAAAMgAEAAAAAYBkeA4ABAAAAAGAZTgM}"
	},
	{
		"{cAEAAAAAYBkuuIDe8-iSnVIHr9jZcQc-mGKNeQ5uyw_8_w}",
		"p\\x01\\x00\\x00\\x00\\x00`\\x19.\\xb8\\x80\\xde\\xf3\\xe8\\x92\\x9dR\\x07\\xaf\\xd8\\xd9q\\x07>\\x98b\\x8dy\\x0en\\xcb\\x0f\\xfc\\xff",
		"{cAEAAAAAYBkuuIDe8-iSnVIHr9jZcQc-mGKNeQ5uyw_8_w}"
	},
	{
		"{D_g3GpgjInsESV4J_WQg0riIkRJUiotpB_AJKsXFtAMAUAndddRyzsTI}",
		"\\x0f\\xf87\\x1a\\x98#\"{\\x04I^\\t\\xfdd \\xd2\\xb8\\x88\\x91\\x12T\\x8a\\x8bi\\x07\\xf0\\t*\\xc5\\xc5\\xb4\\x03\\x00P\\t\\xddu\\xd4r\\xce\\xc4\\xc8",
		"{D_g3GpgjInsESV4J_WQg0riIkRJUiotpB_AJKsXFtAMAUAndddRyzsTI}"
	},
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


int test_serialise_uuid() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_uuids) {
		const auto serialised = repr(Serialise::uuid(test.uuid), true, false);
		if (serialised != test.serialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Serialise::uuid is not working. Result: %s Expect: %s", serialised.c_str(), test.serialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing serialise uuid is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing serialise uuid has mistakes.");
		RETURN(1);
	}
}


int test_unserialise_uuid() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : test_seri_uuids) {
		const auto serialised = Serialise::uuid(test.uuid);
		const auto uuid = Unserialise::uuid(serialised);
		if (uuid != test.unserialised) {
			++cont;
			L_ERR(nullptr, "ERROR: Unserialise::uuid is not working. Result: %s Expect: %s", uuid.c_str(), test.unserialised.c_str());
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing unserialise uuid is correct!");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing unserialise uuid has mistakes.");
		RETURN(1);
	}
}
