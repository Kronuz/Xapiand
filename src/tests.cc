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

#include "tests.h"


const test test_timestamp_date[] = {
	// Date 									Expected timestamp.
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


const test_str_double test_distanceLatLong_fields[] = {
	// Distance LatLong       Expected distance.
	{ "23.56, 48.76 ; 40mi",  64373.76 },
	{ "23.56, 48.76 ; 40km",  40000.00 },
	{ "23.56, 48.76 ; 40m",   40       },
	{ "23.56, 48.76 ; 40",    40       },
	{ "23.56,48.76;40yd",     36.57600 },
	{ "23.56, 48.76; 40ft",   12.19200 },
	{ "23.56, 48.76 ;40in",   1.01600  },
	{ "23.56,48.76 ; 40cm",   0.4      },
	{ "23.56, 48.76 ; 40mm",  0.04     },
	{ "23.56, 48.76 ; 40mmi", -1.0     },
	{ "23.56, 48.76k ; 40mm", -1.0     },
	{ NULL,                   -1.0     },
};


const test test_unserialisedate[] {
	// Date to be serialised.				 Expected date after unserialise.
	{ "2010-10-10T23:05:24.800",             "2010-10-10T23:05:24.800" },
	{ "2010101023:05:24",                    "2010-10-10T23:05:24.000" },
	{ "2010/10/10",                          "2010-10-10T00:00:00.000" },
	{ "2015-10-10T23:55:58.765-6:40||+5y/M", "2020-10-31T23:59:59.999" },
	{ "9115/01/0115:10:50.897-6:40",         "9115-01-01T21:50:50.897" },
	{ NULL,                                   NULL                     },
};


const test test_unserialiseLatLong[] {
	// Set of coordinates to serialise.		 Expected coordinates after unserialse.s
	{ "20.35,78.90,23.45,32.14",             "20.35,78.9,23.45,32.14" },
	{ "20.35, 78.90",                        "20.35,78.9"             },
	{ "20.35 , 78.90 , 23.45 , 32.14",       "20.35,78.9,23.45,32.14" },
	{ "20, 78.90, 23.010, 32",               "20,78.9,23.01,32"       },
	{ NULL,                                   NULL                    },
};


// Testing the transformation between date string and timestamp.
int test_datetotimestamp()
{
	int cont = 0;
	for (const test *p = test_timestamp_date; p->str; ++p) {
		std::string date = std::string(p->str);
		std::string timestamp(timestamp_date(date));
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


// Testing the conversion of units in LatLong Distance.
int test_distanceLatLong()
{
	int cont = 0;
	for (const test_str_double *p = test_distanceLatLong_fields; p->str; ++p) {
		double coords_[3];
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
		LOG(NULL, "Testing the conversion of units in LatLong Distance is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing the conversion of units in LatLong Distance has mistakes.\n");
		return 1;
	}
}


// Testing unserialise date.
int test_unserialise_date()
{
	int cont = 0;
	for (const test *p = test_unserialisedate; p->str; ++p) {
		std::string date_s = serialise_date(p->str);
		std::string date = unserialise_date(date_s);
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


// Testing unserialise LatLong coordinates.
int test_unserialise_geo()
{
	int cont = 0;
	for (const test *p = test_unserialiseLatLong; p->str; ++p) {
		std::string geo_s = serialise_geo(p->str);
		std::string geo = unserialise_geo(geo_s);
		if (geo.compare(p->expect) != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Resul: %s Expect: %s\n", geo.c_str(), p->expect);
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing unserialise LatLong coordinates is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing unserialise LatLong coordinates has mistakes.\n");
		return 1;
	}
}


// Testing the transformation of coordinates between CRS.
int test_cartesian_transforms()
{
	std::vector<test_transform_t> SRID_2_WGS84;
	// WGS72 to WGS84  (4322 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1238
	SRID_2_WGS84.push_back({ 4322, 20.0,   10.0, 30.0, "20°0'0.141702''N  10°0'0.554000''E  30.959384"    });
	SRID_2_WGS84.push_back({ 4322, 20.0,  -10.0, 30.0, "20°0'0.141702''N  9°59'59.446000''W  30.959384"   });
	SRID_2_WGS84.push_back({ 4322, -20.0,  10.0, 30.0, "19°59'59.866682''S  10°0'0.554000''E  27.881203"  });
	SRID_2_WGS84.push_back({ 4322, -20.0, -10.0, 30.0, "19°59'59.866682''S  9°59'59.446000''W  27.881203" });

	// NAD83 to WGS84  (4269 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1252
	SRID_2_WGS84.push_back({ 4269, 20.0,   10.0, 30.0, "19°59'59.956556''N  10°0'0.027905''E  30.746560"   });
	SRID_2_WGS84.push_back({ 4269, 20.0,  -10.0, 30.0, "19°59'59.960418''N  9°59'59.960148''W  30.420209" });
	SRID_2_WGS84.push_back({ 4269, -20.0,  10.0, 30.0, "20°0'0.017671''S  10°0'0.027905''E  31.430600"    });
	SRID_2_WGS84.push_back({ 4269, -20.0, -10.0, 30.0, "20°0'0.021534''S  9°59'59.960148''W  31.104249"   });

	// NAD27 to WGS84  (4267 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1173
	SRID_2_WGS84.push_back({ 4267, 20.0,   10.0, 30.0, "20°0'0.196545''N  10°0'5.468256''E  150.554523"    });
	SRID_2_WGS84.push_back({ 4267, 20.0,  -10.0, 30.0, "20°0'0.814568''N  9°59'54.627272''W  98.338209"    });
	SRID_2_WGS84.push_back({ 4267, -20.0,  10.0, 30.0, "19°59'49.440208''S  10°0'5.468256''E  30.171742"   });
	SRID_2_WGS84.push_back({ 4267, -20.0, -10.0, 30.0, "19°59'50.058155''S  9°59'54.627272''W  -22.045563" });

	// OSGB36 to WGS84  (4277 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1314
	SRID_2_WGS84.push_back({ 4277, 20.0,   10.0, 30.0, "20°0'13.337317''N  9°59'53.865759''E  -86.980683"   });
	SRID_2_WGS84.push_back({ 4277, 20.0,  -10.0, 30.0, "20°0'12.801456''N  10°0'0.769107''W  -46.142419"    });
	SRID_2_WGS84.push_back({ 4277, -20.0,  10.0, 30.0, "19°59'40.643875''S  9°59'54.003573''E  -457.728199" });
	SRID_2_WGS84.push_back({ 4277, -20.0, -10.0, 30.0, "19°59'40.212914''S  10°0'0.693312''W  -416.880621"  });

	// TM75 to WGS84  (4300 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1954
	SRID_2_WGS84.push_back({ 4300, 20.0,   10.0, 30.0, "20°0'13.892799''N  9°59'52.446296''E  -87.320347"   });
	SRID_2_WGS84.push_back({ 4300, 20.0,  -10.0, 30.0, "20°0'13.751990''N  10°0'1.815691''W  -44.678652"    });
	SRID_2_WGS84.push_back({ 4300, -20.0,  10.0, 30.0, "19°59'39.325125''S  9°59'51.677477''E  -473.515164" });
	SRID_2_WGS84.push_back({ 4300, -20.0, -10.0, 30.0, "19°59'38.457075''S  10°0'2.530766''W  -430.919043"  });

	// TM65 to WGS84  (4299 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 4299, 20.0,   10.0, 30.0, "20°0'13.891148''N  9°59'52.446252''E  -87.306642"   });
	SRID_2_WGS84.push_back({ 4299, 20.0,  -10.0, 30.0, "20°0'13.750355''N  10°0'1.815376''W  -44.666252"    });
	SRID_2_WGS84.push_back({ 4299, -20.0,  10.0, 30.0, "19°59'39.326103''S  9°59'51.677433''E  -473.472045" });
	SRID_2_WGS84.push_back({ 4299, -20.0, -10.0, 30.0, "19°59'38.458068''S  10°0'2.530451''W  -430.877230"  });

	// ED79 to WGS84  (4668 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/15752
	SRID_2_WGS84.push_back({ 4668, 20.0,   10.0, 30.0, "19°59'55.589986''N  9°59'57.193708''E  134.068052" });
	SRID_2_WGS84.push_back({ 4668, 20.0,  -10.0, 30.0, "19°59'55.211469''N  10°0'3.833722''W  166.051242"  });
	SRID_2_WGS84.push_back({ 4668, -20.0,  10.0, 30.0, "20°0'2.862582''S  9°59'57.193708''E  215.468007"   });
	SRID_2_WGS84.push_back({ 4668, -20.0, -10.0, 30.0, "20°0'2.484033''S  10°0'3.833722''W  247.450787"    });

	// ED50 to WGS84  (4230 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1133
	SRID_2_WGS84.push_back({ 4230, 20.0,   10.0, 30.0, "19°59'55.539823''N  9°59'57.199681''E  132.458626" });
	SRID_2_WGS84.push_back({ 4230, 20.0,  -10.0, 30.0, "19°59'55.161306''N  10°0'3.839696''W  164.441824"  });
	SRID_2_WGS84.push_back({ 4230, -20.0,  10.0, 30.0, "20°0'2.934649''S  9°59'57.199681''E  215.226660"   });
	SRID_2_WGS84.push_back({ 4230, -20.0, -10.0, 30.0, "20°0'2.556100''S  10°0'3.839696''W  247.209441"    });

	// TOYA to WGS84  (4301 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1230
	SRID_2_WGS84.push_back({ 4301, 20.0,   10.0, 30.0, "20°0'22.962090''N  10°0'18.062821''E  -521.976076"   });
	SRID_2_WGS84.push_back({ 4301, 20.0,  -10.0, 30.0, "20°0'24.921332''N  9°59'43.705140''W  -687.433480"   });
	SRID_2_WGS84.push_back({ 4301, -20.0,  10.0, 30.0, "19°59'41.092892''S  10°0'18.062821''E  -990.556329"  });
	SRID_2_WGS84.push_back({ 4301, -20.0, -10.0, 30.0, "19°59'43.051188''S  9°59'43.705140''W  -1156.025959" });

	// DHDN to WGS84  (4314 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1673
	SRID_2_WGS84.push_back({ 4314, 20.0,   10.0, 30.0, "20°0'7.291150''N  9°59'56.608634''E  48.138765"      });
	SRID_2_WGS84.push_back({ 4314, 20.0,  -10.0, 30.0, "20°0'7.333754''N  9°59'56.393946''W  13.848005"      });
	SRID_2_WGS84.push_back({ 4314, -20.0,  10.0, 30.0, "19°59'42.318425''S  9°59'57.393082''E  -235.013109"  });
	SRID_2_WGS84.push_back({ 4314, -20.0, -10.0, 30.0, "19°59'43.086952''S  9°59'55.697370''W  -269.257292"  });

	// OEG to WGS84  (4229 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1148
	SRID_2_WGS84.push_back({ 4229, 20.0,   10.0, 30.0, "20°0'0.873728''N  10°0'4.503259''E  -13.466677"  });
	SRID_2_WGS84.push_back({ 4229, 20.0,  -10.0, 30.0, "20°0'1.298641''N  9°59'57.049898''W  -49.366075" });
	SRID_2_WGS84.push_back({ 4229, -20.0,  10.0, 30.0, "20°0'1.668233''S  10°0'4.503259''E  -4.574003"   });
	SRID_2_WGS84.push_back({ 4229, -20.0, -10.0, 30.0, "20°0'2.093151''S  9°59'57.049898''W  -40.473350" });

	// AGD84 to WGS84  (4203 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1236
	SRID_2_WGS84.push_back({ 4203, 20.0,   10.0, 30.0, "20°0'5.339442''N  9°59'59.220714''E  -13.586401"    });
	SRID_2_WGS84.push_back({ 4203, 20.0,  -10.0, 30.0, "20°0'5.064184''N  10°0'2.116232''W  2.879302"       });
	SRID_2_WGS84.push_back({ 4203, -20.0,  10.0, 30.0, "19°59'57.371712''S  9°59'59.433464''E  -110.463889" });
	SRID_2_WGS84.push_back({ 4203, -20.0, -10.0, 30.0, "19°59'57.257055''S  10°0'2.001422''W  -93.987306"   });

	// SAD69 to WGS84  (4618 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1864
	SRID_2_WGS84.push_back({ 4618, 20.0,   10.0, 30.0, "19°59'59.357117''N  10°0'0.374382''E  -13.677770" });
	SRID_2_WGS84.push_back({ 4618, 20.0,  -10.0, 30.0, "19°59'59.360979''N  10°0'0.306624''W  -14.004125" });
	SRID_2_WGS84.push_back({ 4618, -20.0,  10.0, 30.0, "20°0'1.862864''S  10°0'0.374382''E  14.368110"    });
	SRID_2_WGS84.push_back({ 4618, -20.0, -10.0, 30.0, "20°0'1.866726''S  10°0'0.306624''W  14.041756"    });

	// PUL42 to WGS84  (4178 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1334
	SRID_2_WGS84.push_back({ 4178, 20.0,   10.0, 30.0, "19°59'57.750301''N  9°59'56.403911''E  92.107732" });
	SRID_2_WGS84.push_back({ 4178, 20.0,  -10.0, 30.0, "19°59'57.019651''N  10°0'3.265190''W  123.917120" });
	SRID_2_WGS84.push_back({ 4178, -20.0,  10.0, 30.0, "20°0'2.270413''S  9°59'57.198773''E  133.835302"  });
	SRID_2_WGS84.push_back({ 4178, -20.0, -10.0, 30.0, "20°0'2.247538''S  10°0'2.616278''W  165.691341"   });

	// MGI1901 to WGS84  (3906 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 3906, 20.0,   10.0, 30.0, "20°0'8.506072''N  9°59'48.107356''E  -15.039391"    });
	SRID_2_WGS84.push_back({ 3906, 20.0,  -10.0, 30.0, "20°0'7.306781''N  10°0'5.296242''W  -75.952463"     });
	SRID_2_WGS84.push_back({ 3906, -20.0,  10.0, 30.0, "19°59'42.260450''S  9°59'52.463078''E  -364.894519" });
	SRID_2_WGS84.push_back({ 3906, -20.0, -10.0, 30.0, "19°59'44.898670''S  10°0'1.823681''W  -425.555326"  });

	// GGRS87 to WGS84  (4121 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1272
	SRID_2_WGS84.push_back({ 4121, 20.0,   10.0, 30.0, "20°0'9.581041''N  10°0'3.727855''E  -58.402327"     });
	SRID_2_WGS84.push_back({ 4121, 20.0,  -10.0, 30.0, "20°0'9.869982''N  9°59'58.660140''W  -82.810562"    });
	SRID_2_WGS84.push_back({ 4121, -20.0,  10.0, 30.0, "19°59'54.508366''S  10°0'3.727855''E  -227.104937"  });
	SRID_2_WGS84.push_back({ 4121, -20.0, -10.0, 30.0, "19°59'54.797256''S  9°59'58.660140''W  -251.513821" });

	std::vector<test_transform_t>::const_iterator it = SRID_2_WGS84.begin();
	int cont = 0;

	try {
		for ( ;it != SRID_2_WGS84.end(); it++) {
			Cartesian c(it->lat_src, it->lon_src, it->h_src, Cartesian::DEGREES, it->SRID);
			double lat, lon, height;
			c.toGeodetic(lat, lon, height);
			std::string get = c.Decimal2Degrees();
			if (get.compare(it->res) != 0) {
				cont++;
				LOG_ERR(NULL, "ERROR: Resul: %s  Expected: %s\n", get.c_str(), it->res.c_str());
			}
		}
	} catch (const std::exception &e) {
		LOG_ERR(NULL, "ERROR: %s\n", e.what());
		cont++;
	}

	if (cont == 0) {
		LOG(NULL, "Testing the transformation of coordinates between CRS is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing the transformation of coordinates between CRS has mistakes.\n");
		return 1;
	}
}


// Return number of mistakes.
int do_tests()
{
	return test_datetotimestamp() + test_distanceLatLong() + test_unserialise_date() + test_unserialise_geo() + test_cartesian_transforms();
}