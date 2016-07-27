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

#include "test_phonetic.h"

#include "../src/phonetic.h"
#include "utils.h"


#define NUM_TESTS 10000


int test_soundex_english() {
	/*
	 * Tests based in the article:
	 * http://ntz-develop.blogspot.mx/2011/03/phonetic-algorithms.html
	 */
	std::string expected[] = {
		"", "A0", "B1905", "C30908", "H093", "L7081096", "N807608"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			""
		},
		{
			"aaaaa", "aaaa", "aaa", "aa", "a"
		},
		{
			"brrraz", "Brooooz"
		},
		{
			"Caren", "Caron", "Carren", "Charon", "Corain", "Coram",
			"Corran", "Corrin", "corwin", "Curran", "Curreen", "currin",
			"Currom", "Currum", "Curwen"
		},
		{
			"Hairs", "Hark", "hars", "Hayers", "heers", "Hiers"
		},
		{
			"Lambard", "lambart", "Lambert", "LambirD", "Lampaert",
			"Lampard", "LaMpart", "laaampeuurd", "Lampert", "Lamport",
			"Limbert", "Lombard"
		},
		{
			"Nolton", "Noulton"
		}
	};

	SoundexEnglish s_eng;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = s_eng.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR(nullptr, "ERROR: [%s] Result: %s  Expected: %s\n", str.c_str(), res.c_str(), expected[i].c_str());
			}
		}
	}

	RETURN(cont);
}


int test_soundex_spanish() {
	std::string expected[] = {
		"", "A0", "O040", "B1602", "K20605", "B1020", "L4051063", "J70403050", "K2020", "K20640", "B1050", "N5050"
	};

	std::vector<std::string> strs[arraySize(expected)] = {
		{
			""
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
			"Caren", "Caron", "Carren", "Charon", "Corain", "Coram",
			"Corran", "Corrin", "corwin", "Curran", "Curreen", "currin",
			"Currom", "Currum", "Curwen", "Karen"
		},
		{
			"vaca", "baca", "vaka", "baka", "vaaacaaa"
		},
		{
			"Lambard", "lambart", "Lambert", "LambirD", "Lampaert",
			"Lampard", "LaMpart", "laaampeuurd", "Lampert", "Lamport",
			"Limbert", "Lombard"
		},
		{
			"Jelatina", "Gelatina", "Jaletina"
		},
		{
			"Queso", "Keso", "kiso", "Quiso", "Quizá"
		},
		{
			"Karla", "Carla", "Kerla"
		},
		{
			"Vena", "Vèná", "bena"
		},
		{
			"Ñoño", "Nono", "Nóno"
		}
	};

	SoundexSpanish spa_s;
	int cont = 0;
	for (size_t i = 0; i < arraySize(expected); ++i) {
		for (const auto& str : strs[i]) {
			auto res = spa_s.encode(str);
			if (res != expected[i]) {
				++cont;
				L_ERR(nullptr, "ERROR: [%s] Result: %s  Expected: %s\n", str.c_str(), res.c_str(), expected[i].c_str());
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
	"laaampeuurd", "Lampert", "Lamport", "Limbert", "Lombard"
};


template <typename S>
void test_time() {
	S impl;
	auto t1 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < NUM_TESTS; ++i) {
		for (const auto& str : time_strs) {
			impl.encode(str);
		}
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	L_INFO(nullptr, "Time %s [%d]: %lld ms\n", impl.description().c_str(), NUM_TESTS, duration);
}


int test_soundex_time() {
	test_time<SoundexEnglish>();
	test_time<SoundexSpanish>();
	RETURN(0);
}
