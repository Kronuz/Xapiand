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

#include "engine/strata/expand.h"

#include <algorithm>               // for std::min, std::sort, std::stable_sort
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <set>                     // for std::set
#include <utility>                 // for std::move

#include "prospect/reader.hh"      // for prospect::inverted_plugin
#include "unicode.hh"              // for unicode::next_utf8

#include "engine/error.h"          // for Error, errc


namespace engine::strata {

namespace {

std::u32string
decode(std::string_view text)
{
	std::u32string out;
	const char* p = text.data();
	const char* const end = p + text.size();
	while (p < end) {
		out.push_back(unicode::next_utf8(p, end));
	}
	return out;
}

bool
is_special(char c, unsigned flags)
{
	return (c == '?' && (flags & PATTERN_SINGLE)) || (c == '*' && (flags & PATTERN_MULTI));
}

// The bytes of the first `chars` characters of the text (all of it when it has fewer).
std::string_view
head(std::string_view text, std::size_t chars)
{
	const char* p = text.data();
	const char* const end = p + text.size();
	while (chars-- > 0 && p < end) {
		(void)unicode::next_utf8(p, end);
	}
	return text.substr(0, static_cast<std::size_t>(p - text.data()));
}

}  // namespace


std::string
wildcard_prefix(std::string_view pattern, unsigned pattern_flags)
{
	if (!(pattern_flags & (PATTERN_SINGLE | PATTERN_MULTI))) {
		return std::string(pattern);
	}
	std::size_t n = 0;
	while (n < pattern.size() && !is_special(pattern[n], pattern_flags)) {
		++n;
	}
	return std::string(pattern.substr(0, n));
}


bool
wildcard_matches(std::string_view pattern, std::string_view term, unsigned pattern_flags)
{
	if (!(pattern_flags & (PATTERN_SINGLE | PATTERN_MULTI))) {
		return term.starts_with(pattern);
	}
	const auto p = decode(pattern);
	const auto t = decode(term);
	const auto single = [&](char32_t c) { return c == U'?' && (pattern_flags & PATTERN_SINGLE); };
	const auto multi = [&](char32_t c) { return c == U'*' && (pattern_flags & PATTERN_MULTI); };
	// Greedy match with one backtrack point: the last '*' seen.
	std::size_t i = 0, j = 0, star = std::u32string::npos, mark = 0;
	while (j < t.size()) {
		if (i < p.size() && multi(p[i])) {
			star = i++;
			mark = j;
		} else if (i < p.size() && (single(p[i]) || p[i] == t[j])) {
			++i;
			++j;
		} else if (star != std::u32string::npos) {
			i = star + 1;
			j = ++mark;
		} else {
			return false;
		}
	}
	while (i < p.size() && multi(p[i])) {
		++i;
	}
	return i == p.size();
}


bool
within_edit_distance(std::string_view a, std::string_view b, unsigned distance)
{
	const auto x = decode(a);
	const auto y = decode(b);
	const std::size_t n = x.size(), m = y.size();
	if ((n > m ? n - m : m - n) > distance) {
		return false;
	}
	// Optimal string alignment: rows i-2, i-1 and i of the edit table.
	std::vector<unsigned> older(m + 1), previous(m + 1), row(m + 1);
	for (std::size_t j = 0; j <= m; ++j) {
		previous[j] = static_cast<unsigned>(j);
	}
	for (std::size_t i = 1; i <= n; ++i) {
		row[0] = static_cast<unsigned>(i);
		unsigned best = row[0];
		for (std::size_t j = 1; j <= m; ++j) {
			const unsigned cost = x[i - 1] == y[j - 1] ? 0 : 1;
			unsigned value = std::min({previous[j] + 1, row[j - 1] + 1, previous[j - 1] + cost});
			if (i > 1 && j > 1 && x[i - 1] == y[j - 2] && x[i - 2] == y[j - 1]) {
				value = std::min(value, older[j - 2] + 1);
			}
			row[j] = value;
			best = std::min(best, value);
		}
		if (best > distance) {
			return false;
		}
		older.swap(previous);
		previous.swap(row);
	}
	return previous[m] <= distance;
}


std::vector<std::string>
expand(const QueryNode& node, std::span<const prospect::segment_snapshot> segments, prospect::term_table& terms, std::string_view key_prefix)
{
	const bool wildcard = node.kind == QueryKind::wildcard;
	std::string prefix;
	std::string_view tail;
	if (wildcard) {
		prefix = wildcard_prefix(node.str, node.pattern_flags);
	} else {
		prefix = std::string(head(node.str, node.prefix_length));
		tail = std::string_view(node.str).substr(prefix.size());
	}
	if (!key_prefix.empty() && prefix.starts_with(key_prefix)) {
		throw Error(errc::syntax, "The strata engine does not expand terms of the key field");
	}

	std::set<std::string> found;
	for (const auto& snapshot : segments) {
		snapshot.seg->get<prospect::inverted_plugin>().scan_dictionary(prefix, [&](std::string_view key, std::string_view) {
			if (!key.starts_with(prefix)) {
				return false;
			}
			if (wildcard ? wildcard_matches(node.str, key, node.pattern_flags) : within_edit_distance(tail, key.substr(prefix.size()), node.distance)) {
				found.emplace(key);
			}
			return true;
		});
	}

	std::vector<std::string> result(found.begin(), found.end());
	const std::size_t limit = node.parameter;
	if (limit != 0 && result.size() > limit) {
		switch (node.limit) {
			case Expansion::error:
				throw Error(errc::invalid, "The pattern \"" + node.str + "\" expands to more than " + std::to_string(limit) + " terms");
			case Expansion::first:
				result.resize(limit);
				break;
			case Expansion::most_frequent: {
				std::vector<std::pair<std::uint64_t, std::size_t>> by_frequency;
				by_frequency.reserve(result.size());
				for (std::size_t i = 0; i < result.size(); ++i) {
					by_frequency.emplace_back(terms.df(result[i]), i);
				}
				std::stable_sort(by_frequency.begin(), by_frequency.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
				by_frequency.resize(limit);
				std::vector<std::size_t> keep;
				for (const auto& entry : by_frequency) {
					keep.push_back(entry.second);
				}
				std::sort(keep.begin(), keep.end());
				std::vector<std::string> kept;
				for (const auto i : keep) {
					kept.push_back(std::move(result[i]));
				}
				result = std::move(kept);
				break;
			}
		}
	}
	return result;
}

}  // namespace engine::strata
