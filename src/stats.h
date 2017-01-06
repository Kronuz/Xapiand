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

#pragma once

#include "xapiand.h"

#include <chrono>       // for system_clock, time_point, duration_cast, seconds
#include <mutex>        // for mutex
#include <vector>       // for vector


constexpr uint16_t SLOT_TIME_MINUTE = 1440;
constexpr uint8_t SLOT_TIME_SECOND = 60;


struct Stats {
	struct Pos {
		uint16_t minute;
		uint8_t second;

		Pos();
		Pos(std::chrono::time_point<std::chrono::system_clock> current);
	};

	struct Counter {
		uint32_t min[SLOT_TIME_MINUTE];
		uint32_t sec[SLOT_TIME_SECOND];
		uint64_t tm_min[SLOT_TIME_MINUTE];
		uint64_t tm_sec[SLOT_TIME_SECOND];
	};

	std::chrono::time_point<std::chrono::system_clock> current;
	Pos current_pos;

	std::mutex mtx;

	Counter index;
	Counter search;
	Counter del;
	Counter patch;

	static Stats& cnt();

	Stats();
	Stats(Stats& other);

	void update_pos_time();

	void fill_zeros_stats_min(uint16_t start, uint16_t end);
	void fill_zeros_stats_sec(uint8_t start, uint8_t end);
	void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt);
	void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt);

	void add(Counter& counter, uint64_t duration);
	static void add_index(uint64_t duration);
	static void add_search(uint64_t duration);
	static void add_del(uint64_t duration);
	static void add_patch(uint64_t duration);
};
