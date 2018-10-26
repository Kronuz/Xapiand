/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "repr.hh"

#include "chars.hh"           // for chars::char_repr


std::string repr(const void* p, size_t size, bool friendly, char quote, size_t max_size) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* q = static_cast<const char *>(p);
	const char *p_end = q + size;
	const char *max_a = max_size != 0u ? q + (max_size * 2 / 3) : p_end + 1;
	const char *max_b = max_size != 0u ? p_end - (max_size / 3) : q - 1;
	if (max_size != 0u) {
		size = ((max_a - q) + (p_end - max_b) - 1) * 4 + 2 + 3;  // Consider "\xNN", two quotes and '...'
	} else {
		size = size * 4 + 2;  // Consider "\xNN" and two quotes
	}
	std::string ret;
	ret.resize(size);
	char *buff = &ret[0];
	char *d = buff;
	if (quote == '\1') { quote = '\''; }
	if (quote != '\0') { *d++ = quote; }
	while (q != p_end) {
		unsigned char c = *q++;
		if (q >= max_a && q <= max_b) {
			if (q == max_a) {
				*d++ = '.';
				*d++ = '.';
				*d++ = '.';
			}
		} else if (friendly) {
			switch (c) {
				// case '\a':
				// 	*d++ = '\\';
				// 	*d++ = 'a';
				// 	break;
				// case '\b':
				// 	*d++ = '\\';
				// 	*d++ = 'b';
				// 	break;
				// case '\f':
				// 	*d++ = '\\';
				// 	*d++ = 'f';
				// 	break;
				// case '\v':
				// 	*d++ = '\\';
				// 	*d++ = 'v';
				// 	break;
				case '\n':
					*d++ = '\\';
					*d++ = 'n';
					break;
				case '\r':
					*d++ = '\\';
					*d++ = 'r';
					break;
				case '\t':
					*d++ = '\\';
					*d++ = 't';
					break;
				case '\\':
					*d++ = '\\';
					*d++ = '\\';
					break;
				default:
					if (quote != '\0' && c == quote) {
						*d++ = '\\';
						*d++ = quote;
					} else if (c < ' ' || c >= 0x7f) {
						*d++ = '\\';
						*d++ = 'x';
						chars::char_repr(c, &d);
					} else {
						*d++ = c;
					}
					break;
			}
		} else {
			*d++ = '\\';
			*d++ = 'x';
			chars::char_repr(c, &d);
		}
		// fprintf(stderr, "%02x: %ld < %ld\n", c, (unsigned long)(d - buff), (unsigned long)(size));
	}
	if (quote != '\0') { *d++ = quote; }
	ret.resize(d - buff);
	return ret;
}
