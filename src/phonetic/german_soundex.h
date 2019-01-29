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

#pragma once

#include "soundex.h"

#include <string>                // for std::string
#include <unordered_map>         // for std::unordered_map
#include <utility>               // for std::forward

#include "string.hh"             // for string::toupper


static const std::unordered_map<std::string, std::string> german_accents({
	{ "Ä",  "A" }, { "ä",  "A" }, { "Ö" , "O" }, { "ö",  "O" },
	{ "Ü",  "U" }, { "ü",  "U" }, { "ß",  "S" }
});


static const std::unordered_map<std::string, std::string> german_composed({
	{ "PH", "3" }, { "CA", "4" }, { "CH", "4" }, { "CK", "4" },
	{ "CO", "4" }, { "CQ", "4" }, { "CU", "4" }, { "CX", "4" },
	{ "DC", "8" }, { "DS", "8" }, { "DZ", "8" }, { "TC", "8" },
	{ "TS", "8" }, { "TZ", "8" }, { "KX", "8" }, { "QX", "8" },
	{ "SC", "8" }, { "ZC", "8" }
});


/*
 * Soundex for German based in Kölner Phonetik:
 *  https://de.wikipedia.org/wiki/Kölner_Phonetik
 *
 * Length of the result is not truncated, so the code does not have a fixed length.
 */
class SoundexGerman : public Soundex<SoundexGerman> {

	friend class Soundex<SoundexGerman>;

	std::string _encode(std::string str) const {
		if (str.empty()) {
			return str;
		}

		// 1. Replace accents.
		replace(str, 0, german_accents);

		// 2. Pass to upper case.
		string::toupper(str);

		// 3. Remove all non alphabetic characters at the begin.
		auto it = str.begin();
		while (it != str.end()) {
			if (*it >= 'A' && *it <= 'Z') {
				break;
			} else {
				it = str.erase(it);
			}
		}

		if (str.empty()) {
			return str;
		}

		// 4. Replace prefix.
		if (str.length() > 1 && *it == 'C') {
			switch (*(it + 1)) {
				case 'A':
				case 'H':
				case 'K':
				case 'L':
				case 'O':
				case 'Q':
				case 'R':
				case 'U':
				case 'X':
					it = str.erase(it);
					*it = '4';
					break;
			}
		}


		// 5. Replace "composed letters".
		replace(str, 0, german_composed);

		// 6. Starts the calculation of Soundex.
		it = str.begin();
		while (it != str.end()) {
			switch (*it) {
				case 'A':
				case 'E':
				case 'I':
				case 'J':
				case 'O':
				case 'U':
				case 'Y':
					if (it == str.begin() || *(it - 1) != '0') {
						*it++ = '0';
					} else {
						it = str.erase(it);
					}
					break;
				case 'B':
				case 'P':
					if (it == str.begin() || *(it - 1) != '1') {
						*it++ = '1';
					} else {
						it = str.erase(it);
					}
					break;
				case 'D':
				case 'T':
					if (it == str.begin() || *(it - 1) != '2') {
						*it++ = '2';
					} else {
						it = str.erase(it);
					}
					break;
				case 'F':
				case 'V':
				case 'W':
					if (it == str.begin() || *(it - 1) != '3') {
						*it++ = '3';
					} else {
						it = str.erase(it);
					}
					break;
				case 'G':
				case 'K':
				case 'Q':
					if (it == str.begin() || *(it - 1) != '4') {
						*it++ = '4';
					} else {
						it = str.erase(it);
					}
					break;
				case 'L':
					if (it == str.begin() || *(it - 1) != '5') {
						*it++ = '5';
					} else {
						it = str.erase(it);
					}
					break;
				case 'M':
				case 'N':
					if (it == str.begin() || *(it - 1) != '6') {
						*it++ = '6';
					} else {
						it = str.erase(it);
					}
					break;
				case 'R':
					if (it == str.begin() || *(it - 1) != '7') {
						*it++ = '7';
					} else {
						it = str.erase(it);
					}
					break;
				case 'C':
				case 'S':
				case 'Z':
					if (it == str.begin() || *(it - 1) != '8') {
						*it++ = '8';
					} else {
						it = str.erase(it);
					}
					break;
				case 'X':
					if (it == str.begin() || *(it - 1) != '4') {
						*it++ = '4';
						it = ++str.insert(it, '8');
					} else {
						*it++ = '8';
					}
					break;
				case '3':
				case '4':
				case '8':
					++it;
					break;
				default:
					it = str.erase(it);
					break;
			}
		}

		return str;
	}

	std::string _description() const noexcept {
		return std::string("Soundex for German Language");
	}

public:
	public:
	SoundexGerman() = default;

	template <typename T>
	SoundexGerman(T&& str)
		: Soundex<SoundexGerman>(_encode(std::string(std::forward<T>(str)))) { }
};
