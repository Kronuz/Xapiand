/*
 * Copyright (c) 2026 Dubalu LLC
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


namespace engine {

// Commands one control-worker job runs before it yields: a job queued behind it waits for at most
// this many, however fast commands arrive.
inline constexpr std::size_t drain_batch = 64;

// Pops at most max items from queue into handler; true when it stopped at max, so more may remain
// and the caller re-posts itself behind whatever the worker already has queued.
template <typename Item, typename Queue, typename Handler>
bool drain_bounded(Queue& queue, std::size_t max, Handler&& handler) {
	Item item;
	for (std::size_t n = 0; n < max; ++n) {
		if (!queue.try_dequeue(item)) {
			return false;
		}
		handler(item);
	}
	return true;
}

}  // namespace engine
