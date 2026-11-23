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

// What a call returns: the document, the converted result and its bounds, and print.

#include "support.h"
#include "tests.h"

namespace detail {

// --- results come back as the document ---------------------------------------------------------------

void test_a_script_transforms_the_document() {
	auto out = call("_doc.x = _doc.x * 2; _doc.method = _method; _doc.fresh = {1, 2, 3}", doc_of({{"x", 21}}));
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("x").i64(), 42);
	CHECK_EQ(str(out.doc, "method"), "PUT");
	CHECK_EQ(out.doc.at("fresh").size(), 3u);
	CHECK_EQ(out.doc.at("fresh").at(2).i64(), 3);
}

void test_params_and_old_doc_are_in_scope() {
	Processor processor("test", "_doc.sum = k + _old_doc.n + extra", MsgPack({{"k", 10}}));
	Outcome out;
	out.doc = MsgPack::MAP();
	MsgPack old_doc({{"n", 5}});
	MsgPack params({{"extra", 100}});
	task::sync_wait(processor("PUT", out.doc, old_doc, params));
	CHECK_EQ(out.doc.at("sum").i64(), 115);
}

void test_numbers_keep_their_kind() {
	auto out = call("_doc.i = 7; _doc.f = 7.5; _doc.whole = 8.0; _doc.neg = -3; _doc.big = 2^62", MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("i").i64(), 7);
	CHECK(out.doc.at("f").f64() == 7.5);
	CHECK_EQ(out.doc.at("whole").i64(), 8);
	CHECK_EQ(out.doc.at("neg").i64(), -3);
	CHECK_EQ(out.doc.at("big").i64(), int64_t(1) << 62);
}

void test_a_script_error_is_a_client_error_and_leaves_the_document_as_it_was() {
	// N3: a script that raises answers 400, not a 2xx that stores the document untransformed.
	auto out = call("_doc.x = 99; error('boom')", doc_of({{"x", 1}}));
	CHECK(out.script_error);
	CHECK(out.what.find("boom") != std::string::npos);
	CHECK_EQ(out.doc.at("x").i64(), 1);
	auto table = call("error({code = 1})", MsgPack::MAP());
	CHECK(table.script_error);
	// N1: the message is cut at a few KiB, with a marker.
	auto huge = call("error(string.rep('m', 100000))", MsgPack::MAP());
	CHECK(huge.script_error);
	CHECK(huge.what.size() < 2048);
	CHECK(huge.what.find("[truncated]") != std::string::npos);
}

void test_a_syntax_error_names_its_line() {
	try {
		Processor processor("named", "local a = 1\nlocal b = 2\nx = = 3", MsgPack::MAP());
		CHECK(false);
	} catch (const ClientError& e) {
		CHECK(std::string(e.what()).find("named") != std::string::npos);
		CHECK(std::string(e.what()).find(":3:") != std::string::npos);
	}
}

// --- X4-S3: the converted result is bounded -------------------------------------------------------------

void test_a_result_past_its_byte_budget_is_refused() {
	// The reviewer's p5: one 1 MiB string referenced many times costs the host a copy per reference.
	auto shared = call("local s = string.rep('x', 1048576) local t = {} for i = 1, 40 do t[i] = s end _doc.t = t", MsgPack::MAP());
	CHECK(shared.too_large);
	CHECK(shared.what.find("bytes") != std::string::npos);
	auto keys = call("local t = {} local k = string.rep('k', 1048576) for i = 1, 40 do t[k .. i] = 1 end _doc.t = t", MsgPack::MAP());
	CHECK(keys.too_large);
	auto within = call("local s = string.rep('x', 1048576) local t = {} for i = 1, 12 do t[i] = s end _doc.t = t", MsgPack::MAP());
	REQUIRE(within.ok());
	CHECK_EQ(within.doc.at("t").size(), 12u);
}

void test_a_result_past_its_node_budget_is_refused() {
	auto shared = call("local o = {a = 1, b = 2} local t = {} for i = 1, 40000 do t[i] = o end _doc.t = t", MsgPack::MAP());
	CHECK(shared.too_large);   // a map costs 8 units, three values in it 3 more
	CHECK(shared.what.find("values") != std::string::npos);
	auto distinct = call("local t = {} for i = 1, 400000 do t[i] = 'v' .. i end _doc.t = t", MsgPack::MAP());
	CHECK(distinct.too_large);
	auto dropped = call("local k = {} local t = {} for i = 1, 300000 do t[{}] = i end _doc.t = t _doc.k = k", MsgPack::MAP());
	CHECK(dropped.too_large);   // entries the conversion drops still cost a visit
	auto within = call("local t = {} for i = 1, 50000 do t[i] = {i, 'v'} end _doc.t = t", MsgPack::MAP());
	REQUIRE(within.ok());
	CHECK_EQ(within.doc.at("t").size(), 50000u);
}

// --- N1: print is bounded -------------------------------------------------------------------------------

void test_print_is_bounded_per_call() {
	const auto before = Processor::printed_bytes();
	auto out = call(
		"local big = string.rep('p', 1 << 22) for i = 1, 200 do print(big, i, nil, {} ~= nil) end "
		"for i = 1, 100000 do print('line', i) end _doc.done = true",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(out.doc.at("done").boolean());
	const auto logged = Processor::printed_bytes() - before;
	CHECK(logged > 0);
	CHECK(logged <= Limits{}.max_print_bytes);              // 20,000 prints of 4 MiB asked; the budget is per call
	const auto small_before = Processor::printed_bytes();      // small prints never hit the line cut: only the call's budget ends them
	CHECK(call("for i = 1, 100000 do print('line', i) end", MsgPack::MAP()).ok());
	const auto small = Processor::printed_bytes() - small_before;
	CHECK(small > 0);
	CHECK(small <= Limits{}.max_print_bytes);
	const auto line_before = Processor::printed_bytes();
	CHECK(call("print(string.rep('q', 1 << 22))", MsgPack::MAP()).ok());
	CHECK_EQ(Processor::printed_bytes() - line_before, 1024u);   // a line is cut at 1 KiB
	const auto again = Processor::printed_bytes();
	CHECK(call("print('x')", MsgPack::MAP()).ok());           // the next call has its own budget
	CHECK_EQ(Processor::printed_bytes() - again, 1u);
}

}  // namespace detail
