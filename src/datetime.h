/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <chrono>       // for system_clock, time_point
#include <cmath>        // for std::round
#include <ctime>        // for time_t
#include <iostream>
#include <regex>        // for regex
#include <string>       // for string
#include <type_traits>  // for forward

#include "exception.h"  // for ClientError


constexpr int DATETIME_EPOCH            = 1970;
constexpr int DATETIME_START_YEAR       = 1900;
constexpr int DATETIME_EPOCH_ORD        = 719163;  /* toordinal(DATETIME_EPOCH, 1, 1) */
constexpr double DATETIME_MICROSECONDS  = 1e-6;
constexpr double DATETIME_MAX_FSEC      = 0.999999;


class MsgPack;


class DatetimeError : public ClientError {
public:
	template<typename... Args>
	DatetimeError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class DateISOError : public DatetimeError {
public:
	template<typename... Args>
	DateISOError(Args&&... args) : DatetimeError(std::forward<Args>(args)...) { }
};


namespace Datetime {
	inline double normalize_fsec(double fsec) {
		if (fsec > DATETIME_MAX_FSEC) {
			return DATETIME_MAX_FSEC;
		} else if (fsec < 0.0) {
			return 0.0;
		} else {
			return std::round(fsec / DATETIME_MICROSECONDS) * DATETIME_MICROSECONDS;
		}
	}

	struct tm_t {
		int year;
		int mon;
		int day;
		int hour;
		int min;
		int sec;
		double fsec;

		tm_t(int y=DATETIME_EPOCH, int M=1, int d=1, int h=0, int m=0, int s=0, double fs=0.0)
			: year(y), mon(M), day(d), hour(h),
			  min(m), sec(s), fsec(fs) { }
	};

	enum class Format : uint8_t {
		VALID,
		INVALID,
		OUT_OF_RANGE,
		ERROR,
	};

	extern const std::regex date_re;
	extern const std::regex date_math_re;

	void dateTimeParser(const std::string& date, tm_t& tm);
	Format ISO8601(const std::string& date, tm_t& tm);
	Format ISO8601(const std::string& date);
	void processDateMath(const std::string& date_math, tm_t& tm);
	void computeTimeZone(tm_t& tm, char op, const std::string& hour, const std::string& min);
	void computeDateMath(tm_t& tm, const std::string& op, char unit);
	bool isleapYear(int year);
	bool isleapRef_year(int tm_year);
	int getDays_month(int year, int month);
	std::time_t toordinal(int year, int month, int day);
	std::time_t timegm(const std::tm& tm);
	std::time_t timegm(tm_t& tm);
	tm_t to_tm_t(std::time_t timestamp);
	tm_t to_tm_t(double timestamp);
	tm_t to_tm_t(const std::string& date);
	tm_t to_tm_t(const MsgPack& value);
	double timestamp(tm_t& tm);
	double timestamp(const std::string& date);
	double timestamp(const std::string& date, tm_t& tm);
	double timestamp(const MsgPack& value);
	double timestamp(const MsgPack& value, tm_t& tm);
	bool isvalidDate(int year, int month, int day);
	std::string isotime(const std::tm& timep);
	std::string isotime(const tm_t& timep, bool trim=true);
	std::string isotime(double epoch, bool trim=true);
	void normalizeMonths(int& year, int& mon);
	bool isDate(const std::string& date);
	bool isTime(const std::string& date);
	bool isTimeDelta(const std::string& date);
	std::string to_string(const std::chrono::time_point<std::chrono::system_clock>& tp, bool trim=true);
	std::string normalizeISO8601(const std::string& iso_date, bool trim=true);
};
