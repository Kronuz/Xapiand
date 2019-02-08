/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_boolparser.h"

#include "booleanParser/BooleanParser.h"
#include "utils.h"


int test_boolparser() {
	INIT_LOG
	int count = 0;

	std::vector<boolparser_t> queries ({
		{ "A AND B", { "A", "B", "AND" } },
		{ "A & B", { "A", "B", "&" } },
		{ "A OR B OR C", { "A", "B", "OR", "C", "OR" } },
		{ "A OR B AND C", { "A", "B", "C", "AND", "OR" } },
		{ "A XOR B AND C", { "A", "B", "C", "AND", "XOR" } },
		{ "A AND B XOR C", { "A", "B", "AND", "C", "XOR" } },
		{ "     A OR        B", { "A", "B", "OR" } },
		{ "( A OR B ) AND C", { "A", "B", "OR", "C", "AND" } },
		{ "( A OR B ) AND ( ( C XOR D ) AND E )", { "A", "B", "OR", "C", "D", "XOR", "E", "AND", "AND" } },
		{ "\"Hello world\" AND \"Bye world\"", { "\"Hello world\"", "\"Bye world\"", "AND" } },
		{ "'Hello world' AND 'Bye world'", { "'Hello world'", "'Bye world'", "AND" } },
		{ "[123, 322] OR [567, 766]", { "[123, 322]", "[567, 766]", "OR" } },
		{ "NOT A", { "A", "NOT" } },
		{ "A OR NOT B", { "A", "B", "NOT", "OR" } },
		{ "NOT ( A AND NOT B ) XOR ( C OR ( D AND NOT E) )", { "A", "B", "NOT", "AND", "NOT", "C", "D", "E", "NOT", "AND", "OR", "XOR" } }
	});

	for (const auto& query : queries) {
		BooleanTree booltree(query.query);

		if (booltree.size() != query.stack_expected.size()) {
			L_ERR("ERROR: Boolean parser mismatch sizes in stacks: expected {}, result is: {}", query.stack_expected.size(), booltree.size());
			++count;
		} else {
			for (const auto& token : query.stack_expected) {
				const auto& lexeme = booltree.front().get_lexeme();
				if (token != lexeme) {
					L_ERR("ERROR: Boolean parser: expected token {}, result is: {}", token, lexeme);
					++count;
				}
				booltree.pop_front();
			}
		}
	}

	RETURN(count);
}
