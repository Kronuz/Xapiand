/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Unit tests for GenerateTerms numeric range-term generation. Revived from the
// retired oldtests generate_terms suite, re-expressed structurally (does a range
// produce terms, and when does it collapse to the empty query) instead of pinning
// the exact Xapian query descriptions, which encode internal term bytes.

#include "test_harness.h"

#include <cstdint>
#include <string>
#include <vector>

#include "database/utils.h"
#include "multivalue/generate_terms.h"

static void test_numeric_terms() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000, 10000, 100000 };
	const std::vector<std::string> prefix = { "N1", "N2", "N3", "N4", "N5", "N6" };

	// A range that fits within the accuracies produces a non-empty query.
	auto q = GenerateTerms::numeric(uint64_t(1200), uint64_t(2500), accuracy, prefix);
	CHECK_FALSE(q.empty());

	// A tight range still produces terms.
	auto q_small = GenerateTerms::numeric(uint64_t(1200), uint64_t(1200), accuracy, prefix);
	CHECK_FALSE(q_small.empty());

	// A large range still produces terms at the coarser accuracies.
	auto q_wide = GenerateTerms::numeric(uint64_t(0), uint64_t(1000000), accuracy, prefix);
	CHECK_FALSE(q_wide.empty());
}

static void test_numeric_signed() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000 };
	const std::vector<std::string> prefix = { "N1", "N2", "N3", "N4" };

	// The signed overload handles negative ranges.
	auto q = GenerateTerms::numeric(int64_t(-500), int64_t(500), accuracy, prefix);
	CHECK_FALSE(q.empty());
}

// The accuracy terms of the id field must not lead with the id term prefix, so that the id term is the
// only one that does; every other field keeps its own prefix.
static void test_id_accuracy_prefix() {
	CHECK_EQ(std::string(acc_term_prefix(DOCUMENT_ID_TERM_PREFIX)), std::string(DOCUMENT_ID_ACCURACY_TERM_PREFIX));
	CHECK_EQ(std::string(acc_term_prefix("age.")), std::string("age."));
	CHECK_EQ(std::string(acc_term_prefix("")), std::string());

	const std::vector<uint64_t> accuracy = { 10, 100 };
	std::vector<std::string> acc_prefix;
	for (auto acc : accuracy) {
		acc_prefix.push_back(std::string(acc_term_prefix(DOCUMENT_ID_TERM_PREFIX)) + get_prefix(acc));
	}
	engine::Document doc;
	GenerateTerms::integer(doc, accuracy, acc_prefix, 1234);
	const auto stored = doc.serialise();
	CHECK(stored.find(std::string(DOCUMENT_ID_ACCURACY_TERM_PREFIX) + get_prefix(100)) != std::string::npos);
	CHECK(stored.find(std::string(DOCUMENT_ID_TERM_PREFIX) + get_prefix(100)) == std::string::npos);
	CHECK_FALSE(GenerateTerms::numeric(int64_t(1200), int64_t(1300), accuracy, acc_prefix).empty());
}

TEST_MAIN(test_numeric_terms, test_numeric_signed, test_id_accuracy_prefix)
