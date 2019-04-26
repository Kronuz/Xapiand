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

#pragma once

#include "soundex.h"

#include <string>                // for std::string
#include <unordered_map>         // for std::unordered_map
#include <utility>               // for std::forward
#include <vector>                // for std::vector

#include "string.hh"             // for string::inplace_upper


static const std::unordered_map<std::string, std::string> french_accents({
	{ "Á", "A" }, { "À", "A" }, { "Ä", "A" }, { "Â", "A" }, { "Ã", "A" },
	{ "É", "E" }, { "È", "E" }, { "Ë", "E" }, { "Ê", "E" }, { "Œ", "E" },
	{ "Í", "I" }, { "Ì", "I" }, { "Ï", "I" }, { "Î", "I" },
	{ "Ó", "O" }, { "Ò", "O" }, { "Ö", "O" }, { "Ô", "O" }, { "Õ", "O" },
	{ "Ú", "U" }, { "Ù", "U" }, { "Ü", "U" }, { "Û", "U" },
	{ "á", "A" }, { "à", "A" }, { "ä", "A" }, { "â", "A" }, { "ã", "A" },
	{ "é", "E" }, { "è", "E" }, { "ë", "E" }, { "ê", "E" }, { "œ", "E" },
	{ "í", "I" }, { "ì", "I" }, { "ï", "I" }, { "î", "I" },
	{ "ó", "O" }, { "ò", "O" }, { "ö", "O" }, { "ô", "O" }, { "õ", "O" },
	{ "ú", "U" }, { "ù", "U" }, { "ü", "U" }, { "û", "U" },
	{ "Ñ", "N" }, { "Ç", "S" }, { "ñ", "N" }, { "ç", "S" }
});


static const std::vector<std::pair<std::string, std::string>> french_composed({
	{ "GUI", "KI" }, { "GUE", "KE" }, { "GA",  "KA" },
	{ "GO",  "KO" }, { "GU",  "K"  }, { "CA",  "KA" },
	{ "CO",  "KO" }, { "CU",  "KU" }, { "Q",   "K"  },
	{ "CC",  "K"  }, { "CK",  "K"  }
 });


static const std::vector<std::pair<std::string, std::string>> french_prefixes({
	{ "KN",  "NN"  }, { "PF",  "FF"  }, { "PH",  "FF"  },
	{ "ASA", "AZA" }, { "SCH", "SSS" }, { "MAC", "MCC" }
});


/*
 * Soundex for French language based in Soundex 2:
 *  http://sqlpro.developpez.com/cours/soundex/
 *
 * Length of the result is not truncated, so the code does not have a fixed length.
 */
class SoundexFrench : public Soundex<SoundexFrench> {

	friend class Soundex<SoundexFrench>;

	std::string _encode(std::string str) const {
		if (str.empty()) {
			return str;
		}

		// 1. Replace accents.
		replace(str, 0, french_accents);

		// 2. Pass to upper case.
		string::inplace_upper(str);

		// 3. Keep only alphabet characters.
		for (auto it = str.begin(); it != str.end(); ) {
			if (*it >= 'A' && *it <= 'Z') {
				++it;
			} else {
				it = str.erase(it);
			}
		}

		if (str.empty()) {
			return str;
		}

		// 4. Replace primary consonants.
		replace(str, 0, french_composed);

		// 5. Replace vowels except the fisrt.
		const auto it_e = str.end();
		for (auto it = ++str.begin(); it != it_e; ++it) {
			switch (*it) {
				case 'E':
				case 'I':
				case 'O':
				case 'U':
					*it = 'A';
					break;
				default:
					break;
			}
		}

		// 6. Replace prefix.
		replace_prefix(str, french_prefixes);

		// 7. Replace complementary substitutions.
		replace(str, 1, french_prefixes.begin(), --french_prefixes.end());

		// 8. Remove 'H' unless it is prededed by a 'C' or an 'S' and 'Y' unless it is preceded by 'A'.
		for (auto it = str.begin(); it != str.end(); ) {
			switch (*it) {
				case 'H':
					if (it == str.begin() || (*(it - 1) != 'C' && *(it - 1) != 'S')) {
						it = str.erase(it);
					} else {
						++it;
					}
					break;
				case 'Y':
					if (it == str.begin() || *(it - 1) != 'A') {
						it = str.erase(it);
					} else {
						++it;
					}
					break;
				default:
					++it;
					break;
			}
		}

		if (str.empty()) {
			return str;
		}

		// 9. Remove the terminations 'A', 'T', 'D', 'S'.
		if (str.length() > 1) {
			switch (str.back()) {
				case 'A':
				case 'T':
				case 'D':
				case 'S':
					str.pop_back();
					break;
				default:
					break;
			}
		}

		if (str.empty()) {
			return str;
		}

		// 10. Remove all 'A' except if it is at the beginning of the string. (We don't do this step).
		// str.erase(std::remove(++str.begin(), str.end(), 'A'), str.end());

		// 11. Remove repetitive letters.
		for (auto it = ++str.begin(); it != str.end(); ) {
			if (*it == *(it - 1)) {
				it = str.erase(it);
			} else {
				++it;
			}
		}

		return str;
	}

	std::string_view _name() const noexcept {
		return "SoundexFrench";
	}

	std::string _description() const noexcept {
		return "Soundex for French Language";
	}

public:
	public:
	SoundexFrench() = default;

	template <typename T>
	SoundexFrench(T&& str)
		: Soundex<SoundexFrench>(_encode(std::string(std::forward<T>(str)))) { }
};
