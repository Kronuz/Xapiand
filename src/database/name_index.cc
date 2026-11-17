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

#include <algorithm>               // for std::sort, std::unique, std::any_of, std::all_of, std::max
#include <exception>               // for std::exception
#include <iterator>                // for std::next, std::make_move_iterator
#include <limits>                  // for std::numeric_limits
#include <string>                  // for std::to_string
#include <utility>                 // for std::move

#include "engine/error.h"          // for engine::Error, engine::errc
#include "index_path.h"            // for index_path::valid
#include "log.h"                   // for L_ERR, L_NOTICE


namespace {

[[noreturn]] void
too_many(std::size_t limit)
{
	throw engine::Error(engine::errc::invalid, "A search can read at most " + std::to_string(limit) + " indexes (--max-search-indexes) and these patterns name more; narrow the index pattern");
}

[[noreturn]] void
too_much_work(std::size_t limit)
{
	throw engine::Error(engine::errc::invalid, "A search can look through at most " + std::to_string(limit) + " index names (--max-search-indexes), and spend " + std::to_string(limit * index_pattern::steps_per_visit) + " steps matching them; these patterns need more; narrow the index pattern");
}

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

void
sort_unique(std::vector<std::string>& names)
{
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
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


NameIndex::~NameIndex()
{
	stop();
}


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
		});
	} catch (const Stopped&) {
		return false;
	}
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


void
NameIndex::start(const std::string& root)
{
	if (complete() || building_.load(std::memory_order_acquire) || stopped_.load(std::memory_order_acquire)) {
		return;
	}
	std::lock_guard<std::mutex> lk(build_mtx_);
	if (complete() || building_.load(std::memory_order_acquire) || stopped_.load(std::memory_order_acquire)) {
		return;
	}
	if (builder_.joinable()) {
		builder_.join();   // a build that failed: this one starts it again
	}
	building_.store(true, std::memory_order_release);
	builder_ = std::thread([this, root] {
		try {
			if (build(root)) {
				L_NOTICE("The index names are complete: {} names", size());
			}
		} catch (const std::exception& exc) {
			L_ERR("The index names could not be built; patterns walk the data directory: {}", exc.what());
		}
		building_.store(false, std::memory_order_release);
	});
}


void
NameIndex::request_stop() noexcept
{
	stopped_.store(true, std::memory_order_release);
}


void
NameIndex::stop()
{
	request_stop();
	std::lock_guard<std::mutex> lk(build_mtx_);
	if (builder_.joinable()) {
		builder_.join();
	}
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
	std::vector<std::string> plain, wanted, excluded, names;
	for (const auto& item : items) {
		if (item.size() > index_pattern::max_pattern_size) {
			throw engine::Error(engine::errc::invalid, "An index pattern is at most " + std::to_string(index_pattern::max_pattern_size) + " bytes");
		}
		if (item.size() > 1 && item[0] == '-') {
			excluded.push_back(item.substr(1));
		} else if (index_pattern::is_wildcard(item)) {
			wanted.push_back(item == "_all" ? std::string("**") : item);
		} else {
			plain.push_back(item);
		}
	}
	sort_unique(plain);
	sort_unique(wanted);
	sort_unique(excluded);
	if (wanted.size() + excluded.size() > index_pattern::max_patterns) {
		throw engine::Error(engine::errc::invalid, "A search names at most " + std::to_string(index_pattern::max_patterns) + " index patterns (wildcards and exclusions together)");
	}
	std::size_t steps = 0;
	for (auto& item : plain) {
		const bool removed = std::any_of(excluded.begin(), excluded.end(), [&](const std::string& pattern) { return index_pattern::match(pattern, item, steps); });
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
				too_much_work(bounds.visits);
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

		std::vector<std::string> found;   // apart from the plain names, so a name in both counts once
		std::string seek;
		std::shared_lock<std::shared_mutex> lk(mtx_);
		Cursor cursor(base_, delta_);
		for (const auto range : ranges) {
			visit();
			cursor.seek(range);
			while (!cursor.done()) {
				const auto name = cursor.name();
				if (!name.starts_with(range)) {
					break;
				}
				visit();
				// The name's segments, one step of every pattern each; a prefix no wanted pattern can match below is
				// a subtree to skip: its names are those in [prefix + '/', prefix + '0').
				Level level = start;
				std::size_t dead = std::string_view::npos;
				for (std::size_t from = 0;;) {
					const auto cut = name.find('/', from);
					const auto segment = name.substr(from, cut == std::string_view::npos ? cut : cut - from);
					for (std::size_t i = 0; i < globs.size(); ++i) {
						level[i] = globs[i].step(level[i], segment, steps);
					}
					if (cut == std::string_view::npos) {
						break;
					}
					if (!live(level)) {
						dead = cut;
						break;
					}
					from = cut + 1;
				}
				if (dead != std::string_view::npos) {
					visit();
					seek.assign(name.substr(0, dead)).push_back('0');
					cursor.seek(seek);
					continue;
				}
				if (matched(level)) {
					found.emplace_back(name);
					if (found.size() > bounds.names) {
						too_many(bounds.names);
					}
				}
				cursor.next();
			}
		}
		lk.unlock();
		names.insert(names.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
	}
	sort_unique(names);
	if (names.size() > bounds.names) {
		too_many(bounds.names);
	}
	return names;
}
