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

// Property tests for the numeric range decomposition (range_terms.h) against brute force: a document
// holding v is indexed under the bucket of v at every accuracy, and a range's terms must contain at
// least one of them for every v in the range, whatever the accuracies and the term budget.

#include "test_harness.h"

#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "multivalue/range_terms.h"

using range_terms::wide_t;

// The index-side bucket of v at accuracy a (floor-aligned, wrapped to 64 bits like the index does).
template <typename T>
static T bucket_of(T v, uint64_t a) {
	const wide_t w = v;
	const wide_t start = range_terms::detail::floor_div(w, static_cast<wide_t>(a)) * static_cast<wide_t>(a);
	return range_terms::detail::wrap<T>(start);
}

template <typename T>
class TermSet {
	std::set<std::pair<size_t, T>> set;

public:
	explicit TermSet(const std::vector<range_terms::Bucket<T>>& buckets) {
		for (const auto& b : buckets) {
			set.insert({b.level, b.start});
		}
	}

	bool matches(T v, const std::vector<uint64_t>& accuracy) const {
		for (size_t level = 0; level < accuracy.size(); ++level) {
			if (set.count({level, bucket_of<T>(v, accuracy[level])})) {
				return true;
			}
		}
		return false;
	}
};

// Every value in [lo, hi] is matched: the first uncovered value of a range is lo or the end of some
// bucket, so probing those (plus the ends and a few random points) is a complete check.
template <typename T>
static bool covers(T lo, T hi, const std::vector<uint64_t>& accuracy, const std::vector<range_terms::Bucket<T>>& buckets, std::mt19937_64& rng) {
	const TermSet<T> terms(buckets);
	auto probe = [&](wide_t v) {
		return v < lo || v > hi || terms.matches(static_cast<T>(v), accuracy);
	};
	if (!probe(lo) || !probe(hi)) {
		return false;
	}
	for (const auto& b : buckets) {
		const wide_t a = accuracy[b.level];
		const wide_t s = b.start;
		for (wide_t base : {s, s - (static_cast<wide_t>(1) << 64)}) {
			for (wide_t v : {base + a, base + a - 1, base - 1}) {
				if (!probe(v)) {
					return false;
				}
			}
		}
	}
	for (int i = 0; i < 8; ++i) {
		const wide_t span = static_cast<wide_t>(hi) - lo + 1;
		const wide_t v = lo + static_cast<wide_t>((static_cast<unsigned __int128>(rng()) << 64 | rng()) % static_cast<unsigned __int128>(span));
		if (!probe(v)) {
			return false;
		}
	}
	return true;
}

// Exhaustive on a small domain: every (lo, hi), every value, nested and non-nested accuracies, tight budgets.
static void test_exhaustive_small_domain() {
	const std::vector<std::vector<uint64_t>> lists = {
		{10, 100}, {3, 7, 50}, {25}, {1, 2, 4, 8, 16, 32}, {100, 1000}, {7, 13, 91}, {5, 10, 20},
	};
	std::mt19937_64 rng(1);
	size_t wrong = 0, cases = 0, refused = 0;
	for (const auto& accuracy : lists) {
		for (size_t budget : {1u, 2u, 3u, 5u, 128u}) {
			for (int lo = -40; lo <= 40; ++lo) {
				for (int hi = lo; hi <= 40; ++hi) {
					std::vector<range_terms::Bucket<int64_t>> buckets;
					const bool ok = range_terms::decompose<int64_t>(lo, hi, accuracy, budget, buckets);
					++cases;
					if (!ok) {
						++refused;
						CHECK(buckets.empty());
						// Refusal only when even the coarsest accuracy alone overflows the budget.
						const wide_t a = accuracy.back();
						const wide_t touched = range_terms::detail::floor_div(hi, a) - range_terms::detail::floor_div(lo, a) + 1;
						CHECK_GT(touched, static_cast<wide_t>(budget));
						continue;
					}
					CHECK_LE(buckets.size(), budget);
					const TermSet<int64_t> terms(buckets);
					for (int v = lo; v <= hi; ++v) {
						if (!terms.matches(v, accuracy)) {
							++wrong;
						}
					}
				}
			}
		}
	}
	CHECK_EQ(wrong, size_t(0));
	CHECK_GT(cases, refused);
	CHECK_GT(refused, size_t(0));
}

// With a generous budget a range is covered at the finest accuracy: it overshoots by less than one finest bucket per end.
static void test_tight_with_ample_budget() {
	const std::vector<uint64_t> accuracy = {100, 1000, 10000, 100000, 1000000, 100000000};
	std::mt19937_64 rng(2);
	for (int i = 0; i < 20000; ++i) {
		const int64_t lo = static_cast<int64_t>(rng() % 2000000000ULL) - 1000000000;
		const int64_t hi = lo + static_cast<int64_t>(rng() % 400000000ULL);
		std::vector<range_terms::Bucket<int64_t>> buckets;
		if (!range_terms::decompose<int64_t>(lo, hi, accuracy, 1u << 20, buckets)) {
			CHECK(false);
			continue;
		}
		wide_t min_start = std::numeric_limits<int64_t>::max(), max_end = std::numeric_limits<int64_t>::min();
		for (const auto& b : buckets) {
			min_start = std::min<wide_t>(min_start, b.start);
			max_end = std::max<wide_t>(max_end, static_cast<wide_t>(b.start) + accuracy[b.level]);
		}
		CHECK_GT(min_start, static_cast<wide_t>(lo) - 100);
		CHECK_LE(min_start, static_cast<wide_t>(lo));
		CHECK_LT(max_end, static_cast<wide_t>(hi) + 1 + 100);
		CHECK_GE(max_end, static_cast<wide_t>(hi) + 1);
	}
}

template <typename T>
static void sweep(const std::vector<uint64_t>& accuracy, std::mt19937_64& rng, size_t n, size_t& wrong, size_t& refused) {
	std::vector<T> marks = {std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), 0, 1, 2, 99, 100, 101};
	for (int k = 0; k < 64; ++k) {
		for (int d = -2; d <= 2; ++d) {
			marks.push_back(static_cast<T>(static_cast<uint64_t>(static_cast<wide_t>(1) << k) + d));
			marks.push_back(static_cast<T>(-static_cast<wide_t>(static_cast<uint64_t>(1) << (k % 63)) + d));
		}
	}
	wide_t p = 1;
	for (int k = 0; k < 19; ++k, p *= 10) {
		for (int d = -2; d <= 2; ++d) {
			marks.push_back(static_cast<T>(p + d));
			marks.push_back(static_cast<T>(-p + d));
		}
	}
	for (auto a : accuracy) {
		for (int d = -1; d <= 1; ++d) {
			marks.push_back(static_cast<T>(static_cast<wide_t>(a) + d));
			marks.push_back(static_cast<T>(static_cast<wide_t>(a) * 2 + d));
		}
	}
	for (size_t i = 0; i < n; ++i) {
		T a = marks[rng() % marks.size()], b = marks[rng() % marks.size()];
		if (rng() % 4 == 0) {
			a = static_cast<T>(rng());
		}
		if (rng() % 4 == 0) {
			b = static_cast<T>(rng());
		}
		if (b < a) {
			std::swap(a, b);
		}
		for (size_t budget : {size_t(8), size_t(128)}) {
			std::vector<range_terms::Bucket<T>> buckets;
			if (!range_terms::decompose<T>(a, b, accuracy, budget, buckets)) {
				++refused;
				CHECK(buckets.empty());
				continue;
			}
			CHECK_LE(buckets.size(), budget);
			if (!covers<T>(a, b, accuracy, buckets, rng)) {
				++wrong;
			}
		}
	}
}

// Random and boundary ranges over the whole 64-bit domain (powers of two and ten, accuracy edges, aligned
// starts, negatives, the full range), signed and unsigned.
static void test_wide_sweep() {
	const std::vector<std::vector<uint64_t>> lists = {
		{100, 1000, 10000, 100000, 1000000, 100000000},  // default numeric
		{60, 3600},                                       // default time
		{1, 10, 100},
		{100, 1000},
		{3, 7, 50, 1000},                                 // not nested
		{4294967296ULL},
		{1ULL << 63},
		{1, 1ULL << 32, 1ULL << 63},
		{7},
	};
	std::mt19937_64 rng(3);
	size_t wrong = 0, refused = 0;
	for (const auto& accuracy : lists) {
		sweep<int64_t>(accuracy, rng, 3000, wrong, refused);
		sweep<uint64_t>(accuracy, rng, 3000, wrong, refused);
	}
	CHECK_EQ(wrong, size_t(0));
	CHECK_GT(refused, size_t(0));
}

// The regressions named by the original reports.
static void test_known_ranges() {
	const std::vector<uint64_t> accuracy = {100, 1000, 10000, 100000, 1000000, 100000000};
	std::mt19937_64 rng(4);
	const std::vector<std::pair<int64_t, int64_t>> ranges = {
		{0, 0}, {100, 100}, {0, 3000000000LL}, {1, 3000000000LL}, {1999, 4294967295LL}, {100000, 134217729},
		{2, 150416086}, {268435455, 268435457}, {55, 2147483647}, {0, 250000000}, {200000000, 300000000},
		{-1, 0}, {-100, -1}, {-3000000000LL, 3000000000LL},
	};
	for (auto [lo, hi] : ranges) {
		std::vector<range_terms::Bucket<int64_t>> buckets;
		CHECK(range_terms::decompose<int64_t>(lo, hi, accuracy, 128, buckets));
		CHECK(covers<int64_t>(lo, hi, accuracy, buckets, rng));
	}

	// Past the budget the filter is dropped, never part of the range.
	std::vector<range_terms::Bucket<int64_t>> buckets;
	CHECK_FALSE(range_terms::decompose<int64_t>(std::numeric_limits<int64_t>::min(), 5, accuracy, 128, buckets));
	CHECK(buckets.empty());
	std::vector<range_terms::Bucket<uint64_t>> ubuckets;
	CHECK_FALSE(range_terms::decompose<uint64_t>(0, std::numeric_limits<uint64_t>::max(), accuracy, 128, ubuckets));
}

TEST_MAIN(test_exhaustive_small_domain, test_tight_with_ample_budget, test_wide_sweep, test_known_ranges)
