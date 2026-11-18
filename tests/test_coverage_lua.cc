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

// Coverage tests for src/lua/processor.cc: the conversions between a document and a script's tables, in both directions,
// for the kinds the call tests do not reach (floats and nil handed in; fractional, out-of-range and non-scalar keys and
// values handed back).

#include "test_harness.h"

#include <cstdint>
#include <exception>
#include <string>

#include "lua/exception.h"
#include "lua/processor.h"
#include "task.hh"

namespace {

// Runs one script over `doc` to its end on this thread; the exception it ended with is left in `error`.
MsgPack run(const std::string& body, MsgPack doc, std::exception_ptr* error = nullptr) {
	lua::Processor processor("test", body, MsgPack::MAP());
	MsgPack old_doc = MsgPack::MAP();
	try {
		task::sync_wait(processor("PUT", doc, old_doc, MsgPack::MAP()));
	} catch (...) {
		if (error != nullptr) {
			*error = std::current_exception();
		}
	}
	return doc;
}

void test_floats_booleans_and_nil_reach_the_script() {
	MsgPack doc = MsgPack::MAP();
	doc["f"] = 2.5;
	doc["b"] = true;
	doc["n"] = MsgPack::NIL();
	const auto out = run("_doc.f2 = _doc.f * 2; _doc.b2 = not _doc.b; _doc.n_is_nil = (_doc.n == nil)", doc);
	CHECK(out.at("f2").f64() == 5.0);
	CHECK_EQ(out.at("b2").boolean(), false);
	CHECK_EQ(out.at("n_is_nil").boolean(), true);
}

void test_numeric_keys_come_back_as_integers_and_others_are_dropped() {
	// A fractional key truncates, one past the integer range becomes 0, and a boolean or function key drops its entry.
	const auto out = run("_doc.t = {[2.5] = 'a', [1e30] = 'b', [true] = 'c', [print] = 'd', x = 'e'}", MsgPack::MAP());
	const auto& t = out.at("t");
	CHECK_EQ(t.size(), 3u);
	CHECK_EQ(t.at("2").str(), "a");
	CHECK_EQ(t.at("0").str(), "b");
	CHECK_EQ(t.at("x").str(), "e");
}

void test_a_function_value_is_stored_as_nil() {
	const auto out = run("_doc.g = print; _doc.keep = 1", MsgPack::MAP());
	CHECK(out.find("g") == out.end());
	CHECK_EQ(out.at("keep").i64(), 1);
}

}  // namespace

TEST_MAIN(test_floats_booleans_and_nil_reach_the_script, test_numeric_keys_come_back_as_integers_and_others_are_dropped,
	test_a_function_value_is_stored_as_nil)
