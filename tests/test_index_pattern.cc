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

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

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

Names names(const Tree& tree, const Names& items, std::size_t limit = 100, std::size_t visits = 1000) {
	return expand(tree.root, items, {limit, visits});
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
	CHECK_TRUE(expand(tree.root.substr(0, tree.root.size() - 1), Names{"b*"}, {10, 100}) == (Names{"b1"}));   // the root needs no slash
}

void test_the_limit() {
	Tree tree;
	CHECK_EQ(names(tree, {"_all"}, 6).size(), std::size_t{6});
	CHECK_THROW(names(tree, {"_all"}, 5), engine::Error);
	CHECK_THROW(names(tree, {"a*", "b*", "c*"}, 5), engine::Error);
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
	CHECK_EQ(steps, std::size_t{3});   // a step a byte of the name
	steps = 0;
	CHECK_FALSE(match(std::string(100, 'a') + "b", std::string(100, 'a') + "c", steps));
	CHECK_TRUE(steps >= 100);
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

// SR2: index paths do not nest.
void test_index_paths_do_not_nest() {
	Tree tree;
	const auto refused = [&](const std::string& path) {
		try {
			index_pattern::check_new_index(tree.root, path, 1000);
			return false;
		} catch (const engine::Error& error) {
			return error.kind == engine::errc::invalid;
		}
	};
	CHECK_TRUE(refused("c"));            // c holds the indexes c/d1 and c/d2
	CHECK_TRUE(refused("a1/x"));         // below the index a1
	CHECK_TRUE(refused("a2/x/y"));       // ... any depth, and a2 has shards
	CHECK_TRUE(refused("c/d1/x"));
	CHECK_TRUE(refused("c/strata"));     // makes c look like an index
	CHECK_TRUE(refused("e/.__1"));
	CHECK_TRUE(refused("e/.__x/y"));
	CHECK_TRUE(refused("n/strata/x"));
	CHECK_FALSE(refused("a1"));          // an existing index is itself
	CHECK_FALSE(refused("c/d3"));        // next to others
	CHECK_FALSE(refused("e"));           // a directory with no index below
	CHECK_FALSE(refused("e/f"));
	CHECK_FALSE(refused("new"));
	CHECK_FALSE(refused("strata"));      // as a first name it is an ordinary index
	CHECK_FALSE(refused("a1x/y"));
	// The directory below is read for at most the budget; a bigger one is refused, it is not assumed empty.
	Wide wide(50);
	fs::create_directories(wide.root + "box");
	for (int i = 0; i < 30; ++i) {
		fs::create_directories(wide.root + "box/e" + std::to_string(i));
	}
	index_pattern::check_new_index(wide.root, "box", 1000);
	CHECK_THROW(index_pattern::check_new_index(wide.root, "box", 10), engine::Error);
}


// SR10: the parents of a new index are read for at most the budget, all of them together.
void test_the_parents_are_read_within_a_budget() {
	fs::remove_all("pattern_parents_root");
	const std::string root = "pattern_parents_root/";
	for (const char* box : {"big", "big/mid"}) {
		for (int i = 0; i < 30; ++i) {
			fs::create_directories(root + box + "/e" + std::to_string(i));   // directories that are no index
		}
	}
	// `big` and `big/mid` hold 31 and 30 entries (mid is one of big's 31).
	index_pattern::check_new_index(root, "big/mid/new", 1000);
	CHECK_THROW(index_pattern::check_new_index(root, "big/new", 30), engine::Error);
	index_pattern::check_new_index(root, "big/new", 31);
	CHECK_THROW(index_pattern::check_new_index(root, "big/mid/new", 60), engine::Error);   // the parents share the budget
	index_pattern::check_new_index(root, "big/mid/new", 61);
	try {
		index_pattern::check_new_index(root, "big/new", 5);
	} catch (const engine::Error& error) {
		CHECK_EQ(error.kind, engine::errc::invalid);
		CHECK_TRUE(error.description().find("too many entries") != std::string::npos);
	}
	fs::remove_all("pattern_parents_root");
}

// SR1: the entries a directory holds are read, and counted, to decide whether it is an index.
void test_the_entries_of_a_candidate_are_counted() {
	fs::remove_all("pattern_entries_root");
	const std::string root = "pattern_entries_root/";
	fs::create_directories(root + "box");
	for (int i = 0; i < 40; ++i) {
		std::ofstream(root + "box/f" + std::to_string(i)) << "x";
	}
	// `bo*` reads `box` (1), the 40 entries that show it is no index (40), and the 40 again as it looks below it.
	CHECK_THROW(expand(root, Names{"bo*"}, {100, 80}), engine::Error);
	CHECK_TRUE(expand(root, Names{"bo*"}, {100, 81}).empty());
	std::size_t calls = 0;
	CHECK_TRUE(expand(root, Names{"bo*"}, {100, 81}, [&] { ++calls; }).empty());
	CHECK_EQ(calls, std::size_t{81});
	fs::remove_all("pattern_entries_root");
}

// SR9: the check once the directory is made. Two creates that raced past the first check find each other here.
void test_a_created_index_is_checked_again() {
	Tree tree;
	const auto abs = fs::absolute(tree.root).string();
	const auto refused = [&](const std::string& shard_path) {
		try {
			index_pattern::check_created(abs, abs + shard_path, 1000);
			return false;
		} catch (const engine::Error& error) {
			return error.kind == engine::errc::invalid;
		}
	};
	fs::create_directories(tree.root + "a1/x/strata");       // made below the index a1
	CHECK_TRUE(refused("a1/x"));
	fs::create_directories(tree.root + "n/strata");          // made above n/m
	fs::create_directories(tree.root + "n/m/strata");
	CHECK_TRUE(refused("n"));
	CHECK_TRUE(refused("n/m/"));                              // ... and n/m sees n from below
	fs::create_directories(tree.root + "s/.__2/strata");     // a shard of s, with an index s/t beside it
	fs::create_directories(tree.root + "s/t/strata");
	CHECK_TRUE(refused("s/.__2"));
	CHECK_TRUE(refused("s/t"));
	fs::create_directories(tree.root + "ok/strata");         // alone
	CHECK_FALSE(refused("ok"));
	fs::create_directories(tree.root + "okd/.__1/strata");
	CHECK_FALSE(refused("okd/.__1"));
	CHECK_FALSE(refused("c/d1"));                            // next to c/d2
	index_pattern::check_created(abs, "/elsewhere/a1/x", 1000);   // not under the data directory: not checked
	CHECK_THROW(index_pattern::check_created(abs, "a1/x", 1000), engine::Error);   // relative: from the data directory
	// A directory that is no index and holds more entries than the budget is refused, not assumed empty.
	for (int i = 0; i < 30; ++i) {
		fs::create_directories(tree.root + "box/e" + std::to_string(i));
	}
	fs::create_directories(tree.root + "box/strata");
	CHECK_THROW(index_pattern::check_created(abs, abs + "box", 10), engine::Error);
}

engine::OpenOptions checked_creation(const std::string& root) {
	engine::OpenOptions options;
	options.on_create = [root](const std::string& path) { index_pattern::check_created(root, path, 1000); };
	return options;
}

bool created(const std::string& root, const std::string& name) {
	try {
		engine::sync_wait(engine::open_local(root + name, true, true, checked_creation(root)));
		return true;
	} catch (const engine::Error& error) {
		CHECK_EQ(error.kind, engine::errc::invalid);
		return false;
	}
}

// SR9: the engine makes the directory and checks again: a create that finds a nested index takes its directory back and
// leaves what others made.
void test_a_creation_that_nests_takes_its_directory_back() {
	fs::remove_all("pattern_create_root");
	fs::create_directories("pattern_create_root");
	const auto root = fs::absolute("pattern_create_root").string() + "/";
	CHECK_TRUE(created(root, "a"));
	CHECK_FALSE(created(root, "a/b"));                        // below an index
	CHECK_FALSE(fs::exists(root + "a/b"));                    // ... and nothing of it is left
	CHECK_TRUE(fs::exists(root + "a/strata"));
	CHECK_TRUE(created(root, "c/d"));
	CHECK_FALSE(created(root, "c"));                          // above an index
	CHECK_FALSE(fs::exists(root + "c/strata"));
	CHECK_TRUE(fs::exists(root + "c/d/strata"));              // the one that was there is intact
	CHECK_TRUE(created(root, "e/f/g"));
	CHECK_FALSE(created(root, "e/f"));
	CHECK_TRUE(fs::exists(root + "e/f/g/strata"));
	fs::create_directories(root + "w/pre");                   // an empty directory that was there stays
	CHECK_TRUE(created(root, "w"));
	CHECK_FALSE(created(root, "w/pre/x"));
	CHECK_TRUE(fs::exists(root + "w/pre"));
	CHECK_FALSE(fs::exists(root + "w/pre/x"));
	CHECK_FALSE(created(root, "w/n1/n2"));                    // the directories it made on the way go too
	CHECK_FALSE(fs::exists(root + "w/n1"));
	CHECK_TRUE(fs::exists(root + "w/strata"));
	fs::remove_all("pattern_create_root");
}

// SR9: the pair of creates of nest2.py, at once: never both stand.
void test_concurrent_creates_never_nest() {
	fs::remove_all("pattern_race_root");
	fs::create_directories("pattern_race_root");
	const auto root = fs::absolute("pattern_race_root").string() + "/";
	int both = 0, neither = 0;
	for (int i = 0; i < 150; ++i) {
		const auto parent = "r" + std::to_string(i);
		std::atomic<int> go{0};
		bool parent_made = false, child_made = false;
		std::thread one([&] { go.fetch_add(1); while (go.load() < 2) { } parent_made = created(root, parent); });
		std::thread two([&] { go.fetch_add(1); while (go.load() < 2) { } child_made = created(root, parent + "/b"); });
		one.join();
		two.join();
		both += parent_made && child_made;
		neither += !parent_made && !child_made;
		// What stands is what the answers said, and no index is inside another.
		CHECK_EQ(fs::exists(root + parent + "/strata"), parent_made);
		CHECK_EQ(fs::exists(root + parent + "/b/strata"), child_made);
	}
	CHECK_EQ(both, 0);
	CHECK_EQ(neither, 0);   // the checks are serialised: one of the pair stands
	fs::remove_all("pattern_race_root");
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
				state = refused.status == 429 ? over_queue : too_late;
				retry_after = refused.retry_after;
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
	CHECK_TRUE(third.settles(Walker::over_queue, 2s));
	CHECK_LT(steady_clock::now() - start, 2s);      // refused at once, not after the wait
	CHECK_EQ(third.retry_after, 1u);
	CHECK_EQ(gate.admitted(), std::size_t{3});      // the refused one took no place
	CHECK_EQ(first.state.load(), int(Walker::waiting));
	running.release = true;                         // the turn passes to the first in line
	CHECK_TRUE(first.settles(Walker::holding));
	CHECK_EQ(second.state.load(), int(Walker::waiting));
	first.release = true;
	CHECK_TRUE(second.settles(Walker::holding));
	second.release = true;
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
	CHECK_LT(waited, 3s);
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

}  // namespace

int main(int argc, char** argv) {
	struct TestCase { const char* name; void (*fn)(); };
	const TestCase cases[] = {
		{"test_match", test_match},
		{"test_expand", test_expand},
		{"test_the_limit", test_the_limit},
		{"test_the_walk_is_bounded", test_the_walk_is_bounded},
		{"test_the_matching_is_bounded", test_the_matching_is_bounded},
		{"test_one_walk_for_all_the_items", test_one_walk_for_all_the_items},
		{"test_the_patterns_are_bounded", test_the_patterns_are_bounded},
		{"test_the_walk_can_be_cancelled", test_the_walk_can_be_cancelled},
		{"test_equal_items_count_once", test_equal_items_count_once},
		{"test_index_paths_do_not_nest", test_index_paths_do_not_nest},
		{"test_the_parents_are_read_within_a_budget", test_the_parents_are_read_within_a_budget},
		{"test_the_entries_of_a_candidate_are_counted", test_the_entries_of_a_candidate_are_counted},
		{"test_a_created_index_is_checked_again", test_a_created_index_is_checked_again},
		{"test_a_creation_that_nests_takes_its_directory_back", test_a_creation_that_nests_takes_its_directory_back},
		{"test_concurrent_creates_never_nest", test_concurrent_creates_never_nest},
		{"test_the_walk_queue_is_bounded", test_the_walk_queue_is_bounded},
		{"test_a_walk_waits_for_the_deadline_at_most", test_a_walk_waits_for_the_deadline_at_most},
		{"test_a_walk_that_is_cancelled_gives_its_place_back", test_a_walk_that_is_cancelled_gives_its_place_back},
		{"test_a_zero_cap_is_refused", test_a_zero_cap_is_refused},
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
