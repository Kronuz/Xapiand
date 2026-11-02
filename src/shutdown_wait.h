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

#pragma once

// The bound on the shutdown's wait for unfinished writes (docs/design/phase7c.md D16, review C14). A write that has
// been admitted is let finish, and it may wait for its index's turn for DB_TIMEOUT, so the wait cannot be unbounded:
// it ends after `bound`, and what was still running is reported and abandoned. A write that was not acknowledged
// was never promised to be durable; an acknowledged one is already in the log.

#include <chrono>                  // for std::chrono
#include <condition_variable>      // for std::condition_variable
#include <functional>              // for std::function
#include <memory>                  // for std::shared_ptr
#include <mutex>                   // for std::mutex
#include <string>                  // for std::string
#include <thread>                  // for std::thread
#include <vector>                  // for std::vector

#include "engine/sync.h"           // for engine::check_may_block


namespace shutdown_wait {

struct Report {
	bool finished = true;                 // stop() returned within the bound
	std::chrono::milliseconds waited{0};  // how long the wait took
	std::vector<std::string> abandoned;   // what describe() said was unfinished at the bound; empty when finished
};

// Runs `stop`, which blocks, on a thread of its own (never on a reactor) and waits for it up to `bound`. When it
// has not returned by then, `describe` lists what is unfinished and the thread is left to end when it can: what
// `stop` uses must outlive it.
inline Report
wait(std::chrono::milliseconds bound, std::function<void()> stop, const std::function<std::vector<std::string>()>& describe)
{
	engine::check_may_block("shutdown_wait::wait");

	struct State {
		std::mutex m;
		std::condition_variable cv;
		bool done = false;
		std::function<void()> stop;
	};
	auto state = std::make_shared<State>();
	state->stop = std::move(stop);

	const auto began = std::chrono::steady_clock::now();
	std::thread([state] {
		state->stop();
		{
			std::lock_guard<std::mutex> lk(state->m);
			state->done = true;
		}
		state->cv.notify_all();
	}).detach();

	Report report;
	{
		std::unique_lock<std::mutex> lk(state->m);
		report.finished = state->cv.wait_for(lk, bound, [&] { return state->done; });
	}
	report.waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began);
	if (!report.finished && describe) {
		report.abandoned = describe();
	}
	return report;
}

}  // namespace shutdown_wait
