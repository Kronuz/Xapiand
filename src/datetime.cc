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

#include "datetime.h"

#include <cctype>     // for std::isdigit
#include <cmath>      // for ceil
#include <exception>  // for exception
#include <stdexcept>  // for invalid_argument, out_of_range
#include <stdio.h>    // for snprintf

#include "log.h"      // for L_ERR, Log
#include "msgpack.h"  // for MsgPack
#include "utils.h"    // for stox


const std::regex Datetime::date_re("([0-9]{4})([-/ ]?)(0[1-9]|1[0-2])\\2(0[0-9]|[12][0-9]|3[01])([T ]?([01]?[0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])([.,]([0-9]+))?)?([ ]*[+-]([01]?[0-9]|2[0-3]):([0-5][0-9])|Z)?)?([ ]*\\|\\|[ ]*([+-/\\dyMwdhms]+))?", std::regex::optimize);
const std::regex Datetime::date_math_re("([+-]\\d+|\\/{1,2})([dyMwhms])", std::regex::optimize);


static constexpr int days[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};


static constexpr int cumdays[2][12] = {
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};


static void process_date_year(Datetime::tm_t& tm, const MsgPack& year) {
	switch (year.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.year = year.as_u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.year = year.as_i64();
			return;
		default:
			THROW(DatetimeError, "_year must be a positive integer value");
	}
}


static void process_date_month(Datetime::tm_t& tm, const MsgPack& month) {
	switch (month.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.mon = month.as_u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.mon = month.as_i64();
			return;
		default:
			THROW(DatetimeError, "_month must be a positive integer value");
	}
}


static void process_date_day(Datetime::tm_t& tm, const MsgPack& day) {
	switch (day.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.day = day.as_u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.day = day.as_i64();
			return;
		default:
			THROW(DatetimeError, "_day must be a positive integer value");
	}
}


static void process_date_time(Datetime::tm_t& tm, const MsgPack& time) {
	try {
		auto str_time = time.as_string();
		auto length = str_time.length();
		switch (length) {
			case 8:
				if (str_time[2] == ':' && str_time[5] == ':') {
					tm.hour = stox(std::stoul, str_time.substr(0, 2));
					if (tm.hour < 60) {
						tm.min = stox(std::stoul, str_time.substr(3, 2));
						if (tm.min < 60) {
							tm.sec = stox(std::stoul, str_time.substr(6, 2));
							if (tm.sec < 60) {
								tm.fsec = 0.0;
								return;
							}
						}
					}
				}
				break;
			default:
				if (length > 9 && (str_time[2] == ':' && str_time[5] == ':' && str_time[8] == '.')) {
					tm.hour = stox(std::stoul, str_time.substr(0, 2));
					if (tm.hour < 60) {
						tm.min = stox(std::stoul, str_time.substr(3, 2));
						if (tm.min < 60) {
							tm.sec = stox(std::stoul, str_time.substr(6, 2));
							if (tm.sec < 60) {
								tm.fsec = Datetime::normalize_fsec(stox(std::stod, str_time.substr(8)));
								return;
							}
						}
					}
				}
				break;
		}
		THROW(DatetimeError, "Error format in: %s, the format must be 00:00:00[.0...]", str_time.c_str());
	} catch (const msgpack::type_error&) {
		THROW(DatetimeError, "_time must be string");
	}
}


static const std::unordered_map<std::string, void (*)(Datetime::tm_t&, const MsgPack&)> map_dispatch_date({
	{ "_year",    &process_date_year   },
	{ "_month",   &process_date_month  },
	{ "_day",     &process_date_day    },
	{ "_time",    &process_date_time   },
});


/*
 * Full struct tm according to the date specified by date.
 */
void
Datetime::dateTimeParser(const std::string& date, tm_t& tm)
{
	// Check if date is ISO 8601.
	auto pos = date.find("||");
	if (pos == std::string::npos) {
		auto format = ISO8601(date, tm);
		switch (format) {
			case Format::VALID:
				return;
			case Format::INVALID:
				break;
			case Format::OUT_OF_RANGE:
				THROW(DatetimeError, "Date: %s is out of range", date.c_str());
			default:
				THROW(DatetimeError, "In dateTimeParser, format %s is incorrect", date.c_str());
		}
	} else {
		auto format = ISO8601(date.substr(0, pos), tm);
		switch (format) {
			case Format::VALID:
				return processDateMath(date.substr(pos + 2), tm);
			case Format::INVALID:
				break;
			case Format::OUT_OF_RANGE:
				THROW(DatetimeError, "Date: %s is out of range", date.c_str());
			default:
				THROW(DatetimeError, "In dateTimeParser, format %s is incorrect", date.c_str());
		}
	}

	std::smatch m;
	if (std::regex_match(date, m, date_re) && static_cast<std::size_t>(m.length(0)) == date.length()) {
		tm.year = std::stoi(m.str(1));
		tm.mon = std::stoi(m.str(3));
		tm.day = std::stoi(m.str(4));
		if (!isvalidDate(tm.year, tm.mon, tm.day)) {
			THROW(DatetimeError, "Date: %s is out of range", date.c_str());
		}

		// Process time
		if (m.length(5) == 0) {
			tm.hour = tm.min = tm.sec = 0;
			tm.fsec = 0.0;
		} else {
			tm.hour = std::stoi(m.str(6));
			tm.min = std::stoi(m.str(7));
			if (m.length(8) == 0) {
				tm.sec = 0;
				tm.fsec = 0.0;
			} else {
				tm.sec = std::stoi(m.str(9));
				if (m.length(10) == 0) {
					tm.fsec = 0.0;
				} else {
					auto fs = m.str(11);
					fs.insert(0, 1, '.');
					tm.fsec = normalize_fsec(std::stod(fs));
				}
			}
			if (m.length(12) != 0) {
				computeTimeZone(tm, date[m.position(13) - 1], m.str(13), m.str(14));
			}
		}

		// Process Date Math
		if (m.length(16) != 0) {
			processDateMath(m.str(16), tm);
		}
	} else {
		THROW(DatetimeError, "In dateTimeParser, format %s is incorrect", date.c_str());
	}
}


/*
 * Full struct tm according to the date in ISO 8601 format.
 */
Datetime::Format
Datetime::ISO8601(const std::string& date, tm_t& tm)
{
	auto length = date.length();
	try {
		switch (length) {
			case 10: // 0000-00-00
				if (date[4] == '-' && date[7] == '-') {
					tm.year  = stox(std::stoul, date.substr(0, 4));
					tm.mon   = stox(std::stoul, date.substr(5, 2));
					tm.day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(tm.year, tm.mon, tm.day)) {
						tm.hour = 0;
						tm.min  = 0;
						tm.sec  = 0;
						tm.fsec = 0.0;
						return Format::VALID;
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			case 19:
				// 0000-00-00[T ]00:00:00
				if (date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') && date[13] == ':' && date[16] == ':') {
					tm.year  = stox(std::stoul, date.substr(0, 4));
					tm.mon   = stox(std::stoul, date.substr(5, 2));
					tm.day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(tm.year, tm.mon, tm.day)) {
						tm.hour = stox(std::stoul, date.substr(11, 2));
						if (tm.hour < 60) {
							tm.min = stox(std::stoul, date.substr(14, 2));
							if (tm.min < 60) {
								tm.sec = stox(std::stoul, date.substr(17, 2));
								if (tm.sec < 60) {
									tm.fsec = 0.0;
									return Format::VALID;
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			case 20:
				// 0000-00-00[T ]00:00:00Z
				if (date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') && date[13] == ':' &&
					date[16] == ':' && date[19] == 'Z') {
					tm.year  = stox(std::stoul, date.substr(0, 4));
					tm.mon   = stox(std::stoul, date.substr(5, 2));
					tm.day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(tm.year, tm.mon, tm.day)) {
						tm.hour = stox(std::stoul, date.substr(11, 2));
						if (tm.hour < 60) {
							tm.min = stox(std::stoul, date.substr(14, 2));
							if (tm.min < 60) {
								tm.sec = stox(std::stoul, date.substr(17, 2));
								if (tm.sec < 60) {
									tm.fsec = 0.0;
									return Format::VALID;
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			default:
				if (length > 20 && date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') &&
					date[13] == ':' && date[16] == ':') {
					tm.year  = stox(std::stoul, date.substr(0, 4));
					tm.mon   = stox(std::stoul, date.substr(5, 2));
					tm.day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(tm.year, tm.mon, tm.day)) {
						tm.hour = stox(std::stoul, date.substr(11, 2));
						if (tm.hour < 60) {
							tm.min = stox(std::stoul, date.substr(14, 2));
							if (tm.min < 60) {
								tm.sec = stox(std::stoul, date.substr(17, 2));
								if (tm.sec < 60) {
									switch (date[19]) {
										case '+':
										case '-':
											if (length == 25 && date[22] == ':') {
												tm.fsec = 0.0;
												auto tz_h = date.substr(20, 2);
												auto tz_m = date.substr(23, 2);
												if (stox(std::stoul, tz_h) < 60 && stox(std::stoul, tz_m) < 60) {
													computeTimeZone(tm, date[19], tz_h, tz_m);
													return Format::VALID;
												}
												return Format::OUT_OF_RANGE;
											}
											return Format::ERROR;
										case '.': {
											auto it = date.begin() + 19;
											const auto it_e = date.end();
											for (auto aux = it + 1; aux != it_e; ++aux) {
												const auto& c = *aux;
												if (c < '0' || c > '9') {
													switch (c) {
														case 'Z':
															if ((aux + 1) == it_e) {
																tm.fsec = normalize_fsec(std::stod(std::string(it, aux)));
																return Format::VALID;
															}
															return Format::ERROR;
														case '+':
														case '-':
															if ((it_e - aux) == 6) {
																auto aux_end = aux + 3;
																if (*aux_end == ':') {
																	auto tz_h = std::string(aux + 1, aux_end);
																	auto tz_m = std::string(aux_end + 1, it_e);
																	if (stox(std::stoul, tz_h) < 60 && stox(std::stoul, tz_m) < 60) {
																		computeTimeZone(tm, c, tz_h, tz_m);
																		tm.fsec = normalize_fsec(std::stod(std::string(it, aux)));
																		return Format::VALID;
																	}
																	return Format::OUT_OF_RANGE;
																}
															}
															return Format::ERROR;
														default:
															return Format::INVALID;
													}
												}
											}
											tm.fsec = normalize_fsec(std::stod(std::string(it, it_e)));
											return Format::VALID;
										}
										default:
											return Format::INVALID;
									}
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
		}
	} catch (const OutOfRange&) {
		return Format::ERROR;
	} catch (const InvalidArgument&) {
		return Format::ERROR;
	}
}


Datetime::Format
Datetime::ISO8601(const std::string& date)
{
	auto length = date.length();
	try {
		switch (length) {
			case 10: // 0000-00-00
				if (date[4] == '-' && date[7] == '-') {
					auto year  = stox(std::stoul, date.substr(0, 4));
					auto mon   = stox(std::stoul, date.substr(5, 2));
					auto day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(year, mon, day)) {
						return Format::VALID;
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			case 19: // 0000-00-00[T ]00:00:00
				if (date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') && date[13] == ':' && date[16] == ':') {
					auto year  = stox(std::stoul, date.substr(0, 4));
					auto mon   = stox(std::stoul, date.substr(5, 2));
					auto day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(year, mon, day)) {
						auto hour = stox(std::stoul, date.substr(11, 2));
						if (hour < 60) {
							auto min = stox(std::stoul, date.substr(14, 2));
							if (min < 60) {
								auto sec = stox(std::stoul, date.substr(17, 2));
								if (sec < 60) {
									return Format::VALID;
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			case 20:
				// 0000-00-00[T ]00:00:00Z
				if (date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') && date[13] == ':' &&
					date[16] == ':' && date[19] == 'Z') {
					auto year  = stox(std::stoul, date.substr(0, 4));
					auto mon   = stox(std::stoul, date.substr(5, 2));
					auto day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(year, mon, day)) {
						auto hour = stox(std::stoul, date.substr(11, 2));
						if (hour < 60) {
							auto min = stox(std::stoul, date.substr(14, 2));
							if (min < 60) {
								auto sec = stox(std::stoul, date.substr(17, 2));
								if (sec < 60) {
									return Format::VALID;
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
			default:
				if (length > 20 && date[4] == '-' && date[7] == '-' && (date[10] == 'T' || date[10] == ' ') &&
					date[13] == ':' && date[16] == ':') {
					auto year  = stox(std::stoul, date.substr(0, 4));
					auto mon   = stox(std::stoul, date.substr(5, 2));
					auto day   = stox(std::stoul, date.substr(8, 2));
					if (isvalidDate(year, mon, day)) {
						auto hour = stox(std::stoul, date.substr(11, 2));
						if (hour < 60) {
							auto min = stox(std::stoul, date.substr(14, 2));
							if (min < 60) {
								auto sec = stox(std::stoul, date.substr(17, 2));
								if (sec < 60) {
									switch (date[19]) {
										case '+':
										case '-':
											if (length == 25 && date[22] == ':') {
												auto tz_h = date.substr(20, 2);
												auto tz_m = date.substr(23, 2);
												if (stox(std::stoul, tz_h) < 60 && stox(std::stoul, tz_m) < 60) {
													return Format::VALID;
												}
												return Format::OUT_OF_RANGE;
											}
											return Format::ERROR;
										case '.': {
											auto it = date.begin() + 19;
											const auto it_e = date.end();
											for (auto aux = it + 1; aux != it_e; ++aux) {
												const auto& c = *aux;
												if (c < '0' || c > '9') {
													switch (c) {
														case 'Z':
															if ((aux + 1) == it_e) {
																return Format::VALID;
															}
															return Format::ERROR;
														case '+':
														case '-':
															if ((it_e - aux) == 6) {
																auto aux_end = aux + 3;
																if (*aux_end == ':') {
																	auto tz_h = std::string(aux + 1, aux_end);
																	auto tz_m = std::string(aux_end + 1, it_e);
																	if (stox(std::stoul, tz_h) < 60 && stox(std::stoul, tz_m) < 60) {
																		return Format::VALID;
																	}
																	return Format::OUT_OF_RANGE;
																}
															}
															return Format::ERROR;
														default:
															return Format::INVALID;
													}
												}
											}
											return Format::VALID;
										}
										default:
											return Format::INVALID;
									}
								}
							}
						}
					}
					return Format::OUT_OF_RANGE;
				}
				return Format::INVALID;
		}
	} catch (const OutOfRange&) {
		return Format::ERROR;
	} catch (const InvalidArgument&) {
		return Format::ERROR;
	}
}


void
Datetime::processDateMath(const std::string& date_math, tm_t& tm)
{
	size_t size_match = 0;
	std::sregex_iterator next(date_math.begin(), date_math.end(), date_math_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		size_match += next->length(0);
		computeDateMath(tm, next->str(1), next->str(2)[0]);
		++next;
	}

	if (date_math.length() != size_match) {
		THROW(DatetimeError, "Date Math (%s) is used incorrectly", date_math.c_str());
	}
}


void
Datetime::computeTimeZone(tm_t& tm, char op, const std::string& hour, const std::string& min)
{
	std::string oph, opm;
	oph.reserve(3);
	opm.reserve(3);
	if (op == '+') {
		oph.push_back('-');
		oph.append(hour);
		opm.push_back('-');
		opm.append(min);
	} else {
		oph.push_back('+');
		oph.append(hour);
		opm.push_back('+');
		opm.append(min);
	}
	computeDateMath(tm, oph, 'h');
	computeDateMath(tm, opm, 'm');
}


/*
 * Compute a Date Math former by op + units.
 * op can be +#, -#, /, //
 * unit can be y, M, w, d, h, m, s
 */
void
Datetime::computeDateMath(tm_t& tm, const std::string& op, char unit)
{
	switch (op[0]) {
		case '+': {
			auto num = std::stoi(std::string(op.c_str() + 1, op.length()));
			switch (unit) {
				case 'y':
					tm.year += num; break;
				case 'M': {
					tm.mon += num;
					normalizeMonths(tm.year, tm.mon);
					auto max_days = getDays_month(tm.year, tm.mon);
					if (tm.day > max_days) tm.day = max_days;
					break;
				}
				case 'w':
					tm.day += 7 * num; break;
				case 'd':
					tm.day += num; break;
				case 'h':
					tm.hour += num; break;
				case 'm':
					tm.min += num; break;
				case 's':
					tm.sec += num; break;
				default:
					THROW(DatetimeError, "Invalid format in Date Math unit: '%c'. Unit must be in { y, M, w, d, h, m, s }", unit);
			}
			break;
		}
		case '-': {
			auto num = std::stoi(std::string(op.c_str() + 1, op.length()));
			switch (unit) {
				case 'y':
					tm.year -= num;
					break;
				case 'M': {
					tm.mon -= num;
					normalizeMonths(tm.year, tm.mon);
					auto max_days = getDays_month(tm.year, tm.mon);
					if (tm.day > max_days) tm.day = max_days;
					break;
				}
				case 'w':
					tm.day -= 7 * num; break;
				case 'd':
					tm.day -= num; break;
				case 'h':
					tm.hour -= num; break;
				case 'm':
					tm.min -= num; break;
				case 's':
					tm.sec -= num; break;
				default:
					THROW(DatetimeError, "Invalid format in Date Math unit: '%c'. Unit must be in { y, M, w, d, h, m, s }", unit);
			}
			break;
		}
		case '/':
			switch (unit) {
				case 'y':
					if (op.length() == 1) {
						tm.mon = 12;
						tm.day = getDays_month(tm.year, 12);
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.mon = tm.day = 1;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				case 'M':
					if (op.length() == 1) {
						tm.day = getDays_month(tm.year, tm.mon);
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.day = 1;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				case 'w': {
					auto dateGMT = timegm(tm);
					struct tm timeinfo;
					gmtime_r(&dateGMT, &timeinfo);
					if (op.length() == 1) {
						tm.day += 6 - timeinfo.tm_wday;
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.day -= timeinfo.tm_wday;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				}
				case 'd':
					if (op.length() == 1) {
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				case 'h':
					if (op.length() == 1) {
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				case 'm':
					if (op.length() == 1) {
						tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
					break;
				case 's':
					if (op.length() == 1) {
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.length() == 2 && op[1] == '/') {
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
					}
				break;
			}
			break;
		default:
			THROW(DatetimeError, "Invalid format in Date Math operator: %s. Operator must be in { +#, -#, /, // }", op.c_str());
	}

	// Update date.
	auto dateGMT = timegm(tm);
	struct tm timeinfo;
	gmtime_r(&dateGMT, &timeinfo);
	tm.year = timeinfo.tm_year + DATETIME_START_YEAR;
	tm.mon = timeinfo.tm_mon + 1;
	tm.day = timeinfo.tm_mday;
	tm.hour = timeinfo.tm_hour;
	tm.min = timeinfo.tm_min;
	tm.sec = timeinfo.tm_sec;
}


/*
 * Returns if a year is leap.
 */
bool
Datetime::isleapYear(int year)
{
	return year % 400 == 0 || (year % 4 == 0 && year % 100);
}


/*
 * Returns if a tm_year is leap.
 */
bool
Datetime::isleapRef_year(int tm_year)
{
	tm_year += DATETIME_START_YEAR;
	return isleapYear(tm_year);
}


/*
 * Returns number of days in month, given the year.
 */
int
Datetime::getDays_month(int year, int month)
{
	if (month < 1 || month > 12) THROW(DatetimeError, "Month must be in 1..12");

	int leap = isleapYear(year);
	return days[leap][month - 1];
}


/*
 * Returns the proleptic Gregorian ordinal of the date,
 * where January 1 of year 1 has ordinal 1 (reference date).
 * year -> Any positive number except zero.
 * month -> Between 1 and 12 inclusive.
 * day -> Between 1 and the number of days in the given month of the given year.
 */
std::time_t
Datetime::toordinal(int year, int month, int day)
{
	if (year < 1) THROW(DatetimeError, "Year is out of range");
	if (day < 1 || day > getDays_month(year, month)) THROW(DatetimeError, "Day is out of range for month");

	int leap = isleapYear(year);
	std::time_t result = 365 * (year - 1) + (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400 + cumdays[leap][month - 1] + day;
	return result;
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 */
std::time_t
Datetime::timegm(const std::tm& tm)
{
	int year = tm.tm_year + DATETIME_START_YEAR, mon = tm.tm_mon + 1;
	normalizeMonths(year, mon);
	auto result = toordinal(year, mon, 1) - DATETIME_EPOCH_ORD + tm.tm_mday - 1;
	result *= 24;
	result += tm.tm_hour;
	result *= 60;
	result += tm.tm_min;
	result *= 60;
	result += tm.tm_sec;
	return result;
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 */
std::time_t
Datetime::timegm(tm_t& tm)
{
	normalizeMonths(tm.year, tm.mon);
	auto result = toordinal(tm.year, tm.mon, 1) - DATETIME_EPOCH_ORD + tm.day - 1;
	result *= 24;
	result += tm.hour;
	result *= 60;
	result += tm.min;
	result *= 60;
	result += tm.sec;
	return result;
}


/*
 * Transforms timestamp to a struct tm_t.
 */
Datetime::tm_t
Datetime::to_tm_t(std::time_t timestamp)
{
	struct tm timeinfo;
	gmtime_r(&timestamp, &timeinfo);
	return tm_t(
		timeinfo.tm_year + DATETIME_START_YEAR, timeinfo.tm_mon + 1,
		timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
		timeinfo.tm_sec
	);
}


/*
 * Transforms timestamp to a struct tm_t.
 */
Datetime::tm_t
Datetime::to_tm_t(double timestamp)
{
	auto _time = static_cast<std::time_t>(timestamp);
	struct tm timeinfo;
	gmtime_r(&_time, &timeinfo);
	return tm_t(
		timeinfo.tm_year + DATETIME_START_YEAR, timeinfo.tm_mon + 1,
		timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
		timeinfo.tm_sec, normalize_fsec(timestamp - _time)
	);
}


/*
 * Transforms date to a struct tm_t.
 */
Datetime::tm_t
Datetime::to_tm_t(const std::string& date)
{
	try {
		auto timestamp = stox(std::stod, date);
		return to_tm_t(timestamp);
	} catch (const std::invalid_argument&) {
		tm_t tm;
		dateTimeParser(date, tm);
		return tm;
	} catch (const std::out_of_range&) {
		THROW(DatetimeError, "%s is very large", date.c_str());
	}
}


Datetime::tm_t
Datetime::to_tm_t(const MsgPack& value)
{
	Datetime::tm_t tm;
	double _timestamp;
	switch (value.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			_timestamp = value.as_u64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::NEGATIVE_INTEGER:
			_timestamp = value.as_i64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::FLOAT:
			_timestamp = value.as_f64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::STR:
			Datetime::timestamp(value.as_string(), tm);
			return tm;
		case MsgPack::Type::MAP:
			for (const auto& key : value) {
				auto str_key = key.as_string();
				try {
					auto func = map_dispatch_date.at(str_key);
					(*func)(tm, value.at(str_key));
				} catch (const std::out_of_range&) {
					THROW(DatetimeError, "Unsupported Key: %s in date", str_key.c_str());
				}
			}
			if (Datetime::isvalidDate(tm.year, tm.mon, tm.day)) {
				return tm;
			}
			THROW(DatetimeError, "Date is out of range");
		default:
			THROW(DatetimeError, "Date value must be numeric or string");
	}
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 * Returns Timestamp with milliseconds as the decimal part.
 */
double
Datetime::timestamp(tm_t& tm)
{
	normalizeMonths(tm.year, tm.mon);
	auto result = static_cast<double>(toordinal(tm.year, tm.mon, 1) - DATETIME_EPOCH_ORD + tm.day - 1);
	result *= 24;
	result += tm.hour;
	result *= 60;
	result += tm.min;
	result *= 60;
	result += tm.sec;
	result < 0 ? result -= tm.fsec : result += tm.fsec;
	return result;
}


/*
 * Returns the timestamp of date.
 */
double
Datetime::timestamp(const std::string& date)
{
	tm_t tm;
	dateTimeParser(date, tm);
	return timestamp(tm);
}


/*
 * Returns the timestamp of date and fill tm.
 */
double
Datetime::timestamp(const std::string& date, tm_t& tm)
{
	dateTimeParser(date, tm);
	return timestamp(tm);
}


double
Datetime::timestamp(const MsgPack& value)
{
	Datetime::tm_t tm = Datetime::to_tm_t(value);
	return Datetime::timestamp(tm);
}


double
Datetime::timestamp(const MsgPack& value, Datetime::tm_t& tm)
{
	tm = Datetime::to_tm_t(value);
	return Datetime::timestamp(tm);
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
	} catch (const std::exception& ex) {
		L_ERR(nullptr, "ERROR: %s.", *ex.what() ? ex.what() : "Unkown exception!");
		return false;
	}

	return true;
}


/*
 * Return a string with the date in ISO 8601 Format.
 */
std::string
Datetime::isotime(const std::tm& tm)
{
	char result[20];
	snprintf(result, 20, "%2.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d",
		tm.tm_year + DATETIME_START_YEAR, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	return std::string(result);
}


/*
 * Return a string with the date in ISO 8601 Format.
 */
std::string
Datetime::isotime(const tm_t& tm)
{
	if (tm.fsec > 0.0) {
		char result[28];
		snprintf(result, 28, "%2.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d%.6f",
			tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec, tm.fsec);
		std::string res(result);
		auto it = res.erase(res.begin() + 19) + 1;
		for (auto it_last = res.end() - 1; it_last != it && *it_last == '0'; --it_last) {
			it_last = res.erase(it_last);
		}
		return res;
	} else {
		char result[20];
		snprintf(result, 20, "%2.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d",
			tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
		return std::string(result);
	}
}


/*
 * Transforms a timestamp in seconds with decimal fraction to ISO 8601 format.
 */
std::string
Datetime::isotime(double timestamp)
{
	auto tm = to_tm_t(timestamp);
	return isotime(tm);
}


/*
 * Normalize months between -11 and 11
 */
void
Datetime::normalizeMonths(int& year, int& mon)
{
	if (mon > 12) {
		year += mon / 12;
		mon %= 12;
	} else while (mon < 1) {
		mon += 12;
		year--;
	}
}


bool
Datetime::isDate(const std::string& date)
{
	auto format = ISO8601(date);
	switch (format) {
		case Format::VALID:
			return true;
		case Format::INVALID: {
			std::smatch m;
			return std::regex_match(date, m, date_re) && static_cast<std::size_t>(m.length(0)) == date.length();
		}
		default:
			return false;
	}
}


bool
Datetime::isTime(const std::string& _time) {
	auto length = _time.length();
	switch (length) {
		case 5: // 00:00
			return std::isdigit(_time[0]) && std::isdigit(_time[1]) && _time[2] == ':' &&
				std::isdigit(_time[3]) && std::isdigit(_time[4]);
		case 8: // 00:00:00
			return std::isdigit(_time[0]) && std::isdigit(_time[1]) && _time[2] == ':' &&
				std::isdigit(_time[3]) && std::isdigit(_time[4]) && _time[5] == ':' &&
				std::isdigit(_time[6]) && std::isdigit(_time[7]);
		default: //  00:00:00[+-]00:00  00:00:00.000...  00:00:00.000...[+-]00:00
			if (length > 9 && std::isdigit(_time[0]) && std::isdigit(_time[1]) && _time[2] == ':' &&
				std::isdigit(_time[3]) && std::isdigit(_time[4]) && _time[5] == ':' &&
				std::isdigit(_time[6]) && std::isdigit(_time[7])) {
				switch (_time[8]) {
					case '+':
					case '-':
						return length == 14 && std::isdigit(_time[9]) && std::isdigit(_time[10]) && _time[11] == ':' &&
							std::isdigit(_time[12]) && std::isdigit(_time[13]);
					case '.':
						for (size_t i = 9; i < length; ++i) {
							const auto c = _time[i];
							if (!std::isdigit(c)) {
								return (c == '+' || c == '-') && length == (i + 6) && std::isdigit(_time[i + 1]) &&
									std::isdigit(_time[i + 2]) && _time[i + 3] == ':' && std::isdigit(_time[i + 4]) &&
									std::isdigit(_time[i + 5]);
							}
						}
						return true;
				}
			}
			return false;
	}
}


bool
Datetime::isTimeDelta(const std::string& timedelta)
{
	auto length = timedelta.length();
	switch (length) {
		case 6: // [+-]00:00
			return (timedelta[0] == '+' || timedelta[0] == '-') && std::isdigit(timedelta[1]) && std::isdigit(timedelta[2]) &&
				timedelta[3] == ':' && std::isdigit(timedelta[4]) && std::isdigit(timedelta[5]);
		case 9: // [+-]00:00:00
			return (timedelta[0] == '+' || timedelta[0] == '-') && std::isdigit(timedelta[1]) && std::isdigit(timedelta[2]) &&
				timedelta[3] == ':' && std::isdigit(timedelta[4]) && std::isdigit(timedelta[5]) &&
				timedelta[6] == ':' && std::isdigit(timedelta[7]) && std::isdigit(timedelta[8]);
		default: // [+-]00:00:00  [+-]00:00:00.000...
			if (length > 10 && (timedelta[0] == '+' || timedelta[0] == '-') && std::isdigit(timedelta[1]) &&
				std::isdigit(timedelta[2]) && timedelta[3] == ':' && std::isdigit(timedelta[4]) &&
				std::isdigit(timedelta[5]) && timedelta[6] == ':' && std::isdigit(timedelta[7]) &&
				std::isdigit(timedelta[8]) && timedelta[9] == '.') {
				for (size_t i = 10; i < length; ++i) {
					if (!std::isdigit(timedelta[i])) {
						return false;
					}
				}
				return true;
			}
			return false;
	}
}


std::string
Datetime::to_string(const std::chrono::time_point<std::chrono::system_clock>& tp)
{
	return isotime(std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() * DATETIME_MICROSECONDS);
}


/*
 * Normalize date in ISO 8601 format.
 */
std::string
Datetime::normalizeISO8601(const std::string& iso_date)
{
	tm_t tm;
	ISO8601(iso_date, tm);
	return isotime(tm);
}
