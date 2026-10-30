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

#include <span>                    // for std::span
#include <utility>                 // for std::move

#include "prospect/index.hh"       // for prospect::index


namespace engine::strata {

// One published generation of a shard's index, held for a read (docs/design/phase7c.md D6): its segments with
// their live bits frozen, and the field table as of its publication. Holding it blocks nothing: a write, a
// refresh, a commit and a merge go on, and the generation stays readable until this is dropped, which can
// happen on any thread. Obtained from Shard::view().
class Viewed {
public:
	using segment_view = ::prospect::index::segment_view;
	using view_guard = ::prospect::index::view_guard;
	using view_type = ::prospect::index::view;

	Viewed() noexcept = default;
	Viewed(view_guard guard, const void* identity) noexcept : guard_(std::move(guard)), identity_(identity) { }

	explicit operator bool() const noexcept { return static_cast<bool>(guard_); }

	const view_type& view() const noexcept { return *guard_; }
	std::span<const segment_view> segments() const noexcept { return guard_->segments(); }
	const ::strata::field_table& fields() const noexcept { return guard_->fields(); }

	// The shard this generation belongs to: handles of one index share it.
	const void* identity() const noexcept { return identity_; }

	// Lets go of the generation now.
	void release() noexcept { guard_.release(); }

private:
	view_guard guard_;
	const void* identity_ = nullptr;
};

}  // namespace engine::strata
