/*
 * Copyright (c) 2026 Dubalu LLC
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

// Fuzz target for the string tests behind wildcard and edit-distance nodes: any bytes for the
// pattern and the term, any flags and distance. The matchers must agree with plain recursive
// references over code points, never read out of bounds, and keep their algebra (the distance is
// symmetric, grows with its bound, and zero only for equal strings).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "unicode.hh"

#include "engine/query.h"
#include "engine/strata/pattern.h"

namespace {

[[noreturn]] void fail(int line) {
	std::fprintf(stderr, "fuzz_strata_pattern: check at line %d failed\n", line);
	std::abort();
}

std::u32string decode(std::string_view text) {
	std::u32string out;
	const char* p = text.data();
	const char* const end = p + text.size();
	while (p < end) {
		out.push_back(unicode::next_utf8(p, end));
	}
	return out;
}

// Whether the bytes are well-formed UTF-8: they re-encode to themselves from the characters read.
bool well_formed(std::string_view text) {
	std::string again;
	for (const char32_t c : decode(text)) {
		unicode::append_utf8(again, c);
	}
	return again == text;
}

bool match_reference(const std::u32string& p, std::size_t i, const std::u32string& t, std::size_t j, unsigned flags) {
	if (i == p.size()) {
		return j == t.size();
	}
	if (p[i] == U'*' && (flags & engine::PATTERN_MULTI)) {
		for (std::size_t k = j; k <= t.size(); ++k) {
			if (match_reference(p, i + 1, t, k, flags)) {
				return true;
			}
		}
		return false;
	}
	if (j == t.size()) {
		return false;
	}
	if ((p[i] == U'?' && (flags & engine::PATTERN_SINGLE)) || p[i] == t[j]) {
		return match_reference(p, i + 1, t, j + 1, flags);
	}
	return false;
}

// Optimal string alignment over the whole table.
unsigned distance_reference(const std::u32string& a, const std::u32string& b) {
	std::vector<std::vector<unsigned>> d(a.size() + 1, std::vector<unsigned>(b.size() + 1));
	for (std::size_t i = 0; i <= a.size(); ++i) {
		d[i][0] = static_cast<unsigned>(i);
	}
	for (std::size_t j = 0; j <= b.size(); ++j) {
		d[0][j] = static_cast<unsigned>(j);
	}
	for (std::size_t i = 1; i <= a.size(); ++i) {
		for (std::size_t j = 1; j <= b.size(); ++j) {
			unsigned v = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0u : 1u)});
			if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
				v = std::min(v, d[i - 2][j - 2] + 1);
			}
			d[i][j] = v;
		}
	}
	return d[a.size()][b.size()];
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if (size < 3 || size > 200) {
		return 0;
	}
	const unsigned flags = data[0] & 3;
	const unsigned distance = data[1] % 5;
	const std::string_view rest(reinterpret_cast<const char*>(data) + 2, size - 2);
	const auto split = rest.find('\xff');
	const auto pattern = rest.substr(0, split);
	const auto term = split == std::string_view::npos ? std::string_view() : rest.substr(split + 1);

	const bool matches = engine::strata::wildcard_matches(pattern, term, flags);
	if (flags == 0) {
		if (matches != term.starts_with(pattern)) {
			fail(__LINE__);
		}
	} else if (std::count(pattern.begin(), pattern.end(), '*') <= 3 && matches != match_reference(decode(pattern), 0, decode(term), 0, flags)) {
		// The reference is exponential in the stars, so only a few are compared.
		fail(__LINE__);
	}
	// Equal characters are equal bytes only in well-formed text, which every term is.
	if (matches && well_formed(pattern) && well_formed(term) && !term.starts_with(engine::strata::wildcard_prefix(pattern, flags))) {
		fail(__LINE__);
	}

	const unsigned exact = distance_reference(decode(pattern), decode(term));
	const bool within = engine::strata::within_edit_distance(pattern, term, distance);
	if (within != (exact <= distance)) {
		fail(__LINE__);
	}
	if (within != engine::strata::within_edit_distance(term, pattern, distance)) {
		fail(__LINE__);
	}
	if (within && !engine::strata::within_edit_distance(pattern, term, distance + 1)) {
		fail(__LINE__);
	}
	return 0;
}
