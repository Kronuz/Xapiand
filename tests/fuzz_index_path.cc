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

// Fuzz target for the bytes an index path may hold: any bytes. index_path::valid must agree with a
// reference that decodes each character to its value and checks the value (shortest form, not a
// surrogate, at most U+10FFFF, no NUL, control byte, DEL or backslash), and must never read past the end.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "index_path.h"

namespace {

bool reference(std::string_view s) {
	std::size_t i = 0;
	while (i < s.size()) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 0;
		if (len == 0 || i + len > s.size()) {
			return false;
		}
		std::uint32_t cp = len == 1 ? c : c & (0x7f >> len);
		for (std::size_t k = 1; k < len; ++k) {
			const unsigned char cc = static_cast<unsigned char>(s[i + k]);
			if ((cc & 0xc0) != 0x80) {
				return false;
			}
			cp = (cp << 6) | (cc & 0x3f);
		}
		static constexpr std::uint32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
		if (len > 1 && cp < least[len]) {
			return false;
		}
		if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
			return false;
		}
		if (cp < 0x20 || cp == 0x7f || cp == '\\') {
			return false;
		}
		i += len;
	}
	return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view s(reinterpret_cast<const char*>(data), size);
	if (index_path::valid(s) != reference(s)) {
		std::fprintf(stderr, "fuzz_index_path: valid() and the reference disagree on %zu bytes\n", size);
		std::abort();
	}
	// A NUL anywhere refuses the path.
	if (s.find('\0') != std::string_view::npos && index_path::valid(s)) {
		std::abort();
	}
	return 0;
}
