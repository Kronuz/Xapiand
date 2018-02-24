/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <limits>


Stats::Counter::Element::Element()
	: cnt(0),
	  total(0),
	  max(0),
	  min(std::numeric_limits<uint64_t>::max()) { }


Stats::Counter::Element::Element(uint64_t duration)
{
	cnt = 1;
	total = duration;
	max = duration;
	min = duration;
}


inline void
Stats::Counter::Element::clear()
{
	cnt = 0;
	total = 0;
	max = 0;
	min = std::numeric_limits<uint64_t>::max();
}


inline void
Stats::Counter::Element::add(const Element& other)
{
	cnt += other.cnt;
	total += other.total;
	if (max < other.max) {
		max = other.max;
	}
	if (min > other.min) {
		min = other.min;
	}
}


inline
Stats::Counter::Counter()
{
	clear_stats_min(0, SLOT_TIME_MINUTE - 1);
	clear_stats_sec(0, SLOT_TIME_SECOND - 1);
}


inline void
Stats::Counter::clear_stats_min(int start, int end)
{
	for (auto i = start; i <= end; ++i) {
		min[i].clear();
	}
}


inline void
Stats::Counter::clear_stats_sec(int start, int end)
{
	for (auto i = start; i <= end; ++i) {
		sec[i].clear();
	}
}


inline void
Stats::Counter::add_stats_min(int start, int end, Stats::Counter::Element& element)
{
	if (start <= end) {
		for (auto i = start; i <= end; ++i) {
			element.add(min[i]);
		}
	} else {
		for (auto i = start; i < SLOT_TIME_MINUTE; ++i) {
			element.add(min[i]);
		}
		for (auto i = 0; i <= end; ++i) {
			element.add(min[i]);
		}
	}
}


inline void
Stats::Counter::add_stats_sec(int start, int end, Element& element)
{
	if (start <= end) {
		for (auto i = start; i <= end; ++i) {
			element.add(sec[i]);
		}
	} else {
		for (auto i = start; i < SLOT_TIME_SECOND; ++i) {
			element.add(sec[i]);
		}
		for (auto i = 0; i <= end; ++i) {
			element.add(sec[i]);
		}
	}
}


Stats::Pos::Pos()
	: minute(0.0),
	  second(0) { }


Stats::Pos::Pos(const std::chrono::time_point<std::chrono::system_clock>& current)
{
	time_t epoch = std::chrono::system_clock::to_time_t(current);
	struct tm *timeinfo = localtime(&epoch);
	timeinfo->tm_hour   = 0;
	timeinfo->tm_min    = 0;
	timeinfo->tm_sec    = 0;
	auto delta = epoch - mktime(timeinfo);
	minute = delta / 60.0;
	second =  delta % SLOT_TIME_SECOND;
}


Stats&
Stats::cnt()
{
	static Stats stats_cnt;
	return stats_cnt;
}


Stats::Stats()
	: current(std::chrono::system_clock::now()) { }


Stats::Stats(Stats& other)
	: current(std::chrono::system_clock::now())
{
	std::lock_guard<std::mutex> lk(other.mtx);
	other.update_pos_time();
	current = other.current;
	current_pos = other.current_pos;
	counters = other.counters;
}


inline void
Stats::update_pos_time()
{
	auto b_time_second = current_pos.second;
	auto b_time_minute = current_pos.minute;

	auto now = std::chrono::system_clock::now();
	auto t_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - current).count();

	current_pos.minute += t_elapsed / 60.0;
	current_pos.second += t_elapsed;

	if (current_pos.second < SLOT_TIME_SECOND) {
		clear_stats_sec(b_time_second + 1, current_pos.second);
	} else {
		current_pos.second %= SLOT_TIME_SECOND;
		if (t_elapsed < SLOT_TIME_SECOND) {
			clear_stats_sec(b_time_second + 1, SLOT_TIME_SECOND - 1);
			clear_stats_sec(0, current_pos.second);
		} else {
			clear_stats_sec(0, SLOT_TIME_SECOND - 1);
		}
	}

	if (current_pos.minute < SLOT_TIME_MINUTE) {
		clear_stats_min(b_time_minute + 1, current_pos.minute);
	} else {
		int int_min = static_cast<int>(current_pos.minute);
		int mod_min = int_min % SLOT_TIME_MINUTE;
		current_pos.minute = current_pos.minute - int_min + mod_min;
		if (t_elapsed < SLOT_TIME_MINUTE) {
			clear_stats_min(b_time_minute + 1, SLOT_TIME_MINUTE - 1);
			clear_stats_min(0, mod_min);
		} else {
			clear_stats_min(0, SLOT_TIME_MINUTE - 1);
		}
	}

	current = now;
}


inline void
Stats::clear_stats_min(int start, int end)
{
	for (auto& counter : counters) {
		counter.second.clear_stats_min(start, end);
	}
}


inline void
Stats::clear_stats_sec(int start, int end)
{
	for (auto& counter : counters) {
		counter.second.clear_stats_sec(start, end);
	}
}


void
Stats::add_stats_min(int start, int end, std::unordered_map<std::string, Stats::Counter::Element>& cnt)
{
	for (auto& counter : counters) {
		auto& element = cnt[counter.first];
		counter.second.add_stats_min(start, end, element);
	}
}


void
Stats::add_stats_sec(int start, int end, std::unordered_map<std::string, Stats::Counter::Element>& cnt)
{
	for (auto& counter : counters) {
		auto& element = cnt[counter.first];
		counter.second.add_stats_sec(start, end, element);
	}
}


inline void
Stats::add(Counter& counter, uint64_t duration)
{
	std::lock_guard<std::mutex> lk(mtx);
	update_pos_time();
	Counter::Element element(duration);
	counter.min[static_cast<int>(current_pos.minute)].add(element);
	counter.sec[current_pos.second].add(element);
}


void
Stats::add(const std::string& counter, uint64_t duration)
{
	auto& stats_cnt = cnt();
	stats_cnt.add(stats_cnt.counters[counter], duration);
}
