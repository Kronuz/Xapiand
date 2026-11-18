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


#include "database/name_index.h"

#include <algorithm>               // for std::sort, std::all_of, std::max, std::min, std::mismatch
#include <iterator>                // for std::next
#include <limits>                  // for std::numeric_limits
#include <mutex>                   // for std::unique_lock
#include <string>                  // for std::string
#include <system_error>            // for std::system_error
#include <utility>                 // for std::move

#include "index_path.h"            // for index_path::valid


namespace {

// The build was stopped.
struct Stopped { };

// Whether the walk reaches and lists a directory with these segments when it is an index: no segment is empty,
// invalid or starts with `.`, and none but the first is `strata`, the engine's own directory of an index.
bool
listable(std::span<const std::string_view> segments) noexcept
{
	for (std::size_t i = 0; i < segments.size(); ++i) {
		const auto segment = segments[i];
		if (segment.empty() || segment[0] == '.' || !index_path::valid(segment) || (i != 0 && segment == "strata")) {
			return false;
		}
	}
	return true;
}

// The bytes every name a pattern matches starts with: those before its first `*`, `?` or class, less a `/` that ends
// them, since a `**` after it may stand for no segment (`a/**` matches `a`).
std::string_view
literal_prefix(std::string_view pattern) noexcept
{
	std::size_t i = 0;
	for (; i < pattern.size(); ++i) {
		const char c = pattern[i];
		if (c == '*' || c == '?' || (c == '[' && index_pattern::class_end(pattern, i) != std::string_view::npos)) {
			break;
		}
	}
	auto prefix = pattern.substr(0, i);
	if (prefix.ends_with('/')) {
		prefix.remove_suffix(1);
	}
	return prefix;
}

// Whether `name` is in the subtree of `prefix`: it starts with `prefix + '/'`.
bool
below(std::string_view name, std::string_view prefix) noexcept
{
	return name.size() > prefix.size() && name.starts_with(prefix) && name[prefix.size()] == '/';
}

// Whether `name` sorts past the subtree of `prefix`, at or after `prefix + '0'` ('0' follows '/').
bool
past(std::string_view name, std::string_view prefix) noexcept
{
	if (!name.starts_with(prefix)) {
		return name > prefix;
	}
	return name.size() > prefix.size() && static_cast<unsigned char>(name[prefix.size()]) >= '0';
}

}  // namespace


std::string_view
NameIndex::Base::at(std::size_t i) const noexcept
{
	const auto begin = i == 0 ? 0 : ends[i - 1];
	return std::string_view(bytes).substr(begin, ends[i] - begin);
}


std::size_t
NameIndex::Base::lower_bound(std::string_view key) const noexcept
{
	std::size_t lo = 0, hi = size();
	while (lo < hi) {
		const auto mid = lo + (hi - lo) / 2;
		if (at(mid) < key) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}


void
NameIndex::Base::push_back(std::string_view name)
{
	bytes.append(name);
	ends.push_back(bytes.size());
}


// The names of the base and the delta in one order. They are disjoint, so the next name is the least of the two.
class NameIndex::Cursor {
public:
	Cursor(const Base& base, const Delta& delta) noexcept : base_(base), delta_(delta), i_(0), j_(delta.begin()) { }

	void seek(std::string_view key)
	{
		i_ = base_.lower_bound(key);
		j_ = delta_.lower_bound(key);
	}

	bool done() const noexcept { return i_ == base_.size() && j_ == delta_.end(); }

	std::string_view name() const noexcept { return from_base() ? base_.at(i_) : std::string_view(*j_); }

	void next() noexcept
	{
		if (from_base()) {
			++i_;
		} else {
			++j_;
		}
	}

private:
	bool from_base() const noexcept { return j_ == delta_.end() || (i_ != base_.size() && base_.at(i_) < *j_); }

	const Base& base_;
	const Delta& delta_;
	std::size_t i_;
	Delta::const_iterator j_;
};


std::vector<std::string>
NameIndex::names_of(std::string_view key)
{
	std::vector<std::string_view> segments;
	for (std::size_t from = 0;;) {
		const auto cut = key.find('/', from);
		segments.push_back(key.substr(from, cut == std::string_view::npos ? cut : cut - from));
		if (cut == std::string_view::npos) {
			break;
		}
		from = cut + 1;
	}
	std::vector<std::string> names;
	for (std::size_t n = 1; n <= segments.size(); ++n) {
		// The directory of the first n segments holds the next one, and the last holds `strata`.
		const auto next = n < segments.size() ? segments[n] : std::string_view("strata");
		if ((next == "strata" || next.starts_with(".__")) && listable(std::span(segments).first(n))) {
			const auto end = static_cast<std::size_t>(segments[n - 1].data() + segments[n - 1].size() - key.data());
			names.emplace_back(key.substr(0, end));
		}
	}
	return names;
}


bool
NameIndex::contains(std::string_view name) const
{
	const auto i = base_.lower_bound(name);
	return (i != base_.size() && base_.at(i) == name) || delta_.find(name) != delta_.end();
}


void
NameIndex::opened(std::string_view key)
{
	const auto names = names_of(key);
	if (names.empty()) {
		return;
	}
	{
		std::shared_lock<std::shared_mutex> lk(mtx_);
		if (std::all_of(names.begin(), names.end(), [this](const std::string& name) { return contains(name); })) {
			return;
		}
	}
	std::unique_lock<std::shared_mutex> lk(mtx_);
	for (const auto& name : names) {
		if (!contains(name)) {
			delta_.insert(name);
		}
	}
	// Until the build ends the delta holds every name resolve adds, so the build's merge keeps them all.
	if (complete() && delta_.size() > std::max<std::size_t>(1024, base_.size() / 8)) {
		merge_delta();
	}
}


void
NameIndex::merge_delta()
{
	Base merged;
	merged.bytes.reserve(base_.bytes.size() + delta_.size() * 16);
	merged.ends.reserve(base_.size() + delta_.size());
	Cursor cursor(base_, delta_);
	for (; !cursor.done(); cursor.next()) {
		merged.push_back(cursor.name());
	}
	base_ = std::move(merged);
	delta_.clear();
}


bool
NameIndex::build(const std::string& root)
{
	const std::string all = "**";
	const auto max = std::numeric_limits<std::size_t>::max();
	std::vector<std::string> found;
	try {
		found = index_pattern::expand(root, std::span(&all, 1), index_pattern::Bounds(max, max, max), [this] {
			if (stopped_.load(std::memory_order_relaxed)) {
				throw Stopped{};
			}
		}, index_pattern::Unreadable::fail);
	} catch (const Stopped&) {
		return false;
	} catch (const std::system_error& exc) {
		failure_ = exc.what();   // a directory it could not read: the names stay incomplete (D110)
		return false;
	}
	failure_.clear();
	Base fresh;
	std::size_t bytes = 0;
	for (const auto& name : found) {
		bytes += name.size();
	}
	fresh.bytes.reserve(bytes);
	fresh.ends.reserve(found.size());
	for (const auto& name : found) {
		fresh.push_back(name);
	}
	found = {};
	std::unique_lock<std::shared_mutex> lk(mtx_);
	// What resolve added meanwhile stays in the delta unless the walk found it too.
	for (auto it = delta_.begin(); it != delta_.end(); ) {
		const auto i = fresh.lower_bound(*it);
		it = i != fresh.size() && fresh.at(i) == *it ? delta_.erase(it) : std::next(it);
	}
	base_ = std::move(fresh);
	complete_.store(true, std::memory_order_release);
	return true;
}


bool
NameIndex::claim() noexcept
{
	if (complete() || stopped_.load(std::memory_order_acquire)) {
		return false;
	}
	bool idle = false;
	if (!building_.compare_exchange_strong(idle, true, std::memory_order_acq_rel)) {
		return false;
	}
	// A build that ended well between the check and the claim stored complete_ before it unclaimed.
	if (complete()) {
		unclaim();
		return false;
	}
	return true;
}


void
NameIndex::unclaim() noexcept
{
	building_.store(false, std::memory_order_release);
}


void
NameIndex::request_stop() noexcept
{
	stopped_.store(true, std::memory_order_release);
}


std::size_t
NameIndex::size() const
{
	std::shared_lock<std::shared_mutex> lk(mtx_);
	return base_.size() + delta_.size();
}


std::size_t
NameIndex::bytes() const
{
	std::shared_lock<std::shared_mutex> lk(mtx_);
	std::size_t total = base_.bytes.capacity() + base_.ends.capacity() * sizeof(std::uint64_t);
	for (const auto& name : delta_) {
		total += 4 * sizeof(void*) + sizeof(std::string) + (name.capacity() > 22 ? name.capacity() + 1 : 0);
	}
	return total;
}


std::vector<std::string>
NameIndex::expand(std::span<const std::string> items, const index_pattern::Bounds& bounds, const index_pattern::Checkpoint& checkpoint) const
{
	auto parsed = index_pattern::parse(items, bounds, index_pattern::names_work);
	const auto& wanted = parsed.wanted;
	const auto& excluded = parsed.excluded;
	auto steps = parsed.steps;
	std::vector<std::string> found;   // apart from the plain names, so a name in both counts once
	if (!wanted.empty()) {
		std::vector<index_pattern::Glob> globs;
		globs.reserve(wanted.size() + excluded.size());
		for (const auto& pattern : wanted) {
			globs.emplace_back(pattern);
		}
		for (const auto& pattern : excluded) {
			globs.emplace_back(pattern);
		}
		// The ranges: each wanted pattern's literal prefix, in order, and none that another one already covers.
		std::vector<std::string_view> prefixes;
		for (const auto& pattern : wanted) {
			prefixes.push_back(literal_prefix(pattern));
		}
		std::sort(prefixes.begin(), prefixes.end());
		std::vector<std::string_view> ranges;
		for (const auto prefix : prefixes) {
			if (ranges.empty() || !prefix.starts_with(ranges.back())) {
				ranges.push_back(prefix);
			}
		}

		using Level = std::vector<index_pattern::Glob::States>;
		Level start;
		for (const auto& glob : globs) {
			start.push_back(glob.start());
		}
		std::size_t visits = 0;
		const auto visit = [&] {
			if (++visits > bounds.visits || steps > bounds.steps) {
				index_pattern::too_much_work(bounds.visits, index_pattern::names_work);
			}
			if (checkpoint) {
				checkpoint();
			}
		};
		const auto live = [&](const Level& level) {
			for (std::size_t i = 0; i < wanted.size(); ++i) {
				if (globs[i].live(level[i])) {
					return true;
				}
			}
			return false;
		};
		const auto matched = [&](const Level& level) {
			bool accepted = false;
			for (std::size_t i = 0; i < level.size(); ++i) {
				if (globs[i].accepts(level[i])) {
					if (i >= wanted.size()) {
						return false;
					}
					accepted = true;
				}
			}
			return accepted;
		};

		// A lookup reads no more than the walk would (D7): one visit per name, which stands for the walk's entry of the
		// name or of the first prefix of it no pattern can match below; a seek counts nothing. A name steps only the
		// segments it does not share with the last one stepped (`levels`, `ends`), as the walk steps an entry once
		// for everything below it.
		std::vector<Level> levels;          // the level after each segment of `stepped`
		std::vector<std::size_t> ends;      // where each of those segments ends
		std::string_view stepped;
		// Names read below which no wanted pattern matches, whose subtrees are still ahead (the walk never enters
		// them); the last one's subtree comes first.
		std::vector<std::string_view> closed;
		std::string seek;
		std::shared_lock<std::shared_mutex> lk(mtx_);
		Cursor cursor(base_, delta_);
		const auto skip = [&](std::string_view prefix) {   // past its subtree, [prefix + '/', prefix + '0')
			seek.assign(prefix).push_back('0');
			cursor.seek(seek);
		};
		for (const auto range : ranges) {
			cursor.seek(range);
			closed.clear();
			while (!cursor.done()) {
				const auto name = cursor.name();
				if (!name.starts_with(range)) {
					break;
				}
				while (!closed.empty() && past(name, closed.back())) {
					closed.pop_back();
				}
				if (!closed.empty() && below(name, closed.back())) {
					skip(closed.back());
					closed.pop_back();
					continue;
				}
				visit();
				// The segments it shares with `stepped`, the last one always stepped anew.
				const auto common = static_cast<std::size_t>(std::mismatch(name.begin(), name.begin() + static_cast<std::ptrdiff_t>(std::min(name.size(), stepped.size())), stepped.begin()).first - name.begin());
				std::size_t shared = 0;
				while (shared < ends.size() && ends[shared] < name.size() && ends[shared] <= common && name[ends[shared]] == '/') {
					++shared;
				}
				levels.resize(shared);
				ends.resize(shared);
				Level level = shared == 0 ? start : levels.back();
				std::size_t dead = std::string_view::npos;
				for (std::size_t from = shared == 0 ? 0 : ends.back() + 1;;) {
					const auto cut = name.find('/', from);
					const auto segment = name.substr(from, cut == std::string_view::npos ? cut : cut - from);
					for (std::size_t i = 0; i < globs.size(); ++i) {
						level[i] = globs[i].step(level[i], segment, steps);
					}
					levels.push_back(level);
					ends.push_back(cut == std::string_view::npos ? name.size() : cut);
					if (cut == std::string_view::npos) {
						break;
					}
					if (!live(level)) {
						dead = cut;
						break;
					}
					from = cut + 1;
				}
				stepped = name;
				if (dead != std::string_view::npos) {
					skip(name.substr(0, dead));   // a prefix that is no name: this visit was its entry
					continue;
				}
				if (matched(level)) {
					found.emplace_back(name);
					if (found.size() > bounds.names) {
						index_pattern::too_many(bounds.names);
					}
				}
				if (!live(level)) {
					closed.push_back(name);
				}
				cursor.next();
			}
		}
	}
	return index_pattern::merged(parsed, found, bounds);
}
