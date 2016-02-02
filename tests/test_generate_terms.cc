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

#include "test_generate_terms.h"

#include "../src/generate_terms.h"
#include "../src/log.h"
#include "../src/utils.h"
#include "../src/schema.h"

#include <limits.h>


const test_t numeric[] {
	// Testing positives.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"1200", "2500", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"(N5:0) AND (N4:1000 OR N4:2000)", { "N5", "N4" }
	},
	// Do not find a Lower accuracy.
	{
		"1200.100", "1200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N1:1200", { "N1" }
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"10200.100", "100200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"(N6:0 OR N6:100000) AND (N5:10000 OR N5:20000 OR N5:30000 OR N5:40000 OR N5:50000 OR N5:60000 OR N5:70000 OR N5:80000 OR N5:90000 OR N5:100000)",
		{ "N6", "N5" }
	},
	// Do not find a upper accuracy.
	{
		"10200.100", "1000200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N6:0 OR N6:100000 OR N6:200000 OR N6:300000 OR N6:400000 OR N6:500000 OR N6:600000 OR N6:700000 OR N6:800000 OR N6:900000 OR N6:1000000",
		{ "N6" }
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"10200.100", "11000200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},

	// Testing special case.
	// When the accuracy it is empty.
	{
		"10200.100", "11000200.200", { }, { }, "", { }
	},
	// When the range is type GE.
	{
		"", "11000200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},
	// When the range is type LE.
	{
		"-100", "", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},
	// When the range is negative.
	{
		"1000", "900", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},
	// Do not find a lower accuracy because it exceeded the number of terms.
	{
		"-1200.300", "1200.200", { 10, 10000, 100000 }, { "N1", "N2", "N3" },
		"N2:0", { "N2" }
	},

	// Testing negatives.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"-2500", "-1200", {1, 10, 100, 1000, 10000, 100000}, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"(N5:0) AND (N4:_2000 OR N4:_1000)", { "N5", "N4" }
	},
	// Do not find a Lower accuracy.
	{
		"-1200.300", "-1200.200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N1:_1200", { "N1" }
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"-100200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"(N6:_100000 OR N6:0) AND (N5:_100000 OR N5:_90000 OR N5:_80000 OR N5:_70000 OR N5:_60000 OR N5:_50000 OR N5:_40000 OR N5:_30000 OR N5:_20000 OR N5:_10000)",
		{ "N6", "N5" }
	},
	// Do not find a upper accuracy.
	{
		"-1000200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N6:_1000000 OR N6:_900000 OR N6:_800000 OR N6:_700000 OR N6:_600000 OR N6:_500000 OR N6:_400000 OR N6:_300000 OR N6:_200000 OR N6:_100000 OR N6:0",
		{"N6"}
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"-11000200.200", "-10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},

	// Testing Mixed.
	// Find lower and upper accuracy, upper accuracy generates only one term.
	{
		"-2500", "1200", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"(N5:0) AND (N4:_2000 OR N4:_1000 OR N4:0 OR N4:1000)", { "N5", "N4" }
	},
	// Find lower and upper accuracy, upper accuracy generates two terms.
	{
		"-100200.200", "10200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N6:_100000 OR N6:0",
		{ "N6"}
	},
	// Do not find a upper accuracy.
	{
		"-1000200.200", "100200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" },
		"N6:_1000000 OR N6:_900000 OR N6:_800000 OR N6:_700000 OR N6:_600000 OR N6:_500000 OR N6:_400000 OR N6:_300000 OR N6:_200000 OR N6:_100000 OR N6:0 OR N6:100000",
		{ "N6" }
	},
	// When the range of search is more big that MAX_TERM * MAX_ACCURACY.
	{
		"-11000200.200", "100200.100", { 1, 10, 100, 1000, 10000, 100000 }, { "N1", "N2", "N3", "N4", "N5", "N6" }, "", { }
	},

	// Testing big accuracies.
	// The maximum accuracy is LLONG_MAX, and this is checked in schema.
	{
		"-1000000", "1000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, { "N1", "N2", "N3" },
		"N1:0", { "N1" }
	},
	{
		"-1000000000000000", "1000000000000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, {"N1", "N2", "N3"},
		"(N2:0) AND (N1:_1000000000000000 OR N1:0 OR N1:1000000000000000)", { "N2", "N1" }
	},
	{
		"-5000000000000000000", "5000000000000000000", { 1000000000000000, 1000000000000000000, 9000000000000000000 }, { "N1", "N2", "N3" },
		"N3:0", { "N3" }
	},

	// Testing other accuracies.
	{
		"-300", "1750", { 250, 2800 }, { "N1", "N2" },
		"(N2:0) AND (N1:_250 OR N1:0 OR N1:250 OR N1:500 OR N1:750 OR N1:1000 OR N1:1250 OR N1:1500 OR N1:1750)", { "N2", "N1" }
	},
	// Testing when the range is out of range for generating terms.
	{
		"-9223372036854775800.0", "9223372036854775806.0", { 9000000000000000000 }, { "N1" },
		"", { }
	},
	{
		"-9223372036854775800.0", "0", { 9000000000000000000 }, { "N1" },
		"", { }
	},
	{
		"0", "9223372036854775800.0", { 9000000000000000000 }, { "N1" },
		"", { }
	}
};


const test_t date[] {
	// There is not a upper accuracy
	{
		"2010-10-10", "2011-12-15", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D6:1262304000 OR D6:1293840000", { "D6" }
	},
	// Do not find a upper accuracy
	{
		"2011-10-10", "2011-12-15", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH) },
		{ "D1", "D2", "D3", "D4", "D5" }, "D5:1317427200 OR D5:1320105600 OR D5:1322697600", { "D5" }
	},
	// Find upper and lower accuracy
	{
		"2010-01-10", "2010-04-10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D6:1262304000 AND (D5:1262304000 OR D5:1264982400 OR D5:1267401600 OR D5:1270080000)", { "D5", "D6" }
	},
	{
		"2010-10-10", "2010-10-15", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D5:1285891200 AND (D4:1286668800 OR D4:1286755200 OR D4:1286841600 OR D4:1286928000 OR D4:1287014400 OR D4:1287100800)", { "D4", "D5" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T12:10:10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D4:1286668800 AND (D3:1286704800 OR D3:1286708400 OR D3:1286712000)", { "D3", "D4" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:12:10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D3:1286704800 AND (D2:1286705400 OR D2:1286705460 OR D2:1286705520)", { "D2", "D3" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:10:12", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D2:1286705400 AND (D1:1286705410 OR D1:1286705411 OR D1:1286705412)", { "D1", "D2" }
	},
	// There is not a lower accuracy.
	{
		"2010-10-10T10:10:10.100", "2010-10-10T10:10:10.900", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D1:1286705410", { "D1" }
	},
	{
		"2010-01-10", "2010-04-10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D6" }, "D6:1262304000", { "D6" }
	},
	{
		"2010-10-10", "2010-10-15", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D5", "D6" }, "D5:1285891200", { "D5" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T12:10:10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2",  "D4", "D5", "D6" }, "D4:1286668800", { "D4" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:12:10", { toUType(unitTime::SECOND), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D3", "D4", "D5", "D6" }, "D3:1286704800", { "D3" }
	},
	{
		"2010-10-10T10:10:10", "2010-10-10T10:10:12", { toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D2", "D3", "D4", "D5", "D6" }, "D2:1286705400", { "D2" }
	},

	// Special cases.
	// When the range is type GE.
	{
		"2010-10-10T10:10:10", "", { toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D2", "D3", "D4", "D5", "D6" }, "", { }
	},
	// When the range is type LE.
	{
		"", "2010-10-10T10:10:12", { toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D2", "D3", "D4", "D5", "D6" }, "", { }
	},
	// When the range is negative.
	{
		"2010-10-10T10:10:12.100", "2010-10-10T10:10:12", { toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D2", "D3", "D4", "D5", "D6" }, "", { }
	},

	// Testing negative timestamps.
	{
		"1800-01-10", "1802-04-10", { toUType(unitTime::SECOND), toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D1", "D2", "D3", "D4", "D5", "D6" }, "D6:_5364662400 OR D6:_5333126400 OR D6:_5301590400", { "D6" }
	},
	{
		"1810-10-10T10:11:10", "1810-10-10T10:12:15", { toUType(unitTime::MINUTE), toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) },
		{ "D2", "D3", "D4", "D5", "D6" }, "D3:_5024728800 AND (D2:_5024728140 OR D2:_5024728080)", { "D2", "D3" }
	}
};


const testG_t geo[] {
	{
		{
			// POLYGON ((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625))
			{ 15061110277275648, 15061247716229119 },
			{ 15061316435705856, 15061385155182591 },
			{ 15622960719069184, 15623510474883071 },
			{ 15623785352790016, 15624060230696959 },
			{ 15625297181278208, 15625365900754943 },
			{ 15627633643487232, 15627702362963967 },
			{ 15628458277208064, 15628526996684799 },
			{ 15628595716161536, 15628733155115007 },
			{ 15629008033021952, 15629420349882367 },
			{ 15629489069359104, 15629626508312575 },
			{ 15635605102788608, 15635673822265343 },
			{ 15637254370230272, 15638353881858047 },
			{ 15638628759764992, 15638697479241727 },
			{ 15638766198718464, 15638903637671935 }
		}, { 0.2, 1, 0, 5, 10, 15, 20, 25 }, { "G1", "G2", "G3", "G4", "G5", "G6" }, "G1:13", { "G1" }
	},
	{
		{
			// "POINT (48.574789910928864 -103.53515625)"
			{ 15629289656149997, 15629289656149997 }
		}, { 0.2, 1, 0, 5, 10, 15, 20, 25 }, { "G1", "G2", "G3", "G4", "G5", "G6" }, "G6:15629289656149997", { "G6" }
	},
	{
		{
			// "CIRCLE (0 0, 2000)"
			{ 9007199254740992,   9007199321849855 },
			{ 9007199472844800,   9007199481233407 },
			{ 9007199485427712,   9007199493816319 },
			{ 9007199498010624,   9007199510593535 },
			{ 12947848928690176, 12947848995799039 },
			{ 12947849146793984, 12947849155182591 },
			{ 12947849159376896, 12947849167765503 },
			{ 12947849171959808, 12947849184542719 },
			{ 13510798882111488, 13510798949220351 },
			{ 13510799100215296, 13510799108603903 },
			{ 13510799112798208, 13510799121186815 },
			{ 13510799125381120, 13510799137964031 },
			{ 17451448556060672, 17451448623169535 },
			{ 17451448774164480, 17451448782553087 },
			{ 17451448786747392, 17451448795135999 },
			{ 17451448799330304, 17451448811913215 }
		}, { 0.2, 1, 0, 5, 10, 15, 20, 25 }, { "G1", "G2", "G3", "G4", "G5", "G6" }, "G3:8388608 OR G3:12058624 OR G3:12582912 OR G3:16252928", { "G3" }
	},
	// There are not ranges.
	{
		{ }, { 0.2, 1, 0, 5, 10, 15, 20, 25 }, { "G1", "G2", "G3", "G4", "G5", "G6" }, "", { }
	},
	// There are not accuracy
	{
		{
			{ 15629289656149997, 15629289656149997 }
		}, { 0.2, 1 }, { }, "", { }
	}
};


int numeric_test() {
	int cont = 0;
	for (int pos = 0, len = arraySize(numeric); pos < len; ++pos) {
		const test_t p = numeric[pos];
		std::string result_terms;
		std::vector<std::string> prefixes;
		GenerateTerms::numeric(result_terms, p.start, p.end, p.accuracy, p.acc_prefix, prefixes);
		if (result_terms.compare(p.expected_terms) == 0) {
			if (prefixes.size() != p.expected_prefixes.size()) {
				L_DEBUG(nullptr, "ERROR: Diferent numbers of prefix");
				++cont;
				continue;
			}
			auto it = prefixes.begin();
			auto ite = p.expected_prefixes.begin();
			for ( ; it != prefixes.end(); ++it, ++ite) {
				if (it->compare(*ite) != 0) {
					L_DEBUG(nullptr, "ERROR: Prefix: %s  Expected: %s", it->c_str(), ite->c_str());
					++cont;
					continue;
				}
			}
		} else {
			L_DEBUG(nullptr, "ERROR: result_terms: %s  Expected: %s", result_terms.c_str(), p.expected_terms.c_str());
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing generation numerical terms is correct!");
		return 0;
	} else {
		L_ERR(nullptr, "ERROR: Testing generation numerical terms has mistakes.");
		return 1;
	}
}


int date_test() {
	int cont = 0;
	for (int pos = 0, len = arraySize(date); pos < len; ++pos) {
		const test_t p = date[pos];
		std::string result_terms;
		std::vector<std::string> prefixes;
		GenerateTerms::date(result_terms, p.start, p.end, p.accuracy, p.acc_prefix, prefixes);
		if (result_terms.compare(p.expected_terms) == 0) {
			if (prefixes.size() != p.expected_prefixes.size()) {
				L_DEBUG(nullptr, "ERROR: Diferent numbers of prefix");
				++cont;
				continue;
			}
			auto it = prefixes.begin();
			auto ite = p.expected_prefixes.begin();
			for ( ; it != prefixes.end(); ++it, ++ite) {
				if (it->compare(*ite) != 0) {
					L_DEBUG(nullptr, "ERROR: Prefix: %s  Expected: %s", it->c_str(), ite->c_str());
					++cont;
					continue;
				}
			}
		} else {
			L_DEBUG(nullptr, "ERROR: result_terms: %s  Expected: %s", result_terms.c_str(), p.expected_terms.c_str());
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing generation of terms for dates is correct!");
		return 0;
	} else {
		L_ERR(nullptr, "ERROR: Testing generation of terms for dates has mistakes.");
		return 1;
	}
}


int geo_test() {
	int cont = 0;
	for (int pos = 0, len = arraySize(geo); pos < len; ++pos) {
		const testG_t p = geo[pos];
		std::string result_terms;
		std::vector<std::string> prefixes;
		GenerateTerms::geo(result_terms, p.ranges, p.accuracy, p.acc_prefix, prefixes);
		if (result_terms.compare(p.expected_terms) == 0) {
			if (prefixes.size() != p.expected_prefixes.size()) {
				L_DEBUG(nullptr, "ERROR: Diferent numbers of prefix");
				++cont;
				continue;
			}
			auto it = prefixes.begin();
			auto ite = p.expected_prefixes.begin();
			for ( ; it != prefixes.end(); ++it, ++ite) {
				if (it->compare(*ite) != 0) {
					L_DEBUG(nullptr, "ERROR: Prefix: %s  Expected: %s", it->c_str(), ite->c_str());
					++cont;
					continue;
				}
			}
		} else {
			L_DEBUG(nullptr, "ERROR: result_terms: %s  Expected: %s", result_terms.c_str(), p.expected_terms.c_str());
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing generation of terms for geospatials is correct!");
		return 0;
	} else {
		L_ERR(nullptr, "ERROR: Testing generation of terms for geospatials has mistakes.");
		return 1;
	}
}
