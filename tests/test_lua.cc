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

// Lua scripts (phase 7c D10): a call's own environment, its slices, and its bounds. Each bound ends
// the call with a ScriptLimitError (a 400), and the state serves the next call.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>

#include "lua/exception.h"
#include "lua/processor.h"
#include "task.hh"

#include <unistd.h>

namespace {

// A bound that fails to end its call would hang the test: stop it instead.
[[maybe_unused]] const auto watchdog = [] {
	std::signal(SIGALRM, [](int) { std::fprintf(stderr, "HANG: a script ran past the watchdog\n"); ::_exit(3); });
	return ::alarm(600);
}();

using lua::Limits;
using lua::Processor;

// What one call left behind.
struct Outcome {
	MsgPack doc;
	bool limit = false;      // ScriptLimitError
	bool too_large = false;  // ScriptResultTooLarge
	bool script_error = false;   // ScriptError: the script's own
	bool cancelled = false;
	bool failed = false;     // any other exception
	std::string what;
	bool ok() const { return !limit && !too_large && !script_error && !cancelled && !failed; }
};

Outcome outcome_of(std::exception_ptr error) {
	Outcome out;
	if (!error) {
		return out;
	}
	try {
		std::rethrow_exception(error);
	} catch (const lua::ScriptLimitError& e) {
		out.limit = true;
		out.what = e.what();
	} catch (const lua::ScriptResultTooLarge& e) {
		out.too_large = true;
		out.what = e.what();
	} catch (const lua::ScriptError& e) {
		out.script_error = true;
		out.what = e.what();
	} catch (const task::cancelled&) {
		out.cancelled = true;
	} catch (const std::exception& e) {
		out.failed = true;
		out.what = e.what();
	}
	return out;
}

MsgPack doc_of(std::initializer_list<std::pair<const char*, int64_t>> fields) {
	MsgPack doc = MsgPack::MAP();
	for (auto& [key, value] : fields) {
		doc[key] = value;
	}
	return doc;
}

// Runs one call to its end on this thread (no budget: it never suspends).
Outcome call(Processor& processor, MsgPack doc, const MsgPack& params = MsgPack::MAP()) {
	::alarm(600);   // the watchdog bounds one call, not the whole run (a sanitizer build is slow)
	Outcome out;
	out.doc = std::move(doc);
	MsgPack old_doc = MsgPack::MAP();
	std::exception_ptr error;
	try {
		task::sync_wait(processor("PUT", out.doc, old_doc, params));
	} catch (...) {
		error = std::current_exception();
	}
	auto result = outcome_of(error);
	result.doc = std::move(out.doc);
	return result;
}

Outcome call(const std::string& body, MsgPack doc, Limits limits = {}) {
	Processor processor("test", body, MsgPack::MAP(), limits);
	return call(processor, std::move(doc));
}

// A call that runs on a scheduler under a budget, to interleave with others.
struct Pending {
	MsgPack doc;
	MsgPack old_doc = MsgPack::MAP();
	MsgPack params = MsgPack::MAP();
	bool finished = false;
	Outcome outcome;
};

void start(task::inline_scheduler& scheduler, Pending& pending, Processor& processor, task::budget& budget) {
	task::spawn(task::scheduler_ref::of(scheduler),
		processor("PUT", pending.doc, pending.old_doc, pending.params, &budget),
		[&pending](std::exception_ptr error) {
			pending.outcome = outcome_of(error);
			pending.finished = true;
		});
}

constexpr std::chrono::nanoseconds every_checkpoint{1};   // a slice so short that every checkpoint yields

// A nil assigned to a field leaves no field.
bool absent(const MsgPack& doc, const char* key) {
	return doc.find(key) == doc.end();
}

std::string str(const MsgPack& doc, const char* key) {
	return doc.at(key).str();
}


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


// --- K1: a call's own environment ----------------------------------------------------------------------

// The architect's L4 and L5 probe: calls A and B of one script interleave; B writes through `_G`
// and into `string`; A, mid-run, sees neither its own `_doc` replaced nor B's writes.
const char* const leak_probe =
	"local mine = _doc.who; "
	"if mine == 'B' then _G.leak = 'B'; string.leak = 'B'; table.leak = 'B'; math.leak = 'B' end; "   // B writes at once...
	"local s = 0; for i = 1, 200000 do s = s + i end; "                                                // ...and both calls take turns...
	"_doc.mine = mine; _doc.seen = _doc.who; _doc.glob = leak; _doc.sleak = string.leak; _doc.tleak = table.leak; "
	"_doc.mleak = math.leak; _doc.same = (_G == _ENV)";                                                  // ...before A reads

void test_interleaved_calls_keep_their_own_doc_and_globals() {
	Processor processor("leak", leak_probe, MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending a, b;
	a.doc = MsgPack({{"who", "A"}});
	b.doc = MsgPack({{"who", "B"}});
	start(scheduler, a, processor, budget);
	start(scheduler, b, processor, budget);
	scheduler.run();
	REQUIRE(a.finished);
	REQUIRE(b.finished);
	REQUIRE(a.outcome.ok());
	REQUIRE(b.outcome.ok());
	for (Pending* p : {&a, &b}) {
		CHECK_EQ(str(p->doc, "mine"), str(p->doc, "who"));
		CHECK_EQ(str(p->doc, "seen"), str(p->doc, "who"));   // its own _doc at the end, not the other call's
		CHECK(p->doc.at("same").boolean());
	}
	CHECK(absent(a.doc, "glob"));     // B's writes through _G and into the libraries do not reach A
	CHECK(absent(a.doc, "sleak"));
	CHECK(absent(a.doc, "tleak"));
	CHECK(absent(a.doc, "mleak"));
	CHECK_EQ(str(b.doc, "glob"), "B");     // B sees what it wrote
	CHECK_EQ(str(b.doc, "sleak"), "B");
	CHECK_EQ(str(b.doc, "tleak"), "B");
}

void test_the_calls_really_interleaved() {
	Processor processor("slices", "local s = 0; for i = 1, 300000 do s = s + i end; _doc.s = s", MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending a, b;
	a.doc = MsgPack::MAP();
	b.doc = MsgPack::MAP();
	start(scheduler, a, processor, budget);
	start(scheduler, b, processor, budget);
	// A cannot be finished by the time B has started: run until B has been resumed once.
	std::size_t resumes = scheduler.run_until([&] { return a.finished || b.finished; });
	CHECK_GT(resumes, 4u);   // each call took several slices, alternating
	CHECK(!a.finished || !b.finished);
	scheduler.run();
	CHECK(a.finished && b.finished);
}

void test_collection_runs_while_calls_are_suspended() {
	// Garbage is made and collected while three calls take turns: each call's thread stays anchored.
	Processor processor("churn",
		"local keep = {} for i = 1, 200000 do local t = {i, tostring(i), {i}}; if i % 1000 == 0 then keep[#keep + 1] = t end end; "
		"_doc.n = #keep; _doc.last = keep[#keep][2]",
		MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending calls[3];
	for (auto& pending : calls) {
		pending.doc = MsgPack::MAP();
		start(scheduler, pending, processor, budget);
	}
	scheduler.run();
	for (auto& pending : calls) {
		REQUIRE(pending.finished);
		REQUIRE(pending.outcome.ok());
		CHECK_EQ(pending.doc.at("n").i64(), 200);
		CHECK_EQ(str(pending.doc, "last"), "200000");
	}
}

void test_writes_do_not_outlive_their_call() {
	// Each call starts from the base: the last call's globals and library writes are gone.
	Processor processor("persist",
		"_doc.before = rawget(_G, 'kept'); _doc.sb = string.kept; _doc.tb = table.kept; _doc.hb = math.huge; "
		"kept = 'yes'; string.kept = 'yes'; table.kept = 1; math.huge = 0",
		MsgPack::MAP());
	for (int i = 0; i < 3; ++i) {
		auto out = call(processor, MsgPack::MAP());
		REQUIRE(out.ok());
		CHECK(absent(out.doc, "before"));
		CHECK(absent(out.doc, "sb"));
		CHECK(absent(out.doc, "tb"));
		CHECK(out.doc.at("hb").f64() > 1e300);
	}
}

void test_the_environment_is_closed() {
	auto out = call(
		"_doc.meta = tostring(getmetatable(_G)); _doc.smeta = tostring(getmetatable('')); "
		"_doc.set = pcall(setmetatable, _G, {}); _doc.load = tostring(load); _doc.dofile = tostring(dofile); "
		"_doc.gc = tostring(collectgarbage); _doc.os = tostring(os); _doc.io = tostring(io); _doc.dbg = tostring(debug); "
		"_doc.req = tostring(require); _doc.upper = ('abc'):upper(); _doc.fmt = string.format('%d', 3)",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(str(out.doc, "meta"), "false");
	CHECK_EQ(str(out.doc, "smeta"), "string");
	CHECK(!out.doc.at("set").boolean());
	for (const char* name : {"load", "dofile", "gc", "os", "io", "dbg", "req"}) {
		CHECK_EQ(str(out.doc, name), "nil");
	}
	CHECK_EQ(str(out.doc, "upper"), "ABC");
	CHECK_EQ(str(out.doc, "fmt"), "3");
}

void test_a_script_cannot_set_a_finalizer() {
	// A __gc runs with hooks off: one that never returns would hold the loop.
	auto out = call(
		"_doc.ok = pcall(setmetatable, {}, {__gc = function() while true do end end}); "
		"for i = 1, 300000 do local t = {i} end; _doc.done = true",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(!out.doc.at("ok").boolean());
	CHECK(out.doc.at("done").boolean());
}

void test_a_changed_script_replaces_its_cached_processor() {
	MsgPack object_v1({{"_name", "cache-me"}, {"_body", "_doc.v = 1"}});   // a Script holds views into its object
	MsgPack object_v2({{"_name", "cache-me"}, {"_body", "_doc.v = 2"}});
	Script script_v1(object_v1);
	Script script_v2(object_v2);
	auto first = Processor::compile_local(script_v1);
	CHECK(Processor::compile_local(script_v1) == first);
	auto second = Processor::compile_local(script_v2);
	CHECK(second != first);
	CHECK(Processor::compile_local(script_v2) == second);
	auto out = call(*second, MsgPack::MAP());
	CHECK_EQ(out.doc.at("v").i64(), 2);
	std::shared_ptr<Processor> other;
	std::thread([&] { other = Processor::compile_local(script_v2); }).join();
	CHECK(other != second);   // each thread keeps its own
}


// --- K2: the bounds ------------------------------------------------------------------------------------

Limits quick() {
	Limits limits;
	limits.max_instructions = 2'000'000;
	limits.max_bytes = 8u << 20;
	return limits;
}

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
	auto rep = call("local s = string.rep('x', 1024 * 1024 * 1024)", MsgPack::MAP(), quick());
	CHECK(rep.limit);
	CHECK(rep.what.find("memory limit") != std::string::npos);
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



// --- X4-S1: a call that crossed a bound cannot be resumed or caught -------------------------------------

// The reviewer's probe (p14): a to-be-closed variable whose handler raises replaces the bound's error in flight,
// the script's own pcall catches the replacement, and the hook raises again 10,000 instructions later.
const char* const close_swallow =
	"local n = 0 while true do pcall(function() "
	"local mt = {} local x <close> = setmetatable({}, mt) mt.__close = function() error('swallowed') end "
	"while true do end end) n = n + 1 end";
const char* const close_swallow_late =   // the metamethod is added after setmetatable, which is how the refusal is walked around
	"while true do pcall(function() "
	"local mt = {} local t = setmetatable({}, mt) mt.__close = function() error('swallowed') end "
	"local x <close> = t while true do end end) end";
const char* const close_swallow_xpcall =
	"while true do xpcall(function() "
	"local mt = {} local t = setmetatable({}, mt) mt.__close = function() error('swallowed') end "
	"local x <close> = t while true do end end, function(m) return m end) end";
const char* const close_swallow_genfor =
	"while true do pcall(function() local mt = {} local t = setmetatable({}, mt) mt.__close = function() error('swallowed') end "
	"for _ in next, {}, nil, t do end local c <close> = t while true do end end) end";

void test_a_script_cannot_declare_a_to_be_closed_variable() {
	auto out = call(
		"local mt = {} local t = setmetatable({}, mt) mt.__close = function() end "
		"_doc.a, _doc.am = pcall(function() local x <close> = t end) "
		"_doc.b = pcall(function() for _ in next, {}, nil, t do end end) "
		"_doc.c = pcall(setmetatable, {}, {__close = function() end}) "
		"_doc.d = pcall(function() local x <close> = nil end)",     // nil and false close nothing
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(!out.doc.at("a").boolean());
	CHECK(str(out.doc, "am").find("to-be-closed") != std::string::npos);
	CHECK(!out.doc.at("b").boolean());
	CHECK(!out.doc.at("c").boolean());
	CHECK(out.doc.at("d").boolean());
}

void test_a_close_handler_cannot_outlive_a_bound() {
	for (const char* body : {close_swallow, close_swallow_late, close_swallow_xpcall, close_swallow_genfor}) {
		auto refused = call(body, MsgPack::MAP(), quick());
		CHECK(refused.limit);
		CHECK(refused.what.find("instruction limit") != std::string::npos);
	}
	// With close handlers permitted the language itself no longer helps: the host's condemned flag must end the call.
	Limits limits = quick();
	limits.allow_close = true;
	for (const char* body : {close_swallow_late, close_swallow_xpcall, close_swallow_genfor}) {
		auto out = call(body, MsgPack::MAP(), limits);
		CHECK(out.limit);
		CHECK(out.what.find("instruction limit") != std::string::npos);
	}
	// ...and a handler that never returns is cut at once: each instruction of a condemned call raises.
	auto spin = call(
		"local mt = {} local t = setmetatable({}, mt) mt.__close = function() while true do end end "
		"local x <close> = t while true do end", MsgPack::MAP(), limits);
	CHECK(spin.limit);
	auto after = call("_doc.ok = true", MsgPack::MAP(), limits);   // the next call on a fresh state, and this thread, are fine
	CHECK(after.ok());
}


// --- X4-S2: work the instruction hook cannot see --------------------------------------------------------

void test_library_calls_are_charged_by_what_they_touch() {
	// Each loop is a few thousand instructions of ordinary Lua around a call that touches megabytes: it ends
	// at the work cap, with the instruction cap nowhere near.
	const char* const header = "local big = string.rep('abcdefgh', 524288) "      // 4 MiB
	                           "local t = {} for i = 1, 100000 do t[i] = 'ab' end ";
	struct Case { const char* name; const char* loop; };
	const Case cases[] = {
		{"plain find, absent", "for i = 1, 20000 do big:find('z', 1, true) end"},
		{"find, no magic", "for i = 1, 20000 do big:find('zz') end"},
		{"sub", "for i = 1, 20000 do local _ = big:sub(2, -2) end"},
		{"upper", "for i = 1, 20000 do local _ = big:upper() end"},
		{"lower", "for i = 1, 20000 do local _ = big:lower() end"},
		{"reverse", "for i = 1, 20000 do local _ = big:reverse() end"},
		{"rep", "for i = 1, 20000 do local _ = ('x'):rep(3000000) end"},
		{"rep of the empty string", "string.rep('', 1e15)"},
		{"format %s", "for i = 1, 20000 do local _ = string.format('%s', big) end"},
		{"concat operator", "for i = 1, 20000 do local _ = big .. big end"},
		{"string equality", "local other = big:sub(1, -2) .. 'x' local same = big:sub(1, -2) .. 'x' for i = 1, 20000 do local _ = (other == same) end"},
		{"string comparison", "local other = big:sub(1, -2) .. 'x' for i = 1, 20000 do local _ = (big < other) end"},
		{"utf8.len", "for i = 1, 20000 do utf8.len(big) end"},
		{"utf8.codepoint", "for i = 1, 20000 do utf8.codepoint(big, 1, 5000) end"},
		{"utf8.offset", "for i = 1, 20000 do utf8.offset(big, 3000000) end"},
		{"table.concat", "for i = 1, 20000 do table.concat(t) end"},
		{"table.unpack", "for i = 1, 20000 do select('#', table.unpack(t, 1, 50000)) end"},
		{"table.insert at the front", "for i = 1, 20000 do table.insert(t, 1, 'x') end"},
		{"table.remove at the front", "for i = 1, 20000 do table.remove(t, 1) end"},
		{"table.move", "for i = 1, 20000 do table.move(t, 1, 100000, 2) end"},
		{"gsub of a long replacement", "for i = 1, 20000 do big:sub(1, 4):gsub('a', big:sub(1, 1000000)) end"},
	};
	for (const auto& c : cases) {
		Limits limits;
		limits.max_instructions = 50'000'000;   // the loops below run far fewer
		auto out = call(std::string(header) + c.loop, MsgPack::MAP(), limits);
		if (out.what.find("work limit") == std::string::npos) { std::fprintf(stderr, "  case %s: %s\n", c.name, out.what.c_str()); }
		CHECK(out.limit);
		CHECK(out.what.find("work limit") != std::string::npos);
	}
}

void test_a_slow_search_ends_at_the_work_cap() {
	// The reviewer's quadratic substring search (p4_cstall quad): it is inside one C call, so the library
	// must charge as it scans.
	auto plain = call("local h = string.rep('a', 400000) local n = string.rep('a', 200000) .. 'b' _doc.r = h:find(n, 1, true)", MsgPack::MAP());
	CHECK(plain.limit);
	CHECK(plain.what.find("work limit") != std::string::npos);
	auto nomagic = call("local h = string.rep('a', 400000) local n = string.rep('a', 200000) .. 'b' _doc.r = h:find(n)", MsgPack::MAP());
	CHECK(nomagic.limit);
}

void test_the_matcher_is_charged_by_work_not_by_entries() {
	// The reviewer's p13 shapes: a long literal run, then a magic character; and a long bracket class. Each
	// costs subject x pattern while taking about one `match` entry per start position.
	auto literal = call("local h = string.rep('a', 65536) local p = string.rep('a', 32768) .. '.b' _doc.r = h:find(p)", MsgPack::MAP());
	CHECK(literal.limit);
	auto klass = call("local h = string.rep('a', 65536) local p = '[' .. string.rep('x', 32768) .. ']' _doc.r = h:find(p)", MsgPack::MAP());
	CHECK(klass.limit);
	auto balance = call("local h = string.rep('(', 100000) _doc.r = h:find('%b()%d')", MsgPack::MAP());
	CHECK(balance.limit);
	auto backref = call("local h = string.rep('a', 20000) .. 'b' _doc.r = h:find('(.-)%1c')", MsgPack::MAP());
	CHECK(backref.limit);
	// Many calls, each well inside its own step budget: the sum is what the call's meter ends.
	auto many = call("local h = string.rep('a', 500) for i = 1, 1000 do h:find('.-b') end", MsgPack::MAP());
	CHECK(many.limit);
	CHECK(many.what.find("work limit") != std::string::npos);
}

void test_a_vararg_chain_ends_at_the_work_cap() {
	// The reviewer's p18: every call copies a growing argument list, which the hook sees as three instructions.
	auto chain = call("local function f(...) return f(1, ...) end f()", MsgPack::MAP());
	CHECK(chain.limit);
	auto spread = call("local t = {} for i = 1, 150000 do t[i] = i end while true do local n = select('#', table.unpack(t)) end", MsgPack::MAP());
	CHECK(spread.limit);
	auto pack = call("local t = {} for i = 1, 100000 do t[i] = i end while true do local p = {table.unpack(t)} end", MsgPack::MAP());
	CHECK(pack.limit);
}

void test_weak_tables_are_refused_and_never_weak() {
	auto out = call(
		"_doc.a = pcall(setmetatable, {}, {__mode = 'k'}) "
		"local mt = {} local t = setmetatable({}, mt) mt.__mode = 'kv' "      // set after setmetatable: walks around the refusal
		"t[{}] = 1 t[2] = {} "
		"for i = 1, 1500000 do local garbage = {i} end "                    // many collections later...
		"local n = 0 for _ in pairs(t) do n = n + 1 end _doc.n = n",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(!out.doc.at("a").boolean());
	CHECK_EQ(out.doc.at("n").i64(), 2);      // ...nothing was collected from the table
}

void test_the_uncharged_functions_are_retired() {
	auto out = call(
		"_doc.a = string.pack == nil and string.unpack == nil and string.packsize == nil and string.dump == nil "
		"_doc.b = load == nil and collectgarbage == nil",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(out.doc.at("a").boolean());
	CHECK(out.doc.at("b").boolean());
}

void test_table_sort_without_a_comparator_is_the_merge_sort() {
	auto out = call(
		"local t = {} for i = 1, 50000 do t[i] = (i * 7919) % 50021 end table.sort(t) "
		"local ok = true for i = 2, #t do if t[i - 1] > t[i] then ok = false end end _doc.ok = ok "
		"local w = {'pear', 'apple', 'fig'} table.sort(w) _doc.w = table.concat(w, ',') "
		"_doc.bad = pcall(table.sort, {1, 'a'})",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(out.doc.at("ok").boolean());
	CHECK_EQ(str(out.doc, "w"), "apple,fig,pear");
	CHECK(!out.doc.at("bad").boolean());
	// A million elements is past the instruction cap, not a stall of the loop.
	auto huge = call("local t = {} for i = 1, 1000000 do t[i] = (i * 7919) % 1000003 end table.sort(t)", MsgPack::MAP());
	CHECK(huge.limit);
}

task::task<void> other_requests(std::size_t rounds, std::size_t& turns, const bool& script_finished, bool& script_finished_at_end);

void test_a_charged_loop_yields_before_its_slice_of_instructions_is_spent() {
	// 200 calls of a 4 MiB scan are ~4 slices' worth of work but only ~1,000 instructions: the call must still
	// hand the loop back far more often than every 10,000 instructions.
	Processor processor("scan", "local big = string.rep('a', 4194304) for i = 1, 60 do big:find('b', 1, true) end", MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending pending;
	pending.doc = MsgPack::MAP();
	std::size_t turns = 0;
	bool script_finished_at_end = true;
	task::spawn(task::scheduler_ref::of(scheduler), other_requests(40, turns, pending.finished, script_finished_at_end), [](std::exception_ptr) {});
	start(scheduler, pending, processor, budget);
	scheduler.run();
	REQUIRE(pending.outcome.ok());
	CHECK_EQ(turns, 40u);
	CHECK(!script_finished_at_end);   // other work got its turns before the scan loop was done
}

void test_patched_library_functions_still_compute() {
	auto out = call(
		"local h = 'the quick brown fox jumps over the lazy dog' "
		"_doc.f1, _doc.f2 = h:find('brown', 1, true); _doc.f3 = h:find('cat', 1, true); _doc.f4 = h:find('dog', -3, true); "
		"_doc.f5 = h:find('', 5, true); _doc.f6 = h:find('o', 20, true); "
		"_doc.sub = h:sub(5, 9) .. '|' .. h:sub(-3) .. '|' .. h:sub(10, 5); _doc.rep = ('ab'):rep(3, '-') .. '|' .. (''):rep(5); "
		"_doc.up = ('mIx'):upper() .. ('mIx'):lower() .. ('abc'):reverse(); _doc.bytes = table.concat({('abc'):byte(1, -1)}, ','); "
		"_doc.u1 = utf8.len('héllo'); _doc.u2 = table.concat({utf8.codepoint('hé', 1, -1)}, ','); _doc.u3 = utf8.offset('héllo', 3); _doc.u4 = utf8.offset('héllo', -1); "
		"_doc.fmt = string.format('%s|%5.2f|%q', 'x', 3.14159, 'a\"b'); "
		"local t = {1, 2, 3, 4, 5}; table.insert(t, 3, 'x'); _doc.ins = table.concat(t, ','); table.remove(t, 1); _doc.rem = table.concat(t, ','); "
		"_doc.mv = table.concat(table.move({1, 2, 3}, 1, 3, 2), ','); _doc.unp = select('#', table.unpack({1, 2, 3, nil, 5}, 1, 5)); "
		"local gs = {} for a, b in ('k1=v1, k2=v2'):gmatch('(%w+)=(%w+)') do gs[#gs + 1] = a .. b end _doc.gm = table.concat(gs, ';'); "
		"_doc.gs = ('hello world'):gsub('o', '0'); _doc.gs2 = ('abc'):gsub('%w', '%0%0'); _doc.gs3 = ('abc'):gsub('(b)', '[%1]'); "
		"_doc.m = ('2026-11-02'):match('(%d+)-(%d+)'); _doc.bal = ('f(a(b)c)d'):match('%b()'); _doc.fr = ('THE (quick) fox'):find('%f[%a]%a+%f[%A]'); "
		"_doc.br = ('a1b2'):gsub('[%d]', '#'); _doc.cat = 'a' .. 1 .. 2.5 .. 'z'; _doc.lt = ('a' < 'b') and ('abc' < 'abd') and not ('b' < 'a'); "
		"_doc.va = select('#', (function(...) return ... end)(table.unpack({1, 2, 3}, 1, 30)))",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("f1").i64(), 11);
	CHECK_EQ(out.doc.at("f2").i64(), 15);
	CHECK(out.doc.find("f3") == out.doc.end());
	CHECK_EQ(out.doc.at("f4").i64(), 41);
	CHECK_EQ(out.doc.at("f5").i64(), 5);
	CHECK_EQ(out.doc.at("f6").i64(), 27);
	CHECK_EQ(str(out.doc, "sub"), "quick|dog|");
	CHECK_EQ(str(out.doc, "rep"), "ab-ab-ab|");
	CHECK_EQ(str(out.doc, "up"), "MIXmixcba");
	CHECK_EQ(str(out.doc, "bytes"), "97,98,99");
	CHECK_EQ(out.doc.at("u1").i64(), 5);
	CHECK_EQ(str(out.doc, "u2"), "104,233");
	CHECK_EQ(out.doc.at("u3").i64(), 4);
	CHECK_EQ(out.doc.at("u4").i64(), 6);
	CHECK_EQ(str(out.doc, "fmt"), "x| 3.14|\"a\\\"b\"");
	CHECK_EQ(str(out.doc, "ins"), "1,2,x,3,4,5");
	CHECK_EQ(str(out.doc, "rem"), "2,x,3,4,5");
	CHECK_EQ(str(out.doc, "mv"), "1,1,2,3");
	CHECK_EQ(out.doc.at("unp").i64(), 5);
	CHECK_EQ(str(out.doc, "gm"), "k1v1;k2v2");
	CHECK_EQ(str(out.doc, "gs"), "hell0 w0rld");
	CHECK_EQ(str(out.doc, "gs2"), "aabbcc");
	CHECK_EQ(str(out.doc, "gs3"), "a[b]c");
	CHECK_EQ(str(out.doc, "m"), "2026");
	CHECK_EQ(str(out.doc, "bal"), "(a(b)c)");
	CHECK_EQ(out.doc.at("fr").i64(), 1);
	CHECK_EQ(str(out.doc, "br"), "a#b#");
	CHECK_EQ(str(out.doc, "cat"), "a12.5z");
	CHECK(out.doc.at("lt").boolean());
	CHECK_EQ(out.doc.at("va").i64(), 30);
}

void test_plain_find_agrees_with_std_string_find() {
	// lmemfind now meters itself: its answers must not change. Random strings over a tiny alphabet.
	Processor processor("find", "_doc.r = (h:find(n, init, true)) or 0", MsgPack::MAP());
	uint32_t seed = 12345;
	auto next = [&] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
	for (int round = 0; round < 400; ++round) {
		std::string h, n;
		const auto hl = next() % 80, nl = next() % 6;
		for (uint32_t i = 0; i < hl; ++i) { h += char('a' + next() % 2); }
		for (uint32_t i = 0; i < nl; ++i) { n += char('a' + next() % 2); }
		const std::size_t init = h.empty() ? 0 : next() % h.size();
		MsgPack doc = MsgPack::MAP();
		MsgPack params({{"h", h}, {"n", n}, {"init", static_cast<int64_t>(init + 1)}});
		MsgPack old_doc = MsgPack::MAP();
		task::sync_wait(processor("PUT", doc, old_doc, params));
		const auto want = h.find(n, init);
		const int64_t expected = want == std::string::npos ? 0 : static_cast<int64_t>(want) + 1;
		CHECK_EQ(doc.at("r").i64(), expected);
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


// --- X4-S4: local names and foreign references are different keys ---------------------------------------

void test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference() {
	// The foreign key is a database path and a document id; a client can name a local script the same.
	const std::string reference = "scripts/shared";
	MsgPack local_object({{"_name", reference}, {"_body", "_doc.who = 'attacker'"}});
	Script local_script(local_object);
	auto foreign = std::make_shared<Processor>("scripts/shared", "_doc.who = 'victim'", MsgPack::MAP());
	Processor::remember_foreign(reference, foreign);
	auto local = Processor::compile_local(local_script);           // saved under the same name afterwards...
	CHECK(Processor::cached_foreign(reference) == foreign);        // ...the foreign entry stays
	CHECK(Processor::cached_local(reference) == local);
	CHECK(local != foreign);
	// The other order: the local script is cached first.
	const std::string reference2 = "scripts/other";
	MsgPack local_object2({{"_name", reference2}, {"_body", "_doc.who = 'attacker'"}});
	Script local_script2(local_object2);
	auto local2 = Processor::compile_local(local_script2);
	CHECK(Processor::cached_foreign(reference2) == nullptr);       // a foreign reference misses
	auto foreign2 = std::make_shared<Processor>("scripts/other", "_doc.who = 'victim'", MsgPack::MAP());
	Processor::remember_foreign(reference2, foreign2);
	CHECK(Processor::cached_local(reference2) == local2);
	CHECK(Processor::cached_foreign(reference2) == foreign2);
	CHECK_EQ(call(*Processor::cached_foreign(reference2), MsgPack::MAP()).doc.at("who").str(), "victim");
}


// --- N1: print is bounded -------------------------------------------------------------------------------

void test_print_is_bounded_per_call() {
	auto out = call(
		"local big = string.rep('p', 1 << 22) for i = 1, 200 do print(big, i, nil, {} ~= nil) end "
		"for i = 1, 100000 do print('line', i) end _doc.done = true",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK(out.doc.at("done").boolean());
}


// --- K3: slices ----------------------------------------------------------------------------------------

// Other work on the loop: `rounds` turns, then it reports whether the script had finished.
task::task<void> other_requests(std::size_t rounds, std::size_t& turns, const bool& script_finished, bool& script_finished_at_end) {
	for (; turns < rounds; ++turns) {
		co_await task::yield();
	}
	script_finished_at_end = script_finished;
}

void test_a_long_script_lets_other_work_run() {
	Processor processor("long", "local s = 0; for i = 1, 5000000 do s = s + i end; _doc.s = s", MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending pending;
	pending.doc = MsgPack::MAP();
	std::size_t turns = 0;
	bool script_finished_at_end = true;
	task::spawn(task::scheduler_ref::of(scheduler), other_requests(200, turns, pending.finished, script_finished_at_end), [](std::exception_ptr) {});
	start(scheduler, pending, processor, budget);
	// The script's 500 slices are demoted behind the other work, which is served first, in full.
	scheduler.run();
	REQUIRE(pending.outcome.ok());
	CHECK_EQ(turns, 200u);
	CHECK(!script_finished_at_end);   // the other requests finished their turns while the script was still running
	CHECK_EQ(pending.doc.at("s").i64(), int64_t(5000000) * 5000001 / 2);
}

void test_without_a_budget_a_script_runs_to_its_end() {
	auto out = call("local s = 0; for i = 1, 1000000 do s = s + i end; _doc.s = s", MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("s").i64(), int64_t(1000000) * 1000001 / 2);
}

void test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread() {
	Processor processor("cancel", "if _doc.spin then while true do end end _doc.ok = true", MsgPack::MAP(), quick());
	task::inline_scheduler scheduler;
	task::cancel_source source;
	task::budget budget(every_checkpoint, source.token());
	Pending pending;
	pending.doc = MsgPack({{"spin", true}});
	start(scheduler, pending, processor, budget);
	std::size_t n = 0;
	scheduler.run_until([&] { return ++n > 6; });
	CHECK(!pending.finished);
	source.request();
	scheduler.run();
	CHECK(pending.finished);
	CHECK(pending.outcome.cancelled);
	auto after = call(processor, MsgPack::MAP());   // the state serves the next call
	REQUIRE(after.ok());
	CHECK(after.doc.at("ok").boolean());
}

}  // namespace

TEST_MAIN(
	test_a_script_transforms_the_document,
	test_params_and_old_doc_are_in_scope,
	test_numbers_keep_their_kind,
	test_a_script_error_is_a_client_error_and_leaves_the_document_as_it_was,
	test_a_syntax_error_names_its_line,
	test_interleaved_calls_keep_their_own_doc_and_globals,
	test_the_calls_really_interleaved,
	test_collection_runs_while_calls_are_suspended,
	test_writes_do_not_outlive_their_call,
	test_the_environment_is_closed,
	test_a_script_cannot_set_a_finalizer,
	test_a_script_cannot_declare_a_to_be_closed_variable,
	test_a_changed_script_replaces_its_cached_processor,
	test_an_infinite_loop_ends_at_the_instruction_cap,
	test_pcall_cannot_swallow_a_bound,
	test_a_runaway_callback_ends_at_the_non_yieldable_bound,
	test_a_runaway_sort_comparator_ends,
	test_table_sort_with_a_comparator_is_a_merge_sort_that_completes,
	test_an_exponential_pattern_ends_at_the_matcher_step_budget,
	test_ordinary_patterns_stay_inside_the_budget,
	test_an_allocation_past_the_cap_ends,
	test_the_state_serves_the_next_call_after_every_bound,
	test_a_document_with_a_cycle_or_a_shared_blowup_is_refused,
	test_conversion_runs_no_script_code,
	test_a_close_handler_cannot_outlive_a_bound,
	test_library_calls_are_charged_by_what_they_touch,
	test_a_slow_search_ends_at_the_work_cap,
	test_the_matcher_is_charged_by_work_not_by_entries,
	test_a_vararg_chain_ends_at_the_work_cap,
	test_weak_tables_are_refused_and_never_weak,
	test_the_uncharged_functions_are_retired,
	test_table_sort_without_a_comparator_is_the_merge_sort,
	test_patched_library_functions_still_compute,
	test_plain_find_agrees_with_std_string_find,
	test_a_result_past_its_byte_budget_is_refused,
	test_a_result_past_its_node_budget_is_refused,
	test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference,
	test_print_is_bounded_per_call,
	test_a_long_script_lets_other_work_run,
	test_without_a_budget_a_script_runs_to_its_end,
	test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread,
	test_a_charged_loop_yields_before_its_slice_of_instructions_is_spent
)
