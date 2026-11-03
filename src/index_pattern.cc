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


#include "index_pattern.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>               // for std::sort, std::unique, std::any_of
#include <cstdint>                 // for std::uint32_t
#include <iterator>                // for std::make_move_iterator
#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::to_string

#include "engine/error.h"          // for Error, errc
#include "index_path.h"            // for index_path::valid


namespace index_pattern {

namespace {

[[noreturn]] void
too_many(std::size_t limit)
{
	throw engine::Error(engine::errc::invalid, "A search can read at most " + std::to_string(limit) + " indexes (--max-search-indexes) and these patterns name more; narrow the index pattern");
}

[[noreturn]] void
too_much_work(std::size_t limit)
{
	throw engine::Error(engine::errc::invalid, "A search can look through at most " + std::to_string(limit) + " directory entries (--max-search-indexes), and spend " + std::to_string(limit * steps_per_visit) + " steps matching them; these patterns need more; narrow the index pattern");
}

// The walk read more entries than its budget.
struct OutOfBudget { };

struct DirCloser {
	void operator()(DIR* dirp) const noexcept { ::closedir(dirp); }
};
using DirPtr = std::unique_ptr<DIR, DirCloser>;

bool
is_directory(const std::string& path, bool follow = true)
{
	struct stat st;
	return (follow ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st)) == 0 && S_ISDIR(st.st_mode);
}

// The length of the character at `s[i]`: a well-formed UTF-8 sequence, else the one byte.
std::size_t
unit(std::string_view s, std::size_t i) noexcept
{
	const auto c = static_cast<unsigned char>(s[i]);
	const std::size_t len = c < 0x80 ? 1 : (c >= 0xc2 && c <= 0xdf) ? 2 : (c >= 0xe0 && c <= 0xef) ? 3 : (c >= 0xf0 && c <= 0xf4) ? 4 : 1;
	if (i + len > s.size()) {
		return 1;
	}
	for (std::size_t k = 1; k < len; ++k) {
		if ((static_cast<unsigned char>(s[i + k]) & 0xc0) != 0x80) {
			return 1;
		}
	}
	return len;
}

std::uint32_t
code(std::string_view s, std::size_t i, std::size_t len) noexcept
{
	const auto c = static_cast<unsigned char>(s[i]);
	if (len == 1) {
		return c;
	}
	std::uint32_t cp = c & (0xff >> (len + 1));
	for (std::size_t k = 1; k < len; ++k) {
		cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3f);
	}
	return cp;
}

// Whether the character `cp` is a member of the class `pattern[open, end)` (`[`, members, `]`).
bool
in_class(std::string_view pattern, std::size_t open, std::size_t end, std::uint32_t cp, std::size_t& steps) noexcept
{
	std::size_t i = open + 1;
	const std::size_t close = end - 1;
	bool negated = false;
	if (pattern[i] == '!' || pattern[i] == '^') {
		negated = true;
		++i;
	}
	bool found = false;
	while (i < close) {
		++steps;
		auto len = unit(pattern, i);
		const auto lo = code(pattern, i, len);
		auto hi = lo;
		i += len;
		if (i + 1 < close && pattern[i] == '-') {
			len = unit(pattern, i + 1);
			hi = code(pattern, i + 1, len);
			i += 1 + len;
		}
		found = found || (lo <= cp && cp <= hi);
	}
	return found != negated;
}

// Whether `name` is a segment `pattern` (one segment, no `/`) stands for: the two-pointer match, remembering the last
// `*` to retry from, so it never takes more than the pattern's length times the name's.
bool
match_segment(std::string_view pattern, std::string_view name, std::size_t& steps) noexcept
{
	constexpr auto none = std::string_view::npos;
	std::size_t p = 0, n = 0, star = none, mark = 0;
	while (n < name.size()) {
		++steps;
		if (p < pattern.size()) {
			const char c = pattern[p];
			if (c == '*') {
				while (p < pattern.size() && pattern[p] == '*') {
					++p;
				}
				star = p;
				mark = n;
				continue;
			}
			const auto len = unit(name, n);
			if (c == '?') {
				++p;
				n += len;
				continue;
			}
			if (c == '[') {
				const auto end = class_end(pattern, p);
				if (end != none) {
					steps += end - p;
					if (in_class(pattern, p, end, code(name, n, len), steps)) {
						p = end;
						n += len;
						continue;
					}
					goto retry;
				}
			}
			if (c == name[n]) {
				++p;
				++n;
				continue;
			}
		}
	retry:
		if (star == none) {
			return false;
		}
		p = star;
		mark += unit(name, mark);
		n = mark;
	}
	while (p < pattern.size() && pattern[p] == '*') {
		++p;
		++steps;
	}
	return p == pattern.size();
}

}  // namespace


Glob::Glob(std::string_view pattern)
{
	if (pattern.size() > max_pattern_size) {
		oversize_ = true;
		return;
	}
	for (std::size_t from = 0;;) {
		const auto cut = pattern.find('/', from);
		const auto text = pattern.substr(from, cut == std::string_view::npos ? cut : cut - from);
		const bool globstar = text == "**";
		if (!(globstar && !segments_.empty() && segments_.back().globstar)) {   // `**/**` is `**`
			segments_.push_back({text, globstar});
		}
		if (cut == std::string_view::npos) {
			break;
		}
		from = cut + 1;
	}
}

void
Glob::add(States& states, std::size_t position) const noexcept
{
	states.set(position);
	while (position < segments_.size() && segments_[position].globstar) {   // a `**` may match no segment
		states.set(++position);
	}
}

Glob::States
Glob::start() const
{
	States states;
	if (!oversize_) {
		add(states, 0);
	}
	return states;
}

Glob::States
Glob::step(const States& from, std::string_view segment, std::size_t& steps) const
{
	States next;
	for (std::size_t i = 0; i < segments_.size(); ++i) {
		if (!from.test(i)) {
			continue;
		}
		++steps;
		if (segments_[i].globstar) {
			add(next, i);
		} else if (match_segment(segments_[i].text, segment, steps)) {
			add(next, i + 1);
		}
	}
	return next;
}

bool
match(std::string_view pattern, std::string_view name, std::size_t& steps)
{
	const Glob glob(pattern);
	auto states = glob.start();
	for (std::size_t from = 0; states.any(); ) {
		const auto cut = name.find('/', from);
		states = glob.step(states, name.substr(from, cut == std::string_view::npos ? cut : cut - from), steps);
		if (cut == std::string_view::npos) {
			return glob.accepts(states);
		}
		from = cut + 1;
	}
	return false;
}


namespace {

// Whether a directory is an index, for one that is not read for what is in it: it holds `strata` (an index of one shard)
// or a shard `.__N`, which may be any of the node's shards of the index. `entry()` is called for each entry it reads.
template <typename Entry>
bool
holds_index(const std::string& dir, Entry&& entry)
{
	if (is_directory(dir + "/strata") || is_directory(dir + "/.__1")) {
		return true;
	}
	DirPtr dirp(::opendir(dir.c_str()));
	if (!dirp) {
		return false;
	}
	while (const dirent* ent = ::readdir(dirp.get())) {
		const std::string_view name(ent->d_name);
		if (name == "." || name == "..") {
			continue;
		}
		entry();
		if (name.starts_with(".__") && ent->d_type != DT_REG) {
			return true;
		}
	}
	return false;
}

// The indexes under a directory that the wanted patterns match and the excluded ones do not, found in one walk. Each
// pattern is a Glob, and the walk carries the live positions of every Glob down with the directory it reads: an entry
// costs a step of each pattern, whatever the tree below, and a directory is entered only while a wanted pattern is live
// in it. The walk reads at most `bounds.visits` directory entries, in index directories too, however few of them match,
// and calls `checkpoint` before each.
class Walk {
public:
	Walk(std::span<const std::string> wanted, std::span<const std::string> excluded, const Bounds& bounds, const Checkpoint& checkpoint, std::vector<std::string>& out)
		: wanted_count_(wanted.size()), bounds_(bounds), checkpoint_(checkpoint), out_(out)
	{
		for (const auto& pattern : wanted) {
			globs_.emplace_back(pattern);
		}
		for (const auto& pattern : excluded) {
			globs_.emplace_back(pattern);
		}
	}

	void run(const std::string& dir)
	{
		Level start;
		for (const auto& glob : globs_) {
			start.push_back(glob.start());
		}
		scan(dir, "", start, true);
	}

	std::size_t visits() const noexcept { return visits_; }

private:
	using Level = std::vector<Glob::States>;   // the live positions of each pattern, wanted first

	void visit()
	{
		if (++visits_ > bounds_.visits || steps_ > bounds_.steps) {
			throw OutOfBudget{};
		}
		if (checkpoint_) {
			checkpoint_();
		}
	}

	Level step(const Level& level, std::string_view name)
	{
		Level next;
		next.reserve(level.size());
		for (std::size_t i = 0; i < level.size(); ++i) {
			next.push_back(globs_[i].step(level[i], name, steps_));
		}
		return next;
	}

	bool matched(const Level& level) const
	{
		bool wanted = false;
		for (std::size_t i = 0; i < level.size(); ++i) {
			if (globs_[i].accepts(level[i])) {
				if (i >= wanted_count_) {
					return false;
				}
				wanted = true;
			}
		}
		return wanted;
	}

	bool deeper(const Level& level) const
	{
		for (std::size_t i = 0; i < wanted_count_; ++i) {
			if (globs_[i].live(level[i])) {
				return true;
			}
		}
		return false;
	}

	// Reads `dir` (`rel` is its index path with a `/`, `level` the state of the patterns after it), lists the indexes it holds
	// that match and goes into every directory a wanted pattern can still match below, an index's too. Returns whether `dir`
	// is an index: it holds `strata` or a shard `.__N`. Those two are the engine's own names for the data of the index
	// and are never taken for a nested index; the rest of the index's directories are its children. The data directory
	// itself (`top`) is no index: a top-level index may be called `strata`.
	bool scan(const std::string& dir, const std::string& rel, const Level& level, bool top = false)
	{
		DirPtr dirp(::opendir(dir.c_str()));
		if (!dirp) {
			return false;
		}
		struct Child {
			std::string name;
			bool matches;
		};
		bool index = false;
		std::vector<Child> children;   // the directories to look inside or to list, after this one is closed
		while (const dirent* ent = ::readdir(dirp.get())) {
			const std::string_view name(ent->d_name);
			if (name == "." || name == "..") {
				continue;
			}
			visit();
			if (name == "strata" && !top) {
				index = index || ent->d_type == DT_DIR || ((ent->d_type == DT_UNKNOWN || ent->d_type == DT_LNK) && is_directory(dir + "/strata"));
				continue;
			}
			if (name.starts_with(".__")) {
				index = index || ent->d_type != DT_REG;
				continue;
			}
			if (name.empty() || name[0] == '.' || !index_path::valid(name)) {
				continue;
			}
			if (ent->d_type != DT_DIR && (ent->d_type != DT_UNKNOWN || !is_directory(dir + "/" + std::string(name), false))) {
				continue;
			}
			const auto below = step(level, name);
			const bool matches = matched(below);
			if (matches || deeper(below)) {
				children.push_back({std::string(name), matches});
			}
		}
		dirp.reset();
		for (const auto& child : children) {
			const auto path = dir + "/" + child.name;
			const auto name = rel + child.name;
			const auto below = step(level, child.name);
			const bool inside = deeper(below);
			const bool is_index = inside ? scan(path, name + "/", below) : holds_index(path, [this] { visit(); });
			if (child.matches && is_index) {
				out_.push_back(name);
				if (out_.size() > bounds_.names) {
					too_many(bounds_.names);
				}
			}
		}
		return index;
	}

	std::vector<Glob> globs_;
	std::size_t wanted_count_;
	Bounds bounds_;
	const Checkpoint& checkpoint_;
	std::vector<std::string>& out_;
	std::size_t visits_ = 0;
	std::size_t steps_ = 0;
};

void
sort_unique(std::vector<std::string>& names)
{
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
}

std::string
trimmed(const std::string& root)
{
	std::string dir = root.empty() ? "." : root;
	while (dir.size() > 1 && dir.back() == '/') {
		dir.pop_back();
	}
	return dir;
}

}  // namespace


std::vector<std::string>
expand(const std::string& root, std::span<const std::string> items, const Bounds& bounds, const Checkpoint& checkpoint)
{
	std::vector<std::string> plain, wanted, excluded, names;
	for (const auto& item : items) {
		if (item.size() > max_pattern_size) {
			throw engine::Error(engine::errc::invalid, "An index pattern is at most " + std::to_string(max_pattern_size) + " bytes");
		}
		if (item.size() > 1 && item[0] == '-') {
			excluded.push_back(item.substr(1));
		} else if (is_wildcard(item)) {
			wanted.push_back(item == "_all" ? std::string("**") : item);
		} else {
			plain.push_back(item);
		}
	}
	sort_unique(plain);
	sort_unique(wanted);
	sort_unique(excluded);
	if (wanted.size() + excluded.size() > max_patterns) {
		throw engine::Error(engine::errc::invalid, "A search names at most " + std::to_string(max_patterns) + " index patterns (wildcards and exclusions together)");
	}
	std::size_t steps = 0;   // a plain name is tested against the exclusions: at most a URL of names, 32 patterns, counted too
	for (auto& item : plain) {
		const bool removed = std::any_of(excluded.begin(), excluded.end(), [&](const std::string& pattern) { return match(pattern, item, steps); });
		if (steps > bounds.steps) {
			too_much_work(bounds.visits);
		}
		if (!removed) {
			names.push_back(std::move(item));
		}
	}
	if (names.size() > bounds.names) {
		too_many(bounds.names);
	}
	if (!wanted.empty()) {
		std::vector<std::string> found;   // apart from the plain names, so a name in both counts once
		try {
			Walk(wanted, excluded, bounds, checkpoint, found).run(trimmed(root));   // stops at the bounds
		} catch (const OutOfBudget&) {
			too_much_work(bounds.visits);
		}
		names.insert(names.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
	}
	sort_unique(names);
	if (names.size() > bounds.names) {
		too_many(bounds.names);
	}
	return names;
}

}  // namespace index_pattern
