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

// Bounds the instruction hook cannot see: close handlers and library work charged by what it touches.

#include "support.h"
#include "tests.h"

namespace detail {

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

}  // namespace detail
