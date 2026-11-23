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

// A call's own environment: interleaved calls, cached scripts, garbage and the string table.

#include "support.h"
#include "tests.h"

namespace detail {

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

void test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap() {
	// Each call holds 36 MiB, under the 64 MiB cap alone and over it together: T10's library-call loops where
	// fewer reactors than loops run (platform-rows run 7 answered [204, 204, 400, 400]).
	Processor processor("halves",
		"local t = {} for i = 1, 36 do t[i] = string.rep('x', 1048000 + i) end "
		"local s = 0 for i = 1, 300000 do s = s + i end _doc.n = #t",
		MsgPack::MAP());
	REQUIRE(call(processor, MsgPack::MAP()).ok());   // alone, a call fits
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending a, b;
	a.doc = MsgPack::MAP();
	b.doc = MsgPack::MAP();
	start(scheduler, a, processor, budget);
	start(scheduler, b, processor, budget);
	std::size_t resumes = scheduler.run_until([&] { return a.finished || b.finished; });
	CHECK_GT(resumes, 4u);   // they overlapped
	scheduler.run();
	REQUIRE(a.finished && b.finished);
	for (Pending* p : {&a, &b}) {
		if (!p->outcome.ok()) {
			std::printf("a concurrent call ended: %s\n", p->outcome.what.c_str());
		}
		REQUIRE(p->outcome.ok());
		CHECK_EQ(p->doc.at("n").i64(), 36);
	}
}

// The x7-gate probe's script (S1): 56 MiB of strings, garbage once the call ends.
const char* const garbage_56 = "local t = {} for i = 1, 56 do t[i] = string.rep('x', 1048000 + i) end _doc.s = #t";

void test_a_call_leaves_no_garbage_in_its_state() {
	Processor processor("garbage", garbage_56, MsgPack::MAP());
	const auto idle = processor.resident_bytes();
	for (int i = 0; i < 3; ++i) {
		auto out = call(processor, MsgPack::MAP());
		REQUIRE(out.ok());
		CHECK_EQ(out.doc.at("s").i64(), 56);
		CHECK_LE(processor.resident_bytes(), idle + (16u << 10));   // what stays is the script, not the call's 56 MiB
	}
	// A call cut by a bound leaves none either.
	Processor capped("capped", "local t = {} for i = 1, 100 do t[i] = string.rep('x', 1048000 + i) end", MsgPack::MAP());
	const auto capped_idle = capped.resident_bytes();
	CHECK(call(capped, MsgPack::MAP()).limit);
	CHECK_LE(capped.resident_bytes(), capped_idle + (16u << 10));
}

void test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was() {
	Processor processor("own", garbage_56, MsgPack::MAP());
	const auto idle = processor.resident_bytes();
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
		CHECK_EQ(pending.doc.at("s").i64(), 56);
	}
	CHECK_LE(processor.resident_bytes(), idle + (16u << 10));
	CHECK(call(processor, MsgPack::MAP()).ok());   // the cached state is free again
}

void test_a_cached_script_keeps_no_call_params() {
	// A local script's params come with each call, so its cached Processor keeps none (S1), and a later call sees
	// only its own (N2).
	std::thread([] {
		MsgPack object({{"_name", "params-once"}, {"_body", "_doc.k = k"}, {"_params", MsgPack({{"k", std::string(1u << 20, 'p')}})}});
		Script script(object);
		auto processor = Processor::compile_local(script);
		auto first = call(*processor, MsgPack::MAP(), script.get_params());
		REQUIRE(first.ok());
		CHECK_EQ(str(first.doc, "k").size(), std::size_t{1} << 20);
		MsgPack name_only({{"_name", "params-once"}});
		Script again(name_only);
		auto hit = Processor::compile_local(again);
		CHECK(hit == processor);   // a name alone is a cache hit
		auto second = call(*hit, MsgPack::MAP(), MsgPack::MAP());
		REQUIRE(second.ok());
		CHECK(absent(second.doc, "k"));
	}).join();
	// A foreign script's stored params stay with its Processor, and the cache charges them.
	MsgPack stored({{"k", std::string(1u << 20, 'p')}});
	Processor foreign("stored", "_doc.k = #k", stored);
	CHECK_GE(foreign.resident_bytes(), stored.serialize().size());
	auto out = call(foreign, MsgPack::MAP());
	REQUIRE(out.ok());
	CHECK_EQ(out.doc.at("k").i64(), 1 << 20);
}

// 700,000 interned strings alive at the call's end: the call grows its state's string table to about 8 MiB (S2).
const char* const interned_700k = "local t = {} for i = 1, 700000 do t[i] = 'k' .. i end _doc.n = #t";

void test_a_call_leaves_its_string_table_as_it_was() {
	Processor processor("intern", interned_700k, MsgPack::MAP());
	const auto idle = processor.resident_bytes();
	for (int i = 0; i < 2; ++i) {
		REQUIRE(call(processor, MsgPack::MAP()).ok());
		CHECK_LE(processor.resident_bytes(), idle + (16u << 10));
	}
}

void test_calls_keep_a_threads_cached_scripts_within_the_budget() {
	// 32 cached scripts, each called once and no insert between: at the 2 MiB of string table a call used to
	// leave on each, past the budget.
	std::size_t after_calls = 0;
	std::thread([&] {
		std::vector<std::shared_ptr<Processor>> held;
		for (int i = 0; i < 32; ++i) {
			MsgPack object({{"_name", "intern-" + std::to_string(i)}, {"_body", std::string(interned_700k) + " -- " + std::to_string(i)}});
			Script script(object);
			held.push_back(Processor::compile_local(script));
		}
		for (auto& processor : held) {
			CHECK(call(*processor, MsgPack::MAP()).ok());
		}
		after_calls = Processor::cached_bytes();
	}).join();
	std::printf("32 cached scripts after a call each: %zu KiB (budget %zu KiB)\n", after_calls >> 10, Processor::cache_budget() >> 10);
	CHECK_LE(after_calls, Processor::cache_budget());
}

void test_overlapping_calls_of_a_large_script_take_turns() {
	// A call that overlaps another call of a large script waits for the cached state rather than compiling the
	// script again on its thread (S3); each still has the whole cap.
	const std::string body = "print('started') local s = '" + std::string(64u << 10, 'k') + "' local n = 0 for i = 1, 300000 do n = n + i end _doc.n = #s";
	Processor processor("large", body, MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending a, b;
	a.doc = MsgPack::MAP();
	b.doc = MsgPack::MAP();
	const auto printed = Processor::printed_bytes();
	start(scheduler, a, processor, budget);
	start(scheduler, b, processor, budget);
	std::size_t resumes = scheduler.run_until([&] { return a.finished || b.finished; });
	CHECK_GT(resumes, 4u);
	CHECK_EQ(Processor::printed_bytes() - printed, 7u);   // "started": only the first call has run
	scheduler.run();
	for (Pending* p : {&a, &b}) {
		REQUIRE(p->finished);
		REQUIRE(p->outcome.ok());
		CHECK_EQ(p->doc.at("n").i64(), 64 << 10);
	}
}

// About 800,000 short strings in 800 tables, alive at the call's end, and a print once they are built.
const std::string short_strings_800k =
	"local t = {} for j = 1, 800 do local u = {} for k = 1, 1000 do u[k] = j .. ':' .. k end t[j] = u end print('built') ";

void test_the_after_call_collection_frees_in_slices() {
	// The call's garbage is freed over many steps, each a slice, not in the first step of a sweep (S4).
	Processor processor("sweep", short_strings_800k + "_doc.n = #t", MsgPack::MAP());
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending pending;
	pending.doc = MsgPack::MAP();
	start(scheduler, pending, processor, budget);
	const auto printed = Processor::printed_bytes();
	scheduler.run_until([&] { return Processor::printed_bytes() > printed; });
	std::size_t after = 0;
	double longest = 0;
	auto last = std::chrono::steady_clock::now();
	auto tick = [&] {
		const auto now = std::chrono::steady_clock::now();
		longest = std::max(longest, std::chrono::duration<double, std::milli>(now - last).count());
		last = now;
	};
	scheduler.run_until([&] {
		tick();
		++after;
		return false;
	});
	tick();
	REQUIRE(pending.finished);
	REQUIRE(pending.outcome.ok());
	std::printf("800,000 strings of garbage: %zu slices after the build, the longest %.2f ms\n", after, longest);
	CHECK_GE(after, 8u);
}

namespace {

// Compiles `n` distinct scripts named `prefix<i>` with `body` on a fresh thread; returns its cache's bytes.
std::size_t cache_after(const std::string& prefix, const std::string& body, int n) {
	std::size_t bytes = 0;
	std::thread([&] {
		for (int i = 0; i < n; ++i) {
			MsgPack object({{"_name", prefix + std::to_string(i)}, {"_body", body + " -- " + std::to_string(i)}});
			Script script(object);
			auto processor = Processor::compile_local(script);
			CHECK(call(*processor, MsgPack::MAP()).ok());
		}
		bytes = Processor::cached_bytes();
	}).join();
	return bytes;
}

}  // namespace

void test_a_threads_cached_scripts_hold_at_most_one_calls_cap() {
	// Scripts as large as a client can send: a string constant of 8 MiB each. The cache keeps what fits.
	const std::string big = "local s = '" + std::string(8u << 20, 'k') + "' _doc.n = #s";
	const auto saved = opts.scripts_cache_size;
	for (ssize_t count : {saved, ssize_t{0}}) {   // 0: no count bound, the bytes still bind
		opts.scripts_cache_size = count;
		const auto bytes = cache_after("big", big, 12);
		std::printf("12 scripts of 8 MiB, count bound %zd: the cache holds %zu MiB (budget %zu MiB)\n", count, bytes >> 20, Processor::cache_budget() >> 20);
		CHECK_LE(bytes, Processor::cache_budget());
		CHECK_GE(bytes, 2 * (8u << 20));   // it still keeps some
	}
	opts.scripts_cache_size = saved;
}

void test_the_script_cache_default_is_one_and_fits_the_budget() {
	// The CLI's default is the struct's (N5), and that many ordinary scripts fit the byte budget.
	std::vector<std::string> args = {"xapiand"};
	std::vector<char*> argv;
	for (auto& arg : args) {
		argv.push_back(arg.data());
	}
	const opts_t parsed = parseOptions(static_cast<int>(argv.size()), argv.data());
	CHECK_EQ(parsed.scripts_cache_size, opts_t{}.scripts_cache_size);
	const auto count = static_cast<int>(opts_t{}.scripts_cache_size);
	const auto bytes = cache_after("small", garbage_56, count);
	std::printf("%d ordinary scripts after their calls: %zu KiB, %zu bytes each\n", count, bytes >> 10, bytes / count);
	CHECK_LE(bytes, Processor::cache_budget() / 4);   // the count binds first for ordinary scripts
}

// fuzz_lua's slowest input under TSan: a vararg chain to the work cap, alone and three interleaved, with the
// fuzzer's limits. Reports what each takes; each must end.
void test_report_the_cost_of_a_vararg_chain() {
	const char* const body = "local function f(...) if select('#', ...) > 2^53 then return ... end return f(1, ...) end _doc.va = select('#', f())";
	Limits limits;
	limits.max_instructions = 300'000;
	limits.max_unyielded_hooks = 20;
	limits.max_bytes = 256u << 20;
	auto t0 = std::chrono::steady_clock::now();
	Processor alone("vararg", body, MsgPack::MAP(), limits);
	const auto out = call(alone, MsgPack::MAP());
	CHECK(!out.ok());
	const auto ms_alone = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	t0 = std::chrono::steady_clock::now();
	Processor shared("vararg", body, MsgPack::MAP(), limits);
	task::inline_scheduler scheduler;
	task::budget budget(every_checkpoint);
	Pending calls[3];
	for (auto& pending : calls) {
		pending.doc = MsgPack::MAP();
		start(scheduler, pending, shared, budget);
	}
	scheduler.run();
	for (auto& pending : calls) {
		CHECK(pending.finished);
	}
	const auto ms_three = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	std::printf("a vararg chain to the work cap (%s): %.0f ms alone, %.0f ms for three interleaved\n", out.what.substr(0, 40).c_str(), ms_alone, ms_three);
}

// What a trivial call costs, end to end on one thread (reported, not checked).
void test_report_the_cost_of_a_trivial_call() {
	Processor processor("cost", "_doc.x = _doc.x * 2", MsgPack::MAP());
	constexpr int n = 20000;
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < n; ++i) {
		MsgPack doc({{"x", i}});
		MsgPack old_doc = MsgPack::MAP();
		task::sync_wait(processor("PUT", doc, old_doc, MsgPack::MAP()));
	}
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
	std::printf("a trivial call: %.2f us\n", static_cast<double>(ns) / n / 1000.0);
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

}  // namespace detail
