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

#pragma once

#include "soundex.h"

#include <unordered_map>


static const std::unordered_map<std::string, std::string> replacement_map({
	{ "Ñ",  "N" }, { "Á",  "A" }, { "É" , "E" }, { "Í",  "I" },
	{ "Ó",  "O" }, { "Ú",  "U" }, { "À",  "A" }, { "È",  "E" },
	{ "Ì",  "I" }, { "Ò",  "O" }, { "Ù",  "U" }, { "Ü",  "U" },
	{ "ñ",  "N" }, { "á",  "A" }, { "é" , "E" }, { "í",  "I" },
	{ "ó",  "O" }, { "ú",  "U" }, { "à",  "A" }, { "è",  "E" },
	{ "è",  "I" }, { "ò",  "O" }, { "ù",  "U" }, { "ü",  "U" },
	{ "CH", "V" }, { "QU", "K" }, { "LL", "J" }, { "CE", "S" },
	{ "CI", "S" }, { "YA", "J" }, { "YE", "J" }, { "YI", "J" },
	{ "YO", "J" }, { "YU", "J" }, { "GE", "J" }, { "GI", "J" },
	{ "NY", "N" }
});


/*
 * Soundex for Spanish based in:
 *  https://wiki.postgresql.org/wiki/SoundexESP.
 *  http://oraclenotepad.blogspot.mx/2008/03/soundex-en-espaol.html
 *
 * Length of the result is not truncated, so the code does not have a fixed length.
 */
class SoundexSpanish : public Soundex<SoundexSpanish> {

	friend class Soundex<SoundexSpanish>;

	void replace(std::string& str) const {
		for (const auto& pattern : replacement_map) {
			auto pos = str.find(pattern.first);
			while (pos != std::string::npos) {
				str.replace(pos, pattern.first.length(), pattern.second);
				pos = str.find(pattern.first, pos + pattern.second.length());
			}
		}
	}

	std::string _encode(const std::string& str) const {
		if (str.empty()) {
			return std::string();
		}

		// 1. Pass to upper case.
		auto res = upper_string(str);

		// 2. Delete the letter 'H' initial.
		auto it = res.begin();
		if (*it == 'H') {
			it = res.erase(it);
		}

		/*
		 * 3. First letter is important, We must associate similar.
		 * e.g. 'vaca' becomes 'baca' and 'zapote' becomes 'sapote'
		 * An important phenomenon is 'GE' and 'GI' become 'JE' and 'JI';
		 * 'CA' becomes 'KA', etc.
		 */
		switch (*it) {
			case 'V':
				*it = 'B';
				break;
			case 'Z':
			case 'X':
				*it = 'S';
				break;
			case 'G':
				try {
					auto c = res.at(1);
					if (c == 'E' || c == 'I') {
						*it = 'J';
					}
				} catch (const std::out_of_range&) { }
				break;
			case 'C':
				try {
					auto c = res.at(1);
					if (c != 'H' || c != 'E' || c != 'I') {
						*it = 'K';
					}
				} catch (const std::out_of_range&) { }
				break;
			default:
				break;
		}

		/*
		 * 4. Fix "composed letters" and turn them one, vowels with accents
		 * and Ñ are replaced using replacement_map.
		 */
		replace(res);

		// 5. Starts the calculation of Soundex.
		it = res.begin();
		it = ++res.insert(it, *res.begin());
		while (it != res.end()) {
			switch (*it) {
				case 'B':
				case 'P':
				case 'F':
				case 'V':
					if (*(it - 1) != '1') {
						*it = '1';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'C':
				case 'G':
				case 'K':
				case 'S':
				case 'X':
				case 'Z':
					if (*(it - 1) != '2') {
						*it = '2';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'D':
				case 'T':
					if (*(it - 1) != '3') {
						*it = '3';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'L':
					if (*(it - 1) != '4') {
						*it = '4';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'M':
				case 'N':
					if (*(it - 1) != '5') {
						*it = '5';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'R':
					if (*(it - 1) != '6') {
						*it = '6';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'Q':
				case 'J':
					if (*(it - 1) != '7') {
						*it = '7';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				case 'A':
				case 'E':
				case 'H':
				case 'I':
				case 'O':
				case 'U':
				case 'W':
				case 'Y':
					if (*(it - 1) != '0') {
						*it = '0';
						++it;
					} else {
						it = res.erase(it);
					}
					break;
				default:
					it = res.erase(it);
					break;
			}
		}

		return res;
	}

	std::string _description() const noexcept {
		return std::string("Soundex for Spanish Language");
	}

public:
	public:
	SoundexSpanish() = default;

	template <typename T>
	SoundexSpanish(T&& str)
		: Soundex<SoundexSpanish>(_encode(std::forward<T>(str))) { }
};
