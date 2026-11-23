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

// Callbacks and operators through the seam. One corpus is written to a set of strata shards (and, as a
// whole, to one more shard); every search is checked against a model of the corpus: the documents found
// and the estimate are exact, a search over shards weighs as one shard holding every document, hits come
// in weight order, a spy sees what the model says and a key maker orders as its keys do. On one shard
// and on five.

#include "test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "test_engine_callbacks/support.h"

namespace detail {

const std::vector<std::string> words = {"apple", "apricot", "apply", "banana", "band", "bandana", "bond", "cat", "cart", "cast", "coat", "caf\xc3\xa9", "cafe", "caf\xc3\xa9s", "na\xc3\xafve"};

}  // namespace detail

using namespace detail;

static void test_sorter_() { test_sorter(); }
static void test_spy_() { test_spy(); }
static void test_sources_() { test_sources(); }
static void test_operators_() { test_operators(); }
static void test_expansions_() { test_expansions(); }

int main() {
	const struct { const char* name; void (*run)(); } tests[] = {
		{"sorter", test_sorter_}, {"boolean", test_boolean_terms}, {"single", test_single_documents}, {"appends", test_appends},
		{"spy", test_spy_}, {"sources", test_sources_}, {"operators", test_operators_}, {"expansions", test_expansions_},
	};
	const char* only = std::getenv("ONLY");
	for (const auto& test : tests) {
		if (only == nullptr || std::string(test.name) == only) {
			test.run();
		}
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
