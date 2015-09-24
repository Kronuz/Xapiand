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

#include <string>

/** Serialise a length as a variable-length string.
 *
 *  The encoding specifies its own length.
 *
 *  @param len	The length to encode.
 *
 *  @return	The encoded length.
 */
std::string serialise_length(size_t len);

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
ssize_t unserialise_length(const char **p, const char *end, bool check_remaining=false);


std::string serialise_string(const std::string &input);

ssize_t unserialise_string(std::string &output, const char **p, const char *end);


#define encode_length serialise_length
#define decode_length unserialise_length
