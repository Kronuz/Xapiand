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

#include <bitset>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The index names a search may give as a pattern, a glob over the segments of an index path (the parts between `/`):
//
//   *     any run of bytes inside one segment, none included; it never matches `/`
//   ?     one character (a UTF-8 sequence) inside a segment
//   [..]  one character of a class inside a segment: `[abc]`, `[a-f]`, `[!x]` or `[^x]` (not x); a `]` first in the
//         class is a member, a `-` last is a member, and a `[` with no closing `]` is a plain `[`
//   **    a whole segment: any number of segments, none included, so `**/some-??` finds `some-ab` at any depth and
//         `logs/**` finds `logs` and everything below it
//
// `_all` is `**`. An item that starts with `-` removes the names it matches from the result wherever it stands in the
// list. A name that starts with `.` (the node's own indexes) is never listed by a pattern; it is searched by naming it.
// An index may hold other indexes: `twitter` and `twitter/user` are both listed by `**`, and `twitter*` lists only the
// first.
namespace index_pattern {

// The longest pattern: matching a segment backtracks over it, so its cost is the pattern's length times the name's.
inline constexpr std::size_t max_pattern_size = 256;

// Where the class `[...]` that opens at `pattern[open]` ends (the index just past its `]`), or npos when it is no class.
inline std::size_t
class_end(std::string_view pattern, std::size_t open) noexcept
{
	std::size_t i = open + 1;
	if (i < pattern.size() && (pattern[i] == '!' || pattern[i] == '^')) {
		++i;
	}
	if (i < pattern.size() && pattern[i] == ']') {
		++i;
	}
	const auto close = pattern.find(']', i);
	return close == std::string_view::npos ? close : close + 1;
}

inline bool
is_wildcard(std::string_view item) noexcept
{
	if (item == "_all" || item.find_first_of("*?") != std::string_view::npos) {
		return true;
	}
	for (auto open = item.find('['); open != std::string_view::npos; open = item.find('[', open + 1)) {
		if (class_end(item, open) != std::string_view::npos) {
			return true;
		}
	}
	return false;
}

// A pattern as a small automaton over the segments of a path: a position for each segment of the pattern, and the set of
// positions a path prefix leaves live. A `**` is a loop on its position (and may be skipped). The set moves one segment at
// a time, so a walk down a tree carries the set of each directory to its entries, and the work for an entry is the
// pattern's segments, however many `**` it holds. `steps` grows by the work done (a position tried, a byte of the name
// walked, a member of a class read), for a caller that bounds the total.
class Glob {
public:
	static constexpr std::size_t max_segments = max_pattern_size + 1;   // `////...`: a segment on either side of each `/`
	using States = std::bitset<max_segments + 1>;

	// `pattern` must outlive the Glob. A pattern past `max_pattern_size` matches nothing.
	explicit Glob(std::string_view pattern);

	// The live positions before any segment.
	States start() const;
	// The live positions after `segment`, from those before it.
	States step(const States& from, std::string_view segment, std::size_t& steps) const;
	// Whether the path read so far is one the pattern matches.
	bool accepts(const States& states) const noexcept { return states.test(segments_.size()); }
	// Whether a longer path can still match: some segment of the pattern is yet to be matched.
	bool live(const States& states) const noexcept { return states.count() > (accepts(states) ? 1u : 0u); }

private:
	struct Segment {
		std::string_view text;
		bool globstar;
	};
	void add(States& states, std::size_t position) const noexcept;

	std::vector<Segment> segments_;
	bool oversize_ = false;
};

// Whether `name` (an index path) is one `pattern` stands for.
bool match(std::string_view pattern, std::string_view name, std::size_t& steps);

inline bool
match(std::string_view pattern, std::string_view name)
{
	std::size_t steps = 0;
	return match(pattern, name, steps);
}

// The longest list of patterns (wildcards and exclusions together) a search may name. The walk tests every entry it
// reads against every pattern, so this bounds the work per entry.
inline constexpr std::size_t max_patterns = 32;

// What one expansion may spend. `names` and `visits` are both `--max-search-indexes`: `names` bounds the indexes the
// patterns name, and `visits` the directory entries the walk reads to find them, in an index directory too, whether or
// not an entry matches (a pattern and its own exclusion match nothing and still read the tree), so the work has a number
// whatever the patterns say. `steps` bounds the work of matching (`steps_per_visit` for each entry it may read): a
// pattern that backtracks over a long name costs the pattern's size times the name's, and 32 of them on every entry of a
// big tree would hold a thread for minutes.
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
// `_all` and a pattern for the indexes on disk it matches, `-item` removes names. An index is a directory that holds
// `strata` (an index of one shard) or a shard `.__N`; it may hold other indexes in its other directories, and a
// directory that holds neither is searched for indexes below it. The walk goes only into the directories a pattern can
// still match below (`logs/2026/*` reads `logs` and `logs/2026` and no other directory), and carries each pattern's
// state down with the directory. Equal items count once. One walk finds the indexes of all the patterns, reading
// directories entry by entry. The names are sorted and have no repeats. It reads the file system and blocks, so it runs
// off a reactor. Raises errc::invalid (a 400) when a pattern is longer than `max_pattern_size`, there are more than
// `max_patterns` of them, the names pass `bounds.names` or the walk reads more than `bounds.visits` entries.
std::vector<std::string> expand(const std::string& root, std::span<const std::string> items, const Bounds& bounds, const Checkpoint& checkpoint = {});

}  // namespace index_pattern
