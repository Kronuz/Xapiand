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
#include <exception>
#include <memory>
#include <string>
#include <thread>

#include "lua/exception.h"
#include "lua/processor.h"
#include "task.hh"

namespace {

using lua::Limits;
using lua::Processor;

// What one call left behind.
struct Outcome {
	MsgPack doc;
	bool limit = false;      // ScriptLimitError
	bool cancelled = false;
	bool failed = false;     // any other exception
	std::string what;
	bool ok() const { return !limit && !cancelled && !failed; }
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

void test_a_script_error_leaves_the_document_as_it_was() {
	auto out = call("_doc.x = 99; error('boom')", doc_of({{"x", 1}}));
	CHECK(out.ok());
	CHECK_EQ(out.doc.at("x").i64(), 1);
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
	"local mine = _doc.who; local s = 0; for i = 1, 200000 do s = s + i end; "
	"if mine == 'B' then _G.leak = 'B'; string.leak = 'B'; table.leak = 'B'; math.leak = 'B' end; "
	"_doc.mine = mine; _doc.seen = _doc.who; _doc.glob = leak; _doc.sleak = string.leak; _doc.tleak = table.leak; "
	"_doc.mleak = math.leak; _doc.same = (_G == _ENV)";

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

void test_writes_do_not_outlive_their_call() {
	Processor processor("persist", "_doc.before = rawget(_G, 'kept'); kept = 'yes'; string.kept = 'yes'; table.kept = 1; math.huge = 0", MsgPack::MAP());
	for (int i = 0; i < 3; ++i) {
		auto out = call(processor, MsgPack::MAP());
		REQUIRE(out.ok());
		CHECK(absent(out.doc, "before"));   // each call starts from the base, not from the last call's globals
	}
	auto probe = call("_doc.s = string.kept; _doc.t = table.kept; _doc.h = math.huge", MsgPack::MAP());
	REQUIRE(probe.ok());
	CHECK(absent(probe.doc, "s"));
	CHECK(absent(probe.doc, "t"));
	CHECK(probe.doc.at("h").f64() > 1e300);
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
	CHECK(cycle.what.find("too large or too deeply nested") != std::string::npos);
	Limits limits;
	limits.max_bytes = 16u << 20;
	auto dag = call("local t = {1} for i = 1, 60 do t = {t, t} end; _doc.t = t", MsgPack::MAP(), limits);
	CHECK(dag.failed);
}

void test_conversion_runs_no_script_code() {
	// A metamethod that never returns would run outside the hook if the conversion read through it.
	auto out = call("setmetatable(_doc, {__index = function() while true do end end, __len = function() while true do end end}); _doc.x = 1",
		MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("x").i64(), 1);
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
	test_a_script_error_leaves_the_document_as_it_was,
	test_a_syntax_error_names_its_line,
	test_interleaved_calls_keep_their_own_doc_and_globals,
	test_the_calls_really_interleaved,
	test_writes_do_not_outlive_their_call,
	test_the_environment_is_closed,
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
	test_a_long_script_lets_other_work_run,
	test_without_a_budget_a_script_runs_to_its_end,
	test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread
)
