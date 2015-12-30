/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "datetime.h"

#include "exception.h"
#include "log.h"
#include "utils.h"

#define MICROSEC 1e-6


const std::regex Datetime::date_re("([0-9]{4})([-/ ]?)(0[1-9]|1[0-2])\\2(0[0-9]|[12][0-9]|3[01])([T ]?([01]?[0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])([.,]([0-9]{1,3}))?)?([ ]*[+-]([01]?[0-9]|2[0-3]):([0-5][0-9])|Z)?)?([ ]*\\|\\|[ ]*([+-/\\dyMwdhms]+))?", std::regex::optimize);
const std::regex Datetime::date_math_re("([+-]\\d+|\\/{1,2})([dyMwhms])", std::regex::optimize);


static constexpr int days[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};


static constexpr int cumdays[2][12] = {
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};


/*
 * Full struct tm according to the date specified by "date".
 */
void
Datetime::dateTimeParser(const std::string &date, tm_t &tm)
{
	std::string oph, opm;
	std::smatch m;
	if (std::regex_match(date, m, date_re) && static_cast<size_t>(m.length(0)) == date.size()) {
		tm.year = std::stoi(m.str(1));
		tm.mon = std::stoi(m.str(3));
		tm.day = std::stoi(m.str(4));
		if (!isvalidDate(tm.year, tm.mon, tm.day)) {
			throw MSG_Error("Date is out of range");
		}

		if (m.length(5) > 0) {
			tm.hour = std::stoi(m.str(6));
			tm.min = std::stoi(m.str(7));
			if (m.length(8) > 0) {
				tm.sec = std::stoi(m.str(9));
				tm.msec = m.length(10) > 0 ? std::stoi(m.str(11)) : 0;
			} else {
				tm.sec = tm.msec = 0;
			}
			if (m.length(12) > 1) {
				if (std::string(date, m.position(13) - 1, 1) == "+") {
					oph = "-" + m.str(13);
					opm = "-" + m.str(14);
				} else {
					oph = "+" + m.str(13);
					opm = "+" + m.str(14);
				}
				computeDateMath(tm, oph, "h");
				computeDateMath(tm, opm, "m");
			}
		} else {
			tm.hour = tm.min = tm.sec = tm.msec = 0;
		}

		//Processing Date Math
		if (m.length(16) != 0) {
			int size_match = 0;
			std::string date_math(m.str(16));
			std::sregex_iterator next(date_math.begin(), date_math.end(), date_math_re, std::regex_constants::match_continuous);
			std::sregex_iterator end;
			while (next != end) {
				size_match += next->length(0);
				computeDateMath(tm, next->str(1), next->str(2));
				++next;
			}

			if (m.length(16) != size_match) {
				throw MSG_Error("Date Math (%s) is used incorrectly.\n", m.str(16).c_str());
			}
		}

		return;
	}

	throw MSG_Error("In dateTimeParser, format is incorrect.");
}


/*
 * Compute a Date Math former by op + units.
 * op can be +#, -#, /, //
 * units can be y, M, w, d, h, m, s
 */
void
Datetime::computeDateMath(tm_t &tm, const std::string &op, const std::string &units)
{
	time_t dateGMT;
	struct tm *timeinfo;
	if (op[0] == '+' || op[0] == '-') {
		int max_days, num = std::stoi(std::string(op.c_str() + 1, op.size()));
		switch (units[0]) {
			case 'y':
				(op[0] == '+') ? tm.year += num : tm.year -= num;
				break;
			case 'M':
				(op[0] == '+') ? tm.mon += num : tm.mon -= num;
				normalizeMonths(tm.year, tm.mon);
				max_days = getDays_month(tm.year, tm.mon);
				if (tm.day > max_days) tm.day = max_days;
				break;
			case 'w':
				(op[0] == '+') ? tm.day += 7 * num : tm.day -= 7 * num; break;
			case 'd':
				(op[0] == '+') ? tm.day += num : tm.day -= num; break;
			case 'h':
				(op[0] == '+') ? tm.hour += num : tm.hour -= num; break;
			case 'm':
				(op[0] == '+') ? tm.min += num : tm.min -= num; break;
			case 's':
				(op[0] == '+') ? tm.sec += num : tm.sec -= num; break;
		}
	} else {
		switch (units[0]) {
			case 'y':
				if (op.compare("/") == 0) {
					tm.mon = 12;
					tm.day = getDays_month(tm.year, 12);
					tm.hour = 23;
					tm.min = tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.mon = tm.day = 1;
					tm.hour = tm.min = tm.sec = tm.msec = 0;
				}
				break;
			case 'M':
				if (op.compare("/") == 0) {
					tm.day = getDays_month(tm.year, tm.mon);
					tm.hour = 23;
					tm.min = tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.day = 1;
					tm.hour = tm.min = tm.sec = tm.msec = 0;
				}
				break;
			case 'w':
				dateGMT = timegm(tm);
				timeinfo = gmtime(&dateGMT);
				if (op.compare("/") == 0) {
					tm.day += 6 - timeinfo->tm_wday;
					tm.hour = 23;
					tm.min = tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.day -= timeinfo->tm_wday;
					tm.hour = tm.min = tm.sec = tm.msec = 0;
				}
				break;
			case 'd':
				if (op.compare("/") == 0) {
					tm.hour = 23;
					tm.min = tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.hour = tm.min = tm.sec = tm.msec = 0;
				}
				break;
			case 'h':
				if (op.compare("/") == 0) {
					tm.min = tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.min = tm.sec = tm.msec = 0;
				}
				break;
			case 'm':
				if (op.compare("/") == 0) {
					tm.sec = 59;
					tm.msec = 999;
				} else {
					tm.sec = tm.msec = 0;
				}
				break;
			case 's':
				if (op.compare("/") == 0) {
					tm.msec = 999;
				} else {
					tm.msec = 0;
				}
			break;
		}
	}

	// Update date
	dateGMT = timegm(tm);
	timeinfo = gmtime(&dateGMT);
	tm.year = timeinfo->tm_year + _START_YEAR;
	tm.mon = timeinfo->tm_mon + 1;
	tm.day = timeinfo->tm_mday;
	tm.hour = timeinfo->tm_hour;
	tm.min = timeinfo->tm_min;
	tm.sec = timeinfo->tm_sec;
}


/*
 * Return if a year is leap.
 */
bool
Datetime::isleapYear(int year)
{
	return year % 400 == 0 || (year % 4 == 0 && year % 100);
}


/*
 * Return if a tm_year is leap.
 */
bool
Datetime::isleapRef_year(int tm_year)
{
	tm_year += _START_YEAR;
	return isleapYear(tm_year);
}


/*
 * Return number of days in month, given the year.
 */
int
Datetime::getDays_month(int year, int month)
{
	if (month < 1 || month > 12) throw MSG_Error("Month must be in 1..12");

	int leap = isleapYear(year);
	return days[leap][month - 1];
}


/*
 * Return the proleptic Gregorian ordinal of the date,
 * where January 1 of year 1 has ordinal 1 (reference date).
 * year -> Any positive number except zero.
 * month -> Between 1 and 12 inclusive.
 * day -> Between 1 and the number of days in the given month of the given year.
 */
time_t
Datetime::toordinal(int year, int month, int day)
{
	if (year < 1) throw MSG_Error("Year is out of range");
	if (day < 1 || day > getDays_month(year, month)) throw MSG_Error("Day is out of range for month");

	int leap = isleapYear(year);
	size_t result = 365 * (year - 1) + (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400 + cumdays[leap][month - 1] + day;

	return result;
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 */
time_t
Datetime::timegm(struct tm *tm)
{
	int year = tm->tm_year + _START_YEAR, mon = tm->tm_mon + 1;
	normalizeMonths(year, mon);
	time_t result = toordinal(year, mon, 1) - _EPOCH_ORD + tm->tm_mday - 1;
	result *= 24;
	result += tm->tm_hour;
	result *= 60;
	result += tm->tm_min;
	result *= 60;
	result += tm->tm_sec;

	return result;
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 */
time_t
Datetime::timegm(tm_t &tm)
{
	normalizeMonths(tm.year, tm.mon);
	time_t result = toordinal(tm.year, tm.mon, 1) - _EPOCH_ORD + tm.day - 1;
	result *= 24;
	result += tm.hour;
	result *= 60;
	result += tm.min;
	result *= 60;
	result += tm.sec;

	return result;
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 * Return Timestamp with milliseconds as the decimal part.
 */
double
Datetime::mtimegm(tm_t &tm)
{
	normalizeMonths(tm.year, tm.mon);
	double result = (double)toordinal(tm.year, tm.mon, 1) - _EPOCH_ORD + tm.day - 1;
	result *= 24;
	result += tm.hour;
	result *= 60;
	result += tm.min;
	result *= 60;
	result += tm.sec;
	(result < 0) ? result -= tm.msec / 1000.0 : result += tm.msec / 1000.0;

	return result;
}


/*
 * Return the timestamp of date.
 */
double
Datetime::timestamp(const std::string &date)
{
	if (!isNumeric(date)) {
		tm_t tm;
		dateTimeParser(date, tm);
		return mtimegm(tm);
	} else {
		double timestamp;
		std::stringstream ss;
		ss << std::dec << date;
		ss >> timestamp;
		ss.flush();
		return timestamp;
	}
}


/*
 * Validate Date.
 */
bool
Datetime::isvalidDate(int year, int month, int day)
{
	if (year < 1) {
		L_ERR(nullptr, "ERROR: Year is out of range.");
		return false;
	}

	try {
		if (day < 1 || day > getDays_month(year, month)) {
			L_ERR(nullptr, "ERROR: Day is out of range for month.");
			return false;
		}
	} catch (const std::exception &ex) {
		L_ERR(nullptr, "ERROR: %s.", ex.what());
		return false;
	}

	return true;
}


/*
 * Return a string with the date in ISO 8601 Format.
 */
char*
Datetime::isotime(const struct tm *tm, int microseconds)
{
	static char result[30];
	sprintf(result, "%2.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d.%2.6d",
		_START_YEAR + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, microseconds);
	return result;
}


/*
 * Transform a epoch string to ISO 8601 format if epoch is numeric,
 * the decimal part of epoch are microseconds.
 * If epoch does not numeric, return epoch.
 */
::std::string
Datetime::ctime(const ::std::string &epoch)
{
	if (isNumeric(epoch)) {
		double utimestamp = std::stod(epoch);
		time_t timestamp = (time_t) utimestamp;
		std::string microseconds = epoch;
		struct tm *timeinfo = gmtime(&timestamp);
		return isotime(timeinfo, std::stod(std::string(microseconds.c_str() + microseconds.find("."), 7)) / MICROSEC);
	} else {
		return epoch;
	}
}


/*
 * Transforms a epoch in seconds to ISO 8601 format,
 * the decimal part of epoch are microseconds.
 */
::std::string
Datetime::ctime(double epoch)
{
	time_t timestamp = (time_t) epoch;
	int microseconds = (epoch - timestamp) / MICROSEC;
	struct tm *timeinfo = gmtime(&timestamp);
	return isotime(timeinfo, microseconds);
}


/*
 * Normalize months between -11 and 11
 */
void
Datetime::normalizeMonths(int &year, int &mon)
{
	if (mon > 12) {
		year += mon / 12;
		mon %= 12;
	} else while(mon < 1) {
		mon += 12;
		year--;
	}
}


bool
Datetime::isDate(const std::string &date)
{
	std::smatch m;
	return std::regex_match(date, m, date_re) && static_cast<size_t>(m.length(0)) == date.size();
}


std::string
Datetime::to_string(const std::chrono::time_point<std::chrono::system_clock> &tp)
{
	return ctime(std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() * MICROSEC);
}
