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


/*
 * Refined Soundex for English.
 *   http://ntz-develop.blogspot.mx/2011/03/phonetic-algorithms.html
 *
 * Length of the result is not truncated, so the code does not have a fixed length.
 */
class SoundexEnglish : public Soundex<SoundexEnglish> {

	friend class Soundex<SoundexEnglish>;

	std::string _encode(std::string str) const {
		// 1. Remove all non alphabetic characters at the begin.
		auto it = str.begin();
		while (it != str.end()) {
			if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z')) {
				break;
			} else {
				it = str.erase(it);
			}
		}

		if (str.empty()) {
			return str;
		}

		// 2. Starts the calculation of Soundex.
		it = ++str.insert(it, std::toupper(*it));
		while (it != str.end()) {
			switch (*it) {
				case 'b':
				case 'p':
				case 'B':
				case 'P':
					if (*(it - 1) != '1') {
						*it++ = '1';
					} else {
						it = str.erase(it);
					}
					break;
				case 'f':
				case 'v':
				case 'F':
				case 'V':
					if (*(it - 1) != '2') {
						*it++ = '2';
					} else {
						it = str.erase(it);
					}
					break;
				case 'c':
				case 'k':
				case 's':
				case 'C':
				case 'K':
				case 'S':
					if (*(it - 1) != '3') {
						*it++ = '3';
					} else {
						it = str.erase(it);
					}
					break;
				case 'g':
				case 'j':
				case 'G':
				case 'J':
					if (*(it - 1) != '4') {
						*it++ = '4';
					} else {
						it = str.erase(it);
					}
					break;
				case 'q':
				case 'x':
				case 'z':
				case 'Q':
				case 'X':
				case 'Z':
					if (*(it - 1) != '5') {
						*it++ = '5';
					} else {
						it = str.erase(it);
					}
					break;
				case 'd':
				case 't':
				case 'D':
				case 'T':
					if (*(it - 1) != '6') {
						*it++ = '6';
					} else {
						it = str.erase(it);
					}
					break;
				case 'l':
				case 'L':
					if (*(it - 1) != '7') {
						*it++ = '7';
					} else {
						it = str.erase(it);
					}
					break;
				case 'm':
				case 'n':
				case 'M':
				case 'N':
					if (*(it - 1) != '8') {
						*it++ = '8';
					} else {
						it = str.erase(it);
					}
					break;
				case 'r':
				case 'R':
					if (*(it - 1) != '9') {
						*it++ = '9';
					} else {
						it = str.erase(it);
					}
					break;
				case 'a':
				case 'e':
				case 'h':
				case 'i':
				case 'o':
				case 'u':
				case 'w':
				case 'y':
				case 'A':
				case 'E':
				case 'H':
				case 'I':
				case 'O':
				case 'U':
				case 'W':
				case 'Y':
					if (*(it - 1) != '0') {
						*it++ = '0';
					} else {
						it = str.erase(it);
					}
					break;
				default:
					it = str.erase(it);
					break;
			}
		}

		return str;
	}

	std::string_view _name() const noexcept {
		return "SoundexEnglish";
	}

	std::string _description() const noexcept {
		return "Soundex for English Language";
	}

public:
	SoundexEnglish() = default;

	template <typename T>
	SoundexEnglish(T&& str)
		: Soundex<SoundexEnglish>(_encode(std::string(std::forward<T>(str)))) { }
};
