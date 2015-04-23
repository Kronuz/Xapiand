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

#include "length.h"

std::string encode_length(size_t len)
{
	std::string result;
	if (len < 255) {
		result += static_cast<unsigned char>(len);
	} else {
		result += '\xff';
		len -= 255;
		while (true) {
			unsigned char b = static_cast<unsigned char>(len & 0x7f);
			len >>= 7;
			if (!len) {
				result += (b | static_cast<unsigned char>(0x80));
				break;
			}
			result += b;
		}
	}
	return result;
}

size_t
decode_length(const char ** p, const char *end, bool check_remaining)
{
	const char *pos = *p;
	if (pos == end) {
		return -1;
	}
	size_t len = static_cast<unsigned char>(*pos++);
	if (len == 0xff) {
		len = 0;
		unsigned char ch;
		int shift = 0;
		do {
			if (*p == end || shift > 28)
				return -1;
			ch = *pos++;
			len |= size_t(ch & 0x7f) << shift;
			shift += 7;
		} while ((ch & 0x80) == 0);
		len += 255;
	}
	if (check_remaining && len > size_t(end - *p)) {
		return -1;
	}
	*p = pos;
	return len;
}