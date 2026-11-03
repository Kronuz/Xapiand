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

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "engine/error.h"
#include "index_pattern.h"

namespace {

namespace fs = std::filesystem;
using index_pattern::expand;
using index_pattern::match;

void test_match() {
	CHECK_TRUE(match("a", "a"));
	CHECK_FALSE(match("a", "ab"));
	CHECK_FALSE(match("ab", "a"));
	CHECK_TRUE(match("*", ""));
	CHECK_TRUE(match("*", "anything/at all"));
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
	CHECK_TRUE(match("a*", "a/b"));   // a star crosses the slash of a nested index
	CHECK_FALSE(match("", "a"));
	CHECK_TRUE(match("", ""));
	CHECK_TRUE(index_pattern::is_wildcard("_all"));
	CHECK_TRUE(index_pattern::is_wildcard("a*"));
	CHECK_FALSE(index_pattern::is_wildcard("a"));
	CHECK_FALSE(index_pattern::is_wildcard("_allx"));
	// A pattern built to make a matcher backtrack: bounded by the pattern's size, so it ends.
	CHECK_FALSE(match(std::string(100, 'a') + std::string("*a*a*a*a*a*a*b"), std::string(2000, 'a')));
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

Names names(const Tree& tree, const Names& items, std::size_t limit = 100) {
	return expand(tree.root, items, limit);
}

void test_expand() {
	Tree tree;
	const Names all = {"a1", "a2", "a3", "b1", "c/d1", "c/d2"};
	CHECK_TRUE(names(tree, {"_all"}) == all);
	CHECK_TRUE(names(tree, {"*"}) == all);
	CHECK_TRUE(names(tree, {"a*"}) == (Names{"a1", "a2", "a3"}));
	CHECK_TRUE(names(tree, {"*1"}) == (Names{"a1", "b1", "c/d1"}));
	CHECK_TRUE(names(tree, {"*d*"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c/*"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"c*"}) == (Names{"c/d1", "c/d2"}));
	CHECK_TRUE(names(tree, {"a*", "-a2"}) == (Names{"a1", "a3"}));
	CHECK_TRUE(names(tree, {"-a2", "a*"}) == (Names{"a1", "a3"}));   // an exclusion applies wherever it stands
	CHECK_TRUE(names(tree, {"_all", "-a*", "-c/*"}) == (Names{"b1"}));
	CHECK_TRUE(names(tree, {"b1", "a*", "b*"}) == (Names{"a1", "a2", "a3", "b1"}));   // sorted, no repeats
	CHECK_TRUE(names(tree, {"zzz", "a1"}) == (Names{"a1", "zzz"}));   // a plain name stands for itself, there or not
	CHECK_TRUE(names(tree, {"zzz", "-zzz"}).empty());
	CHECK_TRUE(names(tree, {"q*"}).empty());
	CHECK_TRUE(names(tree, {".*"}).empty());        // the node's own indexes are not listed by a pattern
	CHECK_TRUE(names(tree, {".hidden"}) == (Names{".hidden"}));   // ... but they can be named
	CHECK_TRUE(names(tree, {"e*"}).empty());        // a directory that is no index
	CHECK_TRUE(names(tree, {"afile*"}).empty());    // a file is no index
	CHECK_TRUE(names(tree, {"-"}) == (Names{"-"}));  // a lone dash is a name
	CHECK_TRUE(expand(tree.root.substr(0, tree.root.size() - 1), Names{"b*"}, 10) == (Names{"b1"}));   // the root needs no slash
}

void test_the_limit() {
	Tree tree;
	CHECK_EQ(names(tree, {"_all"}, 6).size(), std::size_t{6});
	CHECK_THROW(names(tree, {"_all"}, 5), engine::Error);
	CHECK_THROW(names(tree, {"a*", "b*", "c*"}, 5), engine::Error);
	CHECK_EQ(names(tree, {"_all", "-c/*"}, 4).size(), std::size_t{4});   // the names removed do not count
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

}  // namespace

int main(int argc, char** argv) {
	struct TestCase { const char* name; void (*fn)(); };
	const TestCase cases[] = {
		{"test_match", test_match},
		{"test_expand", test_expand},
		{"test_the_limit", test_the_limit},
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
