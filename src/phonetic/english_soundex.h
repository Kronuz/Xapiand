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


/*
 * Refined Soundex for English.
 *   http://ntz-develop.blogspot.mx/2011/03/phonetic-algorithms.html
 *
 * Length of the result is not truncated, so the code does not have a fixed length.
 */
class SoundexEnglish : public Soundex<SoundexEnglish> {

	friend class Soundex<SoundexEnglish>;

	std::string _encode(const std::string& str) const {
		if (str.empty()) {
			return std::string();
		}

		std::string result;
		result.reserve(str.length());
		result.push_back(toupper(str[0]));
		for (const auto& c : str) {
			switch (c) {
				case 'b':
				case 'p':
				case 'B':
				case 'P':
					if (result.back() != '1') {
						result.push_back('1');
					}
					break;
				case 'f':
				case 'v':
				case 'F':
				case 'V':
					if (result.back() != '2') {
						result.push_back('2');
					}
					break;
				case 'c':
				case 'k':
				case 's':
				case 'C':
				case 'K':
				case 'S':
					if (result.back() != '3') {
						result.push_back('3');
					}
					break;
				case 'g':
				case 'j':
				case 'G':
				case 'J':
					if (result.back() != '4') {
						result.push_back('4');
					}
					break;
				case 'q':
				case 'x':
				case 'z':
				case 'Q':
				case 'X':
				case 'Z':
					if (result.back() != '5') {
						result.push_back('5');
					}
					break;
				case 'd':
				case 't':
				case 'D':
				case 'T':
					if (result.back() != '6') {
						result.push_back('6');
					}
					break;
				case 'l':
				case 'L':
					if (result.back() != '7') {
						result.push_back('7');
					}
					break;
				case 'm':
				case 'n':
				case 'M':
				case 'N':
					if (result.back() != '8') {
						result.push_back('8');
					}
					break;
				case 'r':
				case 'R':
					if (result.back() != '9') {
						result.push_back('9');
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
					if (result.back() != '0') {
						result.push_back('0');
					}
					break;
				default:
					break;
			}
		}
		return result;
	}

	std::string _description() const noexcept {
		return std::string("Soundex for English Language");
	}

public:
	SoundexEnglish() = default;

	template <typename T>
	SoundexEnglish(T&& str)
		: Soundex<SoundexEnglish>(_encode(std::forward<T>(str))) { }
};
