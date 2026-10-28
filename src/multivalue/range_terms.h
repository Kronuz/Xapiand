/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Canonical decomposition of an integer range into accuracy-bucket terms. See
// docs/mechanisms/numeric-range-terms.md.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace range_terms {

// A bucket term: the bucket [start, start + accuracy[level]).
template <typename T>
struct Bucket {
	size_t level;
	T start;

	bool operator==(const Bucket& other) const noexcept { return level == other.level && start == other.start; }
};

using wide_t = __int128;

namespace detail {

inline wide_t floor_div(wide_t a, wide_t b) noexcept {
	wide_t q = a / b;
	return (a % b != 0 && a < 0) ? q - 1 : q;
}

// The index term of the bucket starting at `start` (mod 2^64, as the index side computes it).
template <typename T>
inline T wrap(wide_t start) noexcept {
	return static_cast<T>(static_cast<uint64_t>(start));
}

// Buckets covering [lo, hi_excl) using only levels >= finest: whole buckets from the coarsest level
// down, then the pieces left at the ends widened to `finest` buckets. Stops early once `limit`
// buckets would be exceeded. Returns the bucket count, or limit + 1 when over.
template <typename T, typename Sink>
size_t plan(wide_t lo, wide_t hi_excl, const std::vector<uint64_t>& accuracy, size_t finest, size_t limit, Sink&& sink) {
	struct Piece { wide_t lo, hi; };
	std::vector<Piece> pieces{{lo, hi_excl}}, next;
	size_t count = 0;
	for (size_t level = accuracy.size(); level-- > finest;) {
		const wide_t a = accuracy[level];
		if (!a) {
			continue;
		}
		next.clear();
		for (const auto& p : pieces) {
			const wide_t first = -floor_div(-p.lo, a) * a;  // first multiple of a >= p.lo
			const wide_t last = floor_div(p.hi, a) * a;     // last multiple of a <= p.hi
			if (first < last) {
				const wide_t n = (last - first) / a;
				if (n > static_cast<wide_t>(limit - count)) {
					return limit + 1;
				}
				for (wide_t b = first; b < last; b += a) {
					sink(Bucket<T>{level, wrap<T>(b)});
				}
				count += static_cast<size_t>(n);
				if (p.lo < first) {
					next.push_back({p.lo, first});
				}
				if (last < p.hi) {
					next.push_back({last, p.hi});
				}
			} else {
				next.push_back(p);
			}
		}
		pieces.swap(next);
	}

	// What is left is narrower than a bucket of the finest level: widen it to the buckets it touches.
	const size_t level = finest;
	const wide_t a = accuracy[level];
	std::vector<wide_t> starts;
	for (const auto& p : pieces) {
		for (wide_t b = floor_div(p.lo, a) * a; b < p.hi; b += a) {
			starts.push_back(b);
		}
	}
	std::sort(starts.begin(), starts.end());
	starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
	if (starts.size() > limit - count) {
		return limit + 1;
	}
	for (auto b : starts) {
		sink(Bucket<T>{level, wrap<T>(b)});
	}
	return count + starts.size();
}

}  // namespace detail

// Fills `out` with buckets whose union contains every integer in [lo, hi], using at most `max_terms`
// buckets; a value v is indexed under the bucket of every accuracy that contains it, so a document
// with v in [lo, hi] has at least one of the terms. The buckets may also cover values outside
// [lo, hi] (the ends are widened to the finest accuracy that fits the budget): the caller's exact
// value check removes them. Returns false, leaving `out` empty, when even the coarsest accuracy
// alone needs more than `max_terms` buckets: the caller then uses no term prefilter.
// `accuracy` is ascending; a zero accuracy is ignored.
template <typename T>
bool decompose(T lo, T hi, const std::vector<uint64_t>& accuracy, size_t max_terms, std::vector<Bucket<T>>& out) {
	out.clear();
	if (hi < lo) {
		return true;
	}
	const wide_t wlo = lo, whi = static_cast<wide_t>(hi) + 1;
	size_t top = accuracy.size();
	while (top > 0 && !accuracy[top - 1]) {
		--top;
	}
	if (!top) {
		return false;
	}
	for (size_t finest = 0; finest < accuracy.size(); ++finest) {
		if (!accuracy[finest]) {
			continue;
		}
		if (detail::plan<T>(wlo, whi, accuracy, finest, max_terms, [](const Bucket<T>&) {}) <= max_terms) {
			detail::plan<T>(wlo, whi, accuracy, finest, max_terms, [&out](const Bucket<T>& b) { out.push_back(b); });
			return true;
		}
	}
	return false;
}

}  // namespace range_terms
