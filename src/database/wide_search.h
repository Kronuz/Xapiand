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

#include <cstddef>                 // for std::size_t
#include <memory>                  // for std::shared_ptr
#include <mutex>                   // for std::mutex
#include <string>                  // for std::string
#include <utility>                 // for std::exchange
#include <vector>                  // for std::vector

#include "task.hh"                 // for task::task, task::async_mutex, task::event, task::budget


class Endpoint;
class MsgPack;


// What keeps a search over thousands of indexes off the HTTP loops' critical path (docs/design/wide-search-loop.md).
namespace wide_search {

// The node's bound on the indexes searches pin at once. A batch takes its indexes' count before it pins them and
// gives it back when it releases them; a batch that does not fit waits its turn holding no thread, first come first
// served, so concurrent wide searches share the bound instead of adding their batches.
class Pins {
public:
	// The count a batch holds until it ends.
	class [[nodiscard]] Hold {
	public:
		Hold() = default;
		Hold(Hold&& other) noexcept : pins_(std::exchange(other.pins_, nullptr)), n_(std::exchange(other.n_, 0)) { }
		Hold& operator=(Hold&& other) noexcept {
			if (this != &other) {
				release();
				pins_ = std::exchange(other.pins_, nullptr);
				n_ = std::exchange(other.n_, 0);
			}
			return *this;
		}
		Hold(const Hold&) = delete;
		Hold& operator=(const Hold&) = delete;
		~Hold() { release(); }

		std::size_t size() const noexcept { return n_; }

	private:
		friend class Pins;
		Hold(Pins* pins, std::size_t n) noexcept : pins_(pins), n_(n) { }
		void release() noexcept {
			if (auto* pins = std::exchange(pins_, nullptr)) {
				pins->give(std::exchange(n_, 0));
			}
		}
		Pins* pins_ = nullptr;
		std::size_t n_ = 0;
	};

	// At most `capacity` indexes pinned at once (at least 1).
	explicit Pins(std::size_t capacity) noexcept;

	// Takes n indexes' count, waiting while it does not fit; a batch wider than the capacity takes all of it. Throws
	// task::cancelled when `token` is requested while it waits for its place in line.
	task::task<Hold> take(std::size_t n, task::cancel_token token);

	// Takes n at once, or nothing when it does not fit or others wait (the pins of a search that cannot wait).
	Hold try_take(std::size_t n) noexcept;

	std::size_t capacity() const noexcept { return capacity_; }
	std::size_t held() const noexcept;

private:
	void give(std::size_t n) noexcept;

	const std::size_t capacity_;
	task::async_mutex line_;                   // FIFO: the batch at its head waits for room, the rest for the head
	mutable std::mutex mutex_;                 // guards free_ and room_
	std::size_t free_;
	std::shared_ptr<task::event> room_;        // set when room is given back while the head waits
};


// The node's pins, sized at first use to the open indexes the pool keeps (its soft_open).
Pins& pins();


// Each name to its shards, sorted and without repeats. A read's resolution yields the loop between slices of
// `budget` and ends when its token is cancelled; a write's resolves whole, since a client's EOF cancels reads only.
task::task<std::vector<Endpoint>> resolve(const std::vector<std::string>& paths, bool writable, const MsgPack* settings, task::budget& budget);

}  // namespace wide_search
