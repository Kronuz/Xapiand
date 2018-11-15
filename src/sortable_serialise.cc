/** @file sortable-serialise.cc
 * @brief Serialise floating point values to string which sort the same way.
 */
/* Copyright (C) 2007,2009,2015,2016 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "sortable_serialise.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>

#include "cassert.h"    // for ASSERT


size_t
sortable_serialise_(long double value, char * buf)
{
	long double mantissa;
	int exponent;

	// Negative infinity.
	if (value < -LDBL_MAX) { return 0; }

	mantissa = frexpl(value, &exponent);

	/* Deal with zero specially.
	 *
	 * IEEE representation of long doubles uses 15 bits for the exponent, with a
	 * bias of 16383.  We bias this by subtracting 8, and non-IEEE
	 * representations may allow higher exponents, so allow exponents down to
	 * -32759 - if smaller exponents are possible anywhere, we underflow such
	 *  numbers to 0.
	 */
	if (mantissa == 0.0 || exponent < -(LDBL_MAX_EXP + LDBL_MAX_EXP - 1 - 8)) {
		*buf = '\x80';
		return 1;
	}

	bool negative = (mantissa < 0);
	if (negative) { mantissa = -mantissa; }

	// Infinity, or extremely large non-IEEE representation.
	if (value > LDBL_MAX || exponent > LDBL_MAX_EXP + LDBL_MAX_EXP - 1 + 8) {
		if (negative) {
			// This can only happen with a non-IEEE representation, because
			// we've already tested for value < -LDBL_MAX
			return 0;
		}
		memset(buf, '\xff', 18);
		return 18;
	}

	// Encoding:
	//
	// [ 7 | 6 | 5 | 4 3 2 1 0]
	//   Sm  Se  Le
	//
	// Sm stores the sign of the mantissa: 1 = positive or zero, 0 = negative.
	// Se stores the sign of the exponent: Sm for positive/zero, !Sm for neg.
	// Le stores the length of the exponent: !Se for 7 bits, Se for 15 bits.
	unsigned char next = (negative ? 0 : 0xe0);

	// Bias the exponent by 8 so that more small integers get short encodings.
	exponent -= 8;
	bool exponent_negative = (exponent < 0);
	if (exponent_negative) {
		exponent = -exponent;
		next ^= 0x60;
	}

	size_t len = 0;

	/* We store the exponent in 7 or 15 bits.  If the number is negative, we
	 * flip all the bits of the exponent, since larger negative numbers should
	 * sort first.
	 *
	 * If the exponent is negative, we flip the bits of the exponent, since
	 * larger negative exponents should sort first (unless the number is
	 * negative, in which case they should sort later).
	 */

	ASSERT(exponent >= 0);
	if (exponent < 128) {
		next ^= 0x20;
		// Put the top 5 bits of the exponent into the lower 5 bits of the
		// first byte:
		next |= static_cast<unsigned char>(exponent >> 2);
		if (negative ^ exponent_negative) { next ^= 0x1f; }
		buf[len++] = next;

		// And the lower 2 bits of the exponent go into the upper 2 bits
		// of the second byte:
		next = static_cast<unsigned char>(exponent) << 6;
		if (negative ^ exponent_negative) { next ^= 0xc0; }

	} else {
		ASSERT((exponent >> 15) == 0);
		// Put the top 5 bits of the exponent into the lower 5 bits of the
		// first byte:
		next |= static_cast<unsigned char>(exponent >> 10);
		if (negative ^ exponent_negative) { next ^= 0x1f; }
		buf[len++] = next;

		// Put the bits 3-10 of the exponent into the second byte:
		next = static_cast<unsigned char>(exponent >> 2);
		if (negative ^ exponent_negative) { next ^= 0xff; }
		buf[len++] = next;

		// And the lower 2 bits of the exponent go into the upper 2 bits
		// of the third byte:
		next = static_cast<unsigned char>(exponent) << 6;
		if (negative ^ exponent_negative) { next ^= 0xc0; }
	}

	// Convert the 112 (or 113) bits of the mantissa into two 32-bit words.

	mantissa *= (negative ? 1073741824.0 : 2147483648.0);  // 1<<30 : 1<<31
	auto word1 = static_cast<unsigned>(mantissa);
	mantissa -= word1;

	mantissa *= 4294967296.0L;  // 1<<32
	auto word2 = static_cast<unsigned>(mantissa);
	mantissa -= word2;

	mantissa *= 4294967296.0L;  // 1<<32
	auto word3 = static_cast<unsigned>(mantissa);
	mantissa -= word3;

	mantissa *= 4294967296.0L;  // 1<<32
	auto word4 = static_cast<unsigned>(mantissa);

	// If the number is positive, the first bit will always be set because 0.5
	// <= mantissa < 1, unless mantissa is zero, which we handle specially
	// above).  If the number is negative, we negate the mantissa instead of
	// flipping all the bits, so in the case of 0.5, the first bit isn't set
	// so we need to store it explicitly.  But for the cost of one extra
	// leading bit, we can save several trailing 0xff bytes in lots of common
	// cases.

	ASSERT(negative || (word1 & (1<<30)));
	if (negative) {
		// We negate the mantissa for negative numbers, so that the sort order
		// is reversed (since larger negative numbers should come first).
		word1 = -word1;
		if (word2 != 0 || word3 != 0 || word4 != 0) { ++word1; }
		word2 = -word2;
		if (word3 != 0 || word4 != 0) { ++word2; }
		word3 = -word3;
		if (word4 != 0) { ++word3; }
		word4 = -word4;
	}

	word1 &= 0x3fffffff;
	next |= static_cast<unsigned char>(word1 >> 24);
	buf[len++] = next;

	if (word1 != 0) {
		buf[len++] = char(word1 >> 16);
		buf[len++] = char(word1 >> 8);
		buf[len++] = char(word1);
	}

	if (word2 != 0 || word3 != 0 || word4 != 0) {
		buf[len++] = char(word2 >> 24);
		buf[len++] = char(word2 >> 16);
		buf[len++] = char(word2 >> 8);
		buf[len++] = char(word2);
	}

	if (word3 != 0 || word4 != 0) {
		buf[len++] = char(word3 >> 24);
		buf[len++] = char(word3 >> 16);
		buf[len++] = char(word3 >> 8);
		buf[len++] = char(word3);
	}

	if (word4 != 0) {
		buf[len++] = char(word4 >> 24);
		buf[len++] = char(word4 >> 16);
		buf[len++] = char(word4 >> 8);
		buf[len++] = char(word4);
	}

	// Finally, we can chop off any trailing zero bytes.
	while (len > 0 && buf[len - 1] == '\0') {
		--len;
	}

	return len;
}


std::string sortable_serialise(long double value) {
	char buf[18];
	return std::string(buf, sortable_serialise_(value, buf));
}


/// Get a number from the character at a given position in a string, returning
/// 0 if the string isn't long enough.
static inline unsigned char
numfromstr(std::string_view str, std::size_t pos)
{
	return (pos < str.size()) ? static_cast<unsigned char>(str[pos]) : '\0';
}

long double
sortable_unserialise(std::string_view value)
{
	// Zero.
	if (value.size() == 1 && value[0] == '\x80') { return 0.0; }

	// Positive infinity.
	if (value.size() == 18 &&
		memcmp(value.data(), "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 18) == 0  // NOLINT
	) {
		#ifdef INFINITY
			// INFINITY is C99.  Oddly, it's of type "float" so sanity check in
			// case it doesn't cast to double as infinity (apparently some
			// implementations have this problem).
			if ((long double)(INFINITY) > HUGE_VALL) { return INFINITY; }
		#endif
		return HUGE_VALL;
	}

	// Negative infinity.
	if (value.empty()) {
		#ifdef INFINITY
			if ((long double)(INFINITY) > HUGE_VALL) { return -INFINITY; }
		#endif
		return -HUGE_VALL;
	}

	unsigned char first = numfromstr(value, 0);
	size_t i = 0;

	first ^= static_cast<unsigned char>(first & 0xc0) >> 1;
	bool negative = (first & 0x80) == 0;
	bool exponent_negative = (first & 0x40) != 0;
	bool explen = (first & 0x20) == 0;
	int exponent = first & 0x1f;
	if (!explen) {
		first = numfromstr(value, ++i);
		exponent <<= 2;
		exponent |= (first >> 6);
		if (negative ^ exponent_negative) { exponent ^= 0x07f; }
	} else {
		first = numfromstr(value, ++i);
		exponent <<= 8;
		exponent |= first;
		first = numfromstr(value, ++i);
		exponent <<= 2;
		exponent |= (first >> 6);
		if (negative ^ exponent_negative) { exponent ^= 0x7fff; }
	}

	unsigned word1;
	word1 = (unsigned(first & 0x3f) << 24);
	word1 |= numfromstr(value, ++i) << 16;
	word1 |= numfromstr(value, ++i) << 8;
	word1 |= numfromstr(value, ++i);

	unsigned word2 = 0;
	if (i < value.size()) {
		word2 = numfromstr(value, ++i) << 24;
		word2 |= numfromstr(value, ++i) << 16;
		word2 |= numfromstr(value, ++i) << 8;
		word2 |= numfromstr(value, ++i);
	}

	unsigned word3 = 0;
	if (i < value.size()) {
		word3 = numfromstr(value, ++i) << 24;
		word3 |= numfromstr(value, ++i) << 16;
		word3 |= numfromstr(value, ++i) << 8;
		word3 |= numfromstr(value, ++i);
	}

	unsigned word4 = 0;
	if (i < value.size()) {
		word4 = numfromstr(value, ++i) << 24;
		word4 |= numfromstr(value, ++i) << 16;
		word4 |= numfromstr(value, ++i) << 8;
		word4 |= numfromstr(value, ++i);
	}

	if (negative) {
		word1 = -word1;
		if (word2 != 0 || word3 != 0 || word4 != 0) { ++word1; }
		word2 = -word2;
		if (word3 != 0 || word4 != 0) { ++word2; }
		word3 = -word3;
		if (word4 != 0) { ++word3; }
		word4 = -word4;
		ASSERT((word1 & 0xf0000000) != 0);
		word1 &= 0x3fffffff;
	}
	if (!negative) { word1 |= 1<<30; }

	long double mantissa = 0;
	if (word4 != 0u) { mantissa += word4 / 79228162514264337593543950336.0L; }  // 1<<96
	if (word3 != 0u) { mantissa += word3 / 18446744073709551616.0L; }  // 1<<64
	if (word2 != 0u) { mantissa += word2 / 4294967296.0L; }  // 1<<32
	if (word1 != 0u) { mantissa += word1; }
	mantissa /= (negative ? 1073741824.0 : 2147483648.0);  // 1<<30 : 1<<31

	if (exponent_negative) { exponent = -exponent; }
	exponent += 8;

	if (negative) { mantissa = -mantissa; }

	// We use scalbnl() since it's equivalent to ldexp() when FLT_RADIX == 2
	// (which we currently assume), except that ldexp() will set errno if the
	// result overflows or underflows, which isn't really desirable here.
	return scalbnl(mantissa, exponent);
}
