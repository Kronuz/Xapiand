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

#include <memory>                  // for std::unique_ptr
#include <mutex>                   // for std::mutex, std::unique_lock
#include <span>                    // for std::span
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "prospect/index.hh"       // for prospect::index


namespace engine::strata {

// The segments of one shard as of the moment its buffer was flushed, with the shard's lock held until
// the pin is dropped: a write, a merge or a close waits for it. Obtained from Shard::pin().
class Pinned {
public:
	using segment_view = ::prospect::index::segment_view;

	explicit Pinned(std::unique_lock<std::mutex> lock) noexcept : lock_(std::move(lock)) { }

	std::span<const segment_view> segments() const noexcept { return segments_; }

	// The Shard::pin() that made it fills these.
	std::vector<segment_view> segments_;
	const void* identity_ = nullptr;

private:
	std::unique_lock<std::mutex> lock_;
};

}  // namespace engine::strata
