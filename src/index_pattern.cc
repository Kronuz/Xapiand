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

#include "engine/error.h"          // for Error, errc
#include "index_path.h"            // for index_path::valid
#include "xapiand_strings.hh"      // for strings::format


namespace index_pattern {

namespace {

[[noreturn]] void
too_many(std::size_t limit)
{
	throw engine::Error(engine::errc::invalid, strings::format("A search can read at most {} indexes (--max-search-indexes) and these patterns name more; narrow the index pattern", limit));
}

bool
is_directory(const std::string& path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Whether a directory is an index: it holds `strata` (an index of one shard) or a shard `.__N`, which may be any
// of the node's shards of the index.
bool
holds_index(const std::string& dir)
{
	if (is_directory(dir + "/strata") || is_directory(dir + "/.__1")) {
		return true;
	}
	DIR* dirp = ::opendir(dir.c_str());
	if (dirp == nullptr) {
		return false;
	}
	bool found = false;
	while (const dirent* ent = ::readdir(dirp)) {
		const std::string_view name(ent->d_name);
		if (name.starts_with(".__") && ent->d_type != DT_REG) {
			found = true;
			break;
		}
	}
	::closedir(dirp);
	return found;
}

// The indexes under a directory that one pattern matches, without the names `excluded` match.
class Walk {
public:
	Walk(std::string_view pattern, std::span<const std::string> excluded, std::size_t limit, std::vector<std::string>& out)
		: pattern_(pattern), excluded_(excluded), limit_(limit), out_(out),
		  literal_(pattern.substr(0, pattern.find('*'))) { }

	void scan(const std::string& dir, const std::string& prefix)
	{
		std::vector<std::string> children;
		DIR* dirp = ::opendir(dir.c_str());
		if (dirp == nullptr) {
			return;
		}
		while (const dirent* ent = ::readdir(dirp)) {
			const std::string_view name(ent->d_name);
			if (name.empty() || name[0] == '.' || !index_path::valid(name)) {
				continue;
			}
			if (ent->d_type == DT_DIR || (ent->d_type == DT_UNKNOWN && is_directory(dir + "/" + std::string(name)))) {
				children.emplace_back(name);
			}
		}
		::closedir(dirp);
		for (const auto& child : children) {
			const auto rel = prefix + child;
			const bool matched = match(pattern_, rel) && !excluded(rel);
			const bool deeper = below(rel);
			if (!matched && !deeper) {
				continue;
			}
			const auto path = dir + "/" + child;
			if (holds_index(path)) {
				if (matched) {
					out_.push_back(rel);
					if (out_.size() > limit_) {
						too_many(limit_);
					}
				}
			} else if (deeper) {
				scan(path, rel + "/");
			}
		}
	}

private:
	bool excluded(std::string_view name) const
	{
		return std::any_of(excluded_.begin(), excluded_.end(), [&](const std::string& pattern) { return match(pattern, name); });
	}

	// Whether the pattern can match a name below the directory `rel`.
	bool below(const std::string& rel) const
	{
		const std::string_view inside(rel);
		if (literal_.size() > inside.size()) {
			return literal_.substr(0, inside.size()) == inside && literal_[inside.size()] == '/';
		}
		return inside.substr(0, literal_.size()) == literal_;
	}

	std::string_view pattern_;
	std::span<const std::string> excluded_;
	std::size_t limit_;
	std::vector<std::string>& out_;
	std::string_view literal_;   // the bytes of the pattern before its first `*`
};

}  // namespace


std::vector<std::string>
expand(const std::string& root, std::span<const std::string> items, std::size_t limit)
{
	std::vector<std::string> wanted, excluded, names;
	for (const auto& item : items) {
		if (item.size() > max_pattern_size) {
			throw engine::Error(engine::errc::invalid, strings::format("An index pattern is at most {} bytes", max_pattern_size));
		}
		if (item.size() > 1 && item[0] == '-') {
			excluded.push_back(item.substr(1));
		} else {
			wanted.push_back(item == "_all" ? std::string("*") : item);
		}
	}
	std::string dir = root.empty() ? "." : root;
	while (dir.size() > 1 && dir.back() == '/') {
		dir.pop_back();
	}
	const auto removed = [&](std::string_view name) {
		return std::any_of(excluded.begin(), excluded.end(), [&](const std::string& pattern) { return match(pattern, name); });
	};
	for (const auto& item : wanted) {
		if (item.find('*') == std::string::npos) {
			if (!removed(item)) {
				names.push_back(item);
			}
		} else {
			Walk(item, excluded, limit, names).scan(dir, "");
		}
		if (names.size() > limit) {
			too_many(limit);
		}
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	return names;
}

}  // namespace index_pattern
