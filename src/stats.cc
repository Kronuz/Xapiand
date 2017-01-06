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

#include "stats.h"

Stats::Pos::Pos()
	: minute(0),
	  second(0)
{
}


Stats::Pos::Pos(std::chrono::time_point<std::chrono::system_clock> current)
{
	time_t epoch = std::chrono::system_clock::to_time_t(current);
	struct tm *timeinfo = localtime(&epoch);
	timeinfo->tm_hour   = 0;
	timeinfo->tm_min    = 0;
	timeinfo->tm_sec    = 0;
	auto delta = epoch - mktime(timeinfo);
	minute = delta / SLOT_TIME_SECOND;
	second =  delta % SLOT_TIME_SECOND;
}


Stats&
Stats::cnt()
{
	static Stats stats_cnt;
	return stats_cnt;
}


Stats::Stats()
	: current(std::chrono::system_clock::now()),
	  current_pos(current)
{
}


Stats::Stats(Stats& other)
	: current(std::chrono::system_clock::now())
{
	std::lock_guard<std::mutex> lk(other.mtx);
	current = other.current;
	current_pos = other.current_pos;
	index = other.index;
	search = other.search;
	del = other.del;
	patch = other.patch;
}


void
Stats::update_pos_time()
{
	auto b_time_second = current_pos.second;
	auto b_time_minute = current_pos.minute;

	auto now = std::chrono::system_clock::now();
	auto t_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - current).count();

	if (t_elapsed >= SLOT_TIME_SECOND) {
		fill_zeros_stats_sec(0, SLOT_TIME_SECOND - 1);
		current_pos.minute += t_elapsed / SLOT_TIME_SECOND;
		current_pos.second = t_elapsed % SLOT_TIME_SECOND;
	} else {
		current_pos.second += t_elapsed;
		if (current_pos.second >= SLOT_TIME_SECOND) {
			fill_zeros_stats_sec(b_time_second + 1, SLOT_TIME_SECOND - 1);
			fill_zeros_stats_sec(0, current_pos.second % SLOT_TIME_SECOND);
			current_pos.minute += current_pos.second / SLOT_TIME_SECOND;
			current_pos.second = t_elapsed % SLOT_TIME_SECOND;
		} else {
			fill_zeros_stats_sec(b_time_second + 1, current_pos.second);
		}
	}

	current = now;

	if (current_pos.minute >= SLOT_TIME_MINUTE) {
		fill_zeros_stats_min(b_time_minute + 1, SLOT_TIME_MINUTE - 1);
		fill_zeros_stats_min(0, current_pos.minute % SLOT_TIME_MINUTE);
		current_pos.minute = current_pos.minute % SLOT_TIME_MINUTE;
	} else {
		fill_zeros_stats_min(b_time_minute + 1, current_pos.minute);
	}

	ASSERT(current_pos.second < SLOT_TIME_SECOND);
	ASSERT(current_pos.minute < SLOT_TIME_MINUTE);
}


void
Stats::fill_zeros_stats_min(uint16_t start, uint16_t end)
{
	for (auto i = start; i <= end; ++i) {
		index.min[i] = 0;
		index.tm_min[i] = 0;
		search.min[i] = 0;
		search.tm_min[i] = 0;
		del.min[i] = 0;
		del.tm_min[i] = 0;
		patch.min[i] = 0;
		patch.tm_min[i] = 0;
	}
}


void
Stats::fill_zeros_stats_sec(uint8_t start, uint8_t end)
{
	for (auto i = start; i <= end; ++i) {
		index.sec[i] = 0;
		index.tm_sec[i] = 0;
		search.sec[i] = 0;
		search.tm_sec[i] = 0;
		del.sec[i] = 0;
		del.tm_sec[i] = 0;
		patch.sec[i] = 0;
		patch.tm_sec[i] = 0;
	}
}


void
Stats::add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt)
{
	for (auto i = start; i <= end; ++i) {
		cnt[0] += index.min[i];
		cnt[1] += search.min[i];
		cnt[2] += del.min[i];
		cnt[3] += patch.min[i];
		tm_cnt[0] += index.tm_min[i];
		tm_cnt[1] += search.tm_min[i];
		tm_cnt[2] += del.tm_min[i];
		tm_cnt[3] += patch.tm_min[i];
	}
}


void
Stats::add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt)
{
	for (auto i = start; i <= end; ++i) {
		cnt[0] += index.sec[i];
		cnt[1] += search.sec[i];
		cnt[2] += del.sec[i];
		cnt[3] += patch.sec[i];
		tm_cnt[0] += index.tm_sec[i];
		tm_cnt[1] += search.tm_sec[i];
		tm_cnt[2] += del.tm_sec[i];
		tm_cnt[3] += patch.tm_sec[i];
	}
}


void
Stats::add(Counter& counter, uint64_t duration)
{
	std::lock_guard<std::mutex> lk(mtx);
	update_pos_time();
	++counter.min[current_pos.minute];
	++counter.sec[current_pos.second];
	counter.tm_min[current_pos.minute] += duration;
	counter.tm_sec[current_pos.second] += duration;
}


void
Stats::add_index(uint64_t duration)
{
	auto& stats_cnt = cnt();
	stats_cnt.add(stats_cnt.index, duration);
}


void
Stats::add_search(uint64_t duration)
{
	auto& stats_cnt = cnt();
	stats_cnt.add(stats_cnt.search, duration);
}


void
Stats::add_del(uint64_t duration)
{
	auto& stats_cnt = cnt();
	stats_cnt.add(stats_cnt.del, duration);
}


void
Stats::add_patch(uint64_t duration)
{
	auto& stats_cnt = cnt();
	stats_cnt.add(stats_cnt.patch, duration);
}
