/*
 * Copyright (C) 2006,2007,2008,2009,2010,2011,2012 Olly Betts
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

#include <stddef.h>               // for size_t, NULL
#include <string>                 // for string
#include "string_view.hh"         // for std::string_view

#include "cassert.hh"            // for assert

#include "exception.h"           // for MSG_SerialisationError, SerialisationError
#include "likely.h"              // for likely, unlikely


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

std::string serialise_string(std::string_view input);

std::string_view unserialise_string(const char** p, const char* end);

void serialise_length(int fd, unsigned long long len);

unsigned long long unserialise_length(int fd, std::string &buffer, std::size_t& off, std::size_t& acc);

void serialise_string(int fd, std::string_view input);

std::string unserialise_string(int fd, std::string &buffer, std::size_t& off, std::size_t& acc);

void serialise_char(int fd, char ch);

char unserialise_char(int fd, std::string &buffer, std::size_t& off, std::size_t& acc);

std::string serialise_strings(const std::vector<std::string_view>& strings);

std::string_view unserialise_string_at(size_t at, const char** p, const char* end);

std::string serialise_double(double v);

double unserialise_double(const char** p, const char* end);


/** Append an encoded unsigned integer to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template<class T>
inline void serialise_unsigned(std::string & s, T value)
{
	// Check T is an unsigned type.
	static_assert(static_cast<T>(-1) > 0, "Type not unsigned");

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
template<class T>
inline void unserialise_unsigned(const char** p, const char* end, T* result) {
	// Check T is an unsigned type.
	static_assert(static_cast<T>(-1) > 0, "Type not unsigned");

	const char * ptr = *p;
	assert(ptr);
	assert(ptr <= end);

	const char * start = ptr;

	// Check the length of the encoded integer first.
	do {
		if unlikely(ptr == end) {
			// Out of data.
			*p = NULL;
			THROW(SerialisationError, "Bad encoded unsigned: no data");
		}
	} while (static_cast<unsigned char>(*ptr++) >= 128);

	*p = ptr;

	if (!result) return;

	*result = T(*--ptr);
	if (ptr == start) {
		// Special case for small values.
		return;
	}

	size_t maxbits = size_t(ptr - start) * 7;
	if (maxbits <= sizeof(T) * 8) {
		// No possibility of overflow.
		do {
			unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
			*result = (*result << 7) | T(chunk);
		} while (ptr != start);
		return;
	}

	size_t minbits = maxbits - 6;
	if unlikely(minbits > sizeof(T) * 8) {
		// Overflow.
		THROW(SerialisationError, "Bad encoded unsigned: overflow");
	}

	while (--ptr != start) {
		unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
		*result = (*result << 7) | T(chunk);
	}

	T tmp = *result;
	*result <<= 7;
	if unlikely(*result < tmp) {
		// Overflow.
		THROW(SerialisationError, "Bad encoded unsigned: overflow");;
	}
	*result |= T(static_cast<unsigned char>(*ptr) & 0x7f);
	return;
}


inline unsigned long long unserialise_length(std::string_view data, bool check_remaining=false) {
	const char *p = data.data();
	const char *p_end = p + data.size();
	return unserialise_length(&p, p_end, check_remaining);
}


inline std::string_view unserialise_string(std::string_view data) {
	const char *p = data.data();
	const char *p_end = p + data.size();
	return unserialise_string(&p, p_end);
}


inline std::string_view unserialise_string_at(size_t at, std::string_view data) {
	const char *p = data.data();
	const char *p_end = p + data.size();
	return unserialise_string_at(at, &p, p_end);
}


inline double unserialise_double(std::string_view data) {
	const char *p = data.data();
	const char *p_end = p + data.size();
	return unserialise_double(&p, p_end);
}


template<class T>
inline void unserialise_unsigned(std::string_view data, T* result)
{
	const char *p = data.data();
	const char *p_end = p + data.size();
	unserialise_unsigned(&p, p_end, result);
}


template<class T>
inline T unserialise_unsigned(std::string_view data)
{
	T result;
	const char *p = data.data();
	const char *p_end = p + data.size();
	unserialise_unsigned(&p, p_end, &result);
	return result;
}
