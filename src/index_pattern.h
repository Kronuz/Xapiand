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
#include <functional>
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

// Whether `name` is one `pattern` stands for. `steps` grows by the work the match did (one per byte of the name it
// walks, so a pattern that backtracks costs more), for a caller that bounds the total.
inline bool
match(std::string_view pattern, std::string_view name, std::size_t& steps) noexcept
{
	constexpr auto none = std::string_view::npos;
	std::size_t p = 0, n = 0, star = none, mark = 0;
	while (n < name.size()) {
		++steps;
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
		++steps;
	}
	return p == pattern.size();
}

inline bool
match(std::string_view pattern, std::string_view name) noexcept
{
	std::size_t steps = 0;
	return match(pattern, name, steps);
}

// The longest list of patterns (wildcards and exclusions together) a search may name. The walk tests every entry it
// reads against every pattern, so this bounds the work per entry.
inline constexpr std::size_t max_patterns = 32;

// What one expansion may spend. `names` and `visits` are both `--max-search-indexes`: `names` bounds the indexes the
// patterns name, and `visits` the directory entries the walk reads to find them, whether or not an entry matches (a
// pattern and its own exclusion match nothing and still read the tree), so the work has a number whatever the patterns
// say. `steps` bounds the work of matching (`steps_per_visit` for each entry it may read): a pattern that backtracks
// over a long name costs the pattern's size times the name's, and 32 of them on every entry of a big tree would hold a
// thread for minutes.
inline constexpr std::size_t steps_per_visit = 4096;

struct Bounds {
	std::size_t names;
	std::size_t visits;
	std::size_t steps;
	Bounds(std::size_t names_, std::size_t visits_) noexcept : names(names_), visits(visits_), steps(visits_ * steps_per_visit) { }
	Bounds(std::size_t names_, std::size_t visits_, std::size_t steps_) noexcept : names(names_), visits(visits_), steps(steps_) { }
};

// Called before each directory entry the walk reads, on the walking thread; it may throw to stop the walk.
using Checkpoint = std::function<void()>;

// The indexes `items` name under `root` (the node's data directory, ending in `/`): a plain item stands for itself,
// `_all` and a pattern for the indexes on disk it matches (an index is a directory holding `strata` or a shard
// `.__N`; one that holds neither is searched for indexes below it), `-item` removes names. Equal items count once.
// One walk finds the indexes of all the patterns, reading directories entry by entry. The names are sorted and have no
// repeats. It reads the file system and blocks, so it runs off a reactor. Raises errc::invalid (a 400) when a pattern
// is longer than `max_pattern_size`, there are more than `max_patterns` of them, the names pass `bounds.names` or the
// walk reads more than `bounds.visits` entries.
std::vector<std::string> expand(const std::string& root, std::span<const std::string> items, const Bounds& bounds, const Checkpoint& checkpoint = {});

// Raises errc::invalid (a 400) when creating the index `path` under `root` would nest it: when an index is a proper
// prefix of `path`, when an index is below it, or when a name below the first is one the layout keeps (`strata`, `.__N`).
// Patterns do not look inside an index, so a nested index would be invisible to them. It reads the file system and
// blocks; the directory below `path` is read for at most `visits` entries.
void check_new_index(const std::string& root, std::string_view path, std::size_t visits);

}  // namespace index_pattern
