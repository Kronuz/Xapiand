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

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The index names a search may give as a pattern: `_all` is every index, and `*` in a name stands for any run of
// bytes (none, or several, `/` included). An item that starts with `-` removes the names it matches from the
// result wherever it stands in the list. A name that starts with `.` (the node's own indexes) is never
// listed by a pattern; it is searched by naming it.
namespace index_pattern {

// The longest pattern: the match backtracks over the name, so its cost is the pattern's length times the name's.
inline constexpr std::size_t max_pattern_size = 256;

inline bool
is_wildcard(std::string_view item) noexcept
{
	return item == "_all" || item.find('*') != std::string_view::npos;
}

// Whether `name` is one `pattern` stands for.
inline bool
match(std::string_view pattern, std::string_view name) noexcept
{
	constexpr auto none = std::string_view::npos;
	std::size_t p = 0, n = 0, star = none, mark = 0;
	while (n < name.size()) {
		if (p < pattern.size() && pattern[p] == '*') {
			star = p++;
			mark = n;
		} else if (p < pattern.size() && pattern[p] == name[n]) {
			++p;
			++n;
		} else if (star != none) {
			p = star + 1;
			n = ++mark;
		} else {
			return false;
		}
	}
	while (p < pattern.size() && pattern[p] == '*') {
		++p;
	}
	return p == pattern.size();
}

// The indexes `items` name under `root` (the node's data directory, ending in `/`): a plain item stands for itself,
// `_all` and a pattern for the indexes on disk it matches (an index is a directory holding `strata` or a shard
// `.__N`; one that holds neither is searched for indexes below it), `-item` removes names. The names are sorted
// and have no repeats. It reads the file system and blocks, so it runs off a reactor. Raises errc::invalid
// (a 400) when a pattern is longer than `max_pattern_size` or the names pass `limit`.
std::vector<std::string> expand(const std::string& root, std::span<const std::string> items, std::size_t limit);

}  // namespace index_pattern
