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

// Slices: a long call lets other work run, and a cancelled call stops at its next slice.

#include "support.h"
#include "tests.h"

namespace detail {

// --- K3: slices ----------------------------------------------------------------------------------------

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

}  // namespace detail
