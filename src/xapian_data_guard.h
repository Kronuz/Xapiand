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

// Detects the files a Xapian-based release (1.0.0-alpha.10 and earlier) left in a data directory, by file
// name only, so that a node does not start fresh beside indexes it cannot read.

#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace xapian_guard {

// Whether a file name is one of Xapian's glass backend: `iamglass`, `flintlock`, `postlist.*`, `*.glass`.
inline bool is_xapian_file(std::string_view name) {
	return name == "iamglass" || name == "flintlock" ||
		name.substr(0, 9) == "postlist." ||
		(name.size() > 6 && name.substr(name.size() - 6) == ".glass");
}

// The first Xapian file under `root` (not following links, not entering strata's own `strata` directories), or "".
inline std::string scan(const std::string& root) {
	std::vector<std::string> pending{root};
	while (!pending.empty()) {
		auto dir = std::move(pending.back());
		pending.pop_back();
		DIR* dirp = ::opendir(dir.c_str());
		if (dirp == nullptr) {
			continue;
		}
		std::vector<std::string> subdirs;
		std::string found;
		while (auto* ent = ::readdir(dirp)) {
			std::string_view name(ent->d_name);
			if (name == "." || name == "..") {
				continue;
			}
			if (ent->d_type == DT_DIR || ent->d_type == DT_UNKNOWN) {
				if (name != "strata") {
					subdirs.push_back(dir + "/" + std::string(name));
				}
			}
			if (ent->d_type != DT_DIR && is_xapian_file(name)) {
				found = dir + "/" + std::string(name);
				break;
			}
		}
		::closedir(dirp);
		if (!found.empty()) {
			return found;
		}
		pending.insert(pending.end(), subdirs.rbegin(), subdirs.rend());
	}
	return "";
}

// Whether a strata release already started on this directory: its registry holds a `strata`
// directory (in the registry or one of its shards).
inline bool started_by_strata(const std::string& registry) {
	for (const auto& dir : {registry, registry + "/.__1"}) {
		struct stat st;
		if (::stat((dir + "/strata").c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
			return true;
		}
	}
	return false;
}

// The first Xapian file in the data directory `directory`: the registry (`.xapiand/indexes`) first;
// the whole tree only when no strata release has started here, so a strata node with millions of
// indexes reads one or two entries at start, not its whole tree.
inline std::string find(const std::string& directory) {
	const auto registry = directory + "/.xapiand/indexes";
	auto found = scan(registry);
	if (!found.empty() || started_by_strata(registry)) {
		return found;
	}
	return scan(directory);
}

inline constexpr std::string_view UPGRADE_DOC = "https://kronuz.github.io/Xapiand/upgrade/#upgrading-from-100-alpha9";

}  // namespace xapian_guard
