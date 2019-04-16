/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <chrono>          // for system_clock, time_point
#include <cmath>           // for std::round
#include <ctime>           // for time_t
#include <iostream>
#include <regex>           // for regex
#include <string>          // for string
#include <string_view>     // for std::string_view
#include <type_traits>     // for forward

#include "exception.h"     // for ClientError


constexpr int DATETIME_EPOCH            = 1970;
constexpr int DATETIME_START_YEAR       = 1900;
constexpr int DATETIME_EPOCH_ORD        = 719163;  /* toordinal(DATETIME_EPOCH, 1, 1) */
constexpr double DATETIME_NANOSECONDS   = 1e9;
constexpr double DATETIME_MICROSECONDS  = 1e6;
constexpr double DATETIME_MILLISECONDS  = 1e3;
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


class TimeError : public DatetimeError {
public:
	template<typename... Args>
	TimeError(Args&&... args) : DatetimeError(std::forward<Args>(args)...) { }
};


class TimedeltaError : public DatetimeError {
public:
	template<typename... Args>
	TimedeltaError(Args&&... args) : DatetimeError(std::forward<Args>(args)...) { }
};


namespace Datetime {
	inline double normalize_fsec(double fsec) {
		if (fsec > DATETIME_MAX_FSEC) {
			return DATETIME_MAX_FSEC;
		} else if (fsec < 0.0) {
			return 0.0;
		} else {
			return std::round(fsec * DATETIME_MICROSECONDS) / DATETIME_MICROSECONDS;
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

	tm_t DatetimeParser(std::string_view datetime);
	inline tm_t DatetimeParser(const std::string& datetime) {
		return DatetimeParser(std::string_view(datetime));
	}
	tm_t DatetimeParser(const MsgPack& value);
	Format Iso8601Parser(std::string_view datetime, tm_t& tm);
	Format Iso8601Parser(std::string_view datetime);
	void processDateMath(std::string_view date_math, tm_t& tm);
	void computeTimeZone(tm_t& tm, char op, std::string_view hour, std::string_view min);
	void computeDateMath(tm_t& tm, std::string_view op, char unit);
	bool isleapYear(int year);
	bool isleapRef_year(int tm_year);
	int getDays_month(int year, int month);
	void normalizeMonths(int& year, int& mon);
	std::time_t toordinal(int year, int month, int day);
	std::time_t timegm(const std::tm& tm);
	std::time_t timegm(tm_t& tm);
	tm_t to_tm_t(std::time_t timestamp);
	tm_t to_tm_t(double timestamp);
	double timestamp(const tm_t& tm);
	inline double timestamp(const std::chrono::time_point<std::chrono::system_clock>& tp) {
		return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() / DATETIME_MICROSECONDS;
	}
	bool isvalidDate(int year, int month, int day);
	std::string iso8601(const std::tm& tm, bool trim=true, char sep='T');
	std::string iso8601(const tm_t& tm, bool trim=true, char sep='T');
	std::string iso8601(double timestamp, bool trim=true, char sep='T');
	std::string iso8601(const std::chrono::time_point<std::chrono::system_clock>& tp, bool trim=true, char sep='T');
	bool isDate(std::string_view date);
	bool isDatetime(std::string_view datetime);


	/*
	 * Struct for time with zone.
	 */

	struct clk_t {
		int hour;
		int min;
		int sec;
		double fsec;

		char tz_s;
		int tz_h;
		int tz_m;

		clk_t(int h=0, int m=0, int s=0, double fs=0.0, char tzs='+', int tzh=0, int tzm=0)
			: hour(h), min(m), sec(s), fsec(fs), tz_s(tzs), tz_h(tzh), tz_m(tzm)  { }
	};


	/*
	 * Specialized functions for time.
	 */

	clk_t TimeParser(std::string_view _time);
	clk_t time_to_clk_t(double t);
	double time_to_double(const MsgPack& _time);
	double time_to_double(const clk_t& clk);
	std::string time_to_string(const clk_t& clk, bool trim=true);
	std::string time_to_string(double t, bool trim=true);
	bool isTime(std::string_view _time);

	inline bool isvalidTime(double t) {
		static const long long min = -362339LL;       // 00:00:00+99:99
		static const long long max =  724779999999LL; // 99:99:99.9999...-99:99
		long long scaled = static_cast<long long>(t * DATETIME_MICROSECONDS);
		return scaled >= min && scaled <= max;
	}


	/*
	 * Specialized functions for timedelta.
	 */

	clk_t TimedeltaParser(std::string_view timedelta);
	clk_t timedelta_to_clk_t(double t);
	double timedelta_to_double(const MsgPack& timedelta);
	double timedelta_to_double(const clk_t& clk);
	std::string timedelta_to_string(const clk_t& clk, bool trim=true);
	std::string timedelta_to_string(double t, bool trim=true);
	bool isTimedelta(std::string_view timedelta);

	inline bool isvalidTimedelta(double t) {
		static const long long min = -362439999999LL; // -99:99:99.999...
		static const long long max =  362439999999LL; // +99:99:99.999...
		long long scaled = static_cast<long long>(t * DATETIME_MICROSECONDS);
		return scaled >= min && scaled <= max;
	}
}
