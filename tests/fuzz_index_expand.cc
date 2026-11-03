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

// Fuzz target for index_pattern::expand and check_new_index against a fixed tree on disk (FUZZ_ROOT names a scratch
// directory). Input: byte 0 is the names bound (1 + b % 40), byte 1 the entries bound (1 + b % 80), the rest is split on
// '\n' into items. For the items, expand must: raise only engine::Error; return sorted, unique names; return the
// names of a brute-force oracle (every pattern matched against the tree's indexes, minus the exclusions, plus the plain
// items); read at most the entries bound; and raise only when the oracle has more names than the bound, more than
// max_patterns patterns are named, an item is too long, or the walk read exactly its bound. The first item is also a
// path for check_new_index, which must refuse exactly what the oracle refuses.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "engine/error.h"
#include "index_path.h"
#include "index_pattern.h"

namespace {

namespace fs = std::filesystem;

std::string root;
std::vector<std::string> listed;   // the indexes a pattern can list
std::vector<std::string> indexes;  // all the indexes, the dot-led too

void make(const std::string& rel, bool index) {
	fs::create_directories(root + "/" + rel);
	if (index) {
		fs::create_directories(root + "/" + rel + "/.__1");
	}
}

void init() {
	root = std::string(std::getenv("FUZZ_ROOT")) + "/";
	fs::remove_all(root);
	fs::create_directories(root);
	for (const char* name : {"a", "ab", "b", "c/d", "c/e", "c/f/g", "x.y", "a b", "-d", "*s", "z/z/z/z", "q/r"}) {
		make(name, true);
		listed.push_back(name);
	}
	indexes = listed;
	for (const char* name : {".dot", "c/.hid"}) {
		make(name, true);
		indexes.push_back(name);
	}
	make("hidden", false);
	make("empty/empty2", false);
	fs::create_directory_symlink("/", root + "linkroot");   // a link is never followed
	fs::create_directory_symlink(root + "c", root + "linkc");
	std::sort(listed.begin(), listed.end());
}

bool is_wild(const std::string& s) {
	return s == "_all" || s.find('*') != std::string::npos;
}

bool starts_with(const std::string& s, const std::string& p) {
	return s.compare(0, p.size(), p) == 0;
}

// What check_new_index must do for a clean path (no empty or dot components, not through a link).
bool refused(const std::string& path) {
	std::vector<std::string> parts;
	std::string cur;
	for (char c : path + "/") {
		if (c == '/') { parts.push_back(cur); cur.clear(); } else { cur.push_back(c); }
	}
	for (std::size_t i = 1; i < parts.size(); ++i) {
		if (parts[i] == "strata" || starts_with(parts[i], ".__")) {
			return true;
		}
	}
	const auto is_index = [&](const std::string& p) { return std::find(indexes.begin(), indexes.end(), p) != indexes.end(); };
	for (std::size_t i = 1; i < parts.size(); ++i) {
		std::string prefix;
		for (std::size_t j = 0; j < i; ++j) { prefix += (j ? "/" : "") + parts[j]; }
		if (is_index(prefix)) {
			return true;
		}
	}
	if (is_index(path)) {
		return false;
	}
	for (const auto& name : listed) {   // the walk lists no dot-led name
		if (starts_with(name, path + "/")) {
			return true;
		}
	}
	return false;
}

bool clean(const std::string& path) {
	if (path.empty() || path.size() > 64 || starts_with(path, "link")) {
		return false;
	}
	std::string cur;
	for (char c : path + "/") {
		if (c == '\0' || (c >= 'A' && c <= 'Z') || static_cast<unsigned char>(c) >= 0x80) { return false; }   // a file system may fold case and normalise
		if (c == '/') {
			if (cur.empty() || cur == "." || cur == "..") { return false; }
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	return index_path::valid(path);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* d, std::size_t n) {
	static const bool once = (init(), true);
	(void)once;
	if (n < 2 || n > 600) {
		return 0;
	}
	const std::size_t names_bound = 1 + d[0] % 40;
	const std::size_t visits_bound = 1 + d[1] % 80;
	std::vector<std::string> items;
	std::string cur;
	for (std::size_t i = 2; i < n; ++i) {
		if (d[i] == '\n') { items.push_back(cur); cur.clear(); } else { cur.push_back(static_cast<char>(d[i])); }
	}
	items.push_back(cur);
	if (items.size() > 40) {
		items.resize(40);
	}

	std::set<std::string> plain, patterns;
	std::vector<std::string> excl, wild;
	bool too_long = false;
	for (const auto& item : items) {
		too_long = too_long || item.size() > index_pattern::max_pattern_size;
		if (item.size() > 1 && item[0] == '-') {
			excl.push_back(item.substr(1));
			patterns.insert(item);
		} else if (is_wild(item)) {
			wild.push_back(item == "_all" ? "*" : item);
			patterns.insert(item);
		} else {
			plain.insert(item);
		}
	}
	const auto removed = [&](const std::string& s) {
		return std::any_of(excl.begin(), excl.end(), [&](const std::string& e) { return index_pattern::match(e, s); });
	};
	std::set<std::string> expect;
	for (const auto& w : wild) {
		for (const auto& t : listed) {
			if (index_pattern::match(w, t) && !removed(t)) { expect.insert(t); }
		}
	}
	for (const auto& p : plain) {
		if (!removed(p)) { expect.insert(p); }
	}

	std::size_t calls = 0;
	std::vector<std::string> out;
	bool threw = false;
	try {
		out = index_pattern::expand(root, items, {names_bound, visits_bound, std::size_t{1} << 40}, [&] { ++calls; });
	} catch (const engine::Error&) {
		threw = true;
	}
	if (calls > visits_bound) {
		std::abort();
	}
	if (threw) {
		const bool has_wild = !wild.empty();
		if (!(expect.size() > names_bound || too_long || patterns.size() > index_pattern::max_patterns || (has_wild && calls == visits_bound))) {
			std::fprintf(stderr, "spurious throw: %zu names, bound %zu, %zu patterns, %zu of %zu entries\n", expect.size(), names_bound, patterns.size(), calls, visits_bound);
			std::abort();
		}
	} else {
		if (!std::is_sorted(out.begin(), out.end()) || std::adjacent_find(out.begin(), out.end()) != out.end()) {
			std::abort();
		}
		for (const auto& name : out) {
			if (plain.count(name) == 0 && (!index_path::valid(name) || name[0] == '/' || name[0] == '.' || !std::binary_search(listed.begin(), listed.end(), name))) {
				std::fprintf(stderr, "unexpected name %s\n", name.c_str());
				std::abort();
			}
		}
		if (std::set<std::string>(out.begin(), out.end()) != expect) {
			std::fprintf(stderr, "mismatch, names bound %zu\n", names_bound);
			for (const auto& i : items) { std::fprintf(stderr, "  item [%s]\n", i.c_str()); }
			std::abort();
		}
	}

	if (clean(items[0])) {
		bool did = false;
		try {
			index_pattern::check_new_index(root, items[0], 1000);
		} catch (const engine::Error&) {
			did = true;
		}
		if (did != refused(items[0])) {
			std::fprintf(stderr, "check_new_index(%s) = %d\n", items[0].c_str(), did);
			std::abort();
		}
	}
	return 0;
}
