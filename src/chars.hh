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

namespace chars {

constexpr unsigned int HEX          = 0x0000ffff;
constexpr unsigned int IS_SPACE     = 0x00010000;
constexpr unsigned int IS_ALPHA     = 0x00020000;
constexpr unsigned int IS_UPPER     = 0x00040000;
constexpr unsigned int IS_DIGIT     = 0x00080000;
constexpr unsigned int IS_HEX_DIGIT = 0x00100000;
constexpr unsigned int IS_ASCII     = 0x00200000;
constexpr unsigned int IS_KEYWORD   = 0x00400000;  // -.:_0-9a-zA-Z(/\@<>=*[{"
constexpr unsigned int IS_NON_HEX   = 0x00001000;

constexpr unsigned int S = IS_SPACE;
constexpr unsigned int A = IS_ALPHA;
constexpr unsigned int U = IS_UPPER;
constexpr unsigned int D = IS_DIGIT;
constexpr unsigned int H = IS_HEX_DIGIT;
constexpr unsigned int I = IS_ASCII;
constexpr unsigned int K = IS_KEYWORD;
constexpr unsigned int N = IS_NON_HEX;


constexpr unsigned int char_tab[256]{
	I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,
	I|N,          I|S,          I|S,          I|N,          I|S,          I|S,          I|N,          I|N,
	I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,
	I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,          I|N,

	I|S,          I|N,          I|N|K,        I|N,          I|N,          I|N,          I|N,          I|N,
	I|N|K,        I|N,          I|N|K,        I|N,          I|N,          I|N|K,        I|N|K,        I|N|K,
	I|H|D|K|0,    I|H|D|K|1,    I|H|D|K|2,    I|H|D|K|3,    I|H|D|K|4,    I|H|D|K|5,    I|H|D|K|6,    I|H|D|K|7,
	I|H|D|K|8,    I|H|D|K|9,    I|N|K,        I|N,          I|N|K,        I|N|K,        I|N|K,        I|N,

	I|N|K,        I|H|A|U|K|10, I|H|A|U|K|11, I|H|A|U|K|12, I|H|A|U|K|13, I|H|A|U|K|14, I|H|A|U|K|15, I|N|A|U|K,
	I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,
	I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,
	I|N|A|U|K,    I|N|A|U|K,    I|N|A|U|K,    I|N|K,        I|N|K,        I|N,          I|N,          I|N|K,

	I|N|K,        I|H|A|K|10,   I|H|A|K|11,   I|H|A|K|12,   I|H|A|K|13,   I|H|A|K|14,   I|H|A|K|15,   I|N|A|K,
	I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,
	I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|A|K,
	I|N|A|K,      I|N|A|K,      I|N|A|K,      I|N|K,        I|N,          I|N,          I|N,          I|N,

	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,

	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,

	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,

	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
	N,            N,            N,            N,            N,            N,            N,            N,
};


inline constexpr int
is_space(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_SPACE;
}


inline constexpr int
is_alpha(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_ALPHA;
}


inline constexpr int
is_upper(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_UPPER;
}


inline constexpr int
is_digit(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_DIGIT;
}


inline constexpr int
is_alnum(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & (IS_DIGIT | IS_ALPHA);
}


inline constexpr int
is_ascii(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_ASCII;
}


inline constexpr int
is_keyword(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_KEYWORD;
}


inline constexpr int
is_hexdigit(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & IS_HEX_DIGIT;
}


inline constexpr unsigned int
hexdigit(char c) noexcept {
	return char_tab[static_cast<unsigned char>(c)] & HEX;
}


// converts the two hexadecimal characters to an int (a byte)
inline constexpr int
hexdec(const char** ptr) noexcept {
	auto pos = *ptr;
	auto a = hexdigit(*pos++);
	auto b = hexdigit(*pos++);
	int dec = a << 4 | b;
	if (dec < 256) {
		*ptr = pos;
	}
	return dec;
}


// converts a character to lowercase
inline constexpr char tolower(char c) noexcept {
	constexpr char _[256]{
		'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
		'\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
		'\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
		'\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',

		   ' ',    '!',    '"',    '#',    '$',    '%',    '&',   '\'',
		   '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
		   '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
		   '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',

		   '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
		   'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
		   'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
		   'x',    'y',    'z',    '[',    '\\',   ']',    '^',    '_',

		   '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
		   'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
		   'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
		   'x',    'y',    'z',    '{',    '|',    '}',    '~', '\x7f',

		'\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
		'\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
		'\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
		'\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',

		'\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
		'\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
		'\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
		'\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',

		'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
		'\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
		'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
		'\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',

		'\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
		'\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
		'\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
		'\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
	};
	return _[static_cast<unsigned char>(c)];
}


// converts a character to uppercase
inline constexpr char toupper(char c) noexcept {
	constexpr char _[256]{
		'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
		'\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
		'\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
		'\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',

		   ' ',    '!',    '"',    '#',    '$',    '%',    '&',   '\'',
		   '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
		   '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
		   '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',

		   '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
		   'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
		   'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
		   'X',    'Y',    'Z',    '[',    '\\',   ']',    '^',    '_',

		   '`',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
		   'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
		   'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
		   'X',    'Y',    'Z',    '{',    '|',    '}',    '~', '\x7f',

		'\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
		'\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
		'\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
		'\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',

		'\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
		'\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
		'\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
		'\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',

		'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
		'\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
		'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
		'\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',

		'\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
		'\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
		'\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
		'\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
	};
	return _[static_cast<unsigned char>(c)];
}


inline void
char_repr(char c, char** p)
{
	constexpr char _[] =
		"00" "01" "02" "03" "04" "05" "06" "07"
		"08" "09" "0a" "0b" "0c" "0d" "0e" "0f"
		"10" "11" "12" "13" "14" "15" "16" "17"
		"18" "19" "1a" "1b" "1c" "1d" "1e" "1f"

		"20" "21" "22" "23" "24" "25" "26" "27"
		"28" "29" "2a" "2b" "2c" "2d" "2e" "2f"
		"30" "31" "32" "33" "34" "35" "36" "37"
		"38" "39" "3a" "3b" "3c" "3d" "3e" "3f"

		"40" "41" "42" "43" "44" "45" "46" "47"
		"48" "49" "4a" "4b" "4c" "4d" "4e" "4f"
		"50" "51" "52" "53" "54" "55" "56" "57"
		"58" "59" "5a" "5b" "5c" "5d" "5e" "5f"

		"60" "61" "62" "63" "64" "65" "66" "67"
		"68" "69" "6a" "6b" "6c" "6d" "6e" "6f"
		"70" "71" "72" "73" "74" "75" "76" "77"
		"78" "79" "7a" "7b" "7c" "7d" "7e" "7f"

		"80" "81" "82" "83" "84" "85" "86" "87"
		"88" "89" "8a" "8b" "8c" "8d" "8e" "8f"
		"90" "91" "92" "93" "94" "95" "96" "97"
		"98" "99" "9a" "9b" "9c" "9d" "9e" "9f"

		"a0" "a1" "a2" "a3" "a4" "a5" "a6" "a7"
		"a8" "a9" "aa" "ab" "ac" "ad" "ae" "af"
		"b0" "b1" "b2" "b3" "b4" "b5" "b6" "b7"
		"b8" "b9" "ba" "bb" "bc" "bd" "be" "bf"

		"c0" "c1" "c2" "c3" "c4" "c5" "c6" "c7"
		"c8" "c9" "ca" "cb" "cc" "cd" "ce" "cf"
		"d0" "d1" "d2" "d3" "d4" "d5" "d6" "d7"
		"d8" "d9" "da" "db" "dc" "dd" "de" "df"

		"e0" "e1" "e2" "e3" "e4" "e5" "e6" "e7"
		"e8" "e9" "ea" "eb" "ec" "ed" "ee" "ef"
		"f0" "f1" "f2" "f3" "f4" "f5" "f6" "f7"
		"f8" "f9" "fa" "fb" "fc" "fd" "fe" "ff"
	;
	const char* repr = &_[static_cast<unsigned char>(c) * 2];
	*(*p)++ = repr[0];
	*(*p)++ = repr[1];
}

}  // namespace chars
