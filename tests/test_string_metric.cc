/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_string_metric.h"

#include "../src/phonetic.h"
#include "../src/string_metric.h"
#include "utils.h"


#define NUM_TESTS 10000


int test_ranking_results() {
	/*
	 * Tests based in the article:
	 * http://www.catalysoft.com/articles/strikeamatch.html
	 */
	std::string str("Healed");
	std::string strs[] = {
		"Sealed", "Healthy", "Heard", "Herded", "Help", "Sold"
	};
	auto levenshtein = Levenshtein(str);
	auto jaro = Jaro(str);
	auto jaro_winkler = Jaro_Winkler(str);
	auto dice = Sorensen_Dice(str);
	auto jaccard = Jaccard(str);
	auto lcs = LCSubstr(str);
	auto lcsq = LCSubsequence(str);
	std::string metrics[] = {
		levenshtein.description(),
		jaro.description(),
		jaro_winkler.description(),
		dice.description(),
		jaccard.description(),
		lcs.description(),
		lcsq.description()
	};
	double expected[arraySize(metrics)][arraySize(strs)] = {
		{ 0.166667, 0.428571, 0.333333, 0.333333, 0.500000, 0.666667 },
		{ 0.111111, 0.253968, 0.177778, 0.305556, 0.250000, 0.388889 },
		{ 0.111111, 0.152381, 0.124444, 0.305556, 0.200000, 0.388889 },
		{ 0.200000, 0.454545, 0.555556, 0.600000, 0.750000, 1.000000 },
		{ 0.333333, 0.428571, 0.333333, 0.500000, 0.500000, 0.714286 },
		{ 0.166667, 0.428571, 0.500000, 0.666667, 0.666667, 0.833333 },
		{ 0.166667, 0.428571, 0.333333, 0.333333, 0.500000, 0.666667 }
	};

	int res = 0;
	for (size_t i = 0; i < arraySize(strs); ++i) {
		double results[] = {
			levenshtein.distance(strs[i]),
			jaro.distance(strs[i]),
			jaro_winkler.distance(strs[i]),
			dice.distance(strs[i]),
			jaccard.distance(strs[i]),
			lcs.distance(strs[i]),
			lcsq.distance(strs[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR(nullptr, "ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j].c_str(), str.c_str(), strs[i].c_str(), expected[j][i], results[j]);
				++res;
			}
		}
	}

	/*
	 * Real World examples.
	 *
	 * The results are different that in the article, because we do not remove
	 * white spaces.
	 */
	std::string strs_r1[] = {
		"Web Database Applications",
		"PHP Web Applications",
		"Web Aplications"
	};
	std::string strs_r2[] = {
		"Web Database Applications with PHP & MySQL",
		"Creating Database Web Applications with PHP and ASP",
		"Building Database Applications on the Web Using PHP3",
		"Building Web Database Applications with Visual Studio 6",
		"Web Application Development With PHP",
		"WebRAD: Building Database Applications on the Web with Visual FoxPro and Web Connection",
		"Structural Assessment: The Role of Large and Full-Scale Testing",
		"How to Find a Scholarship Online"
	};
	double expected2[arraySize(metrics)][arraySize(strs_r1)][arraySize(strs_r2)] = {
		{
			{ 0.404762, 0.549020, 0.576923, 0.545455, 0.750000, 0.712644, 0.825397, 0.781250 },
			{ 0.642857, 0.666667, 0.711538, 0.690909, 0.694444, 0.827586, 0.841270, 0.781250 },
			{ 0.642857, 0.705882, 0.750000, 0.727273, 0.611111, 0.827586, 0.873016, 0.781250 }
		},
		{
			{ 0.134921, 0.323268, 0.345598, 0.335152, 0.368986, 0.370881, 0.504094, 0.432500 },
			{ 0.415079, 0.419281, 0.443269, 0.437166, 0.392593, 0.464368, 0.569841, 0.415972 },
			{ 0.325397, 0.390850, 0.420574, 0.409091, 0.225926, 0.364751, 0.541534, 0.413889 }
		},
		{
			{ 0.080952, 0.323268, 0.345598, 0.335152, 0.368986, 0.370881, 0.504094, 0.432500 },
			{ 0.415079, 0.419281, 0.443269, 0.437166, 0.392593, 0.464368, 0.569841, 0.415972 },
			{ 0.325397, 0.390850, 0.420574, 0.409091, 0.135556, 0.364751, 0.541534, 0.413889 }
		},
		{
			{ 0.269841, 0.303030, 0.323529, 0.369863, 0.482759, 0.505376, 0.840000, 0.884615 },
			{ 0.355932, 0.387097, 0.437500, 0.536232, 0.370370, 0.640449, 0.915493, 0.833333 },
			{ 0.481481, 0.508772, 0.525424, 0.562500, 0.469388, 0.666667, 0.909091, 0.860465 }
		},
		{
			{ 0.263158, 0.176471, 0.222222, 0.263158, 0.235294, 0.363636, 0.500000, 0.235294 },
			{ 0.263158, 0.176471, 0.222222, 0.263158, 0.235294, 0.363636, 0.500000, 0.235294 },
			{ 0.315789, 0.235294, 0.277778, 0.315789, 0.294118, 0.409091, 0.545455, 0.294118 }
		},
		{
			{ 0.404762, 0.745098, 0.576923, 0.545455, 0.666667, 0.747126, 0.952381, 0.937500 },
			{ 0.690476, 0.666667, 0.750000, 0.763636, 0.583333, 0.850575, 0.968254, 0.937500 },
			{ 0.761905, 0.803922, 0.807692, 0.818182, 0.750000, 0.885057, 0.968254, 0.937500 }
		},
		{
			{ 0.404762, 0.549020, 0.557692, 0.545455, 0.583333, 0.712644, 0.793651, 0.687500 },
			{ 0.619048, 0.666667, 0.711538, 0.690909, 0.583333, 0.816092, 0.825397, 0.718750 },
			{ 0.642857, 0.705882, 0.750000, 0.727273, 0.611111, 0.827586, 0.857143, 0.750000 }
		}
	};
	for (size_t i = 0; i < arraySize(strs_r1); ++i) {
		for (size_t j = 0; j < arraySize(strs_r2); ++j) {
			double results[] = {
				levenshtein.distance(strs_r1[i], strs_r2[j]),
				jaro.distance(strs_r1[i], strs_r2[j]),
				jaro_winkler.distance(strs_r1[i], strs_r2[j]),
				dice.distance(strs_r1[i], strs_r2[j]),
				jaccard.distance(strs_r1[i], strs_r2[j]),
				lcs.distance(strs_r1[i], strs_r2[j]),
				lcsq.distance(strs_r1[i], strs_r2[j])
			};
			for (size_t k = 0; k < arraySize(results); ++k) {
				if (std::abs(results[k] - expected2[k][i][j]) >= 1e-6) {
					L_ERR(nullptr, "ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[k].c_str(), strs_r1[i].c_str(), strs_r2[j].c_str(), expected2[k][i][j], results[k]);
					++res;
				}
			}
		}
	}
	RETURN(res);
}


int test_special_cases() {
	std::string str1[] = { "AA", "A", "A", "A", "AB", "AA", "" };
	std::string str2[] = { "AAAAA", "A", "B", "AB", "B", "AA", "" };
	auto levenshtein = Levenshtein();
	auto jaro = Jaro();
	auto jaro_winkler = Jaro_Winkler();
	auto dice = Sorensen_Dice();
	auto jaccard = Jaccard();
	auto lcs = LCSubstr();
	auto lcsq = LCSubsequence();
	std::string metrics[] = {
		levenshtein.description(),
		jaro.description(),
		jaro_winkler.description(),
		dice.description(),
		jaccard.description(),
		lcs.description(),
		lcsq.description()
	};

	double expected[arraySize(metrics)][arraySize(str1)] = {
		{ 0.600000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.200000, 0.000000, 1.000000, 0.166667, 1.000000, 0.000000, 1.000000 },
		{ 0.160000, 0.000000, 1.000000, 0.150000, 1.000000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 1.000000, 1.000000, 1.000000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.600000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.600000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 }
	};

	int res = 0;
	for (size_t i = 0; i < arraySize(str1); ++i) {
		double results[] = {
			levenshtein.distance(str1[i], str2[i]),
			jaro.distance(str1[i], str2[i]),
			jaro_winkler.distance(str1[i], str2[i]),
			dice.distance(str1[i], str2[i]),
			jaccard.distance(str1[i], str2[i]),
			lcs.distance(str1[i], str2[i]),
			lcsq.distance(str1[i], str2[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR(nullptr, "ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j].c_str(), str1[i].c_str(), str2[i].c_str(), expected[j][i], results[j]);
				++res;
			}
		}
	}
	RETURN(res);
}


int test_case_sensitive() {
	std::string str1[] = { "FRANCE", "FRANCE", "france", "FRaNCe" };
	std::string str2[] = { "france", "french", "FRENCH", "fReNCh" };
	auto levenshtein = Levenshtein();
	auto levenshtein_sensitive = Levenshtein(false);
	auto jaro = Jaro();
	auto jaro_sensitive = Jaro(false);
	auto jaro_winkler = Jaro_Winkler();
	auto jaro_winkler_sensitive = Jaro_Winkler(false);
	auto dice = Sorensen_Dice();
	auto dice_sensitive = Sorensen_Dice(false);
	auto jaccard = Jaccard();
	auto jaccard_sensitive = Jaccard(false);
	auto lcs = LCSubstr();
	auto lcs_sensitive = LCSubstr(false);
	auto lcsq = LCSubsequence();
	auto lcsq_sensitive = LCSubsequence(false);
	std::string metrics[] = {
		levenshtein.description(),
		levenshtein_sensitive.description(),
		jaro.description(),
		jaro_sensitive.description(),
		jaro_winkler.description(),
		jaro_winkler_sensitive.description(),
		dice.description(),
		dice_sensitive.description(),
		jaccard.description(),
		jaccard_sensitive.description(),
		lcs.description(),
		lcs_sensitive.description(),
		lcsq.description(),
		lcsq_sensitive.description()
	};
	double expected[arraySize(metrics)][arraySize(str1)] = {
		{ 0.000000, 0.333333, 0.333333, 0.333333 },
		{ 1.000000, 1.000000, 1.000000, 0.500000 },
		{ 0.000000, 0.222222, 0.222222, 0.222222 },
		{ 1.000000, 1.000000, 1.000000, 0.333333 },
		{ 0.000000, 0.177778, 0.177778, 0.177778 },
		{ 1.000000, 1.000000, 1.000000, 0.333333 },
		{ 0.000000, 0.600000, 0.600000, 0.600000 },
		{ 1.000000, 1.000000, 1.000000, 0.800000 },
		{ 0.000000, 0.285714, 0.285714, 0.285714 },
		{ 1.000000, 1.000000, 1.000000, 0.500000 },
		{ 0.000000, 0.666667, 0.666667, 0.666667 },
		{ 1.000000, 1.000000, 1.000000, 0.666667 },
		{ 0.000000, 0.333333, 0.333333, 0.333333 },
		{ 1.000000, 1.000000, 1.000000, 0.500000 }
	};

	int res = 0;
	for (size_t i = 0; i < arraySize(str1); ++i) {
		double results[] = {
			levenshtein.distance(str1[i], str2[i]),
			levenshtein_sensitive.distance(str1[i], str2[i]),
			jaro.distance(str1[i], str2[i]),
			jaro_sensitive.distance(str1[i], str2[i]),
			jaro_winkler.distance(str1[i], str2[i]),
			jaro_winkler_sensitive.distance(str1[i], str2[i]),
			dice.distance(str1[i], str2[i]),
			dice_sensitive.distance(str1[i], str2[i]),
			jaccard.distance(str1[i], str2[i]),
			jaccard_sensitive.distance(str1[i], str2[i]),
			lcs.distance(str1[i], str2[i]),
			lcs_sensitive.distance(str1[i], str2[i]),
			lcsq.distance(str1[i], str2[i]),
			lcsq_sensitive.distance(str1[i], str2[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR(nullptr, "ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j].c_str(), str1[i].c_str(), str2[i].c_str(), expected[j][i], results[j]);
				++res;
			}
		}
	}
	RETURN(res);
}


template <typename T>
void run_test_v1(T& metric, const std::string& str) {
	auto t1 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < NUM_TESTS; ++i) {
		metric.distance(str);
		metric.similarity(str);
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	L_INFO(nullptr, "Time %s [v1 %d]: %lld ms\n", metric.description().c_str(), NUM_TESTS, duration);
}


template <typename T>
void run_test_v2(T& metric, const std::string& str1, const std::string& str2) {
	auto t1 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < NUM_TESTS; ++i) {
		metric.distance(str1, str2);
		metric.similarity(str1, str2);
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	L_INFO(nullptr, "Time %s [v2 %d]: %lld ms\n", metric.description().c_str(), NUM_TESTS, duration);
}


int test_time() {
	std::string str1("Xapiand Project - Release: Beta");
	std::string str2("Xapiand Beta");

	auto levenshtein = Levenshtein(str1);
	run_test_v1(levenshtein, str2);
	run_test_v2(levenshtein, str1, str2);

	auto jaro = Jaro(str1);
	run_test_v1(jaro, str2);
	run_test_v2(jaro, str1, str2);

	auto jaro_winkler = Jaro_Winkler(str1);
	run_test_v1(jaro_winkler, str2);
	run_test_v2(jaro_winkler, str1, str2);

	auto dice = Sorensen_Dice(str1);
	run_test_v1(dice, str2);
	run_test_v2(dice, str1, str2);

	auto jaccard = Jaccard(str1);
	run_test_v1(jaccard, str2);
	run_test_v2(jaccard, str1, str2);

	auto lcs = LCSubstr(str1);
	run_test_v1(lcs, str2);
	run_test_v2(lcs, str1, str2);

	auto lcsq = LCSubsequence(str1);
	run_test_v1(lcsq, str2);
	run_test_v2(lcsq, str1, str2);

	RETURN(0);
}
