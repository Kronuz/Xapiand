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
// index_pattern::match must agree with a reference that tries every way a pattern can match (a table over the bytes of a
// segment, `*`, `?` and `[...]`; the segments of a path against the pattern's, with `**` for any number of them), and
// must never read past either end. The automaton the walk carries down a tree (Glob) must keep a path's prefix live while
// the path matches, and accept exactly when the whole path does. A pattern with no glob character is the name, byte for
// byte. The pattern is capped as the host caps it.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "index_pattern.h"

namespace {

// The length of the character at name[i]: a well-formed UTF-8 sequence, else one byte.
std::size_t chr(std::string_view s, std::size_t i) {
	const unsigned char c = s[i];
	std::size_t len = 1;
	if (c >= 0xc2 && c <= 0xdf) { len = 2; }
	else if (c >= 0xe0 && c <= 0xef) { len = 3; }
	else if (c >= 0xf0 && c <= 0xf4) { len = 4; }
	if (len == 1 || i + len > s.size()) { return 1; }
	for (std::size_t k = 1; k < len; ++k) {
		if ((static_cast<unsigned char>(s[i + k]) >> 6) != 2) { return 1; }
	}
	return len;
}

std::uint32_t point(std::string_view s, std::size_t i) {
	const auto len = chr(s, i);
	std::uint32_t cp = static_cast<unsigned char>(s[i]);
	if (len == 1) { return cp; }
	cp &= (len == 2 ? 0x1f : len == 3 ? 0x0f : 0x07);
	for (std::size_t k = 1; k < len; ++k) {
		cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3f);
	}
	return cp;
}

struct Token {
	char kind;   // '*', '?', '[' (a class) or 'l' (a byte)
	char byte = 0;
	bool negated = false;
	std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
};

std::vector<Token> tokens(std::string_view seg) {
	std::vector<Token> out;
	for (std::size_t i = 0; i < seg.size();) {
		const char c = seg[i];
		if (c == '*' || c == '?') {
			out.push_back({c});
			++i;
		} else if (c == '[') {
			std::size_t j = i + 1;   // a class closes at the first `]` after its first member
			if (j < seg.size() && (seg[j] == '!' || seg[j] == '^')) { ++j; }
			if (j < seg.size() && seg[j] == ']') { ++j; }
			const auto close = seg.find(']', j);
			if (close == std::string_view::npos) {
				out.push_back({'l', c});
				++i;
				continue;
			}
			Token token{'['};
			std::size_t k = i + 1;
			if (seg[k] == '!' || seg[k] == '^') { token.negated = true; ++k; }
			while (k < close) {
				const auto lo = point(seg, k);
				auto hi = lo;
				k += chr(seg, k);
				if (k + 1 < close && seg[k] == '-') {
					hi = point(seg, k + 1);
					k += 1 + chr(seg, k + 1);
				}
				token.ranges.emplace_back(lo, hi);
			}
			out.push_back(std::move(token));
			i = close + 1;
		} else {
			out.push_back({'l', c});
			++i;
		}
	}
	return out;
}

bool in(const Token& t, std::uint32_t cp) {
	bool found = false;
	for (const auto& [lo, hi] : t.ranges) { found = found || (lo <= cp && cp <= hi); }
	return found != t.negated;
}

// Whether the segment `name` is one the segment pattern `seg` stands for: every way tried.
bool segment(std::string_view seg, std::string_view name) {
	const auto toks = tokens(seg);
	std::vector<std::vector<char>> ok(toks.size() + 1, std::vector<char>(name.size() + 1, 0));
	ok[0][0] = 1;
	for (std::size_t p = 0; p < toks.size(); ++p) {
		for (std::size_t n = 0; n <= name.size(); ++n) {
			if (!ok[p][n]) { continue; }
			const auto& t = toks[p];
			if (t.kind == '*') {
				ok[p + 1][n] = 1;
				if (n < name.size()) { ok[p][n + chr(name, n)] = 1; }   // the star eats a character and stays
			} else if (n < name.size()) {
				if (t.kind == 'l') {
					if (name[n] == t.byte) { ok[p + 1][n + 1] = 1; }
				} else if (t.kind == '?' || in(t, point(name, n))) {
					ok[p + 1][n + chr(name, n)] = 1;
				}
			}
		}
	}
	return ok[toks.size()][name.size()] != 0;
}

std::vector<std::string_view> split(std::string_view s) {
	std::vector<std::string_view> out;
	for (std::size_t from = 0;;) {
		const auto cut = s.find('/', from);
		out.push_back(s.substr(from, cut == std::string_view::npos ? cut : cut - from));
		if (cut == std::string_view::npos) { return out; }
		from = cut + 1;
	}
}

bool reference(std::string_view pattern, std::string_view name) {
	const auto pats = split(pattern);
	const auto segs = split(name);
	std::map<std::pair<std::size_t, std::size_t>, bool> memo;
	const auto go = [&](auto&& self, std::size_t i, std::size_t j) -> bool {
		if (i == pats.size()) { return j == segs.size(); }
		const auto key = std::make_pair(i, j);
		if (const auto it = memo.find(key); it != memo.end()) { return it->second; }
		bool result;
		if (pats[i] == "**") {
			result = self(self, i + 1, j) || (j < segs.size() && self(self, i, j + 1));
		} else {
			result = j < segs.size() && segment(pats[i], segs[j]) && self(self, i + 1, j + 1);
		}
		return memo[key] = result;
	};
	return go(go, 0, 0);
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
	if (pattern.find_first_of("*?[") == std::string_view::npos && got != (pattern == name)) {
		std::abort();
	}
	// The automaton: a prefix of a matching path stays live, and the last step accepts exactly when the path matches.
	const index_pattern::Glob glob(pattern);
	std::size_t steps = 0;
	auto states = glob.start();
	const auto segments = split(name);
	for (std::size_t i = 0; i < segments.size(); ++i) {
		states = glob.step(states, segments[i], steps);
		if (got && i + 1 < segments.size() && !glob.live(states)) {
			std::fprintf(stderr, "pruned a prefix of a match: %.*s / %.*s\n", static_cast<int>(pattern.size()), pattern.data(), static_cast<int>(name.size()), name.data());
			std::abort();
		}
	}
	if (glob.accepts(states) != got) {
		std::abort();
	}
	if (steps > (pattern.size() + 2) * (name.size() + 2) * 8 + 64) {   // the work is the pattern's size times the name's
		std::fprintf(stderr, "too many steps: %zu\n", steps);
		std::abort();
	}
	(void)index_pattern::is_wildcard(pattern);
	return 0;
}
