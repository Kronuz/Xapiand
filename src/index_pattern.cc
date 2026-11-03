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

// Whether a directory is an index: it holds `strata` (an index of one shard) or a shard `.__N`, which may be any
// of the node's shards of the index. `entry()` is called for each entry it reads.
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

// The indexes under a directory that the wanted patterns match and the excluded ones do not, found in one walk:
// each entry is tested against every pattern. The walk reads at most `bounds.visits` directory entries, however
// few of them match, and calls `checkpoint` before each.
class Walk {
public:
	Walk(std::span<const std::string> wanted, std::span<const std::string> excluded, const Bounds& bounds, const Checkpoint& checkpoint, std::vector<std::string>& out)
		: wanted_(wanted), excluded_(excluded), bounds_(bounds), checkpoint_(checkpoint), out_(out)
	{
		for (const auto& pattern : wanted_) {
			literals_.push_back(std::string_view(pattern).substr(0, pattern.find('*')));   // the bytes before its first `*`
		}
	}

	// With `first_only` the walk stops at the first index it finds.
	void run(const std::string& dir, const std::string& prefix, bool first_only = false)
	{
		first_only_ = first_only;
		scan(dir, prefix);
	}

	std::size_t visits() const noexcept { return visits_; }

private:
	void visit()
	{
		if (++visits_ > bounds_.visits || steps_ > bounds_.steps) {
			throw OutOfBudget{};
		}
		if (checkpoint_) {
			checkpoint_();
		}
	}

	void scan(const std::string& dir, const std::string& prefix)
	{
		DirPtr dirp(::opendir(dir.c_str()));
		if (!dirp) {
			return;
		}
		std::vector<std::pair<std::string, std::string>> deeper_dirs;   // the directories to look inside, after this one is closed
		while (const dirent* ent = ::readdir(dirp.get())) {
			const std::string_view name(ent->d_name);
			if (name == "." || name == "..") {
				continue;
			}
			visit();
			if (name.empty() || name[0] == '.' || !index_path::valid(name)) {
				continue;
			}
			if (ent->d_type != DT_DIR && (ent->d_type != DT_UNKNOWN || !is_directory(dir + "/" + std::string(name), false))) {
				continue;
			}
			const auto rel = prefix + std::string(name);
			const bool matched = wanted_match(rel) && !excluded(rel);
			const bool deeper = below(rel);
			if (!matched && !deeper) {
				continue;
			}
			auto path = dir + "/" + std::string(name);
			if (holds_index(path, [this] { visit(); })) {
				if (matched) {
					out_.push_back(rel);
					if (first_only_) {
						return;
					}
					if (out_.size() > bounds_.names) {
						too_many(bounds_.names);
					}
				}
			} else if (deeper) {
				deeper_dirs.emplace_back(std::move(path), rel + "/");
			}
		}
		dirp.reset();
		for (const auto& [path, rel] : deeper_dirs) {
			scan(path, rel);
			if (first_only_ && !out_.empty()) {
				return;
			}
		}
	}

	bool wanted_match(std::string_view name)
	{
		return std::any_of(wanted_.begin(), wanted_.end(), [&](const std::string& pattern) { return match(pattern, name, steps_); });
	}

	bool excluded(std::string_view name)
	{
		return std::any_of(excluded_.begin(), excluded_.end(), [&](const std::string& pattern) { return match(pattern, name, steps_); });
	}

	// Whether a wanted pattern can match a name below the directory `rel`.
	bool below(std::string_view inside) const
	{
		return std::any_of(literals_.begin(), literals_.end(), [&](std::string_view literal) {
			if (literal.size() > inside.size()) {
				return literal.substr(0, inside.size()) == inside && literal[inside.size()] == '/';
			}
			return inside.substr(0, literal.size()) == literal;
		});
	}

	std::span<const std::string> wanted_;
	std::span<const std::string> excluded_;
	Bounds bounds_;
	const Checkpoint& checkpoint_;
	std::vector<std::string>& out_;
	std::vector<std::string_view> literals_;
	std::size_t visits_ = 0;
	std::size_t steps_ = 0;
	bool first_only_ = false;
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
		} else if (item == "_all" || item.find('*') != std::string::npos) {
			wanted.push_back(item == "_all" ? std::string("*") : item);
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
			Walk(wanted, excluded, bounds, checkpoint, found).run(trimmed(root), "");   // stops at the bounds
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


namespace {

// Raises when an index is a proper prefix of `full` (read for at most `visits` entries in all), or when an index is
// below the directory `full` that is not itself an index. `created` is whether `full` was just made: it then holds
// its own `strata`, which is not an obstacle.
void
check_nesting(const std::string& dir, const std::string& full, std::size_t visits, bool created)
{
	const auto refuse = [&](std::string_view why) {
		throw engine::Error(engine::errc::invalid, "Cannot create index '" + full + "': " + std::string(why));
	};

	// Index paths do not nest: patterns do not look inside an index, so an index below another would be invisible to them.
	// Each ancestor that is no index is read entry by entry, and the entries of all of them share the budget.
	std::size_t read = 0;
	for (auto cut = full.find('/'); cut != std::string::npos; cut = full.find('/', cut + 1)) {
		if (holds_index(dir + "/" + full.substr(0, cut), [&] {
			if (++read > visits) {
				refuse("its parent directories hold too many entries to check that none is an index");
			}
		})) {
			refuse("'" + full.substr(0, cut) + "' is an index, and an index path cannot be below another");
		}
	}
	const auto self = dir + "/" + full;
	if (is_directory(self) && (created || !holds_index(self, [] {}))) {
		const std::vector<std::string> any{"*"};
		const Checkpoint none;
		std::vector<std::string> found;
		Walk walk(any, {}, Bounds{1, visits}, none, found);
		try {
			walk.run(self, full + "/", true);
		} catch (const OutOfBudget&) {
			refuse("the directory holds too many entries to check that no index is below it");
		}
		if (!found.empty()) {
			refuse("'" + found.front() + "' is an index, and an index path cannot be above another");
		}
	}
}

}  // namespace


void
check_new_index(const std::string& root, std::string_view path, std::size_t visits)
{
	const std::string full(path);

	// A directory of the layout (`strata`, `.__N`) below the first name would make its parent look like an index.
	std::size_t at = full.find('/');
	while (at != std::string::npos) {
		const auto next = full.find('/', at + 1);
		const auto component = std::string_view(full).substr(at + 1, next == std::string::npos ? next : next - at - 1);
		if (component == "strata" || component.starts_with(".__")) {
			throw engine::Error(engine::errc::invalid, "Cannot create index '" + full + "': '" + std::string(component) + "' is a name the index layout keeps");
		}
		at = next;
	}
	check_nesting(trimmed(root), full, visits, false);
}


void
check_created(const std::string& root, std::string_view shard_path, std::size_t visits)
{
	const auto dir = trimmed(root);
	// The node runs in its data directory, so a shard's path may be relative to it.
	if (shard_path.starts_with('/')) {
		if (shard_path.size() <= dir.size() + 1 || shard_path.substr(0, dir.size() + 1) != dir + "/") {
			return;   // not under the data directory
		}
		shard_path.remove_prefix(dir.size() + 1);
	}
	std::string full(shard_path);
	while (full.size() > 1 && full.back() == '/') {
		full.pop_back();
	}
	// The path of a shard ends in `.__N`; the index is the directory above it.
	const auto last = full.rfind('/');
	const auto leaf = std::string_view(full).substr(last == std::string::npos ? 0 : last + 1);
	if (last != std::string::npos && leaf.starts_with(".__")) {
		full.resize(last);
	}
	check_nesting(dir, full, visits, true);
}

}  // namespace index_pattern
