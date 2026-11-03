/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <string_view>

// The bytes an index path may hold. A name is well-formed UTF-8 (no overlong forms, surrogates or
// values above U+10FFFF) without control bytes, NUL or backslash: the system calls that act on the
// path read it as a C string, so a NUL would hide the rest of it from the check, and the storage
// layer cannot round-trip the others.
namespace index_path {

inline bool
valid(std::string_view path) noexcept
{
	const auto* p = reinterpret_cast<const unsigned char*>(path.data());
	const auto* const end = p + path.size();
	while (p != end) {
		const unsigned c = *p++;
		if (c < 0x80) {
			if (c < 0x20 || c == 0x7f || c == '\\') {
				return false;
			}
			continue;
		}
		// The continuation count, and the range of the first continuation byte that excludes
		// overlong forms (E0, F0), surrogates (ED) and values above U+10FFFF (F4).
		unsigned more;
		unsigned lo = 0x80, hi = 0xbf;
		if (c >= 0xc2 && c <= 0xdf) {
			more = 1;
		} else if (c >= 0xe0 && c <= 0xef) {
			more = 2;
			if (c == 0xe0) { lo = 0xa0; }
			if (c == 0xed) { hi = 0x9f; }
		} else if (c >= 0xf0 && c <= 0xf4) {
			more = 3;
			if (c == 0xf0) { lo = 0x90; }
			if (c == 0xf4) { hi = 0x8f; }
		} else {
			return false;
		}
		for (; more != 0; --more, lo = 0x80, hi = 0xbf) {
			if (p == end || *p < lo || *p > hi) {
				return false;
			}
			++p;
		}
	}
	return true;
}

}  // namespace index_path
