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

// P1 diagnostic probes (explore/p1-light-wait, not for landing): lock-free log2-microsecond histograms per stage and
// index class, printed by :metrics as xapiand_p1_us_bucket{stage,cls,le}.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace p1 {

enum Stage : unsigned {
	turn_wait, turn_hold, fresh_dirty, refresh_run,
	chore_queue_query, chore_run_query, chore_resume_query,
	chore_queue_flush, chore_run_flush, chore_resume_flush,
	chore_queue_merge, chore_run_merge, chore_resume_merge,
	pin_wait,
	get_resolve, get_docid, get_data, get_tail, get_total,
	put_resolve, put_index, put_tail, put_total,
	search_total,
	stages
};

inline constexpr const char* stage_names[stages] = {
	"turn_wait", "turn_hold", "fresh_dirty", "refresh_run",
	"chore_queue_query", "chore_run_query", "chore_resume_query",
	"chore_queue_flush", "chore_run_flush", "chore_resume_flush",
	"chore_queue_merge", "chore_run_merge", "chore_resume_merge",
	"pin_wait",
	"get_resolve", "get_docid", "get_data", "get_tail", "get_total",
	"put_resolve", "put_index", "put_tail", "put_total",
	"search_total",
};

enum Cls : unsigned { accounts, restore, other, classes };
inline constexpr const char* cls_names[classes] = {"accounts", "restore", "other"};

inline Cls cls_of(std::string_view path) noexcept {
	if (path.find("restore") != std::string_view::npos) { return restore; }
	if (path.find("accounts") != std::string_view::npos) { return accounts; }
	return other;
}

inline constexpr unsigned BUCKETS = 28;  // bucket i: below 2^i us; the last also takes the rest

struct Hist {
	std::array<std::atomic<std::uint64_t>, BUCKETS> b{};
	std::atomic<std::uint64_t> sum_us{0};
};

inline Hist& hist(Stage s, Cls c) noexcept {
	static Hist h[stages][classes];
	return h[s][c];
}

using clock = std::chrono::steady_clock;

inline void add(Stage s, Cls c, clock::duration d) noexcept {
	const auto us = static_cast<std::uint64_t>(std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::microseconds>(d).count()));
	unsigned i = 0;
	while (i + 1 < BUCKETS && us >= (std::uint64_t{1} << i)) { ++i; }
	auto& h = hist(s, c);
	h.b[i].fetch_add(1, std::memory_order_relaxed);
	h.sum_us.fetch_add(us, std::memory_order_relaxed);
}

inline void add(Stage s, Cls c, clock::time_point from, clock::time_point to) noexcept {
	add(s, c, to - from);
}

inline std::string text() {
	std::string out;
	for (unsigned s = 0; s < stages; ++s) {
		for (unsigned c = 0; c < classes; ++c) {
			auto& h = hist(static_cast<Stage>(s), static_cast<Cls>(c));
			std::uint64_t n = 0;
			for (unsigned i = 0; i < BUCKETS; ++i) { n += h.b[i].load(std::memory_order_relaxed); }
			if (n == 0) { continue; }
			std::uint64_t cum = 0;
			for (unsigned i = 0; i < BUCKETS; ++i) {
				cum += h.b[i].load(std::memory_order_relaxed);
				out += "xapiand_p1_us_bucket{stage=\"";
				out += stage_names[s];
				out += "\",cls=\"";
				out += cls_names[c];
				out += "\",le=\"";
				out += i + 1 < BUCKETS ? std::to_string(std::uint64_t{1} << i) : std::string("+Inf");
				out += "\"} ";
				out += std::to_string(cum);
				out += '\n';
			}
			out += "xapiand_p1_us_sum{stage=\"";
			out += stage_names[s];
			out += "\",cls=\"";
			out += cls_names[c];
			out += "\"} ";
			out += std::to_string(h.sum_us.load(std::memory_order_relaxed));
			out += '\n';
		}
	}
	return out;
}

}  // namespace p1
