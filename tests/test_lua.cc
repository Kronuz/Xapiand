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
#include "utils_node.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "endpoint.h"
#include "lua/exception.h"
#include "lua/processor.h"
#include "opts.h"
#include "task.hh"

#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/handler.h"
#include "engine/sync.h"
#include "manager.h"
#include "msgpack.h"
#include "traceback.h"
#include "xapiand_exception.h"

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



// --- X4-S1: a call that crossed a bound cannot be resumed or caught -------------------------------------

// The reviewer's probe (p14): a to-be-closed variable whose handler raises (a table: a string would meet a charged `find` and end the call by accident) replaces the bound's error in flight,
// the script's own pcall catches the replacement, and the hook raises again 10,000 instructions later.
const char* const close_swallow =
	"local n = 0 while true do pcall(function() "
	"local mt = {} local x <close> = setmetatable({}, mt) mt.__close = function() error({}) end "
	"while true do end end) n = n + 1 end";
const char* const close_swallow_late =   // the metamethod is added after setmetatable, which is how the refusal is walked around
	"while true do pcall(function() "
	"local mt = {} local t = setmetatable({}, mt) mt.__close = function() error({}) end "
	"local x <close> = t while true do end end) end";
const char* const close_swallow_xpcall =
	"while true do xpcall(function() "
	"local mt = {} local t = setmetatable({}, mt) mt.__close = function() error({}) end "
	"local x <close> = t while true do end end, function(m) return m end) end";
const char* const close_swallow_genfor =
	"while true do pcall(function() local mt = {} local t = setmetatable({}, mt) mt.__close = function() error({}) end "
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

void test_one_call_cannot_hold_the_loop_past_the_call_cap() {
	// X4-R1: no uninterruptible stretch may do more than `max_call_work` (16 x 10^6 units, about 20 ms), however
	// far the whole call's cap (5 x 10^8) is.
	const Limits defaults;
	CHECK_EQ(defaults.max_call_work, 16'000'000u);
	auto has = [](const Outcome& out, const char* what) { return out.limit && out.what.find(what) != std::string::npos; };
	// Linear library calls on strings the memory cap allows: refused before they run.
	auto upper = call("local s = string.rep('abcdefgh', 1024 * 1024) local r = s:upper()", MsgPack::MAP());
	CHECK(has(upper, "per-call work limit"));
	auto reverse = call("local s = string.rep('abcdefgh', 1024 * 1024) local r = s:reverse()", MsgPack::MAP());
	CHECK(has(reverse, "per-call work limit"));
	auto utf = call("local s = string.rep('abcdefgh', 2560 * 1024) local n = utf8.len(s)", MsgPack::MAP());
	CHECK(has(utf, "per-call work limit"));
	// A one-byte repeat costs about 3 ns a copy: rep('x', 31 MiB) is refused before it runs (it held a reactor 0.12 s).
	auto rep = call("local s = string.rep('x', 31 * 1024 * 1024)", MsgPack::MAP());
	CHECK(has(rep, "per-call work limit"));
	// Many small charges inside one call add up the same way: a gsub that would complete over 8 MiB is cut.
	auto gsub = call("local s = string.rep('abcdefgh', 1024 * 1024) local r = s:gsub('a', 'b')", MsgPack::MAP());
	CHECK(has(gsub, "step budget") || has(gsub, "per-call work limit"));
	auto plain = call("local h = string.rep('a', 1 << 20) local n = string.rep('a', 1 << 19) .. 'b' local r = h:find(n, 1, true)", MsgPack::MAP());
	CHECK(has(plain, "per-call work limit"));
	// A quadratic pattern over a 1 MiB subject ends at the matcher's share of the cap (3.2 x 10^6 steps), not at 64 x 10^6.
	auto quad = call("local s = string.rep('a', 1 << 20) local r = s:find('.-b')", MsgPack::MAP());
	CHECK(has(quad, "step budget"));
	// A call below the cap is untouched, and so is a loop of them (each yields between calls).
	auto fine = call("local s = string.rep('a', 1 << 20) local r = s:upper() local g = s:gsub('a', 'b') _doc.n = #g _doc.u = utf8.len(s)", MsgPack::MAP());
	REQUIRE(fine.ok());
	CHECK_EQ(fine.doc.at("n").i64(), 1 << 20);
	auto loop = call("local s = string.rep('a', 1 << 20) local n = 0 for i = 1, 100 do n = n + #s:upper() end _doc.n = n", MsgPack::MAP());
	REQUIRE(loop.ok());
	CHECK_EQ(loop.doc.at("n").i64(), 100 << 20);
	// The matcher's share follows the limit: a pattern of a few hundred thousand steps completes by default, and ends at the
	// step budget (a fifth of the cap) when the cap is 500,000 units, before the sum of the charges reaches the cap.
	const char* const some_steps = "local s = string.rep('a', 600) local r = s:find('.-c') _doc.done = r == nil";
	auto roomy = call(some_steps, MsgPack::MAP());
	REQUIRE(roomy.ok());
	CHECK(roomy.doc.at("done").boolean());
	Limits tight;
	tight.max_call_work = 500'000;
	CHECK(has(call(some_steps, MsgPack::MAP(), tight), "step budget"));
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


// --- S4: a cached foreign script is trusted for a second -----------------------------------------------

void test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again() {
	using namespace std::chrono_literals;
	CHECK(Processor::foreign_revalidation() == 1000ms);
	const std::string reference = "scripts/trusted";
	auto cached = std::make_shared<Processor>("scripts/trusted", "_doc.v = 1", MsgPack::MAP());
	Processor::remember_foreign(reference, cached);
	CHECK(Processor::foreign_trusted(reference));
	// Inside the window `compile` serves the cached processor without reading the document.
	MsgPack object({{"_type", "foreign/object"}, {"_foreign", reference}});
	Script foreign_script(object);
	CHECK(task::sync_wait(Processor::compile(foreign_script)) == cached);
	// Past it the entry is stale, so `compile` would read the document again.
	Processor::set_foreign_revalidation(20ms);
	std::this_thread::sleep_for(60ms);
	CHECK(!Processor::foreign_trusted(reference));
	Processor::remember_foreign(reference, cached);        // a read stamps the entry anew
	CHECK(Processor::foreign_trusted(reference));
	Processor::set_foreign_revalidation(1000ms);
	CHECK(!Processor::foreign_trusted("scripts/never-seen"));
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

void test_a_cancelled_call_leaves_no_garbage() {
	Processor processor("cancel-garbage", "local t = {} for i = 1, 40 do t[i] = string.rep('x', 1048000 + i) end while true do end", MsgPack::MAP());
	const auto idle = processor.resident_bytes();
	task::inline_scheduler scheduler;
	task::cancel_source source;
	task::budget budget(every_checkpoint, source.token());
	Pending pending;
	pending.doc = MsgPack::MAP();
	start(scheduler, pending, processor, budget);
	std::size_t n = 0;
	scheduler.run_until([&] { return ++n > 200; });   // past its 40 strings, into the spin
	CHECK(!pending.finished);
	source.request();
	scheduler.run();
	CHECK(pending.finished);
	CHECK(pending.outcome.cancelled);
	CHECK_LE(processor.resident_bytes(), idle + (16u << 10));
}


void test_a_cancel_inside_the_collection_lets_it_finish() {
	// A request cancelled after its script ended keeps its result, and the collection runs on in slices of its own
	// (N3, N4): the cancel lands in the collection of 800,000 strings.
	Processor processor("cancel-collect", short_strings_800k + "_doc.n = #t", MsgPack::MAP());
	const auto idle = processor.resident_bytes();
	for (std::size_t when : {0, 3, 10}) {
		task::inline_scheduler scheduler;
		task::cancel_source source;
		task::budget budget(every_checkpoint, source.token());
		Pending pending;
		pending.doc = MsgPack::MAP();
		start(scheduler, pending, processor, budget);
		const auto printed = Processor::printed_bytes();
		scheduler.run_until([&] { return Processor::printed_bytes() > printed; });
		std::size_t n = 0;
		scheduler.run_until([&] { return ++n > when; });
		REQUIRE(!pending.finished);
		source.request();
		const std::size_t after = scheduler.run_until([] { return false; });
		REQUIRE(pending.finished);
		CHECK(pending.outcome.ok());
		CHECK_EQ(pending.doc.at("n").i64(), 800);
		CHECK_GT(after, 4u);   // still in slices
		CHECK_LE(processor.resident_bytes(), idle + (16u << 10));
	}
}

// --- SR1: a foreign link is an index path, and an index path names a place inside the data directory ----------

void test_a_foreign_link_that_leaves_the_data_directory_is_refused() {
	Endpoint::cwd = "/var/db/xapiand/";
	for (const char* reference : {"..%00/scripts/doc", "%2Fetc/scripts/doc", "%2e%2e/doc", "scripts%5cx/doc"}) {
		MsgPack object({{"_type", "foreign/object"}, {"_foreign", reference}});
		Script foreign_script(object);
		bool refused = false;
		try {
			task::sync_wait(Processor::compile(foreign_script));
		} catch (const ClientError&) {
			refused = true;
		}
		CHECK(refused);
	}
}

void test_a_name_only_request_for_a_script_that_is_not_cached_is_refused() {
	// Evicted by the count bound, and dropped for passing the byte budget: both answer an error naming the script.
	const auto saved = opts.scripts_cache_size;
	std::thread([saved] {
		auto name_only_refused = [](const std::string& name) {
			MsgPack object({{"_name", name}});
			Script script(object);
			MsgPack doc = doc_of({{"x", 1}});
			try {
				auto processor = Processor::compile_local(script);
				call(*processor, doc);
			} catch (const ClientError& e) {
				const bool is_400 = dynamic_cast<const PayloadTooLarge*>(&e) == nullptr;   // a plain ClientError answers 400
				return is_400 && doc == doc_of({{"x", 1}}) && std::string(e.what()).find(name) != std::string::npos;
			}
			return false;
		};
		auto upload = [](const std::string& name, const std::string& body) {
			MsgPack object({{"_name", name}, {"_body", body}});
			Script script(object);
			return Processor::compile_local(script);
		};
		opts.scripts_cache_size = 1;
		upload("miss-evicted-a", "_doc.ran = 1");
		upload("miss-evicted-b", "_doc.ran = 1");
		CHECK(Processor::cached_local("miss-evicted-a") == nullptr);
		CHECK(name_only_refused("miss-evicted-a"));
		opts.scripts_cache_size = saved;
		std::string big = "local function f() return {";
		for (int i = 0; big.size() < (9u << 20); ++i) {
			big += "'k" + std::to_string(i) + "',";
		}
		big += "} end _doc.ran = 1";
		upload("miss-big", big);
		CHECK(Processor::cached_local("miss-big") == nullptr);   // its charge alone passes the budget
		CHECK(name_only_refused("miss-big"));
		upload("miss-kept", "_doc.ran = 1");
		MsgPack object({{"_name", "miss-kept"}});
		Script script(object);
		CHECK(Processor::compile_local(script) != nullptr);   // a cached script still runs by name
	}).join();
	opts.scripts_cache_size = saved;
}

void test_a_schema_stores_the_body_of_a_name_only_script_or_refuses_it() {
	const auto saved = opts.scripts_cache_size;
	std::thread([] {
		auto declare = [](const std::string& name) {
			MsgPack object({{"_name", name}});
			Script script(object);
			return script.process_script(false);
		};
		MsgPack full({{"_name", "decl-cached"}, {"_body", "_doc.ran = _doc.x + 1"}});
		Script cached_script(full);
		Processor::compile_local(cached_script);
		// Cached: the stored script carries the body.
		auto stored = declare("decl-cached");
		CHECK_EQ(stored.at("_lua").at("_body").str(), "_doc.ran = _doc.x + 1");
		// Not cached: refused at declaration, with a plain ClientError (400).
		bool refused = false;
		try {
			declare("decl-missing");
		} catch (const ClientError& e) {
			refused = dynamic_cast<const PayloadTooLarge*>(&e) == nullptr && std::string(e.what()).find("decl-missing") != std::string::npos;
		}
		CHECK(refused);
		// A write through the stored schema on a fresh thread (its own, empty cache) still runs the script.
		std::thread([stored] {
			CHECK(Processor::cached_local("decl-cached") == nullptr);
			Script from_schema(stored.at("_lua"));
			auto processor = Processor::compile_local(from_schema);
			auto out = call(*processor, doc_of({{"x", 41}}));
			CHECK(out.ok());
			CHECK_EQ(out.doc.at("ran").i64(), 42);
		}).join();
	}).join();
	opts.scripts_cache_size = saved;
}

// --- a call runs in the heavy lane (docs/design/loop-scheduling.md D7) ---------------------------------------------

// A scheduler with a heavy lane: it resumes each handle in the lane it was posted to and records that lane.
class LaneScheduler {
public:
	void post(std::coroutine_handle<> h) { queue_.push_back({h, false}); }
	void post_heavy(std::coroutine_handle<> h) { queue_.push_back({h, true}); }
	std::uint64_t now() const { return std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()); }

	// Runs to the end of the queue; `resumes` holds the lane of each resume, in order.
	void run() {
		while (!queue_.empty()) {
			const auto [h, heavy] = queue_.front();
			queue_.pop_front();
			task::running_on scope(task::scheduler_ref::of(*this).with_heavy(heavy));
			resumes.push_back(heavy);
			h.resume();
		}
	}

	std::vector<bool> resumes;

private:
	std::deque<std::pair<std::coroutine_handle<>, bool>> queue_;
};

// What a request around one call sees: the lane it ran in before the call, during its slices and after.
struct LaneTrace {
	bool before = false;
	bool after = false;
	std::size_t resumes_before = 0;   // resumes of the scheduler when the call began and ended
	std::size_t resumes_after = 0;
	Outcome outcome;
};

task::task<void> traced_call(Processor& processor, MsgPack& doc, task::budget& budget, LaneTrace& trace, LaneScheduler& scheduler) {
	trace.before = task::current().heavy();
	trace.resumes_before = scheduler.resumes.size();
	MsgPack old_doc = MsgPack::MAP();
	std::exception_ptr error;
	try {
		co_await processor("PUT", doc, old_doc, MsgPack::MAP(), &budget);
	} catch (...) {
		error = std::current_exception();
	}
	trace.resumes_after = scheduler.resumes.size();
	trace.after = task::current().heavy();
	trace.outcome = outcome_of(error);
}

// Runs a traced call from a request that starts in `lane`.
LaneTrace run_traced(const std::string& body, MsgPack doc, bool heavy_start, LaneScheduler& scheduler) {
	Processor processor("lane", body, MsgPack::MAP());
	task::budget budget(every_checkpoint);
	LaneTrace trace;
	task::spawn(task::scheduler_ref::of(scheduler).with_heavy(heavy_start), traced_call(processor, doc, budget, trace, scheduler), [](std::exception_ptr) {});
	scheduler.run();
	trace.outcome.doc = std::move(doc);
	return trace;
}

// A light request's call enters the heavy lane before its first turn and every slice of it runs there; the request
// is light again after. Run in the light lane (the mutant that stays light fails the first two checks).
void test_a_call_runs_every_slice_in_the_heavy_lane_and_the_request_is_light_after() {
	LaneScheduler scheduler;
	const auto trace = run_traced("local s = 0; for i = 1, 200000 do s = s + i end; _doc.s = s", MsgPack::MAP(), false, scheduler);
	REQUIRE(trace.outcome.ok());
	CHECK_EQ(trace.outcome.doc.at("s").i64(), 20000100000);    // the sum is the same as without the lane
	CHECK_FALSE(trace.before);
	std::size_t heavy = 0;
	for (auto i = trace.resumes_before; i < trace.resumes_after; ++i) {
		heavy += scheduler.resumes[i] ? 1 : 0;
	}
	const auto during = trace.resumes_after - trace.resumes_before;
	std::printf("a call of 200,000 iterations: %zu resumes after its entry, %zu of them heavy\n", during, heavy);
	CHECK_GE(heavy, 20u);       // 200,000 iterations of 2 instructions at a hook every 10,000: at least 20 slices
	CHECK_EQ(heavy, during);    // none of them light
	CHECK_FALSE(trace.after);   // and the request leaves the lane
}

// The lane the request had is the lane it keeps: a request that is already heavy stays heavy after the call.
void test_a_heavy_request_stays_heavy_after_its_call() {
	LaneScheduler scheduler;
	const auto trace = run_traced("_doc.x = 1", MsgPack::MAP(), true, scheduler);
	REQUIRE(trace.outcome.ok());
	CHECK_EQ(trace.outcome.doc.at("x").i64(), 1);
	CHECK(trace.before);
	CHECK(trace.after);
}

// A script error ends the call in the heavy lane and the request is light again after it.
void test_a_script_error_leaves_the_request_light() {
	LaneScheduler scheduler;
	const auto trace = run_traced("error('boom')", MsgPack::MAP(), false, scheduler);
	CHECK(trace.outcome.script_error);
	CHECK_FALSE(trace.before);
	CHECK_FALSE(trace.after);
	const auto bound = run_traced("while true do end", MsgPack::MAP(), false, scheduler);   // a bound's end
	CHECK(bound.outcome.limit);
	CHECK_FALSE(bound.after);
}

// --- a script reads the method of its write in `_method` ------------------------------------------------------

// A node in this process with one HTTP loop, on a port of its own, in a data directory of its own.
class MethodNode {
public:
	MethodNode() {
		::signal(SIGPIPE, SIG_IGN);
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sa.sa_sigaction = traceback::collect_callstack_sig_handler;
		::sigaction(SIGUSR2, &sa, nullptr);
		const auto root = std::filesystem::current_path() / "lua_method_data";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".xapiand");
		std::filesystem::current_path(root);
		Endpoint::cwd = root.string() + "/";
		std::vector<std::string> args = {"xapiand", "--port", std::to_string(port_.port()), "--name", "luamethod", "-D", root.string(), "--shards", "1", "--processors", "1"};
		std::vector<char*> argv;
		for (auto& arg : args) {
			argv.push_back(arg.data());
		}
		opts = parseOptions(static_cast<int>(argv.size()), argv.data());
		XapiandManager::make();
		thread_ = std::thread([this] {
			try {
				XapiandManager::manager()->run();
			} catch (const SystemExit&) { }
			exited_ = true;
		});
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (!exited_.load() && XapiandManager::get_state() != XapiandManager::State::READY && std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		ready_ = !exited_.load() && XapiandManager::get_state() == XapiandManager::State::READY;
	}

	~MethodNode() {
		if (auto manager = XapiandManager::manager()) {
			const auto now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			manager->shutdown(now, now);
		}
		if (thread_.joinable()) {
			thread_.join();
		}
		XapiandManager::reset();
	}

	bool ready() const { return ready_; }

	// One request on a connection of its own: the response's status (0 when there is none), its body into `reply`.
	int call(const std::string& method, const std::string& path, const std::string& body, std::string* reply = nullptr) const {
		const int fd = test_utils::connect_to(port_.port());
		if (fd < 0) {
			return 0;
		}
		timeval timeout{60, 0};
		::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		const std::string request = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAccept: application/json\r\n"
			"Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
		for (std::size_t sent = 0; sent < request.size();) {
			const auto n = ::send(fd, request.data() + sent, request.size() - sent, 0);
			if (n <= 0) {
				break;
			}
			sent += static_cast<std::size_t>(n);
		}
		std::string response;
		char buf[65536];
		ssize_t n;
		while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
			response.append(buf, static_cast<std::size_t>(n));
		}
		::close(fd);
		if (reply != nullptr) {
			const auto end = response.find("\r\n\r\n");
			*reply = end == std::string::npos ? std::string() : response.substr(end + 4);
		}
		return response.rfind("HTTP/1.1 ", 0) == 0 && response.size() > 12 ? std::atoi(response.c_str() + 9) : 0;
	}

private:
	test_utils::ReservedPort port_;   // held while the node lives
	std::thread thread_;
	bool ready_ = false;
	std::atomic<bool> exited_{false};
};

// The value of the field `seen` in a JSON reply, "<none>" when it has none.
std::string seen_of(const std::string& reply) {
	std::string compact;
	for (char c : reply) {
		if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
			compact.push_back(c);
		}
	}
	const std::string key = "\"seen\":\"";
	const auto at = compact.find(key);
	if (at == std::string::npos) {
		return "<none>";
	}
	const auto begin = at + key.size();
	return compact.substr(begin, compact.find('"', begin) - begin);
}

// The method a write's script saw, as its document stored it.
std::string stored_seen(const MethodNode& node, const std::string& path) {
	std::string reply;
	if (node.call("GET", path, "", &reply) != 200) {
		return "<status>";
	}
	return seen_of(reply);
}

#define CHECK_SEEN(got, want) \
	do { \
		const std::string got_ = (got); \
		if (got_ != (want)) { std::fprintf(stderr, "  `_method` was %s, %s expected\n", got_.c_str(), (want)); } \
		CHECK_EQ(got_, std::string(want)); \
	} while (0)

// Every write that runs a script hands it the method that wrote: the request's ("PUT", "POST", "PATCH", "UPDATE",
// "RESTORE"), a restore line's `_op_type` ("UPDATE"), and "RESTORE" for a restore from the command line.
void test_a_script_reads_the_method_of_its_write() {
	const auto saved_cwd = std::filesystem::current_path();
	{
		MethodNode node;
		REQUIRE(node.ready());
		const std::string script = R"("_script":"_doc.seen = _method")";
		std::string reply;

		CHECK_EQ(node.call("PUT", "/lua_method/put?commit&echo", "{\"n\":1," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "PUT");
		CHECK_SEEN(stored_seen(node, "/lua_method/put"), "PUT");

		CHECK_EQ(node.call("POST", "/lua_method/?commit&echo", "{\"n\":2," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "POST");

		CHECK_EQ(node.call("PATCH", "/lua_method/put?commit&echo", R"([{"op":"replace","path":"/n","value":3},{"op":"add","path":"/_script","value":"_doc.seen = _method"}])", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "PATCH");
		CHECK_SEEN(stored_seen(node, "/lua_method/put"), "PATCH");

		CHECK_EQ(node.call("UPDATE", "/lua_method/put?commit&echo", "{\"n\":4," + script + "}", &reply) / 100, 2);
		CHECK_SEEN(seen_of(reply), "UPDATE");

		CHECK_EQ(node.call("RESTORE", "/lua_method/?commit", "[{\"_id\":\"restored\",\"n\":5," + script + "}]") / 100, 2);
		CHECK_SEEN(stored_seen(node, "/lua_method/restored"), "RESTORE");

		// The command line's restore has no request: a plain line sees "RESTORE", an `update` line "UPDATE".
		const auto dump = std::filesystem::current_path() / "lua_method.dump";
		{
			std::string bytes = MsgPack({{"_id", "line"}, {"n", 6}, {"_script", "_doc.seen = _method"}}).serialize();
			bytes += MsgPack({{"_id", "op"}, {"_op_type", "update"}, {"n", 7}, {"_script", "_doc.seen = _method"}}).serialize();
			const int fd = ::open(dump.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
			REQUIRE(fd >= 0);
			CHECK_EQ(::write(fd, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
			::close(fd);
		}
		const int fd = ::open(dump.c_str(), O_RDONLY);
		REQUIRE(fd >= 0);
		engine::sync_wait(DatabaseHandler::restore_documents(Endpoint("lua_method_cli"), fd));
		::close(fd);
		CHECK_SEEN(stored_seen(node, "/lua_method_cli/line"), "RESTORE");
		CHECK_SEEN(stored_seen(node, "/lua_method_cli/op"), "UPDATE");
	}
	std::filesystem::current_path(saved_cwd);
}

}  // namespace

TEST_MAIN(
	test_a_name_only_request_for_a_script_that_is_not_cached_is_refused,
	test_a_schema_stores_the_body_of_a_name_only_script_or_refuses_it,
	test_a_script_transforms_the_document,
	test_params_and_old_doc_are_in_scope,
	test_numbers_keep_their_kind,
	test_a_script_error_is_a_client_error_and_leaves_the_document_as_it_was,
	test_a_syntax_error_names_its_line,
	test_interleaved_calls_keep_their_own_doc_and_globals,
	test_the_calls_really_interleaved,
	test_collection_runs_while_calls_are_suspended,
	test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap,
	test_a_call_leaves_no_garbage_in_its_state,
	test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was,
	test_a_cached_script_keeps_no_call_params,
	test_a_call_leaves_its_string_table_as_it_was,
	test_calls_keep_a_threads_cached_scripts_within_the_budget,
	test_overlapping_calls_of_a_large_script_take_turns,
	test_the_after_call_collection_frees_in_slices,
	test_a_threads_cached_scripts_hold_at_most_one_calls_cap,
	test_the_script_cache_default_is_one_and_fits_the_budget,
	test_report_the_cost_of_a_vararg_chain,
	test_report_the_cost_of_a_trivial_call,
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
	test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again,
	test_one_call_cannot_hold_the_loop_past_the_call_cap,
	test_a_long_script_lets_other_work_run,
	test_without_a_budget_a_script_runs_to_its_end,
	test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread,
	test_a_cancelled_call_leaves_no_garbage,
	test_a_cancel_inside_the_collection_lets_it_finish,
	test_a_charged_loop_yields_before_its_slice_of_instructions_is_spent,
	test_a_foreign_link_that_leaves_the_data_directory_is_refused,
	test_a_call_runs_every_slice_in_the_heavy_lane_and_the_request_is_light_after,
	test_a_heavy_request_stays_heavy_after_its_call,
	test_a_script_error_leaves_the_request_light,
	test_a_script_reads_the_method_of_its_write   // last: it starts a node, which sets the options
)
