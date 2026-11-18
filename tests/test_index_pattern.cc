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


// The index patterns a search may name (docs/api.md, "Multiple Indexes"): the glob, and the expansion against the
// indexes on the node's disk.

#include "test_harness.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "database/name_index.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/sync.h"
#include "index_pattern.h"
#include "opts.h"
#include "task.hh"
#include "walk_gate.h"

namespace {

namespace fs = std::filesystem;
using index_pattern::expand;
using index_pattern::match;

void test_match() {
	CHECK_TRUE(match("a", "a"));
	CHECK_FALSE(match("a", "ab"));
	CHECK_FALSE(match("ab", "a"));
	CHECK_TRUE(match("*", ""));
	CHECK_TRUE(match("*", "anything at all"));
	CHECK_TRUE(match("test*", "test"));
	CHECK_TRUE(match("test*", "test1"));
	CHECK_FALSE(match("test*", "tes"));
	CHECK_TRUE(match("*test", "mytest"));
	CHECK_FALSE(match("*test", "testy"));
	CHECK_TRUE(match("te*t", "test"));
	CHECK_TRUE(match("te*t", "tet"));
	CHECK_TRUE(match("te*t", "te12t"));
	CHECK_FALSE(match("te*t", "te12"));
	CHECK_TRUE(match("*test*", "a test b"));
	CHECK_TRUE(match("a*b*c", "aXbYc"));
	CHECK_TRUE(match("a*b*c", "abbc"));
	CHECK_FALSE(match("a*b*c", "acb"));
	CHECK_TRUE(match("a**b", "ab"));
	CHECK_FALSE(match("", "a"));
	CHECK_TRUE(match("", ""));
	// A `*` and a `?` stay in one segment of the path.
	CHECK_FALSE(match("a*", "a/b"));
	CHECK_FALSE(match("*", "a/b"));
	CHECK_FALSE(match("*b", "a/b"));
	CHECK_FALSE(match("a?b", "a/b"));
	CHECK_TRUE(match("a/*", "a/b"));
	CHECK_TRUE(match("*/*", "a/b"));
	CHECK_FALSE(match("a/*", "a/b/c"));
	CHECK_FALSE(match("a/*", "a"));
	CHECK_TRUE(match("a*/b*", "ax/by"));
	CHECK_FALSE(match("a*/b*", "ax/cy"));
	CHECK_TRUE(match("some-??", "some-ab"));
	CHECK_FALSE(match("some-??", "some-a"));
	CHECK_FALSE(match("some-??", "some-abc"));
	CHECK_TRUE(match("?", "\xc3\xa9"));          // a character is a UTF-8 sequence
	CHECK_FALSE(match("??", "\xc3\xa9"));
	CHECK_TRUE(match("*\xc3\xa9?", "x\xc3\xa9y"));
	CHECK_TRUE(match("a?c", "a\xe2\x82\xac" "c"));
	// A class is one character of a set.
	CHECK_TRUE(match("[abc]", "b"));
	CHECK_FALSE(match("[abc]", "d"));
	CHECK_FALSE(match("[abc]", "ab"));
	CHECK_TRUE(match("[a-c]x", "bx"));
	CHECK_FALSE(match("[a-c]x", "dx"));
	CHECK_TRUE(match("[!a-c]x", "dx"));
	CHECK_TRUE(match("[^a-c]x", "dx"));
	CHECK_FALSE(match("[!a-c]x", "bx"));
	CHECK_TRUE(match("[]]", "]"));                 // a `]` first is a member
	CHECK_TRUE(match("[!]]", "a"));
	CHECK_FALSE(match("[!]]", "]"));
	CHECK_TRUE(match("[a-]", "-"));                // a `-` last is a member
	CHECK_TRUE(match("[a-]", "a"));
	CHECK_TRUE(match("[\xc3\xa0-\xc3\xbf]", "\xc3\xa9"));   // a range of characters
	CHECK_TRUE(match("a[0-9]*", "a7zz"));
	CHECK_FALSE(match("a[0-9]*", "az"));
	CHECK_TRUE(match("[", "["));                   // no closing `]`: a plain `[`
	CHECK_TRUE(match("a[b", "a[b"));
	CHECK_FALSE(match("a[b", "ab"));
	CHECK_TRUE(match("[]", "[]"));
	CHECK_FALSE(match("[a-c]", "a/c"));            // a class never matches `/`
	CHECK_TRUE(match("*[0-9]", "ab12"));
	CHECK_FALSE(match("*[0-9]", "ab12x"));
	// `**` is a whole segment of any number of segments (none included); inside a segment it is a `*`.
	CHECK_TRUE(match("**", "a"));
	CHECK_TRUE(match("**", "a/b/c"));
	CHECK_TRUE(match("**/some-??", "some-ab"));
	CHECK_TRUE(match("**/some-??", "x/some-ab"));
	CHECK_TRUE(match("**/some-??", "x/y/z/some-ab"));
	CHECK_FALSE(match("**/some-??", "some-abc"));
	CHECK_FALSE(match("**/some-??", "some-ab/x"));
	CHECK_TRUE(match("a/**", "a"));
	CHECK_TRUE(match("a/**", "a/b"));
	CHECK_TRUE(match("a/**", "a/b/c"));
	CHECK_FALSE(match("a/**", "b/a"));
	CHECK_FALSE(match("a/**", "ab"));
	CHECK_TRUE(match("a/**/b", "a/b"));
	CHECK_TRUE(match("a/**/b", "a/x/y/b"));
	CHECK_FALSE(match("a/**/b", "a/x/y/c"));
	CHECK_FALSE(match("a/**/b", "a/x/y/b/c"));
	CHECK_TRUE(match("a**b", "aXb"));
	CHECK_FALSE(match("a**b", "a/b"));
	CHECK_TRUE(match("abc-*", "abc-x"));
	CHECK_FALSE(match("abc-*", "d/abc-x"));       // one level
	// More than one `**`.
	CHECK_TRUE(match("**/some-*/**/nested-??", "some-1/nested-ab"));
	CHECK_TRUE(match("**/some-*/**/nested-??", "x/some-1/y/z/nested-ab"));
	CHECK_TRUE(match("**/some-*/**/nested-??", "x/some-1/some-2/nested-ab"));
	CHECK_FALSE(match("**/some-*/**/nested-??", "x/other/nested-ab"));
	CHECK_FALSE(match("**/some-*/**/nested-??", "some-1/nested-abc"));
	CHECK_FALSE(match("**/some-*/**/nested-??", "nested-ab/some-1"));
	CHECK_TRUE(match("**/a/**/b/**", "a/b"));
	CHECK_TRUE(match("**/a/**/b/**", "x/a/y/b/z"));
	CHECK_FALSE(match("**/a/**/b/**", "b/a"));
	CHECK_TRUE(match("**/**/**/a", "a"));
	CHECK_TRUE(match("**/**/**/a", "x/y/a"));
	CHECK_FALSE(match("**/**/**/a", "x/y/b"));
	// The names: `_all` is `**`, and a `?` or a class makes a pattern.
	CHECK_TRUE(index_pattern::is_wildcard("_all"));
	CHECK_TRUE(index_pattern::is_wildcard("a*"));
	CHECK_TRUE(index_pattern::is_wildcard("a?"));
	CHECK_TRUE(index_pattern::is_wildcard("a[bc]"));
	CHECK_TRUE(index_pattern::is_wildcard("**"));
	CHECK_FALSE(index_pattern::is_wildcard("a"));
	CHECK_FALSE(index_pattern::is_wildcard("a[b"));      // no closing `]`
	CHECK_FALSE(index_pattern::is_wildcard("a[]"));
	CHECK_FALSE(index_pattern::is_wildcard("_allx"));
	CHECK_FALSE(index_pattern::is_wildcard("a/b"));
	// A pattern built to make a matcher backtrack: bounded by the pattern's size, so it ends.
	CHECK_FALSE(match(std::string(100, 'a') + std::string("*a*a*a*a*a*a*b"), std::string(2000, 'a')));
}

// `**` is a loop of an automaton over the segments, never a backtrack: the work for a path is its segments times the
// pattern's, however many `**` the pattern holds.
void test_many_globstars_stay_linear() {
	std::string deep = "x";
	for (int i = 0; i < 1999; ++i) {
		deep += "/x";
	}
	for (const std::string pattern : {"**/**/**/a", "**/*/**/*/**/*/**/*/**/a", "**/x/**/x/**/x/**/x/**/x/**/x/**/x/**/x/b", "**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/**/a"}) {
		std::size_t steps = 0;
		CHECK_FALSE(match(pattern, deep, steps));
		std::size_t segments = 1;
		for (char c : pattern) {
			segments += c == '/';
		}
		CHECK_LE(steps, std::size_t{2000} * (segments + 2) * 4);      // segments of the path x segments of the pattern, a few steps each
		CHECK_GE(steps, std::size_t{2000});
	}
	std::size_t steps = 0;
	CHECK_TRUE(match("**/**/**/a", deep + "/a", steps));
	CHECK_LE(steps, std::size_t{2000} * 12);
	// Many `**` are one: `**/**` is `**`.
	steps = 0;
	CHECK_FALSE(match("**/**/**/**/**/**/**/**/**/**/**/**/a", deep, steps));
	CHECK_LE(steps, std::size_t{2000} * 8);
	// A segment that backtracks is still bounded by its length times the name's.
	steps = 0;
	CHECK_FALSE(match("**/*a*a*a*a*a*a*b/**", std::string(200, 'a'), steps));
	CHECK_LE(steps, std::size_t{200} * 40);
}

// A node's data directory: a1 (one shard), a2 (shards 1 and 2), a3 (only shard 3), b1, c/d1, c/d2, a stray
// directory, a hidden index, and a file.
struct Tree {
	std::string root = "pattern_test_root/";
	Tree() {
		fs::remove_all(root);
		for (const char* index : {"a1/strata", "a2/.__1/strata", "a2/.__2/strata", "a3/.__3/strata", "b1/strata", "c/d1/strata", "c/d2/strata", ".hidden/strata", "e/empty"}) {
			fs::create_directories(root + index);
		}
		std::ofstream(root + "afile") << "x";
	}
	~Tree() { fs::remove_all(root); }
};

using Names = std::vector<std::string>;

std::size_t reads(const std::string& root, const Names& items, std::size_t visits = 1000) {
	std::size_t calls = 0;
	expand(root, items, {100, visits}, [&] { ++calls; });
	return calls;
}

Names names(const Tree& tree, const Names& items, std::size_t limit = 100, std::size_t visits = 1000) {
	return expand(tree.root, items, {limit, visits});
}

void test_expand() {
	Tree tree;
	const Names all = {"a1", "a2", "a3", "b1", "c/d1", "c/d2"};
	CHECK_TRUE(names(tree, {"_all"}) == all);
	CHECK_TRUE(names(tree, {"*"}) == (Names{"a1", "a2", "a3", "b1"}));   // one segment: c is no index
	CHECK_TRUE(names(tree, {"a*"}) == (Names{"a1", "a2", "a3"}));
	CHECK_TRUE(names(tree, {"*1"}) == (Names{"a1", "b1"}));
	CHECK_TRUE(names(tree, {"**/*1"}) == (Names{"a1", "b1", "c/d1"}));
	CHECK_TRUE(names(tree, {"*d*"}).empty());                         // a star stays in its segment: c is no index
	CHECK_TRUE(names(tree, {"**/*d*"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c/*"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c*"}).empty());
	CHECK_TRUE(names(tree, {"c/**"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c/d?"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c/d[1]"}) == (Names{"c/d1"}));
	CHECK_TRUE(names(tree, {"a*", "-a2"}) == (Names{"a1", "a3"}));
	CHECK_TRUE(names(tree, {"-a2", "a*"}) == (Names{"a1", "a3"}));   // an exclusion applies wherever it stands
	CHECK_TRUE(names(tree, {"_all", "-a*", "-c/*"}) == (Names{"b1"}));
	CHECK_TRUE(names(tree, {"_all", "-**/d?"}) == (Names{"a1", "a2", "a3", "b1"}));
	CHECK_TRUE(names(tree, {"b1", "a*", "b*"}) == (Names{"a1", "a2", "a3", "b1"}));   // sorted, no repeats
	CHECK_TRUE(names(tree, {"zzz", "a1"}) == (Names{"a1", "zzz"}));   // a plain name stands for itself, there or not
	CHECK_TRUE(names(tree, {"zzz", "-zzz"}).empty());
	CHECK_TRUE(names(tree, {"q*"}).empty());
	CHECK_TRUE(names(tree, {".*"}).empty());        // the node's own indexes are not listed by a pattern
	CHECK_TRUE(names(tree, {".hidden"}) == (Names{".hidden"}));   // ... but they can be named
	CHECK_TRUE(names(tree, {"e*"}).empty());        // a directory that is no index
	CHECK_TRUE(names(tree, {"afile*"}).empty());    // a file is no index
	CHECK_TRUE(names(tree, {"-"}) == (Names{"-"}));  // a lone dash is a name
	CHECK_TRUE(expand(tree.root.substr(0, tree.root.size() - 1), Names{"b*"}, {10, 100}) == (Names{"b1"}));   // the root needs no slash
}

void test_the_limit() {
	Tree tree;
	CHECK_EQ(names(tree, {"_all"}, 6).size(), std::size_t{6});
	CHECK_THROW(names(tree, {"_all"}, 5), engine::Error);
	CHECK_THROW(names(tree, {"a*", "b*", "c/*"}, 5), engine::Error);
	CHECK_EQ(names(tree, {"_all", "-c/*"}, 4).size(), std::size_t{4});   // the names removed do not count as names
	CHECK_THROW(names(tree, {"x1", "x2", "x3"}, 2), engine::Error);
	try {
		names(tree, {"_all"}, 5);
	} catch (const engine::Error& error) {
		CHECK_EQ(error.kind, engine::errc::invalid);
		CHECK_TRUE(error.description().find("--max-search-indexes") != std::string::npos);
	}
	// A pattern over the size a pattern may have.
	CHECK_THROW(names(tree, {std::string(index_pattern::max_pattern_size + 1, 'a') + "*"}), engine::Error);
	CHECK_TRUE(names(tree, {std::string(index_pattern::max_pattern_size - 1, 'a') + "*"}).empty());
}

// A tree of `n` index directories under one level, `d` directories with an index below them each.
struct Wide {
	std::string root = "pattern_wide_root/";
	explicit Wide(int n) {
		fs::remove_all(root);
		for (int i = 0; i < n; ++i) {
			fs::create_directories(root + "i" + std::to_string(1000 + i) + "/strata");
		}
	}
	~Wide() { fs::remove_all(root); }
};

// SR1: the work of the walk is bounded by the entries it reads, not by the names that survive.
void test_the_walk_is_bounded() {
	Wide wide(50);
	// `*` and its own exclusion leave no name, and still read the 50 entries: the budget trips on the work.
	CHECK_THROW(expand(wide.root, Names{"*", "-*"}, {100, 49}), engine::Error);
	CHECK_TRUE(expand(wide.root, Names{"*", "-*"}, {100, 50}).empty());
	CHECK_THROW(expand(wide.root, Names{"i1*", "-i1*"}, {100, 49}), engine::Error);
	try {
		expand(wide.root, Names{"_all", "-_all"}, {100, 10});
		CHECK_TRUE(false);
	} catch (const engine::Error& error) {
		CHECK_EQ(error.kind, engine::errc::invalid);
		CHECK_TRUE(error.description().find("--max-search-indexes") != std::string::npos);
		CHECK_TRUE(error.description().find("directory entries") != std::string::npos);
	}
	// The entries read are counted as they are read: the walk stops at the budget, it does not list the level first.
	std::size_t calls = 0;
	CHECK_THROW(expand(wide.root, Names{"*", "-*"}, {100, 7}, [&] { ++calls; }), engine::Error);
	CHECK_EQ(calls, std::size_t{7});
	// Names that are only plain items read nothing.
	calls = 0;
	CHECK_EQ(expand(wide.root, Names{"x", "y", "z"}, {100, 0}, [&] { ++calls; }).size(), std::size_t{3});
	CHECK_EQ(calls, std::size_t{0});
}

// SR1: the work of matching is bounded as well: patterns built to backtrack over long names cost the pattern's size
// times the name's on every entry, however few entries there are.
void test_the_matching_is_bounded() {
	fs::remove_all("pattern_long_root");
	for (int i = 0; i < 50; ++i) {
		fs::create_directories("pattern_long_root/" + std::string(150, 'a') + std::to_string(1000 + i) + "/strata");
	}
	Names slow = {"*"};   // matches every name, so every exclusion is tried on every entry
	for (int k = 0; k < 31; ++k) {
		slow.push_back("-*" + std::string(100 + k, 'a') + "c*");
	}
	// 50 entries x 31 patterns x about 15,000 steps is far past the 4,096 steps an entry may cost.
	CHECK_THROW(expand("pattern_long_root/", slow, {100, 1000}), engine::Error);
	try {
		expand("pattern_long_root/", slow, {100, 1000});
	} catch (const engine::Error& error) {
		CHECK_EQ(error.kind, engine::errc::invalid);
		CHECK_TRUE(error.description().find("steps") != std::string::npos);
	}
	Names slow_wanted;   // the same, as wanted patterns none of which matches
	for (int k = 0; k < 32; ++k) {
		slow_wanted.push_back("*" + std::string(100 + k, 'a') + "c*");
	}
	CHECK_THROW(expand("pattern_long_root/", slow_wanted, {100, 1000}), engine::Error);
	CHECK_TRUE(expand("pattern_long_root/", slow_wanted, {100, 1000, std::size_t{1} << 40}).empty());
	CHECK_EQ(expand("pattern_long_root/", slow, {100, 1000, std::size_t{1} << 40}).size(), std::size_t{50});   // with the room it is served
	CHECK_EQ(expand("pattern_long_root/", Names{"a*"}, {100, 1000}).size(), std::size_t{50});   // an ordinary pattern costs a step a byte
	// The exclusions of plain names are matched too, and counted.
	Names plain = slow;
	plain.erase(plain.begin());
	for (int i = 0; i < 50; ++i) {
		plain.push_back(std::string(150, 'a') + std::to_string(i));
	}
	CHECK_THROW(expand("pattern_long_root/", plain, {100, 1000, 100000}), engine::Error);
	fs::remove_all("pattern_long_root");
	std::size_t steps = 0;
	CHECK_TRUE(match("abc", "abc", steps));
	CHECK_EQ(steps, std::size_t{4});   // a step for the position and a step a byte of the name
	steps = 0;
	CHECK_FALSE(match(std::string(100, 'a') + "b", std::string(100, 'a') + "c", steps));
	CHECK_TRUE(steps >= 100);
}

// SR1: the entries a directory holds are read, and counted, to decide whether it is an index.
void test_the_entries_of_a_candidate_are_counted() {
	fs::remove_all("pattern_entries_root");
	const std::string root = "pattern_entries_root/";
	fs::create_directories(root + "box");
	for (int i = 0; i < 40; ++i) {
		std::ofstream(root + "box/f" + std::to_string(i)) << "x";
	}
	// `bo*` reads `box` (1) and the 40 entries that show it is no index (40).
	CHECK_THROW(expand(root, Names{"bo*"}, {100, 40}), engine::Error);
	CHECK_TRUE(expand(root, Names{"bo*"}, {100, 41}).empty());
	CHECK_EQ(reads(root, {"bo*"}), std::size_t{41});
	// A pattern that goes into the directory reads its entries once.
	CHECK_EQ(reads(root, {"**"}), std::size_t{41});
	CHECK_EQ(reads(root, {"**/zz"}), std::size_t{41});
	fs::remove_all("pattern_entries_root");
}

// SR1: one walk serves all the patterns; the entries read do not depend on how many patterns there are.
void test_one_walk_for_all_the_items() {
	Wide wide(50);
	const auto reads = [&](const Names& items) {
		std::size_t calls = 0;
		expand(wide.root, items, {100, 1000}, [&] { ++calls; });
		return calls;
	};
	const auto one = reads({"i1*"});
	CHECK_EQ(one, std::size_t{50});
	Names many;
	for (int i = 0; i < 30; ++i) {
		many.push_back("i10" + std::to_string(i % 5) + "*");
		many.push_back("-i1" + std::to_string(i) + "x");
	}
	many.resize(index_pattern::max_patterns);
	CHECK_EQ(reads(many), one);
	// ... and each name still comes out for the patterns that match it.
	CHECK_EQ(expand(wide.root, Names{"i1000", "i1001*", "i1002"}, {100, 1000}).size(), std::size_t{3});
	CHECK_TRUE(expand(wide.root, Names{"i100*", "i101*", "-i1001", "-i1012"}, {100, 1000}) == (Names{"i1000", "i1002", "i1003", "i1004", "i1005", "i1006", "i1007", "i1008", "i1009", "i1010", "i1011", "i1013", "i1014", "i1015", "i1016", "i1017", "i1018", "i1019"}));
}

// SR1: the number of patterns is bounded.
void test_the_patterns_are_bounded() {
	Wide wide(3);
	Names items;
	for (std::size_t i = 0; i < index_pattern::max_patterns; ++i) {
		items.push_back("p" + std::to_string(i) + "*");
	}
	CHECK_TRUE(expand(wide.root, items, {100, 1000}).empty());
	items.push_back("-q");
	CHECK_THROW(expand(wide.root, items, {100, 1000}), engine::Error);
	items.resize(2);
	for (int i = 0; i < 5000; ++i) {
		items.push_back("name" + std::to_string(i));   // plain names are not patterns: the names bound counts them
	}
	CHECK_EQ(expand(wide.root, items, {5000, 1000}).size(), std::size_t{5000});
	CHECK_THROW(expand(wide.root, items, {4999, 1000}), engine::Error);
}

// SR1: the walk looks at the request between entries.
void test_the_walk_can_be_cancelled() {
	Wide wide(50);
	struct Stop { };
	std::size_t calls = 0;
	CHECK_THROW(expand(wide.root, Names{"*"}, {100, 1000}, [&] { if (++calls == 5) { throw Stop{}; } }), Stop);
	CHECK_EQ(calls, std::size_t{5});
}

// SR4: equal items count once.
void test_equal_items_count_once() {
	Tree tree;
	CHECK_TRUE(names(tree, {"a*", "a*"}, 3) == (Names{"a1", "a2", "a3"}));
	CHECK_TRUE(names(tree, {"a*", "a*", "a1", "a1", "a*"}, 3) == (Names{"a1", "a2", "a3"}));   // a name in a pattern and by itself too
	CHECK_TRUE(names(tree, {"a*", "a2*", "a*", "-b1", "-b1"}, 3) == (Names{"a1", "a2", "a3"}));
	CHECK_THROW(names(tree, {"a*"}, 2), engine::Error);
}

// A node's data directory with indexes inside indexes: `twitter` holds `twitter/user` (which holds `twitter/user/lists`)
// and a sharded `twitter/tweet`; `tenant` is no index, `tenant/a` and `tenant/b` are; logs three levels down; and
// indexes at several depths for the `**` patterns. 7 entries at the top, 37 in all.
struct Nested {
	std::string root = "pattern_nested_root/";
	Nested() {
		fs::remove_all(root);
		for (const char* index : {"twitter/strata", "twitter/user/strata", "twitter/user/lists/strata", "twitter/tweet/.__1/strata",
				"twitter/tweet/.__2/strata", "twitter/.hid/strata", "tenant/a/strata", "tenant/b/strata", "logs/2026/jan/strata",
				"logs/2026/feb/strata", "logs/2025/jan/strata", "some-ab/strata", "deep/x/y/some-cd/strata",
				"deep/some-ef/mid/nested-xy/strata", ".hid/strata"}) {
			fs::create_directories(root + index);
		}
		std::ofstream(root + "afile") << "x";
	}
	~Nested() { fs::remove_all(root); }
};

Names nested_names(const Nested& tree, const Names& items) {
	return expand(tree.root, items, {100, 1000});
}

// An index may hold indexes: the walk goes into an index directory too and lists its children as indexes.
void test_nested_indexes_are_listed() {
	Nested tree;
	const Names every = {"deep/some-ef/mid/nested-xy", "deep/x/y/some-cd", "logs/2025/jan", "logs/2026/feb", "logs/2026/jan", "some-ab",
		"tenant/a", "tenant/b", "twitter", "twitter/tweet", "twitter/user", "twitter/user/lists"};
	CHECK_TRUE(nested_names(tree, {"_all"}) == every);
	CHECK_TRUE(nested_names(tree, {"**"}) == every);
	CHECK_TRUE(nested_names(tree, {"*"}) == (Names{"some-ab", "twitter"}));            // one segment, the parent only
	CHECK_TRUE(nested_names(tree, {"twitter*"}) == (Names{"twitter"}));
	CHECK_TRUE(nested_names(tree, {"twitter/*"}) == (Names{"twitter/tweet", "twitter/user"}));
	CHECK_TRUE(nested_names(tree, {"twitter/**"}) == (Names{"twitter", "twitter/tweet", "twitter/user", "twitter/user/lists"}));
	CHECK_TRUE(nested_names(tree, {"twitter/*/*"}) == (Names{"twitter/user/lists"}));
	CHECK_TRUE(nested_names(tree, {"**/user"}) == (Names{"twitter/user"}));
	CHECK_TRUE(nested_names(tree, {"**/some-??"}) == (Names{"deep/x/y/some-cd", "some-ab"}));   // deep/some-ef is no index
	CHECK_TRUE(nested_names(tree, {"**/some-*/**/nested-??"}) == (Names{"deep/some-ef/mid/nested-xy"}));
	CHECK_TRUE(nested_names(tree, {"**/some-*/**/*-??"}) == (Names{"deep/some-ef/mid/nested-xy"}));
	CHECK_TRUE(nested_names(tree, {"logs/2026/*"}) == (Names{"logs/2026/feb", "logs/2026/jan"}));
	CHECK_TRUE(nested_names(tree, {"**/jan"}) == (Names{"logs/2025/jan", "logs/2026/jan"}));
	CHECK_TRUE(nested_names(tree, {"**/[ab]"}) == (Names{"tenant/a", "tenant/b"}));
	CHECK_TRUE(nested_names(tree, {"tenant/[!a]"}) == (Names{"tenant/b"}));
	CHECK_TRUE(nested_names(tree, {"tenant"}) == (Names{"tenant"}));                   // a name stands for itself, index or not
	CHECK_TRUE(nested_names(tree, {"tenant*"}).empty());                                // tenant holds indexes and is none
	CHECK_TRUE(nested_names(tree, {"**", "-twitter/**"}) == (Names{"deep/some-ef/mid/nested-xy", "deep/x/y/some-cd", "logs/2025/jan", "logs/2026/feb", "logs/2026/jan", "some-ab", "tenant/a", "tenant/b"}));
	CHECK_TRUE(nested_names(tree, {"twitter*", "twitter/user/*"}) == (Names{"twitter", "twitter/user/lists"}));
	CHECK_TRUE(nested_names(tree, {"**/.hid"}).empty());                                // the node's own are not listed, nested or not
	CHECK_TRUE(nested_names(tree, {"**/strata"}).empty());                              // the engine's directories are not indexes
	CHECK_TRUE(nested_names(tree, {"**/.__?"}).empty());
	CHECK_TRUE(nested_names(tree, {"twitter/tweet/**"}) == (Names{"twitter/tweet"}));
	CHECK_TRUE(nested_names(tree, {"**/**/**/a"}) == (Names{"tenant/a"}));
}

// SR2: creating an index never hides the indexes below it: the walk goes into an index directory.
void test_an_index_does_not_hide_the_ones_below_it() {
	Nested tree;
	fs::create_directories(tree.root + "tenant/strata");            // `PUT /tenant/d`: tenant is an index now
	CHECK_TRUE(nested_names(tree, {"tenant/**"}) == (Names{"tenant", "tenant/a", "tenant/b"}));
	CHECK_TRUE(nested_names(tree, {"tenant*"}) == (Names{"tenant"}));
	CHECK_TRUE(nested_names(tree, {"**/[ab]"}) == (Names{"tenant/a", "tenant/b"}));
	// Five shards instead of one: the same.
	fs::remove_all(tree.root + "tenant/strata");
	for (int shard = 1; shard <= 5; ++shard) {
		fs::create_directories(tree.root + "tenant/.__" + std::to_string(shard) + "/strata");
	}
	CHECK_TRUE(nested_names(tree, {"tenant/**"}) == (Names{"tenant", "tenant/a", "tenant/b"}));
	// A child named `strata` is the engine's own directory of the index: it is not a nested index (and hides nothing else).
	fs::create_directories(tree.root + "tenant/strata/strata");
	CHECK_TRUE(nested_names(tree, {"tenant/**"}) == (Names{"tenant", "tenant/a", "tenant/b"}));
	// At the top, `strata` is an ordinary name: an index called so is listed.
	fs::create_directories(tree.root + "strata/strata");
	CHECK_TRUE(nested_names(tree, {"strata"}) == (Names{"strata"}));
	CHECK_TRUE(nested_names(tree, {"stra*"}) == (Names{"strata"}));
	CHECK_EQ(nested_names(tree, {"**"}).size(), std::size_t{14});
	fs::remove_all(tree.root + "strata");
	// A name that is a prefix of another's: both are indexes, and each pattern finds what its segments say.
	fs::create_directories(tree.root + "twitter2/strata");
	CHECK_TRUE(nested_names(tree, {"twitter*"}) == (Names{"twitter", "twitter2"}));
	CHECK_TRUE(nested_names(tree, {"twitter/**"}) == (Names{"twitter", "twitter/tweet", "twitter/user", "twitter/user/lists"}));
}

// The walk goes into a directory only while a pattern can still match below it, and counts every entry it reads.
void test_the_walk_goes_only_where_a_pattern_can_match() {
	Nested tree;
	CHECK_EQ(reads(tree.root, {"_all"}), std::size_t{37});                  // every entry of the tree, the index directories' too
	CHECK_EQ(reads(tree.root, {"twitter*"}), std::size_t{7});               // the top level only
	CHECK_EQ(reads(tree.root, {"logs/2026/*"}), std::size_t{7 + 2 + 2});    // the top, logs, logs/2026: no other directory
	CHECK_EQ(reads(tree.root, {"twitter/*"}), std::size_t{7 + 4});          // the top, twitter
	CHECK_EQ(reads(tree.root, {"twitter/user/*"}), std::size_t{7 + 4 + 2});
	CHECK_EQ(reads(tree.root, {"**/some-??"}), std::size_t{37});            // a `**` goes everywhere
	CHECK_EQ(reads(tree.root, {"logs/2026/*", "twitter/*"}), std::size_t{7 + 2 + 2 + 4});   // one walk for both
	CHECK_EQ(reads(tree.root, {"nomatch/*"}), std::size_t{7});
	// The budget counts all of those, at every depth: 36 entries pass, 37 do not.
	CHECK_THROW(expand(tree.root, Names{"_all"}, {100, 36}), engine::Error);
	CHECK_EQ(expand(tree.root, Names{"_all"}, {100, 37}).size(), std::size_t{12});
	CHECK_THROW(expand(tree.root, Names{"_all", "-_all"}, {100, 36}), engine::Error);
	CHECK_THROW(expand(tree.root, Names{"logs/2026/*"}, {100, 10}), engine::Error);
	CHECK_EQ(expand(tree.root, Names{"logs/2026/*"}, {100, 11}).size(), std::size_t{2});
}

// SR1: nesting does not let a tree escape the budget: a deep chain and a wide one are read for at most the budget.
void test_a_deep_tree_stays_within_the_budget() {
	fs::remove_all("pattern_deep_root");
	std::string path = "pattern_deep_root";
	for (int depth = 0; depth < 60; ++depth) {
		path += "/d";
		fs::create_directories(path + "/strata");     // an index inside an index, 60 deep
	}
	for (std::size_t budget : {10u, 59u, 100u}) {
		std::size_t calls = 0;
		try {
			expand("pattern_deep_root/", Names{"_all"}, {1000, budget}, [&] { ++calls; });
		} catch (const engine::Error&) {
		}
		CHECK_LE(calls, budget);
	}
	CHECK_THROW(expand("pattern_deep_root/", Names{"_all"}, {1000, 119}), engine::Error);   // 60 `d` and 60 `strata`, minus the last d's ... 120 entries
	CHECK_EQ(expand("pattern_deep_root/", Names{"_all"}, {1000, 120}).size(), std::size_t{60});
	CHECK_EQ(expand("pattern_deep_root/", Names{"**/d"}, {1000, 120}).size(), std::size_t{60});
	// A fan-out of 30 under 30: 930 directories.
	fs::remove_all("pattern_deep_root");
	for (int i = 0; i < 30; ++i) {
		for (int j = 0; j < 30; ++j) {
			fs::create_directories("pattern_deep_root/i" + std::to_string(i) + "/j" + std::to_string(j) + "/strata");
		}
		fs::create_directories("pattern_deep_root/i" + std::to_string(i) + "/strata");
	}
	std::size_t calls = 0;
	CHECK_THROW(expand("pattern_deep_root/", Names{"_all", "-_all"}, {10000, 500}, [&] { ++calls; }), engine::Error);
	CHECK_EQ(calls, std::size_t{500});
	CHECK_EQ(expand("pattern_deep_root/", Names{"i*/j1*"}, {10000, 2000}).size(), std::size_t{30 * 11});
	fs::remove_all("pattern_deep_root");
}

// The gate of the walks.
struct Walker {
	enum State { waiting, holding, over_queue, too_late, cancelled, other };
	std::atomic<int> state{waiting};
	std::atomic<bool> release{false};
	task::cancel_source stop;   // ends the wait of a walk that is still queued when the test is over
	std::thread thread;
	Walker(index_pattern::WalkGate& gate, const task::cancel_token& parent = {}) : stop(parent) {
		thread = std::thread([this, &gate, token = stop.token()] {
			try {
				auto turn = engine::sync_wait(gate.enter(token));
				state = holding;
				while (!release) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			} catch (const index_pattern::WalkRefused& refused) {
				retry_after = refused.retry_after;   // stored before `state`, which publishes it
				state = refused.status == 429 ? over_queue : too_late;
			} catch (const task::cancelled&) {
				state = cancelled;
			} catch (...) {
				state = other;
			}
		});
	}
	unsigned retry_after = 0;
	bool settles(int wanted, std::chrono::milliseconds within = std::chrono::seconds(5)) const {
		const auto until = std::chrono::steady_clock::now() + within;
		while (state.load() != wanted && std::chrono::steady_clock::now() < until) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return state.load() == wanted;
	}
	~Walker() {
		stop.request();
		release = true;
		thread.join();
	}
};

bool admitted(const index_pattern::WalkGate& gate, std::size_t wanted) {
	for (int i = 0; i < 5000 && gate.admitted() != wanted; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return gate.admitted() == wanted;
}

// SR8: walks beyond the queue are turned away at once with a 429, whatever the walks ahead are doing.
void test_the_walk_queue_is_bounded() {
	using namespace std::chrono;
	index_pattern::WalkGate gate(2, 30s);
	Walker running(gate);
	CHECK_TRUE(running.settles(Walker::holding));
	Walker first(gate), second(gate);
	CHECK_TRUE(admitted(gate, 3));                  // one runs, two wait: the queue is full
	const auto start = steady_clock::now();
	Walker third(gate);
	CHECK_TRUE(third.settles(Walker::over_queue, 10s));
	CHECK_LT(steady_clock::now() - start, 10s);      // refused at once, not after the wait
	CHECK_EQ(third.retry_after, 1u);
	CHECK_EQ(gate.admitted(), std::size_t{3});      // the refused one took no place
	CHECK_EQ(first.state.load(), int(Walker::waiting));
	running.release = true;                         // the turn passes to one of the two, the other still waits
	for (int i = 0; i < 5000 && first.state.load() != Walker::holding && second.state.load() != Walker::holding; ++i) {
		std::this_thread::sleep_for(1ms);
	}
	Walker& next = first.state.load() == Walker::holding ? first : second;
	Walker& last = &next == &first ? second : first;
	CHECK_EQ(next.state.load(), int(Walker::holding));
	CHECK_EQ(last.state.load(), int(Walker::waiting));
	next.release = true;
	CHECK_TRUE(last.settles(Walker::holding));
	last.release = true;
	CHECK_TRUE(admitted(gate, 0));
	Walker after(gate);                             // the places came back
	CHECK_TRUE(after.settles(Walker::holding));
}

// SR8: a walk waits for its turn for the deadline at most, and gives its place back.
void test_a_walk_waits_for_the_deadline_at_most() {
	using namespace std::chrono;
	index_pattern::WalkGate gate(8, 300ms);
	Walker running(gate);
	CHECK_TRUE(running.settles(Walker::holding));
	const auto start = steady_clock::now();
	Walker late(gate);
	CHECK_TRUE(late.settles(Walker::too_late, 5s));
	const auto waited = steady_clock::now() - start;
	CHECK_GE(waited, 250ms);
	CHECK_LT(waited, 10s);
	CHECK_EQ(late.retry_after, 1u);
	CHECK_TRUE(admitted(gate, 1));                  // only the running walk holds a place
	// The walk that holds the turn is not interrupted by the deadline of those behind it.
	CHECK_EQ(running.state.load(), int(Walker::holding));
	running.release = true;
	CHECK_TRUE(admitted(gate, 0));
	Walker next(gate);                              // a free gate is taken at once
	CHECK_TRUE(next.settles(Walker::holding, 1s));
}

// A client that leaves while it waits is dropped, and its place is free.
void test_a_walk_that_is_cancelled_gives_its_place_back() {
	using namespace std::chrono;
	index_pattern::WalkGate gate(8, 30s);
	Walker running(gate);
	CHECK_TRUE(running.settles(Walker::holding));
	task::cancel_source leaving;
	Walker waiting(gate, leaving.token());
	CHECK_TRUE(admitted(gate, 2));
	leaving.request();
	CHECK_TRUE(waiting.settles(Walker::cancelled, 5s));
	CHECK_TRUE(admitted(gate, 1));
}

// SR5: `--max-search-indexes 0` is refused at the command line.
void test_a_zero_cap_is_refused() {
	const auto exits = [](std::vector<std::string> args) {
		std::fflush(nullptr);
		const pid_t pid = ::fork();
		if (pid == 0) {
			std::freopen("/dev/null", "w", stderr);
			std::freopen("/dev/null", "w", stdout);
			std::vector<char*> argv;
			for (auto& arg : args) {
				argv.push_back(arg.data());
			}
			parseOptions(static_cast<int>(argv.size()), argv.data());
			::_exit(0);
		}
		int status = 0;
		::waitpid(pid, &status, 0);
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	};
	CHECK_EQ(exits({"xapiand", "--max-search-indexes", "1"}), 0);
	CHECK_NE(exits({"xapiand", "--max-search-indexes", "0"}), 0);
	CHECK_NE(exits({"xapiand", "--max-search-indexes", "0"}), -1);   // it exits, it does not crash
	CHECK_NE(exits({"xapiand", "--max-search-indexes", "-1"}), 0);
	CHECK_EQ(exits({"xapiand", "--search-walk-queue", "0"}), 0);     // no queue: one walk, the rest turned away
	CHECK_NE(exits({"xapiand", "--search-walk-wait", "0"}), 0);
	opts_t defaults;
	CHECK_EQ(defaults.search_walk_queue, ssize_t{8});
	CHECK_EQ(defaults.search_walk_wait, 10.0);
}

// ---- the name index (docs/design/name-index.md) ----

// The names a directory makes the walk list: its own, and a parent whose marker it is.
void test_the_names_of_an_index_directory() {
	CHECK_TRUE(NameIndex::names_of("twitter") == (Names{"twitter"}));
	CHECK_TRUE(NameIndex::names_of("twitter/user") == (Names{"twitter/user"}));
	CHECK_TRUE(NameIndex::names_of("twitter/.__2") == (Names{"twitter"}));      // a shard is its index
	CHECK_TRUE(NameIndex::names_of("a/strata") == (Names{"a"}));                // the walk takes it as a's own data
	CHECK_TRUE(NameIndex::names_of("strata") == (Names{"strata"}));             // at the top it is a name like any
	CHECK_TRUE(NameIndex::names_of("a/.__1/b") == (Names{"a"}));                // a/.__1 is a's shard; b is inside it
	CHECK_TRUE(NameIndex::names_of("a/strata/b") == (Names{"a"}));
	CHECK_TRUE(NameIndex::names_of(".xapiand") == (Names{}));                   // the node's own: never listed
	CHECK_TRUE(NameIndex::names_of("a/.hidden") == (Names{}));
	CHECK_TRUE(NameIndex::names_of(".xapiand/strata") == (Names{}));
}

// A tree of random nested index directories, some on disk before the build, some opened while it is about to run,
// and some made after it; the walk over the same tree is the specification.
struct RandomTree {
	std::string root = "names_random_root/";
	std::mt19937 rng;
	NameIndex index;

	explicit RandomTree(unsigned seed) : rng(seed) { fs::remove_all(root); }
	~RandomTree() { fs::remove_all(root); }

	std::string pick(std::initializer_list<const char*> choices) {
		return *(choices.begin() + std::uniform_int_distribution<std::size_t>(0, choices.size() - 1)(rng));
	}
	std::string key() {
		std::string key;
		const auto depth = std::uniform_int_distribution<int>(1, 4)(rng);
		for (int i = 0; i < depth; ++i) {
			key += (i != 0 ? "/" : "") + pick({"a", "b", "ab", "a-1", "a.b", "strata", ".__1", ".__2", ".h", "x y", "\xc3\xa9", "a[1]", "w0"});
		}
		return key;
	}
	void make(const std::string& key) { fs::create_directories(root + key + "/strata"); }
	void noise() {
		const auto k = key();
		fs::create_directories(root + k);
		std::ofstream(root + k + "/.__9") << "x";   // a file is no shard
		std::ofstream(root + k + "/strata0") << "x";
	}
	std::string pattern() {
		std::string pattern;
		const auto depth = std::uniform_int_distribution<int>(1, 3)(rng);
		for (int i = 0; i < depth; ++i) {
			pattern += (i != 0 ? "/" : "") + pick({"*", "**", "?", "a*", "*b", "[ab]", "[!a]*", "a", "ab", "strata", "a?1", "*-1", "w*", "\xc3\xa9*", "a[1]*", "a[[]1]", "?.b"});
		}
		return pattern;
	}
	Names items() {
		Names items;
		const auto wanted = std::uniform_int_distribution<int>(1, 3)(rng);
		for (int i = 0; i < wanted; ++i) {
			items.push_back(std::uniform_int_distribution<int>(0, 9)(rng) == 0 ? std::string("_all") : pattern());
		}
		const auto excluded = std::uniform_int_distribution<int>(0, 2)(rng);
		for (int i = 0; i < excluded; ++i) {
			items.push_back("-" + pattern());
		}
		if (std::uniform_int_distribution<int>(0, 3)(rng) == 0) {
			items.push_back(key());   // a plain name
		}
		return items;
	}
};

// A3: every pattern answer from the names equals the walk's over the same tree, nested paths, shards, the engine's own
// `strata` and hidden names included, whether a name came from the build, a resolve before it, or one after it.
void test_the_names_answer_as_the_walk() {
	const index_pattern::Bounds big(1000000, 1000000);
	std::size_t compared = 0, nonempty = 0;
	for (unsigned seed = 1; seed <= 300; ++seed) {
		RandomTree tree(seed);
		const auto before = std::uniform_int_distribution<int>(0, 12)(tree.rng);
		for (int i = 0; i < before; ++i) {
			tree.make(tree.key());
		}
		for (int i = 0; i < 3; ++i) {
			tree.noise();
		}
		for (int i = 0; i < 3; ++i) {   // resolved before the build: in the delta, and on disk
			const auto key = tree.key();
			tree.make(key);
			tree.index.opened(key);
		}
		CHECK_TRUE(tree.index.build(tree.root));
		const auto after = std::uniform_int_distribution<int>(0, 12)(tree.rng);
		for (int i = 0; i < after; ++i) {   // made by a create after the build
			const auto key = tree.key();
			tree.make(key);
			tree.index.opened(key);
		}
		CHECK_TRUE(tree.index.expand(Names{"**"}, big) == expand(tree.root, Names{"**"}, big));
		for (int round = 0; round < 30; ++round) {
			const auto items = tree.items();
			const auto walked = expand(tree.root, items, big);
			const auto indexed = tree.index.expand(items, big);
			if (walked != indexed) {
				std::string text;
				for (const auto& item : items) {
					text += " " + item;
				}
				std::fprintf(stderr, "  seed %u, items%s: the walk lists %zu names, the index %zu\n", seed, text.c_str(), walked.size(), indexed.size());
			}
			CHECK_TRUE(walked == indexed);
			++compared;
			nonempty += walked.empty() ? 0 : 1;
		}
	}
	std::fprintf(stderr, "  %zu comparisons, %zu with names\n", compared, nonempty);
	CHECK_GT(nonempty, compared / 10);   // the comparison is not vacuous
}

// A name resolve adds while the build runs is kept: the walk may have passed its directory already.
void test_a_name_added_during_the_build_is_kept() {
	const std::string root = "names_late_root/";
	fs::remove_all(root);
	fs::create_directories(root + "early/strata");
	NameIndex index;
	index.opened("late/.__1");   // its directory comes after the walk read the top
	CHECK_TRUE(index.build(root));
	fs::create_directories(root + "late/.__1/strata");
	CHECK_TRUE(index.expand(Names{"_all"}, {100, 100}) == (Names{"early", "late"}));
	CHECK_TRUE(index.expand(Names{"_all"}, {100, 100}) == expand(root, Names{"_all"}, {100, 100}));
	fs::remove_all(root);
}

// The delta merges into the packed base once it is large; the answers stay the walk's.
void test_the_delta_merges_into_the_base() {
	const std::string root = "names_merge_root/";
	fs::remove_all(root);
	NameIndex index;
	CHECK_TRUE(index.build(root));
	for (int i = 0; i < 3000; ++i) {
		const auto key = "m" + std::to_string(i % 7) + "/n" + std::to_string(i);
		fs::create_directories(root + key + "/strata");
		index.opened(key);
	}
	CHECK_EQ(index.size(), std::size_t{3000});
	CHECK_LT(index.bytes(), std::size_t{3000} * 40);   // packed: about a name's bytes and 8 more each, not a tree node
	const index_pattern::Bounds big(100000, 100000);
	for (const char* pattern : {"_all", "m3/*", "m*/n1*", "**/n29??", "m6/n[12]*"}) {
		CHECK_TRUE(index.expand(Names{pattern}, big) == expand(root, Names{pattern}, big));
	}
	fs::remove_all(root);
}

// The visits of one lookup: each name read.
std::size_t lookups(const NameIndex& index, const Names& items) {
	std::size_t calls = 0;
	index.expand(items, {100000, 100000}, [&] { ++calls; });
	return calls;
}

// A prefix pattern reads its range alone, and a subtree no pattern can match below is passed with a seek (A2's mechanism).
void test_a_prefix_pattern_reads_its_range() {
	Wide wide(1000);   // i1000 .. i1999
	NameIndex index;
	CHECK_TRUE(index.build(wide.root));
	CHECK_TRUE(index.expand(Names{"i101*"}, {100, 100}) == expand(wide.root, Names{"i101*"}, {100, 2000}));
	CHECK_LE(lookups(index, {"i101*"}), std::size_t{12});    // a seek and its 10 names, not the 1,000 the walk reads
	CHECK_LE(lookups(index, {"i101?", "i102?"}), std::size_t{24});
	CHECK_EQ(lookups(index, {"q*"}), std::size_t{0});
	for (int i = 0; i < 500; ++i) {   // below `j`, which is no index: its first name stands for it, the rest are skipped
		const auto key = "j/c" + std::to_string(i);
		fs::create_directories(wide.root + key + "/strata");
		index.opened(key);
	}
	CHECK_EQ(lookups(index, {"j*"}), std::size_t{1});
	for (int i = 0; i < 500; ++i) {
		const auto key = "i1500/c" + std::to_string(i);
		fs::create_directories(wide.root + key + "/strata");
		index.opened(key);
	}
	// `i15*` lists i1500 .. i1599 and steps past i1500's 500 children with one seek.
	CHECK_EQ(index.expand(Names{"i15*"}, {1000, 1000}).size(), std::size_t{100});
	CHECK_LE(lookups(index, {"i15*"}), std::size_t{110});
}

// A4: on a node with more indexes than the cap, a pattern answers from the names, where the walk is refused.
void test_the_names_answer_over_the_cap() {
	Wide wide(300);
	NameIndex index;
	CHECK_TRUE(index.build(wide.root));
	CHECK_THROW(expand(wide.root, Names{"i101*"}, {50, 50}), engine::Error);   // the walk reads all 300 entries
	CHECK_TRUE(index.expand(Names{"i101*"}, {50, 50}) == (Names{"i1010", "i1011", "i1012", "i1013", "i1014", "i1015", "i1016", "i1017", "i1018", "i1019"}));
	CHECK_THROW(index.expand(Names{"_all"}, {50, 50}), engine::Error);          // the cap on names stays
	CHECK_THROW(index.expand(Names{"*0", "-i1*"}, {50, 50}), engine::Error);    // so does the cap on the names read
	CHECK_THROW(index.expand(Names{"i1*"}, {50, 1000}), engine::Error);
}

// The outcome of an expansion: its names, or the text of its 400.
struct Answer {
	bool refused = false;
	std::string text;
	Names names;
};
template <typename F>
Answer answer(F&& f) {
	Answer a;
	try {
		a.names = f();
	} catch (const engine::Error& e) {
		a.refused = true;
		a.text = e.reason();
	}
	return a;
}

// D7: under any cap, the names never refuse what the walk answers, since a lookup reads one name per entry the walk
// reads; when both refuse and the names refuse for their work, the walk refuses for its work too (names that refuse
// for listing past the cap may meet a walk that read past it first). Over the random trees of A3 at caps 1 to 40, and over
// indexes that each hold a nested one (D22), where the walk reads one entry per index and never enters it.
void test_the_names_never_refuse_what_the_walk_answers() {
	const auto bound = [](const std::string& text) { return text.find("indexes (--max-search-indexes)") != std::string::npos ? 'n' : 'w'; };
	std::size_t cases = 0, refused = 0, both = 0;
	for (unsigned seed = 1; seed <= 100; ++seed) {
		RandomTree tree(seed);
		const auto count = std::uniform_int_distribution<int>(1, 24)(tree.rng);
		for (int i = 0; i < count; ++i) {
			tree.make(tree.key());
		}
		tree.noise();
		CHECK_TRUE(tree.index.build(tree.root));
		for (int round = 0; round < 15; ++round) {
			const auto items = tree.items();
			for (std::size_t cap = 1; cap <= 40; cap += 3) {
				const index_pattern::Bounds small(cap, cap);
				const auto walked = answer([&] { return expand(tree.root, items, small); });
				const auto indexed = answer([&] { return tree.index.expand(items, small); });
				++cases;
				if (!walked.refused) {
					if (indexed.refused || indexed.names != walked.names) {
						std::string text;
						for (const auto& item : items) {
							text += " " + item;
						}
						std::fprintf(stderr, "  seed %u, cap %zu, items%s: the walk lists %zu names, the index: %s\n", seed, cap, text.c_str(), walked.names.size(), indexed.refused ? indexed.text.c_str() : "other names");
					}
					CHECK_FALSE(indexed.refused);
					CHECK_TRUE(indexed.names == walked.names);
				} else if (indexed.refused) {
					++both;
					if (bound(indexed.text) == 'w') {
						CHECK_EQ(bound(walked.text), 'w');
					}
				}
				refused += walked.refused ? 1 : 0;
			}
		}
	}
	std::fprintf(stderr, "  %zu cases, %zu refused by the walk, %zu by both\n", cases, refused, both);
	CHECK_GT(refused, cases / 20);   // the caps bite
	CHECK_LT(refused, cases - cases / 20);

	const std::string root = "names_nested_root/";
	fs::remove_all(root);
	const std::size_t n = 50;
	for (std::size_t k = 0; k < n; ++k) {
		fs::create_directories(root + "i" + std::to_string(100 + k) + "/strata");
		fs::create_directories(root + "i" + std::to_string(100 + k) + "/c/strata");
	}
	NameIndex nested;
	CHECK_TRUE(nested.build(root));
	const index_pattern::Bounds cap(n, n);
	const auto walked = expand(root, Names{"i*"}, cap);   // n entries
	CHECK_EQ(walked.size(), n);
	CHECK_TRUE(nested.expand(Names{"i*"}, cap) == walked);
	fs::remove_all(root);
}

// D110: a directory the build cannot read (here EMFILE, the descriptors all taken) fails the build instead of
// reading as empty, so the names stay incomplete; a build once the fault is gone lists what the walk lists.
void test_an_unreadable_directory_fails_the_build() {
	const std::string root = "names_unreadable_root/";
	fs::remove_all(root);
	for (int g = 0; g < 20; ++g) {
		fs::create_directories(root + "g" + std::to_string(g) + "/i/strata");
	}
	NameIndex index;
	struct rlimit saved;
	CHECK_EQ(::getrlimit(RLIMIT_NOFILE, &saved), 0);
	struct rlimit low = saved;
	low.rlim_cur = std::min<rlim_t>(saved.rlim_cur, 4096);
	CHECK_EQ(::setrlimit(RLIMIT_NOFILE, &low), 0);
	std::vector<int> taken;
	for (int fd; (fd = ::dup(2)) >= 0; ) {   // every descriptor the limit allows
		taken.push_back(fd);
	}
	CHECK_EQ(errno, EMFILE);
	const bool built = index.build(root);
	for (const int fd : taken) {
		::close(fd);
	}
	CHECK_EQ(::setrlimit(RLIMIT_NOFILE, &saved), 0);
	CHECK_FALSE(built);
	CHECK_FALSE(index.complete());
	CHECK_FALSE(index.failure().empty());
	const index_pattern::Bounds big(100000, 100000);
	CHECK_TRUE(index.build(root));
	CHECK_TRUE(index.complete());
	CHECK_TRUE(index.failure().empty());
	CHECK_EQ(index.expand(Names{"**"}, big).size(), std::size_t{20});
	CHECK_TRUE(index.expand(Names{"**"}, big) == expand(root, Names{"**"}, big));
	// A directory that is gone reads as empty: the walk raced a removal, not a fault.
	CHECK_TRUE(expand(root + "missing/", Names{"**"}, big, {}, index_pattern::Unreadable::fail).empty());
	fs::remove_all(root);
}

// One build is claimed at a time, again after one that did not end well, never after one that did; a stopped index
// builds nothing and claims no build.
void test_one_build_is_claimed_at_a_time() {
	Wide wide(50);
	NameIndex index;
	CHECK_TRUE(index.claim());
	CHECK_FALSE(index.claim());   // one runs
	index.unclaim();
	CHECK_TRUE(index.claim());    // it failed: a later pattern starts it again
	CHECK_TRUE(index.build(wide.root));
	index.unclaim();
	CHECK_FALSE(index.claim());   // complete
	NameIndex stopped;
	stopped.request_stop();
	CHECK_FALSE(stopped.claim());
	CHECK_FALSE(stopped.build(wide.root));
	CHECK_FALSE(stopped.complete());
}

// A2: a prefix pattern that names 10 of N top-level indexes, answered by the walk and by the names. Runs only when
// NAME_INDEX_BENCH lists the counts (`10000,100000`); prints one row per count. With NAME_INDEX_BENCH_ROOT the trees
// are kept there (`<root>/<N>/`, made once, `<root>/<N>.made` once complete); NAME_INDEX_BENCH_MAKE=1 only makes them,
// so a light job makes the 10^6 tree and a strict one times it. Without a root each tree is made and removed here.
void bench_name_index() {
	const char* counts = std::getenv("NAME_INDEX_BENCH");
	if (counts == nullptr) {
		return;
	}
	const char* kept = std::getenv("NAME_INDEX_BENCH_ROOT");
	const bool make_only = std::getenv("NAME_INDEX_BENCH_MAKE") != nullptr;
	using clock = std::chrono::steady_clock;
	const auto seconds = [](clock::duration d) { return std::chrono::duration<double>(d).count(); };
	const auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
	std::fprintf(stderr, "  %9s %9s %9s %12s %12s %12s %11s %8s\n", "indexes", "mkdir s", "build s", "walk ms", "names us", "faster", "names read", "B/name");
	for (std::string_view rest = counts; !rest.empty(); ) {
		const auto comma = rest.find(',');
		const auto n = static_cast<std::size_t>(std::stoull(std::string(rest.substr(0, comma))));
		rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
		const std::string root = kept != nullptr ? std::string(kept) + "/" + std::to_string(n) + "/" : std::string("names_bench_root/");
		const std::string made_mark = kept != nullptr ? std::string(kept) + "/" + std::to_string(n) + ".made" : std::string();
		char name[32];
		auto began = clock::now();
		double made = 0;
		if (kept == nullptr || !fs::exists(made_mark)) {
			if (kept != nullptr && !make_only) {
				std::fprintf(stderr, "  %9zu: no tree at %s; make it with NAME_INDEX_BENCH_MAKE=1\n", n, root.c_str());
				CHECK_TRUE(false);
				continue;
			}
			fs::remove_all(root);
			fs::create_directories(root);
			for (std::size_t i = 0; i < n; ++i) {
				std::snprintf(name, sizeof(name), "i%07zu", i);
				const auto dir = root + name;
				CHECK_EQ(::mkdir(dir.c_str(), 0755), 0);
				CHECK_EQ(::mkdir((dir + "/strata").c_str(), 0755), 0);
			}
			made = seconds(clock::now() - began);
			if (kept != nullptr) {
				std::ofstream(made_mark) << n << "\n";
			}
		}
		if (make_only) {
			std::fprintf(stderr, "  %9zu %9.2f\n", n, made);
			continue;
		}
		NameIndex index;
		began = clock::now();
		CHECK_TRUE(index.build(root));
		const auto built = seconds(clock::now() - began);
		CHECK_EQ(index.size(), n);
		std::snprintf(name, sizeof(name), "i%06zu?", n / 20);   // 10 names in the middle
		const Names items{name};
		const index_pattern::Bounds bounds(n * 4, n * 4);
		std::vector<double> walks, lookups_us;
		Names walked, indexed;
		for (int run = 0; run < 5; ++run) {
			began = clock::now();
			walked = expand(root, items, bounds);
			walks.push_back(seconds(clock::now() - began) * 1e3);
		}
		std::size_t read = 0;
		index.expand(items, bounds, [&] { ++read; });
		for (int run = 0; run < 2000; ++run) {
			began = clock::now();
			indexed = index.expand(items, bounds);
			lookups_us.push_back(seconds(clock::now() - began) * 1e6);
		}
		CHECK_EQ(walked.size(), std::size_t{10});
		CHECK_TRUE(walked == indexed);
		const auto walk_ms = median(walks), names_us = median(lookups_us);
		std::fprintf(stderr, "  %9zu %9.2f %9.2f %12.3f %12.3f %11.0fx %11zu %8.1f\n", n, made, built, walk_ms, names_us, walk_ms * 1e3 / names_us, read, static_cast<double>(index.bytes()) / static_cast<double>(n));
		if (kept == nullptr) {
			fs::remove_all(root);
		}
	}
}

}  // namespace

int main(int argc, char** argv) {
	struct TestCase { const char* name; void (*fn)(); };
	const TestCase cases[] = {
		{"test_a_zero_cap_is_refused", test_a_zero_cap_is_refused},   // first: it forks, before any test starts threads
		{"test_match", test_match},
		{"test_many_globstars_stay_linear", test_many_globstars_stay_linear},
		{"test_expand", test_expand},
		{"test_the_limit", test_the_limit},
		{"test_the_walk_is_bounded", test_the_walk_is_bounded},
		{"test_the_matching_is_bounded", test_the_matching_is_bounded},
		{"test_one_walk_for_all_the_items", test_one_walk_for_all_the_items},
		{"test_the_patterns_are_bounded", test_the_patterns_are_bounded},
		{"test_the_walk_can_be_cancelled", test_the_walk_can_be_cancelled},
		{"test_equal_items_count_once", test_equal_items_count_once},
		{"test_nested_indexes_are_listed", test_nested_indexes_are_listed},
		{"test_an_index_does_not_hide_the_ones_below_it", test_an_index_does_not_hide_the_ones_below_it},
		{"test_the_walk_goes_only_where_a_pattern_can_match", test_the_walk_goes_only_where_a_pattern_can_match},
		{"test_a_deep_tree_stays_within_the_budget", test_a_deep_tree_stays_within_the_budget},
		{"test_the_entries_of_a_candidate_are_counted", test_the_entries_of_a_candidate_are_counted},
		{"test_the_walk_queue_is_bounded", test_the_walk_queue_is_bounded},
		{"test_a_walk_waits_for_the_deadline_at_most", test_a_walk_waits_for_the_deadline_at_most},
		{"test_a_walk_that_is_cancelled_gives_its_place_back", test_a_walk_that_is_cancelled_gives_its_place_back},
		{"test_the_names_of_an_index_directory", test_the_names_of_an_index_directory},
		{"test_the_names_answer_as_the_walk", test_the_names_answer_as_the_walk},
		{"test_a_name_added_during_the_build_is_kept", test_a_name_added_during_the_build_is_kept},
		{"test_the_delta_merges_into_the_base", test_the_delta_merges_into_the_base},
		{"test_a_prefix_pattern_reads_its_range", test_a_prefix_pattern_reads_its_range},
		{"test_the_names_answer_over_the_cap", test_the_names_answer_over_the_cap},
		{"test_the_names_never_refuse_what_the_walk_answers", test_the_names_never_refuse_what_the_walk_answers},
		{"test_an_unreadable_directory_fails_the_build", test_an_unreadable_directory_fails_the_build},
		{"test_one_build_is_claimed_at_a_time", test_one_build_is_claimed_at_a_time},
		{"bench_name_index", bench_name_index},   // only with NAME_INDEX_BENCH
	};
	for (const auto& c : cases) {
		if (argc > 1 && std::string(c.name).find(argv[1]) == std::string::npos) {
			continue;
		}
		const int before = testh::failures;
		try {
			c.fn();
		} catch (const engine::Error& error) {
			std::fprintf(stderr, "  FAIL %s: threw engine::Error %s: %s\n", c.name, engine::to_string(error.kind), error.description().c_str());
			++testh::failures;
		} catch (const std::exception& exc) {
			std::fprintf(stderr, "  FAIL %s: threw %s\n", c.name, exc.what());
			++testh::failures;
		}
		std::fprintf(stderr, "%s %s\n", testh::failures == before ? "ok  " : "FAIL", c.name);
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
