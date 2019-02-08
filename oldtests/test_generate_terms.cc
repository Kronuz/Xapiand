/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_generate_terms.h"

#include <limits.h>

#include "../src/multivalue/generate_terms.h"
#include "utils.h"


const std::vector<testQuery_t> numeric_tests({
	// Testing positives.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"1200", "2500", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query((N5N\\x80 AND (N4N\\xc0\\xbd OR N4N\\xc0\\xfd)))"
		// "N5:0 AND (N4:1000 OR N4:2000)"
	},
	// Do not find a Lower accuracy.
	{
		"1200.100", "1200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(N1N\\xc0\\xcb)"
		// N1:1200"
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"10200.100", "100200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((N6N\\x80 AND ((((((((N5N\\xc1\\x8e  OR N5N\\xc1\\xce ) OR N5N\\xc1\\xf50) OR N5N\\xc2\\x0e ) OR N5N\\xc2!\\xa8) OR N5N\\xc250) OR N5N\\xc2D\\x5c) OR N5N\\xc2N ) OR N5N\\xc2W\\xe4)) OR N5N\\xc2a\\xa8))"
		// (N6:0 AND (N5:10000 OR N5:20000 OR N5:30000 OR N5:40000 OR N5:50000 OR N5:60000 OR N5:70000 OR N5:80000 OR N5:90000) OR N5:100000
	},
	// Do not find a upper accuracy.
	{
		"10200.100", "1000200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((((((((((N6N\\xc3:\\x12 OR N6N\\x80) OR N6N\\xc2a\\xa8) OR N6N¡\\xa8) OR N6N\\xc2\\xc9>) OR N6N\\xc2\\xe1\\xa8) OR N6N\\xc2\\xfa\\x12) OR N6N\\xc3\\x09>) OR N6N\\xc3\\x15s) OR N6N\\xc3!\\xa8) OR N6N\\xc3-\\xdd))"
		// "N6:0 OR N6:100000 OR N6:200000 OR N6:300000 OR N6:400000 OR N6:500000 OR N6:600000 OR N6:700000 OR N6:800000 OR N6:900000 OR N6:1000000"
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"10200.100", "55000200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query()"
	},

	// Testing special case.
	// When the accuracy it is empty.
	{
		"10200.100", "11000200.200", { }, { },
		"Query()"
	},
	// When the range is negative.
	{
		"1000", "900", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query()"
	},
	// Do not find a lower accuracy because it exceeded the number of terms.
	{
		"-1200.300", "1200.200", { 10, 10000, 100000 }, { "N1", "N2", "N3" },
		"Query((N2N\\x80 OR N2N>X\\xf0))"
		// "N2:-10000 OR N2:0"
	},

	// Testing negatives.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"-2500", "-1200", {1, 10, 100, 1000, 10000, 100000}, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query((N5N>X\\xf0 AND (N4N>\\xd1  OR N4N?\\x01\\x80)))"
		// "N5:-10000 AND (N4:-3000 OR N4:-2000)"
	},
	// Do not find a Lower accuracy.
	{
		"-1200.300", "-1200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(N1N?\\x1a\\x80)"
		// "N1:-1200"
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"-100200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((N6N=O, AND N5N=\\x8aJ) OR (N6N=\\x8f, AND ((((((((N5N=\\x8f, OR N5N=\\x94\\x0e) OR N5N=\\x98\\xf0) OR N5N=\\x9d\\xd2) OR N5N=\\xc5h) OR N5N=\\xcf,) OR N5N=\\xd8\\xf0) OR N5N>\\x05h) OR N5N>\\x18\\xf0))))"
		// "(N6:-200000 AND (N5:-110000)) OR (N6:-100000 AND (N5:-100000 OR N5:-90000 OR N5:-80000 OR N5:-70000 OR N5:-60000 OR N5:-50000 OR N5:-40000 OR N5:-30000 OR N5:-20000))"
	},
	// Do not find a upper accuracy.
	{
		"-1000200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((((((((((N6N=\\x8f, OR N6N<\\x9en@) OR N6N<\\xc2\\xf7) OR N6N<\\xc9\\x11\\x80) OR N6N<\\xcf,) OR N6N<\\xd5F\\x80) OR N6N<\\xdba) OR N6N=\\x02\\xf7) OR N6N=\\x0f,) OR N6N=\\x1ba) OR N6N=O,))"
		// "N6:-1100000 OR N6:-1000000 OR N6:-900000 OR N6:-800000 OR N6:-700000 OR N6:-600000 OR N6:-500000 OR N6:-400000 OR N6:-300000 OR N6:-200000 OR N6:-100000"
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"-11000200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query()"
	},

	// Testing Mixed.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"-2500", "1200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((N5N>X\\xf0 AND ((N4N>\\xd1  OR N4N?\\x01\\x80) OR N4N?A\\x80)) OR (N5N\\x80 AND (N4N\\x80 OR N4N\\xc0\\xbd))))"
		// "(N5:-10000 AND (N4:-3000 OR N4:-2000 OR N4:-1000)) OR (N5:0 AND (N4:0 OR N4:1000))"
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"-100200.200", "10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((N6N\\x80 OR N6N=O,) OR N6N=\\x8f,))"
		// "N6:-200000 OR N6:-100000 OR N6:0"
	},
	// Do not find a upper accuracy.
	{
		"-1000200.200", "100200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query(((((((((((((N6N\\xc2a\\xa8 OR N6N<\\x9en@) OR N6N<\\xc2\\xf7) OR N6N<\\xc9\\x11\\x80) OR N6N<\\xcf,) OR N6N<\\xd5F\\x80) OR N6N<\\xdba) OR N6N=\\x02\\xf7) OR N6N=\\x0f,) OR N6N=\\x1ba) OR N6N=O,) OR N6N=\\x8f,) OR N6N\\x80))"
		// "N6:-1100000 OR N6:-1000000 OR N6:-900000 OR N6:-800000 OR N6:-700000 OR N6:-600000 OR N6:-500000 OR N6:-400000 OR N6:-300000 OR N6:-200000 OR N6:-100000 OR N6:0 OR N6:100000"
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"-11000200.200", "100200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"Query()"
	},

	// Testing big accuracies in big ranges.
	// The maximum accuracy is LLONG_MAX, and this is checked in schema.
	{
		"-1000000", "1000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, { "N1", "N2", "N3" },
		"Query((N1N\\x80 OR N1N5G(\\x15\\xb5\\x98))"
		// "N1:-1000000000000000 OR N1:0"
	},
	{
		"-1000000000000000", "1000000000000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, {"N1", "N2", "N3"},
		"Query(((N2N2\\xc8}%3bp AND N1N5G(\\x15\\xb5\\x98) OR (N2N\\x80 AND (N1N\\x80 OR N1Nʱ\\xafԘ\\xd0))))"
		// "(N2:-1000000000000000000 AND (N1:-1000000000000000)) OR (N2:0 AND (N1:0 OR N1:1000000000000000))"
	},
	{
		"-5000000000000000000", "5000000000000000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, { "N1", "N2", "N3" },
		"Query((N3N\\x80 OR N3N2\\x01\\x8c\\xc9َ\\xbe))"
		// "N3:-9000000000000000000 OR N3:0"
	},

	// Testing other accuracies.
	{
		"-300", "1750", { 250, 2800 }, { "N1", "N2" },
		"Query(((N2N>\\xd4@ AND (N1N?\\x81\\x80 OR N1N?\\xc1\\x80)) OR (N2N\\x80 AND (((((((N1N\\x80 OR N1N\\xc0=) OR N1N\\xc0}) OR N1N\\xc0\\x9d\\xc0) OR N1N\\xc0\\xbd) OR N1N\\xc0\\xce ) OR N1N\\xc0\\xdd\\xc0) OR N1N\\xc0\\xed`))))"
		// "(N2:-2800 AND (N1:-500 OR N1:-250)) OR (N2:0 AND (N1:0 OR N1:250 OR N1:500 OR N1:750 OR N1:1000 OR N1:1250 OR N1:1500 OR N1:1750))"
	}
});


const std::vector<testQuery_t> date_tests({
	// There is not an upper accuracy and the lower accuracy are several terms.
	{
		"0001-10-10", "9999-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8" },
		"Query()"
	},
	{
		"1900-10-10", "2000-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7" },
		"Query()"
	},
	{
		"2000-10-10", "2010-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH) },
		{ "D1", "D2", "D3", "D4", "D5" },
		"Query()"
	},
	{
		"2000-10-10", "2000-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY) },
		{ "D1", "D2", "D3", "D4" },
		"Query()"
	},
	{
		"2000-10-10", "2000-10-10T00:01", { toUType(UnitTime::SECOND) },
		{ "D1" },
		"Query()"
	},

	// There is not an upper accuracy.
	{
		"1000-10-10", "4000-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((((D9D\\xc77Rx\\x18 OR D9D9\\x06\\xfb\\xe8d) OR D9DŰڇ) OR D9D\\xc6\\xf9\\x15\\xec\\xc0))"
		// "D9:-30610224000 OR D9:946684800 OR D9:32503680000 OR D9:64060588800"
	},
	{
		"1900-10-10", "2200-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8" },
		"Query((((D8D\\xc6l'\\x86@ OR D8D9\\xdf\\x15``) OR D8DŰڇ) OR D8D\\xc6:C+\\x80))"
		// "D8:-2208988800 OR D8:946684800 OR D8:4102444800 OR D8:7258118400"
	},
	{
		"1960-10-10", "1990-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7" },
		"Query((((D7Dŋ=; OR D7D:\\x9a`\\x11) OR D7D\\x80) OR D7D\\xc5K:\\x98))"
		// "D7:-315619200 OR D7:0 OR D7:315532800 OR D7:631152000"
	},
	{
		"1968-10-10", "1971-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" },
		"Query((((D6D\\xc4xL\\xe0 OR D6D;C\\xc4x) OR D6D;\\x83ِ) OR D6D\\x80))"
		// "D6:-63158400 OR D6:-31536000 OR D6:0 OR D6:31536000"
	},
	{
		"2011-09-10", "2011-12-05", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH) },
		{ "D1", "D2", "D3", "D4", "D5" },
		"Query((((D5D\\xc5\\xce\\xd6À OR D5D\\xc5\\xce^\\xcb) OR D5D\\xc5ΆX) OR D5D\\xc5ί6\\x80))"
		// "D5:1314835200 OR D5:1317427200 OR D5:1320105600 OR D5:1322697600"
	},
	{
		"2011-10-10", "2011-10-13", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY) },
		{ "D1", "D2", "D3", "D4" },
		"Query((((D4D\\xc5Ζ* OR D4D\\xc5Β5\\x80) OR D4D\\xc5Γ\\x87) OR D4D\\xc5Δ؀))"
		// "D4:1318204800 OR D4:1318291200 OR D4:1318377600 OR D4:1318464000"
	},
	{
		"2011-10-10T10:00:00", "2011-10-10T13:00:00", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR) },
		{ "D1", "D2", "D3" },
		"Query((((D3D\\xc5Β\\xecP OR D3D\\xc5Β\\xc2 ) OR D3D\\xc5Β\\xd00) OR D3D\\xc5Β\\xde@))"
		// "D3:1318240800 OR D3:1318244400 OR D3:1318248000 OR D3:1318251600"
	},
	{
		"2011-10-10T10:10:00", "2011-10-10T10:13:00", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE) },
		{ "D1", "D2" },
		"Query((((D2D\\xc5Β\\xc5, OR D2D\\xc5Β\\xc4x) OR D2D\\xc5ΒĴ) OR D2D\\xc5Β\\xc4\\xf0))"
		// "D2:1318241400 OR D2:1318241460 OR D2:1318241520 OR D2:1318241580"
	},
	{
		"2011-10-10T10:10:10", "2011-10-10T10:10:13", { toUType(UnitTime::SECOND) },
		{ "D1" },
		"Query((((D1D\\xc5Βą OR D1D\\xc5ΒĂ) OR D1D\\xc5Βă) OR D1D\\xc5ΒĄ))"
		// "D1:1318241410 OR D1:1318241411 OR D1:1318241412 OR D1:1318241413"
	},

	// There are upper and lower accuracy.
	{
		"1900-10-10", "2200-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query(((D9DŰڇ OR D9D9\\x06\\xfb\\xe8d) AND (((D8D\\xc6l'\\x86@ OR D8D9\\xdf\\x15``) OR D8DŰڇ) OR D8D\\xc6:C+\\x80)))"
		// "(D9:-30610224000 OR D9:946684800) AND (D8:-2208988800 OR D8:946684800 OR D8:4102444800 OR D8:7258118400)"
	},
	{
		"1960-10-10", "1990-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D8D9\\xdf\\x15`` AND (((D7Dŋ=; OR D7D:\\x9a`\\x11) OR D7D\\x80) OR D7D\\xc5K:\\x98)))"
		// "(D8:-2208988800) AND (D7:-315619200 OR D7:0 OR D7:315532800 OR D7:631152000)"
	},
	{
		"1968-10-10", "1971-12-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query(((D7D\\x80 OR D7D:\\x9a`\\x11) AND (((D6D\\xc4xL\\xe0 OR D6D;C\\xc4x) OR D6D;\\x83ِ) OR D6D\\x80)))"
		// "(D7:-315619200 OR D7:0) AND (D6:-63158400 OR D6:-31536000 OR D6:0 OR D6:31536000)"
	},
	{
		"2011-09-10", "2011-12-05", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D6D\\xc5\\xcd\\x1en\\x80 AND (((D5D\\xc5\\xce\\xd6À OR D5D\\xc5\\xce^\\xcb) OR D5D\\xc5ΆX) OR D5D\\xc5ί6\\x80)))"
		// "(D6:1293840000) AND (D5:1314835200 OR D5:1317427200 OR D5:1320105600 OR D5:1322697600)"
	},
	{
		"2011-10-10", "2011-10-13", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D5D\\xc5ΆX AND (((D4D\\xc5Ζ* OR D4D\\xc5Β5\\x80) OR D4D\\xc5Γ\\x87) OR D4D\\xc5Δ؀)))"
		// "(D5:1317427200) AND (D4:1318204800 OR D4:1318291200 OR D4:1318377600 OR D4:1318464000)"
	},
	{
		"2011-10-10T10:00:00", "2011-10-10T13:00:00", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D4D\\xc5Β5\\x80 AND (((D3D\\xc5Β\\xecP OR D3D\\xc5Β\\xc2 ) OR D3D\\xc5Β\\xd00) OR D3D\\xc5Β\\xde@)))"
		// "(D4:1318204800) AND (D3:1318240800 OR D3:1318244400 OR D3:1318248000 OR D3:1318251600)"
	},
	{
		"2011-10-10T10:10:00", "2011-10-10T10:13:00", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D3D\\xc5Β\\xc2  AND (((D2D\\xc5Β\\xc5, OR D2D\\xc5Β\\xc4x) OR D2D\\xc5ΒĴ) OR D2D\\xc5Β\\xc4\\xf0)))"
		// "(D3:1318240800) AND (D2:1318241400 OR D2:1318241460 OR D2:1318241520 OR D2:1318241580)"
	},
	{
		"2011-10-10T10:10:10", "2011-10-10T10:10:13", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D2D\\xc5Β\\xc4x AND (((D1D\\xc5Βą OR D1D\\xc5ΒĂ) OR D1D\\xc5Βă) OR D1D\\xc5ΒĄ)))"
		// "(D2:1318241400) AND (D1:1318241410 OR D1:1318241411 OR D1:1318241412 OR D1:1318241413)"
	},

	// There is not a lower accuracy.
	{
		"2010-10-10T10:10:10.100", "2010-10-10T10:10:10.900", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D2D\\xc5̱\\x90\\xf8 AND D1D\\xc5̱\\x91\\x02))"
		// "(D2:1286705400) AND (D1:1286705410)"
	},
	{
		"2010-01-10", "2010-04-10", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D6", "D7", "D8", "D9" },
		"Query(D6D\\xc5\\xcb=;)"
		// "D6:1262304000"
	},
	{
		"2010-10-10", "2010-10-15", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D5", "D6", "D7", "D8", "D9" },
		"Query(D5D\\xc5̥$\\x80)"
		// "D5:1285891200"
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T12:10:10", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2",  "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query(D4D\\xc5̱\\x02)"
		// "D4:1286668800"
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:12:10", { toUType(UnitTime::SECOND), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query(D3D\\xc5̱\\x8e\\xa0)"
		// "D3:1286704800"
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:10:12", { toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query(D2D\\xc5̱\\x90\\xf8)"
		// "D2:1286705400"
	},

	// Special cases.
	// When the range is negative.
	{
		"2010-10-10T10:10:12.100", "2010-10-10T10:10:12", { toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query()"
	},

	// Testing negative timestamps.
	{
		"1800-01-10", "1802-04-10", { toUType(UnitTime::SECOND), toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D7D9\\x98\\x07\\xb7\\xf0 AND ((D6D9\\x98\\x80\\x04\\xd0 OR D6D9\\x98\\x07\\xb7\\xf0) OR D6D9\\x98C\\xde`)))"
		// "(D7:-5364662400) AND (D6:-5364662400 OR D6:-5333126400 OR D6:-5301590400)"
	},
	{
		"1810-10-10T10:11:10", "1810-10-10T10:12:15", { toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY), toUType(UnitTime::MILLENNIUM) },
		{ "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" },
		"Query((D3D9\\x9a\\x90\\x17$ AND (D2D9\\x9a\\x90\\x17~ OR D2D9\\x9a\\x90\\x17x\\x80)))"
		// "(D3:-5024728800) AND (D2:-5024728140 OR D2:-5024728080)"
	}
});


const std::vector<testQueryG_t> geo_tests({
	// Error: 0.3   partials: true
	{
		// CIRCLE(-104.026930 48.998427, 20015114)
		std::vector<range_t>({
			{ 9007199254740992, 18014398509481983 },
		}), { 25, 20, 15, 10, 5, 0 }, { "G1", "G2", "G3", "G4", "G5", "G6" },
		"Query((((((((G6G\\xbe\\xc0 OR G6G\\xbe\\xc8) OR G6G\\xbe\\xd0) OR G6G\\xbe\\xd8) OR G6G\\xbe\\xe0) OR G6G\\xbe\\xe8) OR G6G\\xbe\\xf0) OR G6G\\xbe\\xf8))"
		// "G6:8 OR G6:9 OR G6:10 OR G6:11 OR G6:12 OR G6:13 OR G6:14 OR G6:15"
	},
	{
		// CIRCLE(-104.026930 48.998427, 15011335.5)
		std::vector<range_t>({
			range_t(9007199254740992,  13792273858822143),
			range_t(13933011347177472, 14003380091355135),
			range_t(14073748835532800, 14214486323888127),
			range_t(14284855068065792, 14355223812243455),
			range_t(14425592556421120, 14707067533131775),
			range_t(14777436277309440, 15058911254020095),
			range_t(15129279998197760, 15621861207441407),
			range_t(15692229951619072, 18014398509481983),
		}), { 25, 20, 15, 10, 5, 0 }, { "G1", "G2", "G3", "G4", "G5", "G6" },
		"Query((((((((G6G\\xbe\\xc0 OR G6G\\xbe\\xc8) OR G6G\\xbe\\xd0) OR G6G\\xbe\\xd8) OR G6G\\xbe\\xe0) OR G6G\\xbe\\xe8) OR G6G\\xbe\\xf0) OR G6G\\xbe\\xf8))"
		// "G6:8 OR G6:9 OR G6:10 OR G6:11 OR G6:12 OR G6:13 OR G6:14 OR G6:15"
	},
	{
		std::vector<range_t>({
			{ 15629289656149997, 15629289656149997 }
		}), { 25, 20, 15, 10, 5, 0 }, { "G1", "G2", "G3", "G4", "G5", "G6" },
		"Query(G1G˯\\x0d\\x83$\\x17\\xcf\\xda)"
		// G1:15629289656149997
	},

	// Upper and lower accuracy
	{
		std::vector<range_t>({
			range_t(11125203589398528, 11125203656507391),
			range_t(11125204394704896, 11125205602664447),
			range_t(11125205669773312, 11125205736882175),
			range_t(11125205871099904, 11125205938208767),
			range_t(11125206407970816, 11125206475079679),
			range_t(11125211105591296, 11125211239809023),
			range_t(11125211306917888, 11125211374026751),
			range_t(11125211910897664, 11125212984639487),
			range_t(11125213521510400, 11125213789945855),
			range_t(11125214058381312, 11125214326816767),
			range_t(11125214595252224, 11125215132123135),
			range_t(11125215400558592, 11125215668994047),
			range_t(11125215803211776, 11125215870320639),
			range_t(11125215937429504, 11125216004538367),
			range_t(11125216205864960, 11125217279606783),
			range_t(11125217816477696, 11125217883586559),
			range_t(11125217950695424, 11125218084913151),
			range_t(11125218353348608, 11125218621784063),
			range_t(11125218688892928, 11125219427090431),
			range_t(11126371552067584, 11126372625809407),
			range_t(11126375310163968, 11126375377272831),
			range_t(11126376115470336, 11126376182579199),
			range_t(11126376920776704, 11126378531389439),
			range_t(11126378799824896, 11126379068260351),
			range_t(11126383900098560, 11126383967207423),
			range_t(11126385510711296, 11126386718670847),
			range_t(11126386785779712, 11126386919997439),
			range_t(11126386987106304, 11126387121324031),
			range_t(11126387523977216, 11126387591086079),
		}), { 25, 20, 15, 10, 5, 0 }, { "G1", "G2", "G3", "G4", "G5", "G6" },
		"Query(((G5G\\xc1\\x8f\\x0c OR G5G\\xc1\\x8f\\x0e) AND ((((((((((((((((((G4G\\xc4\\x0f\\x0c\\xa0\\x80 OR G4G\\xc4\\x0f\\x0c\\xa1) OR G4G\\xc4\\x0f\\x0c\\xa1\\x80) OR G4G\\xc4\\x0f\\x0c\\xa4) OR G4G\\xc4\\x0f\\x0c\\xa4\\x80) OR G4G\\xc4\\x0f\\x0c\\xa5) OR G4G\\xc4\\x0f\\x0c\\xa5\\x80) OR G4G\\xc4\\x0f\\x0c\\xa6) OR G4G\\xc4\\x0f\\x0c\\xa6\\x80) OR G4G\\xc4\\x0f\\x0c\\xa7) OR G4G\\xc4\\x0f\\x0c\\xa7\\x80) OR G4G\\xc4\\x0f\\x0e\\xc0\\x80) OR G4G\\xc4\\x0f\\x0e\\xc2) OR G4G\\xc4\\x0f\\x0e) OR G4G\\xc4\\x0f\\x0e\\xc3) OR G4G\\xc4\\x0f\\x0eÀ) OR G4G\\xc4\\x0f\\x0e\\xc6) OR G4G\\xc4\\x0f\\x0e\\xc7) OR G4G\\xc4\\x0f\\x0eǀ)))"
		// ((G5:10118 OR G5:10119) AND (G4:10361153 OR G4:10361154 OR G4:10361155 OR G4:10361160 OR G4:10361161 OR G4:10361162 OR G4:10361163 OR G4:10361164 OR G4:10361165 OR G4:10361166 OR G4:10361167 OR G4:10362241 OR G4:10362244 OR G4:10362245 OR G4:10362246 OR G4:10362247 OR G4:10362252 OR G4:10362254 OR G4:10362255))
	},

	// There are not accuracy
	{
		std::vector<range_t>({
			{ 15629289656149997, 15629289656149997 }
		}), { }, { },
		"Query()"
	}
});


int numeric_test() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : numeric_tests) {
		std::string result_query_terms;
		// try to convert string to uint64_t.
		try {
			uint64_t val_s = strict_stoull(test.start);
			uint64_t val_e = strict_stoull(test.end);
			result_query_terms = GenerateTerms::numeric(val_s, val_e, test.accuracy, test.acc_prefix).get_description();
		} catch (const std::exception&) { }

		// try to convert string to int64_t
		try {
			int64_t val_s = strict_stoll(test.start);
			int64_t val_e = strict_stoll(test.end);
			result_query_terms = GenerateTerms::numeric(val_s, val_e, test.accuracy, test.acc_prefix).get_description();
		} catch (const std::exception&) { }

		// try to convert string to double
		try {
			int64_t val_s = (int64_t)strict_stod(test.start);
			int64_t val_e = (int64_t)strict_stod(test.end);
			result_query_terms = GenerateTerms::numeric(val_s, val_e, test.accuracy, test.acc_prefix).get_description();
		} catch (const std::exception&) { }

		if (result_query_terms != test.expected_query_terms) {
			L_ERR("ERROR: Different numeric filter query.\n\t  Result: {}\n\tExpected: {}", result_query_terms, test.expected_query_terms);
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG("Testing generation of numerical terms is correct!");
		RETURN(0);
	} else {
		L_ERR("ERROR: Testing generation of numerical terms has mistakes.");
		RETURN(1);
	}
}


int date_test() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : date_tests) {
		auto val_s = Datetime::timestamp(Datetime::DateParser(test.start));
		auto val_e = Datetime::timestamp(Datetime::DateParser(test.end));
		auto result_query_terms = GenerateTerms::date(val_s, val_e, test.accuracy, test.acc_prefix).get_description();
		if (result_query_terms != test.expected_query_terms) {
			L_ERR("ERROR: Different numeric filter query.\n\t  Result: {}\n\tExpected: {}", result_query_terms, test.expected_query_terms);
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG("Testing generation of terms for dates is correct!");
		RETURN(0);
	} else {
		L_ERR("ERROR: Testing generation of terms for dates has mistakes.");
		RETURN(1);
	}
}


int geo_test() {
	INIT_LOG
	int cont = 0;
	for (const auto& test : geo_tests) {
		auto result_query_terms = GenerateTerms::geo(test.ranges, test.accuracy, test.acc_prefix).get_description();
		if (result_query_terms != test.expected_query_terms) {
			L_ERR("ERROR: Different numeric filter query.\n\t  Result: {}\n\tExpected: {}", result_query_terms, test.expected_query_terms);
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG("Testing generation of terms for geospatials is correct!");
		RETURN(0);
	} else {
		L_ERR("ERROR: Testing generation of terms for geospatials has mistakes.");
		RETURN(1);
	}
}
