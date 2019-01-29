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

#include "test_string_metric.h"

#include "../src/phonetic.h"
#include "../src/string_metric.h"
#include "utils.h"


#define NUM_TESTS 1000


int test_ranking_results() {
	INIT_LOG
	std::string str("Healed");
	std::string strs[] = {
		"Sealed", "Healthy", "Heard", "Herded", "Help", "Sold", "ealed"
	};
	auto levenshtein = Levenshtein(str);
	auto jaro = Jaro(str);
	auto jaro_winkler = Jaro_Winkler(str);
	auto dice = Sorensen_Dice(str);
	auto jaccard = Jaccard(str);
	auto lcs = LCSubstr(str);
	auto lcsq = LCSubsequence(str);
	auto soundexE = SoundexMetric<SoundexEnglish, LCSubsequence>(str);
	auto soundexF = SoundexMetric<SoundexFrench, LCSubsequence>(str);
	auto soundexG = SoundexMetric<SoundexGerman, LCSubsequence>(str);
	auto soundexS = SoundexMetric<SoundexSpanish, LCSubsequence>(str);
	std::string metrics[] = {
		levenshtein.description(),
		jaro.description(),
		jaro_winkler.description(),
		dice.description(),
		jaccard.description(),
		lcs.description(),
		lcsq.description(),
		soundexE.description(),
		soundexF.description(),
		soundexG.description(),
		soundexS.description()
	};
	double expected[arraySize(metrics)][arraySize(strs)] = {
		{ 0.166667, 0.428571, 0.333333, 0.333333, 0.500000, 0.666667, 0.166667 },
		{ 0.111111, 0.253968, 0.177778, 0.305556, 0.250000, 0.388889, 0.055556 },
		{ 0.111111, 0.152381, 0.124444, 0.305556, 0.200000, 0.388889, 0.055556 },
		{ 0.200000, 0.454545, 0.555556, 0.600000, 0.750000, 1.000000, 0.111111 },
		{ 0.333333, 0.428571, 0.333333, 0.500000, 0.500000, 0.714286, 0.200000 },
		{ 0.166667, 0.428571, 0.500000, 0.666667, 0.666667, 0.833333, 0.166667 },
		{ 0.166667, 0.428571, 0.333333, 0.333333, 0.500000, 0.666667, 0.166667 },
		{ 0.333333, 0.200000, 0.400000, 0.333333, 0.400000, 0.400000, 0.200000 },
		{ 0.250000, 0.333333, 0.666667, 0.500000, 0.333333, 0.333333, 0.250000 },
		{ 0.200000, 0.250000, 0.500000, 0.400000, 0.500000, 0.250000, 0.000000 },
		{ 0.333333, 0.200000, 0.400000, 0.333333, 0.400000, 0.400000, 0.000000 }
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
			lcsq.distance(strs[i]),
			soundexE.distance(strs[i]),
			soundexF.distance(strs[i]),
			soundexG.distance(strs[i]),
			soundexS.distance(strs[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR("ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j], str, strs[i], expected[j][i], results[j]);
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
		},
		{
			{ 0.354839, 0.538462, 0.526316, 0.525000, 0.517241, 0.672131, 0.695652, 0.434783 },
			{ 0.548387, 0.641026, 0.631579, 0.625000, 0.620690, 0.754098, 0.760870, 0.608696 },
			{ 0.580645, 0.692308, 0.684211, 0.700000, 0.586207, 0.786885, 0.782609, 0.652174 }
		},
		{
			{ 0.344828, 0.564103, 0.552632, 0.525000, 0.535714, 0.703125, 0.717391, 0.521739 },
			{ 0.586207, 0.692308, 0.710526, 0.700000, 0.571429, 0.812500, 0.804348, 0.652174 },
			{ 0.586207, 0.692308, 0.710526, 0.700000, 0.571429, 0.812500, 0.804348, 0.652174 }
		},
		{
			{ 0.344828, 0.540541, 0.552632, 0.512821, 0.518519, 0.698413, 0.711111, 0.478261 },
			{ 0.551724, 0.648649, 0.684211, 0.666667, 0.592593, 0.793651, 0.800000, 0.652174 },
			{ 0.586207, 0.675676, 0.710526, 0.692308, 0.592593, 0.809524, 0.822222, 0.695652 }
		},
		{
			{ 0.354839, 0.538462, 0.526316, 0.525000, 0.482759, 0.672131, 0.652174, 0.458333 },
			{ 0.548387, 0.641026, 0.631579, 0.625000, 0.620690, 0.754098, 0.739130, 0.541667 },
			{ 0.580645, 0.692308, 0.684211, 0.700000, 0.586207, 0.786885, 0.760870, 0.583333 }
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
				lcsq.distance(strs_r1[i], strs_r2[j]),
				soundexE.distance(strs_r1[i], strs_r2[j]),
				soundexF.distance(strs_r1[i], strs_r2[j]),
				soundexG.distance(strs_r1[i], strs_r2[j]),
				soundexS.distance(strs_r1[i], strs_r2[j])
			};
			for (size_t k = 0; k < arraySize(results); ++k) {
				if (std::abs(results[k] - expected2[k][i][j]) >= 1e-6) {
					L_ERR("ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[k], strs_r1[i], strs_r2[j], expected2[k][i][j], results[k]);
					++res;
				}
			}
		}
	}
	RETURN(res);
}


int test_special_cases() {
	INIT_LOG
	std::string str1[] = { "AA", "A", "A", "A", "A", "AB", "AA", "" };
	std::string str2[] = { "AAAAA", "A", "AA", "B", "AB", "B", "AA", "" };
	auto levenshtein = Levenshtein();
	auto jaro = Jaro();
	auto jaro_winkler = Jaro_Winkler();
	auto dice = Sorensen_Dice();
	auto jaccard = Jaccard();
	auto lcs = LCSubstr();
	auto lcsq = LCSubsequence();
	auto soundexE = SoundexMetric<SoundexEnglish, LCSubsequence>();
	auto soundexF = SoundexMetric<SoundexFrench, LCSubsequence>();
	auto soundexG = SoundexMetric<SoundexGerman, LCSubsequence>();
	auto soundexS = SoundexMetric<SoundexSpanish, LCSubsequence>();
	std::string metrics[] = {
		levenshtein.description(),
		jaro.description(),
		jaro_winkler.description(),
		dice.description(),
		jaccard.description(),
		lcs.description(),
		lcsq.description(),
		soundexE.description(),
		soundexF.description(),
		soundexG.description(),
		soundexS.description()
	};

	double expected[arraySize(metrics)][arraySize(str1)] = {
		{ 0.600000, 0.000000, 0.500000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.200000, 0.000000, 0.166667, 1.000000, 0.166667, 1.000000, 0.000000, 1.000000 },
		{ 0.160000, 0.000000, 0.150000, 1.000000, 0.150000, 1.000000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 1.000000, 1.000000, 1.000000, 1.000000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.600000, 0.000000, 0.500000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.600000, 0.000000, 0.500000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 0.000000, 1.000000, 0.333333, 0.666667, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 0.000000, 1.000000, 0.500000, 0.500000, 0.000000, 1.000000 },
		{ 0.000000, 0.000000, 0.000000, 1.000000, 0.333333, 0.666667, 0.000000, 1.000000 }
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
			lcsq.distance(str1[i], str2[i]),
			soundexE.distance(str1[i], str2[i]),
			soundexF.distance(str1[i], str2[i]),
			soundexG.distance(str1[i], str2[i]),
			soundexS.distance(str1[i], str2[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR("ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j], str1[i], str2[i], expected[j][i], results[j]);
				++res;
			}
		}
	}
	RETURN(res);
}


int test_case_sensitive() {
	INIT_LOG
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
	auto soundexE = SoundexMetric<SoundexEnglish, LCSubsequence>();
	auto soundexE_sensitive = SoundexMetric<SoundexEnglish, LCSubsequence>(false);
	auto soundexF = SoundexMetric<SoundexFrench, LCSubsequence>();
	auto soundexF_sensitive = SoundexMetric<SoundexFrench, LCSubsequence>(false);
	auto soundexG = SoundexMetric<SoundexGerman, LCSubsequence>();
	auto soundexG_sensitive = SoundexMetric<SoundexGerman, LCSubsequence>(false);
	auto soundexS = SoundexMetric<SoundexSpanish, LCSubsequence>();
	auto soundexS_sensitive = SoundexMetric<SoundexSpanish, LCSubsequence>(false);
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
		lcsq_sensitive.description(),
		soundexE.description(),
		soundexE_sensitive.description(),
		soundexF.description(),
		soundexF_sensitive.description(),
		soundexG.description(),
		soundexG_sensitive.description(),
		soundexS.description(),
		soundexS_sensitive.description()
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
		{ 1.000000, 1.000000, 1.000000, 0.500000 },
		{ 0.000000, 0.000000, 0.000000, 0.000000 },
		{ 0.000000, 0.000000, 0.000000, 0.000000 },
		{ 0.000000, 0.166667, 0.166667, 0.166667 },
		{ 0.000000, 0.166667, 0.166667, 0.166667 },
		{ 0.000000, 0.000000, 0.000000, 0.000000 },
		{ 0.000000, 0.000000, 0.000000, 0.000000 },
		{ 0.000000, 0.166667, 0.166667, 0.166667 },
		{ 0.000000, 0.166667, 0.166667, 0.166667 }
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
			lcsq_sensitive.distance(str1[i], str2[i]),
			soundexE.distance(str1[i], str2[i]),
			soundexE_sensitive.distance(str1[i], str2[i]),
			soundexS.distance(str1[i], str2[i]),
			soundexS_sensitive.distance(str1[i], str2[i])
		};
		for (size_t j = 0; j < arraySize(results); ++j) {
			if (std::abs(results[j] - expected[j][i]) >= 1e-6) {
				L_ERR("ERROR: Distance of %s(%s, %s) -> Expected: %f Result: %f\n", metrics[j], str1[i], str2[i], expected[j][i], results[j]);
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
	L_INFO("Time %s [v1 %d]: %lld ms\n", metric.description(), NUM_TESTS, duration);
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
	L_INFO("Time %s [v2 %d]: %lld ms\n", metric.description(), NUM_TESTS, duration);
}


int test_time() {
	INIT_LOG
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

	auto soundexE = SoundexMetric<SoundexEnglish, LCSubsequence>(str1);
	run_test_v1(soundexE, str2);
	run_test_v2(soundexE, str1, str2);

	auto soundexF = SoundexMetric<SoundexFrench, LCSubsequence>(str1);
	run_test_v1(soundexF, str2);
	run_test_v2(soundexF, str1, str2);

	auto soundexG = SoundexMetric<SoundexGerman, LCSubsequence>(str1);
	run_test_v1(soundexG, str2);
	run_test_v2(soundexG, str1, str2);

	auto soundexS = SoundexMetric<SoundexSpanish, LCSubsequence>(str1);
	run_test_v1(soundexS, str2);
	run_test_v2(soundexS, str1, str2);

	RETURN(0);
}
