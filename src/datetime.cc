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


pcre *Datetime::compiled_date_re = NULL;
pcre *Datetime::compiled_date_math_re = NULL;

static const int days[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static const int cumdays[2][12] = {
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
};


/*
 * Full struct tm according to the date specified by "date".
 */
void
Datetime::dateTimeParser(const std::string &date, tm_t &tm)
{
	int len = (int)date.size();
	int ret, offset = 0;
	std::string oph, opm;
	std::unique_ptr<group_t, group_t_deleter> unique_gr;
	ret = pcre_search(date.c_str(), len, offset, 0, DATE_RE, &compiled_date_re, unique_gr);
	group_t *gr = unique_gr.get();

	if (ret != -1 && len == gr[0].end - gr[0].start) {
		std::string parse(date, gr[1].start, gr[1].end - gr[1].start);
		tm.year = strtoint(parse);
		parse.assign(date, gr[3].start, gr[3].end - gr[3].start);
		tm.mon = strtoint(parse);
		parse.assign(date, gr[4].start, gr[4].end - gr[4].start);
		tm.day = strtoint(parse);
		if (!isvalidDate(tm.year, tm.mon, tm.day)) throw MSG_Error("Date is out of range");

		if (gr[5].end - gr[5].start > 0) {
			parse.assign(date, gr[6].start, gr[6].end - gr[6].start);
			tm.hour = strtoint(parse);
			parse.assign(date, gr[7].start, gr[7].end - gr[7].start);
			tm.min = strtoint(parse);
			if (gr[8].end - gr[8].start > 0) {
				parse.assign(date, gr[9].start, gr[9].end - gr[9].start);
				tm.sec = strtoint(parse);
				if (gr[10].end - gr[10].start > 0) {
					parse.assign(date, gr[11].start, gr[11].end - gr[11].start);
					tm.msec = strtoint(parse);
				} else {
					tm.msec = 0;
				}
			} else {
				tm.sec = tm.msec = 0;
			}
			if (gr[12].end - gr[12].start > 1) {
				if (std::string(date, gr[13].start - 1, 1).compare("+") == 0) {
					oph = "-" + std::string(date, gr[13].start, gr[13].end - gr[13].start);
					opm = "-" + std::string(date, gr[14].start, gr[14].end - gr[14].start);
				} else {
					oph = "+" + std::string(date, gr[13].start, gr[13].end - gr[13].start);
					opm = "+" + std::string(date, gr[14].start, gr[14].end - gr[14].start);
				}
				computeDateMath(tm, oph, "h");
				computeDateMath(tm, opm, "m");
			}
		} else {
			tm.hour = tm.min = tm.sec = tm.msec = 0;
		}

		//Processing Date Math
		std::string date_math;
		len = gr[16].end - gr[16].start;
		if (len != 0) {
			date_math.assign(date, gr[16].start, len);
			unique_gr.reset();

			while (pcre_search(date_math.c_str(), len, offset, 0, DATE_MATH_RE, &compiled_date_math_re, unique_gr) == 0) {
				gr = unique_gr.get();
				offset = gr[0].end;
				computeDateMath(tm, std::string(date_math, gr[1].start, gr[1].end - gr[1].start), std::string(date_math, gr[2].start, gr[2].end - gr[2].start));
			}

			if (offset != len) throw MSG_Error("Date Math (%s) is used incorrectly.\n", date_math.c_str());
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
	int num = strtoint(std::string(op.c_str() + 1, op.size())), max_days;
	time_t dateGMT;
	struct tm *timeinfo;
	if (op.at(0) == '+' || op.at(0) == '-') {
		switch (units.at(0)) {
			case 'y':
				(op.at(0) == '+') ? tm.year += num : tm.year -= num;
				break;
			case 'M':
				(op.at(0) == '+') ? tm.mon += num : tm.mon -= num;
				normalizeMonths(tm.year, tm.mon);
				max_days = getDays_month(tm.year, tm.mon);
				if (tm.day > max_days) tm.day = max_days;
				break;
			case 'w':
				(op.at(0) == '+') ? tm.day += 7 * num : tm.day -= 7 * num; break;
			case 'd':
				(op.at(0) == '+') ? tm.day += num : tm.day -= num; break;
			case 'h':
				(op.at(0) == '+') ? tm.hour += num : tm.hour -= num; break;
			case 'm':
				(op.at(0) == '+') ? tm.min += num : tm.min -= num; break;
			case 's':
				(op.at(0) == '+') ? tm.sec += num : tm.sec -= num; break;
		}
	} else {
		switch (units.at(0)) {
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
long double
Datetime::mtimegm(tm_t &tm)
{
	normalizeMonths(tm.year, tm.mon);
	long double result = (long double)toordinal(tm.year, tm.mon, 1) - _EPOCH_ORD + tm.day - 1;
	result *= 24;
	result += tm.hour;
	result *= 60;
	result += tm.min;
	result *= 60;
	result += tm.sec;
	(result < 0) ? result -= tm.msec / 1000.0L : result += tm.msec / 1000.0L;

	return result;
}


/*
 * Return the timestamp of date.
 */
long double
Datetime::timestamp(const std::string &date)
{
	if (!isNumeric(date)) {
		tm_t tm;
		dateTimeParser(date, tm);
		return mtimegm(tm);
	} else {
		long double timestamp;
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
		LOG_ERR(NULL, "ERROR: Year is out of range.\n");
		return false;
	}

	try {
		if (day < 1 || day > getDays_month(year, month)) {
			LOG_ERR(NULL, "ERROR: Day is out of range for month.\n");
			return false;
		}
	} catch (const std::exception &ex) {
		LOG_ERR(NULL, "ERROR: %s.\n", ex.what());
		return false;
	}

	return true;
}


/*
 * Return a string with the date in ISO 8601 Format.
 */
char*
Datetime::isotime(const struct tm *tm)
{
	static char result[20];
	sprintf(result, "%02.4d-%02.2d-%02.2dT%02.2d:%02.2d:%02.2d",
		_START_YEAR + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return result;
}


/*
 * Transform a epoch string to ISO 8601 format if epoch is numeric,
 * the decimal part of epoch are milliseconds.
 * If epoch does not numeric, return epoch.
 */
::std::string
Datetime::ctime(const ::std::string &epoch)
{
	if (isNumeric(epoch)) {
		double mtimestamp = strtodouble(epoch);
		time_t timestamp = (time_t) mtimestamp;
		std::string milliseconds = epoch;
		milliseconds.assign(milliseconds.c_str() + milliseconds.find("."), 4);
		struct tm *timeinfo = gmtime(&timestamp);
		return isotime(timeinfo);
	} else {
		return epoch;
	}
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