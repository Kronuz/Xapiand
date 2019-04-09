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

#include "escape.hh"

#include <cassert>               // for assert

#include "chars.hh"              // for chars::char_repr


std::string escape(const void* p, size_t size, char quote) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* q = static_cast<const char *>(p);
	const char *p_end = q + size;
	size = size * 4 + 2;  // Consider "\xNN" and two quotes
	std::string ret;
	ret.resize(size);  // Consider "\xNN" and quotes
	char *buff = &ret[0];
	char *d = buff;
	if (quote == '\1') { quote = '\''; }
	if (quote != '\0') { *d++ = quote; }
	while (q != p_end) {
		unsigned char c = *q++;
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
		// fprintf(stderr, "%02x: %ld < %ld\n", c, (unsigned long)(d - buff), (unsigned long)(size));
	}
	if (quote != '\0') { *d++ = quote; }
	ret.resize(d - buff);
	return ret;
}
