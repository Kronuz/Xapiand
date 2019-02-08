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

#include "test_phonetic.h"

#include "../src/phonetic.h"
#include "utils.h"


#define NUM_TESTS 10000


int test_soundex_english() {
	/*
	 * Tests based in the article:
	 * http://ntz-develop.blogspot.mx/2011/03/phonetic-algorithms.html
	 */
	INIT_LOG
	std::string expected[] = {
		"", "A0", "B1905", "C30908", "H093", "L7081096", "N807608"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			"", "!?,.:;' ", "áéíóúñ"
		},
		{
			"aaaaa", "aaaa", "aaa", "aa", "a"
		},
		{
			"brrraz", "Brooooz"
		},
		{
			"Caren", "!!caron-", "Carren", "Charon", "Corain", "Coram",
			"Corran", "Corrin", "corwin", "Curran", "Curreen", "currin",
			"Currom", "Currum", "Curwen"
		},
		{
			"Hairs", "Hark", "hars", "Hayers", "heers", "Hiers"
		},
		{
			"Lambard", "lambart", "Lambert", "LambirD", "Lampaert",
			"Lampard", "LaMpart", "laaampeuurd", "lampert", "Lamport",
			"Limbert", "LomBAard"
		},
		{
			"Nolton", "noulton"
		}
	};

	SoundexEnglish s_eng;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = s_eng.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR("ERROR: [{}] Result: {}  Expected: {}\n", str, res, expected[i]);
			}
		}
	}

	RETURN(cont);
}


int test_soundex_french() {
	/*
	 * Tests based in the article:
	 * http://www.phpclasses.org/package/2972-PHP-Implementation-of-the-soundex-algorithm-for-French.html#view_files/files/13492
	 */
	INIT_LOG
	std::string expected[] = {
		"", "A", "MALAN", "GRA", "RASA", "LAMBAR", "LAMPAR", "KATAR", "FAR"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			"", "hhhh", "hyyyy", "hhhyyyh", "yyyyy", "yyyyh", "yhhyh", "!?,.:;' "
		},
		{
			"aaaaa", "aaaa", "aaa", "aa", "haaaa", "a", "ya"
		},
		{
			"MALLEIN", "moleins", "MOLIN", "MOULIN"
		},
		{
			"GRAU", "GROS", "GRAS"
		},
		{
			"ROUSSOT", "RASSAT", "ROSSAT"
		},
		{
			"Lambard", "!!lambart-", "Lambert", "LambirD",
			"Limbert", "Lombard"
		},
		{
			"Lampaert", "Lampard", "LaMpart", "laaam - peuurS", "Lampert", "Lamport"
		},
		{
			"GAUTHIER", "gautier", "GOUTHIER", "CATTIER", "cottier", "COUTIER"
		},
		{
			"FARRE", "faure", "FORT", "four-r", "PHAURE"
		}
	};

	SoundexFrench s_fr;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = s_fr.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR("ERROR: [{}] Result: {}  Expected: {}\n", str, res, expected[i]);
			}
		}
	}

	RETURN(cont);
}


int test_soundex_german() {
	INIT_LOG
	std::string expected[] = {
		"", "0", "6050750206802", "607", "5061072", "60507"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			"", "hhhh", "!?,.:;' "
		},
		{
			"aaaaa", "aaaa", "aaa", "aa", "haaaa"
		},
		{
			"Müller-Lüdenscheidt", "Muller Ludeanscheidt", "Mueller Luedenscheidt",
			"Müller-Lü denscheidt"
		},
		{
			"Meier", "Meyer", "Mayr"
		},
		{
			"Lambard", "!!lambart-", "Lambert", "LambirD",
			"Limbert", "LombarD"
		},
		{
			"Müller", "mellar", "meller", "mell´ar", "miehler",
			"milar", "milor", "moeller", "mouller", "möllor",
			"müler", "möhler"
		}
	};

	SoundexGerman s_grm;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = s_grm.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR("ERROR: [{}] Result: {}  Expected: {}\n", str, res, expected[i]);
			}
		}
	}

	RETURN(cont);
}


int test_soundex_spanish() {
	INIT_LOG
	std::string expected[] = {
		"", "A0", "O040", "B1602", "K20605", "B1020", "L4051063", "J70403050",
		"K2020", "K20640", "B1050", "N5050"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			"", "h", "hhhhh", "!?,.:;' "
		},
		{
			"aaaaa", "aaaa", "aaa", "aa", "a"
		},
		{
			"oooolaaaaaa", "olaaa", "ola"
		},
		{
			"brrraz", "Brooooz"
		},
		{
			"Caren", "!!Caron-", "Carren", "Charon", "Corain", "Coram",
			"Corran", "Corrin", "corwin", "Curran", "Curreen", "currin",
			"Currom", "Currum", "Curwen", "KaRen"
		},
		{
			"vaca", "baca", "vaka", "baka", "va c-a"
		},
		{
			"Lambard", "lambart", "Lambert", "LambirD", "Lampaert",
			"Lampard", "LaMpart", "laaampeuurd", "Lampert", "Lamport",
			"Limbert", "lomBarD"
		},
		{
			"Jelatina", "Gelatina", "jale - tina"
		},
		{
			"Queso", "Keso", "kiso", "Quiso", "Quizá"
		},
		{
			"Karla", "Carla", "Ker la"
		},
		{
			"Vena", "Vèná", "bena"
		},
		{
			"Ñoño", "nono", "Nó - No"
		}
	};

	SoundexSpanish spa_s;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = spa_s.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR("ERROR: [{}] Result: {}  Expected: {}\n", str, res, expected[i]);
			}
		}
	}

	RETURN(cont);
}


static const std::vector<std::string> time_strs = {
	"Caren", "Caron", "Carren", "Charon", "Corain", "Coram",
	"Corran", "Corrin", "corwin", "Curran", "Curreen", "currin",
	"Currom", "Currum", "Curwen", "Karen", "Lambard", "lambart",
	"Lambert", "LambirD", "Lampaert", "Lampard", "LaMpart",
	"laaampeuurd", "Lampert", "Lamport", "Limbert", "Lombard",
	"Gelatina", "Mallein", "Cottier", "Müller-Lüdenscheidt",
	"Meier"
};


template <typename S>
void test_time() {
	S soundex;
	auto t1 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < NUM_TESTS; ++i) {
		for (const auto& str : time_strs) {
			soundex.encode(str);
		}
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	L_INFO("Time {} [{}]: {} ms\n", soundex.description(), NUM_TESTS, duration);
}


int test_soundex_time() {
	INIT_LOG
	test_time<SoundexEnglish>();
	test_time<SoundexFrench>();
	test_time<SoundexGerman>();
	test_time<SoundexSpanish>();
	RETURN(0);
}
