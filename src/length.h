/*
 * Copyright (C) 2006,2007,2008,2009,2010,2011,2012 Olly Betts
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

#include "xapian.h"

#include "exception.h"
#include "xapiand.h"

#include <assert.h>
#include <string>

/** Serialise a length as a variable-length string.
 *
 *  The encoding specifies its own length.
 *
 *  @param len	The length to encode.
 *
 *  @return	The encoded length.
 */
std::string serialise_length(unsigned long long len);

/** Unserialise a length encoded by serialise_length.
 *
 *  @param p	Pointer to a pointer to the string, which will be advanced past
 *		the encoded length.
 *  @param end	Pointer to the end of the string.
 *  @param check_remaining	Check the result against the amount of data
 *				remaining after the length has been decoded.
 *
 *  @return	The decoded length.
 */
unsigned long long unserialise_length(const char** p, const char* end, bool check_remaining=false);

std::string serialise_string(const std::string &input);

std::string unserialise_string(const char** p, const char* end);

std::string serialise_double(double v);

double unserialise_double(const char** p, const char* end);

/** Append an encoded unsigned integer to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template<class U>
inline void
serialise_unsigned(std::string & s, U value)
{
	// Check U is an unsigned type.
	static_assert(static_cast<U>(-1) > 0, "Type not unsigned");

	while (value >= 128) {
		s += static_cast<char>(static_cast<unsigned char>(value) | 0x80);
		value >>= 7;
	}
	s += static_cast<char>(value);
}

/** Decode an unsigned integer from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result (or NULL to just skip it).
 */
template<class U>
inline void
unserialise_unsigned(const char ** p, const char * end, U * result)
{
	// Check U is an unsigned type.
	static_assert(static_cast<U>(-1) > 0, "Type not unsigned");

	const char * ptr = *p;
	assert(ptr);
	const char * start = ptr;

	// Check the length of the encoded integer first.
	do {
		if unlikely(ptr == end) {
			// Out of data.
			*p = NULL;
			throw MSG_SerialisationError("Bad encoded unsigned: no data");
		}
	} while (static_cast<unsigned char>(*ptr++) >= 128);

	*p = ptr;

	if (!result) return;

	*result = U(*--ptr);
	if (ptr == start) {
		// Special case for small values.
		return;
	}

	size_t maxbits = size_t(ptr - start) * 7;
	if (maxbits <= sizeof(U) * 8) {
		// No possibility of overflow.
		do {
			unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
			*result = (*result << 7) | U(chunk);
		} while (ptr != start);
		return;
	}

	size_t minbits = maxbits - 6;
	if unlikely(minbits > sizeof(U) * 8) {
		// Overflow.
		throw MSG_SerialisationError("Bad encoded unsigned: overflow");
	}

	while (--ptr != start) {
		unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
		*result = (*result << 7) | U(chunk);
	}

	U tmp = *result;
	*result <<= 7;
	if unlikely(*result < tmp) {
		// Overflow.
		throw MSG_SerialisationError("Bad encoded unsigned: overflow");;
	}
	*result |= U(static_cast<unsigned char>(*ptr) & 0x7f);
	return;
}
