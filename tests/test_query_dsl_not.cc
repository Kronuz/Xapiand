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

// QueryDSL::make_dsl_query builds the same tree on every toolchain: _not holds a flat list of
// one expression, never a list nested in a list (a braced {{ x }} parses differently per compiler).

#include "test_harness.h"

#include <cstdio>
#include <memory>
#include <string>

#include "database/schema.h"
#include "msgpack.h"
#include "query_dsl.h"

namespace {

QueryDSL make_dsl() {
	return QueryDSL(std::make_shared<Schema>(nullptr, nullptr, ""));
}

void test_not_is_a_flat_list_of_one() {
	auto dsl = make_dsl();
	const auto tree = dsl.make_dsl_query("NOT a:b");
	std::fprintf(stderr, "tree: %s\n", tree.to_string().c_str());
	CHECK_TRUE(tree.is_map());
	CHECK_EQ(tree.size(), std::size_t(1));
	const auto& operand = tree.at("_not");
	CHECK_TRUE(operand.is_array());
	CHECK_EQ(operand.size(), std::size_t(1));
	CHECK_FALSE(operand.at(0).is_array());
	CHECK_EQ(tree.to_string(), std::string(R"({"_not":[{"a":"b"}]})"));
}

void test_not_of_a_group() {
	auto dsl = make_dsl();
	const auto tree = dsl.make_dsl_query("NOT (a:b OR c:d)");
	std::fprintf(stderr, "tree: %s\n", tree.to_string().c_str());
	const auto& operand = tree.at("_not");
	CHECK_TRUE(operand.is_array());
	CHECK_EQ(operand.size(), std::size_t(1));
	CHECK_FALSE(operand.at(0).is_array());
}

void test_binary_operators_are_lists_of_two() {
	auto dsl = make_dsl();
	for (const char* op : { "AND", "OR", "XOR", "AND MAYBE" }) {
		const auto tree = dsl.make_dsl_query(std::string("a:b ") + op + " c:d");
		std::fprintf(stderr, "tree: %s\n", tree.to_string().c_str());
		CHECK_EQ(tree.size(), std::size_t(1));
		const auto& operands = tree.begin()->second;
		CHECK_TRUE(operands.is_array());
		CHECK_EQ(operands.size(), std::size_t(2));
		CHECK_FALSE(operands.at(0).is_array());
		CHECK_FALSE(operands.at(1).is_array());
	}
}

}  // namespace

TEST_MAIN(test_not_is_a_flat_list_of_one, test_not_of_a_group, test_binary_operators_are_lists_of_two)
