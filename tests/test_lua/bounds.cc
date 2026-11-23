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

// The bounds: instruction, callback, sort, pattern and allocation caps, and the state after each.

#include "support.h"
#include "tests.h"

namespace detail {

// --- K2: the bounds ------------------------------------------------------------------------------------

void test_an_infinite_loop_ends_at_the_instruction_cap() {
	auto out = call("while true do end", MsgPack::MAP(), quick());
	CHECK(out.limit);
	CHECK(out.what.find("instruction limit") != std::string::npos);
	auto defaults = call("local n = 0; while true do n = n + 1 end", MsgPack::MAP());   // the design's 10^8
	CHECK(defaults.limit);
}

void test_pcall_cannot_swallow_a_bound() {
	auto out = call("while true do pcall(function() while true do end end) end", MsgPack::MAP(), quick());
	CHECK(out.limit);
	auto xout = call("while true do xpcall(function() while true do end end, function(e) return 'caught' end) end", MsgPack::MAP(), quick());
	CHECK(xout.limit);
	auto nested = call("while true do pcall(pcall, function() while true do end end) end", MsgPack::MAP(), quick());
	CHECK(nested.limit);
}

void test_a_runaway_callback_ends_at_the_non_yieldable_bound() {
	// A gsub callback and an __lt called by the native sort cannot yield: 100 hook calls in a row end the call.
	auto gsub = call("string.gsub('abc', '.', function() while true do end end)", MsgPack::MAP());
	CHECK(gsub.limit);
	CHECK(gsub.what.find("cannot yield") != std::string::npos);
	auto lt = call(
		"local mt = {__lt = function() while true do end end}; "
		"local t = {setmetatable({}, mt), setmetatable({}, mt), setmetatable({}, mt)}; table.sort(t)",
		MsgPack::MAP());
	CHECK(lt.limit);
	// ...and a script that catches the error in the callback and loops cannot go on.
	auto caught = call("string.gsub('abc', '.', function() while true do pcall(function() while true do end end) end end)", MsgPack::MAP());
	CHECK(caught.limit);
}

void test_a_runaway_sort_comparator_ends() {
	auto out = call("local t = {3, 1, 2}; table.sort(t, function(a, b) while true do end end)", MsgPack::MAP(), quick());
	CHECK(out.limit);
}

void test_table_sort_with_a_comparator_is_a_merge_sort_that_completes() {
	// C11: 100,000 elements with a comparator finish inside the default caps, comparator and all.
	auto big = call(
		"local t = {} for i = 1, 100000 do t[i] = (i * 7919) % 100003 end "
		"table.sort(t, function(x, y) return x < y end) "
		"local ok = true for i = 2, #t do if t[i - 1] > t[i] then ok = false end end _doc.ok = ok; _doc.n = #t",
		MsgPack::MAP());
	REQUIRE(big.ok());
	CHECK(big.doc.at("ok").boolean());
	CHECK_EQ(big.doc.at("n").i64(), 100000);

	auto small = call(
		"local function sorted(t, c) table.sort(t, c) return table.concat(t, ',') end "
		"_doc.a = sorted({}); _doc.b = sorted({1}, function(x, y) return x < y end); "
		"_doc.c = sorted({3, 1, 2}, function(x, y) return x > y end); _doc.d = sorted({5, 5, 1, 5, 2}, function(x, y) return x < y end); "
		"_doc.e = sorted({'b', 'a', 'c'}); _doc.f = sorted({4, 3, 2, 1}, function(x, y) return x < y end); "
		"local recs = {{k = 1, v = 'x'}, {k = 0, v = 'y'}, {k = 1, v = 'z'}, {k = 0, v = 'w'}}; "
		"table.sort(recs, function(p, q) return p.k < q.k end); local s = '' for _, r in ipairs(recs) do s = s .. r.v end; _doc.stable = s; "
		"_doc.bad = pcall(table.sort, {1, 2}, 5)",
		MsgPack::MAP());
	REQUIRE(small.ok());
	CHECK_EQ(str(small.doc, "a"), "");
	CHECK_EQ(str(small.doc, "b"), "1");
	CHECK_EQ(str(small.doc, "c"), "3,2,1");
	CHECK_EQ(str(small.doc, "d"), "1,2,5,5,5");
	CHECK_EQ(str(small.doc, "e"), "a,b,c");
	CHECK_EQ(str(small.doc, "f"), "1,2,3,4");
	CHECK_EQ(str(small.doc, "stable"), "ywxz");
	CHECK(!small.doc.at("bad").boolean());
}

void test_an_exponential_pattern_ends_at_the_matcher_step_budget() {
	auto out = call("_doc.r = string.rep('a', 200):find(string.rep('a*', 12) .. 'b')", MsgPack::MAP());
	CHECK(out.limit);
	CHECK(out.what.find("step budget") != std::string::npos);
	// Caught in a loop: the first error is the last.
	auto looped = call("local s = string.rep('a', 200); local p = string.rep('a*', 12) .. 'b'; while true do pcall(string.find, s, p) end", MsgPack::MAP());
	CHECK(looped.limit);
	// The budget spans the whole API call (C12): a quadratic find has no step over max(10^5, 8n) at one start.
	auto quadratic = call("_doc.r = string.find(string.rep('a', 20000), '.-b')", MsgPack::MAP());
	CHECK(quadratic.limit);
	auto gmatch = call("local n = 0; for w in string.gmatch(string.rep('a', 20000), '.-b') do n = n + 1 end; _doc.n = n", MsgPack::MAP());
	CHECK(gmatch.limit);
	auto gsub = call("_doc.r = string.gsub(string.rep('a', 20000), '.-b', 'x')", MsgPack::MAP());
	CHECK(gsub.limit);
}

void test_ordinary_patterns_stay_inside_the_budget() {
	Limits limits;
	limits.max_instructions = 50'000'000;
	auto out = call(
		"local s = string.rep('k=v ', 250000); local n = 0; "
		"for k, v in s:gmatch('(%w+)=(%w+)') do n = n + 1 end; _doc.n = n; "
		"local r, c = s:gsub('(%w+)=(%w+)', '%2=%1'); _doc.c = c; "
		"_doc.m = string.match('2026-11-01', '(%d+)-(%d+)-(%d+)'); _doc.u = string.find('hello world', 'o w', 1, true)",
		MsgPack::MAP(), limits);
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("n").i64(), 250000);
	CHECK_EQ(out.doc.at("c").i64(), 250000);
	CHECK_EQ(str(out.doc, "m"), "2026");
	CHECK_EQ(out.doc.at("u").i64(), 5);
}

void test_an_allocation_past_the_cap_ends() {
	auto rep = call("local a = string.rep('x', 3 * 1024 * 1024) local b = a .. a .. a", MsgPack::MAP(), quick());   // 9 MiB live against 8 MiB
	CHECK(rep.limit);
	CHECK(rep.what.find("memory limit") != std::string::npos);
	CHECK(call("local s = string.rep('x', 1024 * 1024 * 1024)", MsgPack::MAP(), quick()).limit);   // refused whole: past the per-call cap
	auto grow = call("local t = {} for i = 1, 100000000 do t[i] = i end", MsgPack::MAP(), quick());
	CHECK(grow.limit);
	auto strings = call("local t = {} for i = 1, 100000000 do t[i] = 'k' .. i end", MsgPack::MAP(), quick());
	CHECK(strings.limit);
	auto caught = call("while true do pcall(string.rep, 'x', 1024 * 1024 * 1024) end", MsgPack::MAP(), quick());
	CHECK(caught.limit);
	auto defaults = call("local s = string.rep('x', 1024 * 1024 * 1024)", MsgPack::MAP());   // the design's 64 MiB
	CHECK(defaults.limit);
}

void test_the_state_serves_the_next_call_after_every_bound() {
	Processor processor("after",
		"if _doc.mode == 'loop' then while true do end elseif _doc.mode == 'mem' then local s = string.rep('x', 1 << 30) "
		"elseif _doc.mode == 'pat' then string.rep('a', 200):find(string.rep('a*', 12) .. 'b') "
		"elseif _doc.mode == 'cb' then string.gsub('abc', '.', function() while true do end end) end "
		"_doc.done = true",
		MsgPack::MAP(), quick());
	for (const char* mode : {"loop", "mem", "pat", "cb", "none", "loop", "none"}) {
		MsgPack doc({{"mode", mode}});
		auto out = call(processor, doc);
		if (std::string(mode) == "none") {
			REQUIRE(out.ok());
			CHECK(out.doc.at("done").boolean());
		} else {
			CHECK(out.limit);
		}
	}
}

void test_a_document_with_a_cycle_or_a_shared_blowup_is_refused() {
	auto cycle = call("_doc.me = _doc", MsgPack::MAP());
	CHECK(cycle.failed);
	CHECK(cycle.what.find("too deeply nested") != std::string::npos);
	Limits limits;
	limits.max_bytes = 16u << 20;
	auto dag = call("local t = {1} for i = 1, 60 do t = {t, t} end; _doc.t = t", MsgPack::MAP(), limits);
	CHECK(dag.too_large);
}

void test_conversion_runs_no_script_code() {
	// A metamethod that never returns would run outside the hook if the conversion read through it.
	auto out = call("setmetatable(_doc, {__index = function() while true do end end, __len = function() while true do end end}); _doc.x = 1",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("x").i64(), 1);
}

}  // namespace detail
