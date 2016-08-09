/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include <chrono>
#include <ctime>
#include <iostream>
#include <regex>

#define _EPOCH      1970
#define _START_YEAR 1900
#define _EPOCH_ORD  719163  /* toordinal(_EPOCH, 1, 1) */


namespace Datetime {
	struct tm_t {
		int year;
		int mon;
		int day;
		int hour;
		int min;
		int sec;
		int msec;

		tm_t(int y=_EPOCH, int M=1, int d=1, int h=0, int m=0, int s=0, int ms=0)
			: year(y), mon(M), day(d), hour(h),
			  min(m), sec(s), msec(ms) { }
	};

	extern const std::regex date_re;
	extern const std::regex date_math_re;

	void dateTimeParser(const std::string& date, tm_t& tm);
	void computeDateMath(tm_t& tm, const std::string& op, char unit);
	bool isleapYear(int year);
	bool isleapRef_year(int tm_year);
	int getDays_month(int year, int month);
	std::time_t toordinal(int year, int month, int day);
	std::time_t timegm(std::tm* tm);
	std::time_t timegm(tm_t& tm);
	tm_t to_tm_t(double timestamp);
	double timestamp(tm_t& tm);
	double timestamp(const std::string& date);
	double timestamp(const std::string& date, tm_t& tm);
	bool isvalidDate(int year, int month, int day);
	std::string isotime(const std::tm* timep);
	std::string isotime(double epoch);
	void normalizeMonths(int& year, int& mon);
	bool isDate(const std::string& date);
	std::string to_string(const std::chrono::time_point<std::chrono::system_clock>& tp);
};
