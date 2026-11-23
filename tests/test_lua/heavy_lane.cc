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

// A call runs in the heavy lane and the request keeps the lane it had.

#include "support.h"
#include "tests.h"

namespace detail {

// --- a call runs in the heavy lane (docs/design/loop-scheduling.md D7) ---------------------------------------------

namespace {

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

}  // namespace

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

}  // namespace detail
