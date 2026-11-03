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


// Fuzz target for the index patterns a search names: the first bytes split the input into a pattern and a name.
// index_pattern::match must agree with a reference that tabulates every prefix pair (the cost of the matcher is
// bounded by the pattern's size times the name's, so a long pattern is capped as the host caps it), and must never
// read past either end. A pattern with no star is the name, byte for byte.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "index_pattern.h"

namespace {

bool reference(std::string_view pattern, std::string_view name) {
	std::vector<std::vector<char>> ok(pattern.size() + 1, std::vector<char>(name.size() + 1, 0));
	ok[0][0] = 1;
	for (std::size_t p = 1; p <= pattern.size(); ++p) {
		if (pattern[p - 1] == '*') { ok[p][0] = ok[p - 1][0]; }
		for (std::size_t n = 1; n <= name.size(); ++n) {
			ok[p][n] = pattern[p - 1] == '*' ? (ok[p - 1][n] || ok[p][n - 1]) : (ok[p - 1][n - 1] && pattern[p - 1] == name[n - 1]);
		}
	}
	return ok[pattern.size()][name.size()] != 0;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if (size == 0) {
		return 0;
	}
	const std::size_t cut = std::min<std::size_t>(data[0], index_pattern::max_pattern_size);
	const std::string_view rest(reinterpret_cast<const char*>(data) + 1, size - 1);
	const auto pattern = rest.substr(0, cut);
	const auto name = rest.substr(pattern.size());
	if (name.size() > 512) {
		return 0;
	}
	const bool got = index_pattern::match(pattern, name);
	if (got != reference(pattern, name)) {
		std::fprintf(stderr, "match(%.*s, %.*s) = %d\n", static_cast<int>(pattern.size()), pattern.data(), static_cast<int>(name.size()), name.data(), got);
		std::abort();
	}
	if (pattern.find('*') == std::string_view::npos && got != (pattern == name)) {
		std::abort();
	}
	(void)index_pattern::is_wildcard(pattern);
	return 0;
}
