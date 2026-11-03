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

#include <atomic>                  // for std::atomic
#include <chrono>                  // for std::chrono
#include <cstddef>                 // for std::size_t
#include <utility>                 // for std::move

#include "engine/host.h"           // for engine::Host
#include "task.hh"                 // for task::task, task::async_mutex, task::deadline

// The gate of the pattern walks: one walk runs at a time on the node, a bounded number wait, and none waits past a
// deadline. A request over either bound is turned away with a status the client can act on, holding no thread
// whichever way it goes (docs/mechanisms/wide-search.md, "Limits").
namespace index_pattern {

// Why a walk was turned away: the HTTP status, the seconds a client should wait before it tries again, and the reason.
struct WalkRefused {
	int status;
	unsigned retry_after;
	const char* reason;
};

class WalkGate {
public:
	// The walk that runs and, until it ends, its place at the gate.
	class [[nodiscard]] Turn {
	public:
		Turn() = default;
		Turn(Turn&& other) noexcept : gate_(std::exchange(other.gate_, nullptr)), guard_(std::move(other.guard_)) { }
		Turn& operator=(Turn&& other) noexcept {
			if (this != &other) {
				release();
				gate_ = std::exchange(other.gate_, nullptr);
				guard_ = std::move(other.guard_);
			}
			return *this;
		}
		~Turn() { release(); }

	private:
		friend class WalkGate;
		Turn(WalkGate* gate, task::async_mutex::guard guard) noexcept : gate_(gate), guard_(std::move(guard)) { }
		void release() noexcept {
			guard_.unlock();
			if (auto* gate = std::exchange(gate_, nullptr)) {
				gate->admitted_.fetch_sub(1, std::memory_order_acq_rel);
			}
		}
		WalkGate* gate_ = nullptr;
		task::async_mutex::guard guard_;
	};

	// `queue` walks may wait behind the one that runs; one waits `wait` at most.
	WalkGate(std::size_t queue, std::chrono::steady_clock::duration wait) noexcept : queue_(queue), wait_(wait) { }

	// The turn to walk. Throws WalkRefused{429} when `queue` walks already wait, WalkRefused{503} when the turn did not
	// come within the wait, and task::cancelled when `token` is requested first.
	task::task<Turn> enter(task::cancel_token token)
	{
		if (admitted_.fetch_add(1, std::memory_order_acq_rel) > queue_) {
			admitted_.fetch_sub(1, std::memory_order_acq_rel);
			throw WalkRefused{429, retry_after(), "Too many searches are waiting to look up index names; try again shortly"};
		}
		Turn turn(this, {});   // owns the place at the gate until it ends, on every path out
		bool late = false;
		if (auto* host = engine::Host::current()) {
			task::deadline<engine::Host> expiry(*host, std::chrono::steady_clock::now() + wait_, token);
			turn.guard_ = co_await mutex_.lock(expiry.token());
			late = expiry.expired();
		} else {
			turn.guard_ = co_await mutex_.lock(token);
		}
		if (!turn.guard_) {
			if (late) {
				throw WalkRefused{503, retry_after(), "The search waited too long to look up index names; try again shortly"};
			}
			throw task::cancelled{};
		}
		co_return std::move(turn);
	}

	std::size_t admitted() const noexcept { return admitted_.load(std::memory_order_acquire); }

private:
	static constexpr unsigned retry_after() noexcept { return 1; }   // as the admission gates say

	task::async_mutex mutex_;
	std::atomic<std::size_t> admitted_{0};   // the walk that runs and those that wait
	const std::size_t queue_;
	const std::chrono::steady_clock::duration wait_;
};

}  // namespace index_pattern
