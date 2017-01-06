/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stats.h>

pos_time_t b_time;
std::chrono::time_point<std::chrono::system_clock> init_time;
times_row_t stats_cnt;
std::mutex stats_mutex;


void update_pos_time() {
	auto b_time_second = b_time.second;
	auto b_time_minute = b_time.minute;

	auto t_current = std::chrono::system_clock::now();
	auto t_elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_current - init_time).count();

	if (t_elapsed >= SLOT_TIME_SECOND) {
		fill_zeros_stats_sec(0, SLOT_TIME_SECOND - 1);
		b_time.minute += t_elapsed / SLOT_TIME_SECOND;
		b_time.second = t_elapsed % SLOT_TIME_SECOND;
	} else {
		b_time.second += t_elapsed;
		if (b_time.second >= SLOT_TIME_SECOND) {
			fill_zeros_stats_sec(b_time_second + 1, SLOT_TIME_SECOND - 1);
			fill_zeros_stats_sec(0, b_time.second % SLOT_TIME_SECOND);
			b_time.minute += b_time.second / SLOT_TIME_SECOND;
			b_time.second = t_elapsed % SLOT_TIME_SECOND;
		} else {
			fill_zeros_stats_sec(b_time_second + 1, b_time.second);
		}
	}

	init_time = t_current;

	if (b_time.minute >= SLOT_TIME_MINUTE) {
		fill_zeros_stats_min(b_time_minute + 1, SLOT_TIME_MINUTE - 1);
		fill_zeros_stats_min(0, b_time.minute % SLOT_TIME_MINUTE);
		b_time.minute = b_time.minute % SLOT_TIME_MINUTE;
	} else {
		fill_zeros_stats_min(b_time_minute + 1, b_time.minute);
	}

	ASSERT(b_time.second < SLOT_TIME_SECOND);
	ASSERT(b_time.minute < SLOT_TIME_MINUTE);
}


void fill_zeros_stats_min(uint16_t start, uint16_t end) {
	for (auto i = start; i <= end; ++i) {
		stats_cnt.index.min[i] = 0;
		stats_cnt.index.tm_min[i] = 0;
		stats_cnt.search.min[i] = 0;
		stats_cnt.search.tm_min[i] = 0;
		stats_cnt.del.min[i] = 0;
		stats_cnt.del.tm_min[i] = 0;
		stats_cnt.patch.min[i] = 0;
		stats_cnt.patch.tm_min[i] = 0;
	}
}


void fill_zeros_stats_sec(uint8_t start, uint8_t end) {
	for (auto i = start; i <= end; ++i) {
		stats_cnt.index.sec[i] = 0;
		stats_cnt.index.tm_sec[i] = 0;
		stats_cnt.search.sec[i] = 0;
		stats_cnt.search.tm_sec[i] = 0;
		stats_cnt.del.sec[i] = 0;
		stats_cnt.del.tm_sec[i] = 0;
		stats_cnt.patch.sec[i] = 0;
		stats_cnt.patch.tm_sec[i] = 0;
	}
}


void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.min[i];
		cnt[1] += stats_cnt_cpy.search.min[i];
		cnt[2] += stats_cnt_cpy.del.min[i];
		cnt[3] += stats_cnt_cpy.patch.min[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_min[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_min[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_min[i];
		tm_cnt[3] += stats_cnt_cpy.patch.tm_min[i];
	}
}


void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.sec[i];
		cnt[1] += stats_cnt_cpy.search.sec[i];
		cnt[2] += stats_cnt_cpy.del.sec[i];
		cnt[3] += stats_cnt_cpy.patch.sec[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_sec[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_sec[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_sec[i];
		tm_cnt[3] += stats_cnt_cpy.patch.tm_sec[i];
	}
}
