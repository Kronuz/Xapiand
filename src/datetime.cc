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

#include "datetime.h"

#include <cassert>                                // for assert
#include <cctype>                                 // for std::isdigit
#include <cmath>                                  // for ceil
#include <exception>                              // for exception
#include <stdexcept>                              // for invalid_argument, out_of_range
#include <string_view>                            // for std::string_view

#include "hashes.hh"                              // for fnv1ah32
#include "log.h"                                  // for L_ERR
#include "msgpack.h"                              // for MsgPack
#include "phf.hh"                                 // for phf
#include "repr.hh"                                // for repr
#include "reserved/types.h"                       // for RESERVED_TIME
#include "reserved/datetime.h"                    // for RESERVED_
#include "strict_stox.hh"                         // for strict_stoull
#include "strings.hh"                             // for strings::format


const std::regex Datetime::date_re(R"(([0-9]{4})([-/ ]?)(0[1-9]|1[0-2])\2(0[0-9]|[12][0-9]|3[01])([T ]?([01]?[0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])([.,]([0-9]+))?)?([ ]*[+-]([01]?[0-9]|2[0-3]):([0-5][0-9])|Z)?)?([ ]*\|\|[ ]*([+-/\dyMwdhms]+))?)", std::regex::optimize);
const std::regex Datetime::date_math_re("([+-]\\d+|\\/{1,2})([dyMwhms])", std::regex::optimize);


static constexpr int days[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};


static constexpr int cumdays[2][12] = {
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};


static inline void process_date_year(Datetime::tm_t& tm, const MsgPack& year) {
	switch (year.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.year = year.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.year = year.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_YEAR);
	}
}


static inline void process_date_month(Datetime::tm_t& tm, const MsgPack& month) {
	switch (month.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.mon = month.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.mon = month.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_MONTH);
	}
}


static inline void process_date_day(Datetime::tm_t& tm, const MsgPack& day) {
	switch (day.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.day = day.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.day = day.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_DAY);
	}
}


static inline void process_date_hour(Datetime::tm_t& tm, const MsgPack& hour) {
	switch (hour.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.hour = hour.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.hour = hour.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_HOUR);
	}
}


static inline void process_date_min(Datetime::tm_t& tm, const MsgPack& min) {
	switch (min.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.min = min.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.min = min.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_MIN);
	}
}


static inline void process_date_sec(Datetime::tm_t& tm, const MsgPack& sec) {
	switch (sec.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.sec = sec.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.sec = sec.i64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a positive integer value", RESERVED_SEC);
	}
}


static inline void process_date_fsec(Datetime::tm_t& tm, const MsgPack& fsec) {
	switch (fsec.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.fsec = fsec.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.fsec = fsec.i64();
			return;
		case MsgPack::Type::FLOAT:
			tm.fsec = fsec.f64();
			return;
		default:
			THROW(DatetimeError, "'{}' must be a numeric value", RESERVED_FSEC);
	}
}


static inline void process_date_time(Datetime::tm_t& tm, std::string_view str_time) {
	int errno_save;
	auto size = str_time.size();
	switch (size) {
		case 5: // 00:00
			if (str_time[2] == ':') {
				tm.hour = strict_stoul(&errno_save, str_time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				if (tm.hour < 24) {
					tm.min = strict_stoul(&errno_save, str_time.substr(3, 2));
					if (errno_save != 0) { goto error; }
					if (tm.min < 60) {
						tm.sec = 0;
						tm.fsec = 0.0;
						return;
					}
				}
				goto error_out_of_range;
			}
			break;
		case 8: // 00:00:00
			if (str_time[2] == ':' && str_time[5] == ':') {
				tm.hour = strict_stoul(&errno_save, str_time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				if (tm.hour < 24) {
					tm.min = strict_stoul(&errno_save, str_time.substr(3, 2));
					if (errno_save != 0) { goto error; }
					if (tm.min < 60) {
						tm.sec = strict_stoul(&errno_save, str_time.substr(6, 2));
						if (errno_save != 0) { goto error; }
						if (tm.sec < 60) {
							tm.fsec = 0.0;
							return;
						}
					}
				}
				goto error_out_of_range;
			}
			break;
		default: //  00:00:00[+-]00:00  00:00:00.000...  00:00:00.000...[+-]00:00
			if (size > 9 && (str_time[2] == ':' && str_time[5] == ':')) {
				tm.hour = strict_stoul(&errno_save, str_time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				if (tm.hour < 24) {
					tm.min = strict_stoul(&errno_save, str_time.substr(3, 2));
					if (errno_save != 0) { goto error; }
					if (tm.min < 60) {
						tm.sec = strict_stoul(&errno_save, str_time.substr(6, 2));
						if (errno_save != 0) { goto error; }
						if (tm.sec < 60) {
							switch (str_time[8]) {
								case '+':
								case '-':
									if (size == 14 && str_time[11] == ':') {
										tm.fsec = 0.0;
										auto tz_h = str_time.substr(9, 2);
										auto h = strict_stoul(&errno_save, tz_h);
										if (errno_save != 0) { goto error; }
										if (h < 24) {
											auto tz_m = str_time.substr(12, 2);
											auto m = strict_stoul(&errno_save, tz_m);
											if (errno_save != 0) { goto error; }
											if (m < 60) {
												computeTimeZone(tm, str_time[8], tz_h, tz_m);
												return;
											}
										}
										goto error_out_of_range;
									}
									goto error;
								case '.': {
									auto it = str_time.begin() + 8;
									const auto it_e = str_time.end();
									for (auto aux = it + 1; aux != it_e; ++aux) {
										const auto& c = *aux;
										if (c < '0' || c > '9') {
											if (c == '+' || c == '-') {
												if ((it_e - aux) == 6) {
													auto aux_end = aux + 3;
													if (*aux_end == ':') {
														auto tz_h = std::string_view(aux + 1, aux_end - aux - 1);
														auto h = strict_stoul(&errno_save, tz_h);
														if (errno_save != 0) { goto error; }
														if (h < 24) {
															auto tz_m = std::string_view(aux_end + 1, it_e - aux_end - 1);
															auto m = strict_stoul(&errno_save, tz_m);
															if (errno_save != 0) { goto error; }
															if (m < 60) {
																computeTimeZone(tm, c, tz_h, tz_m);
																auto fsec = strict_stod(&errno_save, std::string_view(it, aux - it));
																if (errno_save != 0) { goto error; }
																tm.fsec = Datetime::normalize_fsec(fsec);
																return;
															}
														}
														goto error_out_of_range;
													}
												}
											}
											goto error;
										}
									}
									auto fsec = strict_stod(&errno_save, std::string_view(it, it_e - it));
									if (errno_save != 0) { goto error; }
									tm.fsec = Datetime::normalize_fsec(fsec);
									return;
								}
								default:
									break;
							}
						}
					}
				}
			}
			break;
	}

error:
	THROW(DatetimeError, "Error format in _time: {}, the format must be '00:00(:00(.0...)([+-]00:00))'", str_time);

error_out_of_range:
	THROW(DatetimeError, "Time: {} is out of range", str_time);
}


static inline void process_date_datetime(Datetime::tm_t& tm, std::string_view str_datetime) {
	std::cmatch m;

	// Check if datetime is ISO 8601.
	auto pos = str_datetime.find("||");
	if (pos == std::string_view::npos) {
		auto format = Iso8601Parser(str_datetime, tm);
		switch (format) {
			case Datetime::Format::VALID:
				return;
			case Datetime::Format::INVALID:
				break;
			case Datetime::Format::OUT_OF_RANGE:
				goto error_out_of_range;
			default:
				goto error;
		}
	} else {
		auto format = Iso8601Parser(str_datetime.substr(0, pos), tm);
		switch (format) {
			case Datetime::Format::VALID:
				processDateMath(str_datetime.substr(pos + 2), tm);
				return;
			case Datetime::Format::INVALID:
				break;
			case Datetime::Format::OUT_OF_RANGE:
				goto error_out_of_range;
			default:
				goto error;
		}
	}

	int errno_save;
	if (std::regex_match(str_datetime.begin(), str_datetime.end(), m, Datetime::date_re) && static_cast<std::size_t>(m.length(0)) == str_datetime.size()) {
		tm.year = strict_stoi(&errno_save, m.str(1));
		if (errno_save != 0) { goto error; }
		tm.mon = strict_stoi(&errno_save, m.str(3));
		if (errno_save != 0) { goto error; }
		tm.day = strict_stoi(&errno_save, m.str(4));
		if (errno_save != 0) { goto error; }
		if (!Datetime::isValidDate(tm.year, tm.mon, tm.day)) {
			goto error_out_of_range;
		}

		// Process time
		if (m.length(5) == 0) {
			tm.hour = tm.min = tm.sec = 0;
			tm.fsec = 0.0;
		} else {
			tm.hour = strict_stoi(&errno_save, m.str(6));
			if (errno_save != 0) { goto error; }
			tm.min = strict_stoi(&errno_save, m.str(7));
			if (errno_save != 0) { goto error; }
			if (m.length(8) == 0) {
				tm.sec = 0;
				tm.fsec = 0.0;
			} else {
				tm.sec = strict_stoi(&errno_save, m.str(9));
				if (errno_save != 0) { goto error; }
				if (m.length(10) == 0) {
					tm.fsec = 0.0;
				} else {
					auto fs = m.str(11);
					fs.insert(0, 1, '.');
					auto fsec = strict_stod(&errno_save, fs);
					if (errno_save != 0) { goto error; }
					tm.fsec = Datetime::normalize_fsec(fsec);
				}
			}
			if (m.length(12) != 0) {
				computeTimeZone(tm, str_datetime[m.position(13) - 1], m.str(13), m.str(14));
			}
		}

		// Process Datetime Math
		if (m.length(16) != 0) {
			processDateMath(m.str(16), tm);
		}

		return;
	}

error:
	THROW(DatetimeError, "Error format in _datetime: {}", str_datetime);

error_out_of_range:
	THROW(DatetimeError, "Datetime: {} is out of range", str_datetime);
}


static inline void process_date_time(Datetime::tm_t& tm, const MsgPack& time) {
	switch (time.get_type()) {
		case MsgPack::Type::MAP: {
			const auto it_e = time.end();
			for (auto it = time.begin(); it != it_e; ++it) {
				auto str_key = it->str_view();
				auto& it_value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_HOUR),
					hh(RESERVED_MIN),
					hh(RESERVED_MINUTE),
					hh(RESERVED_SEC),
					hh(RESERVED_SECOND),
					hh(RESERVED_FSEC),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_HOUR):
						process_date_hour(tm, it_value);
						break;
					case _.fhh(RESERVED_MIN):
					case _.fhh(RESERVED_MINUTE):
						process_date_min(tm, it_value);
						break;
					case _.fhh(RESERVED_SEC):
					case _.fhh(RESERVED_SECOND):
						process_date_sec(tm, it_value);
						break;
					case _.fhh(RESERVED_FSEC):
						process_date_fsec(tm, it_value);
						break;
					default:
						THROW(DatetimeError, "Unsupported Key: {} in time", repr(str_key));
				}
			}
			return;
		}

		case MsgPack::Type::STR:
			process_date_time(tm, time.str_view());
			return;

		default:
			THROW(DatetimeError, "'{}' must be a map or string value", RESERVED_TIME);
	}
}


static inline void process_date_date(Datetime::tm_t& tm, const MsgPack& date) {
	switch (date.get_type()) {
		case MsgPack::Type::MAP: {
			const auto it_e = date.end();
			for (auto it = date.begin(); it != it_e; ++it) {
				auto str_key = it->str_view();
				auto& it_value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_YEAR),
					hh(RESERVED_MONTH),
					hh(RESERVED_DAY),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_YEAR):
						process_date_year(tm, it_value);
						break;
					case _.fhh(RESERVED_MONTH):
						process_date_month(tm, it_value);
						break;
					case _.fhh(RESERVED_DAY):
						process_date_day(tm, it_value);
						break;
					default:
						THROW(DatetimeError, "Unsupported Key: {} in date", repr(str_key));
				}
			}
			return;
		}

		default:
			THROW(DatetimeError, "'{}' must be a map value", RESERVED_DATE);
	}
}


static inline void process_date_datetime(Datetime::tm_t& tm, const MsgPack& datetime) {
	switch (datetime.get_type()) {
		case MsgPack::Type::MAP: {
			const auto it_e = datetime.end();
			for (auto it = datetime.begin(); it != it_e; ++it) {
				auto str_key = it->str_view();
				auto& it_value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_TIME),
					hh(RESERVED_DATE),
					hh(RESERVED_YEAR),
					hh(RESERVED_MONTH),
					hh(RESERVED_DAY),
					hh(RESERVED_HOUR),
					hh(RESERVED_MIN),
					hh(RESERVED_MINUTE),
					hh(RESERVED_SEC),
					hh(RESERVED_SECOND),
					hh(RESERVED_FSEC),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_TIME):
						process_date_time(tm, it_value);
						break;
					case _.fhh(RESERVED_DATE):
						process_date_date(tm, it_value);
						break;
					case _.fhh(RESERVED_YEAR):
						process_date_year(tm, it_value);
						break;
					case _.fhh(RESERVED_MONTH):
						process_date_month(tm, it_value);
						break;
					case _.fhh(RESERVED_DAY):
						process_date_day(tm, it_value);
						break;
					case _.fhh(RESERVED_HOUR):
						process_date_hour(tm, it_value);
						break;
					case _.fhh(RESERVED_MIN):
					case _.fhh(RESERVED_MINUTE):
						process_date_min(tm, it_value);
						break;
					case _.fhh(RESERVED_SEC):
					case _.fhh(RESERVED_SECOND):
						process_date_sec(tm, it_value);
						break;
					case _.fhh(RESERVED_FSEC):
						process_date_fsec(tm, it_value);
						break;
					default:
						THROW(DatetimeError, "Unsupported Key: {} in datetime", repr(str_key));
				}
			}
			return;
		}

		case MsgPack::Type::STR:
			process_date_time(tm, datetime.str_view());
			return;

		default:
			THROW(DatetimeError, "'{}' must be a map value", RESERVED_DATETIME);
	}
}


/*
 * Returns struct tm according to the datetime specified by datetime.
 */
Datetime::tm_t
Datetime::DatetimeParser(std::string_view datetime)
{
	tm_t tm;
	process_date_datetime(tm, datetime);
	return tm;
}


/*
 * Returnd struct tm according to the datetime specified by value.
 */
Datetime::tm_t
Datetime::DatetimeParser(const MsgPack& value)
{
	L_CALL("Datetime::DatetimeParser({})", value.to_string());

	double _timestamp;
	switch (value.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			_timestamp = value.u64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::NEGATIVE_INTEGER:
			_timestamp = value.i64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::FLOAT:
			_timestamp = value.f64();
			return Datetime::to_tm_t(_timestamp);
		case MsgPack::Type::STR:
			return Datetime::DatetimeParser(value.str_view());
		case MsgPack::Type::MAP: {
			Datetime::tm_t tm;
			const auto it_e = value.end();
			for (auto it = value.begin(); it != it_e; ++it) {
				auto str_key = it->str_view();
				auto& it_value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_TIME),
					hh(RESERVED_DATE),
					hh(RESERVED_DATETIME),

					hh(RESERVED_YEAR),
					hh(RESERVED_MONTH),
					hh(RESERVED_DAY),
					hh(RESERVED_HOUR),
					hh(RESERVED_MIN),
					hh(RESERVED_MINUTE),
					hh(RESERVED_SEC),
					hh(RESERVED_SECOND),
					hh(RESERVED_FSEC),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_TIME):
						process_date_time(tm, it_value);
						break;
					case _.fhh(RESERVED_DATE):
						process_date_date(tm, it_value);
						break;
					case _.fhh(RESERVED_DATETIME):
						process_date_datetime(tm, it_value);
						break;

					case _.fhh(RESERVED_YEAR):
						process_date_year(tm, it_value);
						break;
					case _.fhh(RESERVED_MONTH):
						process_date_month(tm, it_value);
						break;
					case _.fhh(RESERVED_DAY):
						process_date_day(tm, it_value);
						break;
					case _.fhh(RESERVED_HOUR):
						process_date_hour(tm, it_value);
						break;
					case _.fhh(RESERVED_MIN):
					case _.fhh(RESERVED_MINUTE):
						process_date_min(tm, it_value);
						break;
					case _.fhh(RESERVED_SEC):
					case _.fhh(RESERVED_SECOND):
						process_date_sec(tm, it_value);
						break;
					case _.fhh(RESERVED_FSEC):
						process_date_fsec(tm, it_value);
						break;

					default:
						THROW(Error, "Unsupported Datetime: {}", repr(str_key));
				}
			}
			if (!Datetime::isValidDate(tm.year, tm.mon, tm.day)) {
				THROW(DatetimeError, "Datetime is out of range");
			}
			return tm;
		}
		default:
			THROW(DatetimeError, "Datetime value must be numeric or string");
	}
}


/*
 * Full struct tm according to the datetime in ISO 8601 format.
 */
Datetime::Format
Datetime::Iso8601Parser(std::string_view datetime, tm_t& tm)
{
	int errno_save;
	auto size = datetime.size();
	switch (size) {
		case 10: // 0000-00-00
			if (datetime[4] == '-' && datetime[7] == '-') {
				tm.year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				tm.mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				tm.day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(tm.year, tm.mon, tm.day)) {
					tm.hour = 0;
					tm.min  = 0;
					tm.sec  = 0;
					tm.fsec = 0.0;
					return Format::VALID;
				}
				return Format::OUT_OF_RANGE;
			}
			return Format::INVALID;
		case 19: // 0000-00-00[T ]00:00:00
			if (datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') && datetime[13] == ':' && datetime[16] == ':') {
				tm.year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				tm.mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				tm.day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(tm.year, tm.mon, tm.day)) {
					tm.hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (tm.hour < 24) {
						tm.min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (tm.min < 60) {
							tm.sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
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
		case 20: // 0000-00-00[T ]00:00:00Z
			if (datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') && datetime[13] == ':' &&
				datetime[16] == ':' && datetime[19] == 'Z') {
				tm.is_utc = true;
				tm.year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				tm.mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				tm.day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(tm.year, tm.mon, tm.day)) {
					tm.hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (tm.hour < 24) {
						tm.min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (tm.min < 60) {
							tm.sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
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
		default: // 0000-00-00[T ]00:00:00[+-]00:00  0000-00-00[T ]00:00:00.0...  0000-00-00[T ]00:00:00.0...[+-]00:00
			if (size > 20 && datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') &&
				datetime[13] == ':' && datetime[16] == ':') {
				tm.year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				tm.mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				tm.day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(tm.year, tm.mon, tm.day)) {
					tm.hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (tm.hour < 24) {
						tm.min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (tm.min < 60) {
							tm.sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
							if (tm.sec < 60) {
								switch (datetime[19]) {
									case '+':
									case '-':
									    tm.is_utc = true;
										if (size == 25 && datetime[22] == ':') {
											tm.fsec = 0.0;
											auto tz_h = datetime.substr(20, 2);
											auto h = strict_stoul(&errno_save, tz_h);
											if (errno_save != 0) { return Format::ERROR; }
											if (h < 24) {
												auto tz_m = datetime.substr(23, 2);
												auto m = strict_stoul(&errno_save, tz_m);
												if (errno_save != 0) { return Format::ERROR; }
												if (m < 60) {
													computeTimeZone(tm, datetime[19], tz_h, tz_m);
													return Format::VALID;
												}
											}
											return Format::OUT_OF_RANGE;
										}
										return Format::INVALID;
									case '.': {
										auto it = datetime.begin() + 19;
										const auto it_e = datetime.end();
										for (auto aux = it + 1; aux != it_e; ++aux) {
											const auto& c = *aux;
											if (c < '0' || c > '9') {
												switch (c) {
													case 'Z':
														tm.is_utc = true;
														if ((aux + 1) == it_e) {
															auto fsec = strict_stod(&errno_save, std::string_view(it, aux - it));
															if (errno_save != 0) { return Format::ERROR; }
															tm.fsec = normalize_fsec(fsec);
															return Format::VALID;
														}
														return Format::ERROR;
													case '+':
													case '-':
														tm.is_utc = true;
														if ((it_e - aux) == 6) {
															auto aux_end = aux + 3;
															if (*aux_end == ':') {
																auto tz_h = std::string_view(aux + 1, aux_end - aux - 1);
																auto h = strict_stoul(&errno_save, tz_h);
																if (errno_save != 0) { return Format::ERROR; }
																if (h < 24) {
																	auto tz_m = std::string_view(aux_end + 1, it_e - aux_end - 1);
																	auto m = strict_stoul(&errno_save, tz_m);
																	if (errno_save != 0) { return Format::ERROR; }
																	if (m < 60) {
																		auto fsec = strict_stod(&errno_save, std::string_view(it, aux - it));
																		if (errno_save != 0) { return Format::ERROR; }
																		tm.fsec = normalize_fsec(fsec);
																		computeTimeZone(tm, c, tz_h, tz_m);
																		return Format::VALID;
																	}
																}
																return Format::OUT_OF_RANGE;
															}
														}
														return Format::INVALID;
													default:
														return Format::INVALID;
												}
											}
										}
										auto fsec = strict_stod(&errno_save, std::string_view(it, it_e - it));
										if (errno_save != 0) { return Format::ERROR; }
										tm.fsec = normalize_fsec(fsec);
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
}


Datetime::Format
Datetime::Iso8601Parser(std::string_view datetime)
{
	int errno_save;
	auto size = datetime.size();
	switch (size) {
		case 10: // 0000-00-00
			if (datetime[4] == '-' && datetime[7] == '-') {
				auto year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				auto mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				auto day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(year, mon, day)) {
					return Format::VALID;
				}
				return Format::OUT_OF_RANGE;
			}
			return Format::INVALID;
		case 19: // 0000-00-00[T ]00:00:00
			if (datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') && datetime[13] == ':' && datetime[16] == ':') {
				auto year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				auto mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				auto day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(year, mon, day)) {
					auto hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (hour < 24) {
						auto min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (min < 60) {
							auto sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
							if (sec < 60) {
								return Format::VALID;
							}
						}
					}
				}
				return Format::OUT_OF_RANGE;
			}
			return Format::INVALID;
		case 20: // 0000-00-00[T ]00:00:00Z
			if (datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') && datetime[13] == ':' &&
				datetime[16] == ':' && datetime[19] == 'Z') {
				auto year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				auto mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				auto day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(year, mon, day)) {
					auto hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (hour < 24) {
						auto min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (min < 60) {
							auto sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
							if (sec < 60) {
								return Format::VALID;
							}
						}
					}
				}
				return Format::OUT_OF_RANGE;
			}
			return Format::INVALID;
		default: // 0000-00-00[T ]00:00:00[+-]00:00  0000-00-00[T ]00:00:00.0...  0000-00-00[T ]00:00:00.0...[+-]00:00
			if (size > 20 && datetime[4] == '-' && datetime[7] == '-' && (datetime[10] == 'T' || datetime[10] == ' ') &&
				datetime[13] == ':' && datetime[16] == ':') {
				auto year  = strict_stoul(&errno_save, datetime.substr(0, 4));
				if (errno_save != 0) { return Format::ERROR; }
				auto mon   = strict_stoul(&errno_save, datetime.substr(5, 2));
				if (errno_save != 0) { return Format::ERROR; }
				auto day   = strict_stoul(&errno_save, datetime.substr(8, 2));
				if (errno_save != 0) { return Format::ERROR; }
				if (isValidDate(year, mon, day)) {
					auto hour = strict_stoul(&errno_save, datetime.substr(11, 2));
					if (errno_save != 0) { return Format::ERROR; }
					if (hour < 24) {
						auto min = strict_stoul(&errno_save, datetime.substr(14, 2));
						if (errno_save != 0) { return Format::ERROR; }
						if (min < 60) {
							auto sec = strict_stoul(&errno_save, datetime.substr(17, 2));
							if (errno_save != 0) { return Format::ERROR; }
							if (sec < 60) {
								switch (datetime[19]) {
									case '+':
									case '-':
										if (size == 25 && datetime[22] == ':') {
											auto tz_h = datetime.substr(20, 2);
											auto h = strict_stoul(&errno_save, tz_h);
											if (errno_save != 0) { return Format::ERROR; }
											if (h < 24) {
												auto tz_m = datetime.substr(23, 2);
												auto m = strict_stoul(&errno_save, tz_m);
												if (errno_save != 0) { return Format::ERROR; }
												if (m < 60) {
													return Format::VALID;
												}
											}
											return Format::OUT_OF_RANGE;
										}
										return Format::INVALID;
									case '.': {
										auto it = datetime.begin() + 19;
										const auto it_e = datetime.end();
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
																auto tz_h = std::string_view(aux + 1, aux_end - aux - 1);
																auto h = strict_stoul(&errno_save, tz_h);
																if (errno_save != 0) { return Format::ERROR; }
																if (h < 24) {
																	auto tz_m = std::string_view(aux_end + 1, it_e - aux_end - 1);
																	auto m = strict_stoul(&errno_save, tz_m);
																	if (errno_save != 0) { return Format::ERROR; }
																	if (m < 60) {
																		return Format::VALID;
																	}
																}
																return Format::OUT_OF_RANGE;
															}
														}
														return Format::INVALID;
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
}


void
Datetime::processDateMath(std::string_view date_math, tm_t& tm)
{
	size_t size_match = 0;

	std::cregex_iterator next(date_math.begin(), date_math.end(), date_math_re, std::regex_constants::match_continuous);
	std::cregex_iterator end;
	while (next != end) {
		size_match += next->length(0);
		computeDateMath(tm, next->str(1), next->str(2)[0]);
		++next;
	}

	if (date_math.size() != size_match) {
		THROW(DatetimeError, "Datetime Math ({}) is used incorrectly", date_math);
	}
}


void
Datetime::computeTimeZone(tm_t& tm, char op, std::string_view hour, std::string_view min)
{
	std::string oph, opm;
	oph.reserve(3);
	opm.reserve(3);
	if (op == '+') {
		oph.push_back('-');
		oph.append(hour.data(), hour.size());
		opm.push_back('-');
		opm.append(min.data(), min.size());
	} else {
		oph.push_back('+');
		oph.append(hour.data(), hour.size());
		opm.push_back('+');
		opm.append(min.data(), min.size());
	}
	computeDateMath(tm, oph, 'h');
	computeDateMath(tm, opm, 'm');
}


/*
 * Compute a Datetime Math former by op + units.
 * op can be +#, -#, /, //
 * unit can be y, M, w, d, h, m, s
 */
void
Datetime::computeDateMath(tm_t& tm, std::string_view op, char unit)
{
	int errno_save;
	switch (op[0]) {
		case '+': {
			auto num = strict_stoi(&errno_save, op.substr(1));
			if (errno_save != 0) {
				THROW(DatetimeError, "Invalid format in Datetime Math unit: '{}'. {} must be numeric", repr(op.substr(1)));
			}
			switch (unit) {
				case 'y':
					tm.year += num; break;
				case 'M': {
					tm.mon += num;
					normalizeMonths(tm.year, tm.mon);
					auto max_days = getDays_month(tm.year, tm.mon);
					if (tm.day > max_days) { tm.day = max_days; }
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
					THROW(DatetimeError, "Invalid format in Datetime Math unit: '{}'. Unit must be in {{ y, M, w, d, h, m, s }}", unit);
			}
			break;
		}
		case '-': {
			auto num = strict_stoi(&errno_save, op.substr(1));
			if (errno_save != 0) {
				THROW(DatetimeError, "Invalid format in Datetime Math unit: '{}'. {} must be numeric", repr(op.substr(1)));
			}
			switch (unit) {
				case 'y':
					tm.year -= num;
					break;
				case 'M': {
					tm.mon -= num;
					normalizeMonths(tm.year, tm.mon);
					auto max_days = getDays_month(tm.year, tm.mon);
					if (tm.day > max_days) { tm.day = max_days; }
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
					THROW(DatetimeError, "Invalid format in Datetime Math unit: '{}'. Unit must be in {{ y, M, w, d, h, m, s }}", unit);
			}
			break;
		}
		case '/':
			switch (unit) {
				case 'y':
					if (op.size() == 1) {
						tm.mon = 12;
						tm.day = getDays_month(tm.year, 12);
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.mon = tm.day = 1;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				case 'M':
					if (op.size() == 1) {
						tm.day = getDays_month(tm.year, tm.mon);
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.day = 1;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				case 'w': {
					auto dateGMT = timegm(tm);
					struct tm timeinfo;
					gmtime_r(&dateGMT, &timeinfo);
					if (op.size() == 1) {
						tm.day += 6 - timeinfo.tm_wday;
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.day -= timeinfo.tm_wday;
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				}
				case 'd':
					if (op.size() == 1) {
						tm.hour = 23;
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.hour = tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				case 'h':
					if (op.size() == 1) {
						tm.min = tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.min = tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				case 'm':
					if (op.size() == 1) {
						tm.sec = 59;
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.sec = 0;
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
					break;
				case 's':
					if (op.size() == 1) {
						tm.fsec = DATETIME_MAX_FSEC;
					} else if (op.size() == 2 && op[1] == '/') {
						tm.fsec = 0.0;
					} else {
						THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
					}
				break;
			}
			break;
		default:
			THROW(DatetimeError, "Invalid format in Datetime Math operator: {}. Operator must be in {{ +#, -#, /, // }}", op);
	}

	// Update datetime.
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
	return year % 400 == 0 || (year % 4 == 0 && ((year % 100) != 0));
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


int
_getDays_month(int year, int month)
{
	assert(month > 0);
	assert(month <= 12);
	auto leap = static_cast<int>(Datetime::isleapYear(year));
	return days[leap][month - 1];
}

/*
 * Returns number of days in month, given the year.
 */
int
Datetime::getDays_month(int year, int month)
{
	if (month < 1 || month > 12) {
		THROW(DatetimeError, "Month must be in 1..12");
	}
	return _getDays_month(year, month);
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
	} else {
		while (mon < 1) {
			mon += 12;
			year--;
		}
	}
}


/*
 * Returns the proleptic Gregorian ordinal of the datetime,
 * where January 1 of year 1 has ordinal 1 (reference datetime).
 * year -> Any positive number except zero.
 * month -> Between 1 and 12 inclusive.
 * day -> Between 1 and the number of days in the given month of the given year.
 */
std::time_t
Datetime::toordinal(int year, int month, int day)
{
	if (year < 1) { THROW(DatetimeError, "Year is out of range"); }
	if (day < 1 || day > getDays_month(year, month)) { THROW(DatetimeError, "Day is out of range for month"); }

	auto leap = static_cast<int>(isleapYear(year));
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
	return {
		timeinfo.tm_year + DATETIME_START_YEAR, timeinfo.tm_mon + 1,
		timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
		timeinfo.tm_sec
	};
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
	return {
		timeinfo.tm_year + DATETIME_START_YEAR, timeinfo.tm_mon + 1,
		timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
		timeinfo.tm_sec, normalize_fsec(timestamp - _time)
	};
}


/*
 * Function to calculate Unix timestamp from Coordinated Universal Time (UTC).
 * Only for year greater than 0.
 * Returns Timestamp with milliseconds as the decimal part.
 */
double
Datetime::timestamp(const tm_t& tm)
{
	int year = tm.year, mon = tm.mon;
	normalizeMonths(year, mon);
	auto result = static_cast<double>(toordinal(year, mon, 1) - DATETIME_EPOCH_ORD + tm.day - 1);
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
 * Validate Datetime.
 */
bool
Datetime::isValidDate(int year, int month, int day)
{
	if (year < 1) {
		return false;
	}

	if (month < 1 || month > 12) {
		return false;
	}

	if (day < 1 || day > _getDays_month(year, month)) {
		return false;
	}

	return true;
}


/*
 * Return a string with the datetime in ISO 8601 Format.
 */
std::string
Datetime::iso8601(const std::tm& tm, bool trim, char sep)
{
	if (trim) {
		return strings::format("{:04}-{:02}-{:02}{}{:02}:{:02}:{:02}",
			tm.tm_year + DATETIME_START_YEAR, tm.tm_mon + 1, tm.tm_mday,
			sep, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	return strings::format("{:04}-{:02}-{:02}{}{:02}:{:02}:{:02}.000000",
		tm.tm_year + DATETIME_START_YEAR, tm.tm_mon + 1, tm.tm_mday,
		sep, tm.tm_hour, tm.tm_min, tm.tm_sec);
}


/*
 * Return a string with the datetime in ISO 8601 Format.
 */
std::string
Datetime::iso8601(const tm_t& tm, bool trim, char sep)
{
	if (trim) {
		auto res = strings::format("{:04}-{:02}-{:02}{}{:02}:{:02}:{:02}{}",
			tm.year, tm.mon, tm.day, sep,
			tm.hour, tm.min, tm.sec, tm.is_utc ? "Z" : "");
		if (tm.fsec > 0.0) {
			if (tm.is_utc) {
				res.pop_back();
			}
			res += strings::format("{:.6f}", tm.fsec).erase(0, 1);
			auto it_e = res.end();
			auto it = it_e - 1;
			for (; *it == '0'; --it) { }
			if (*it != '.') {
				++it;
			}
			res.erase(it, it_e);
			if (tm.is_utc) {
				res.push_back('Z');
			}
		}
		return res;
	}

	if (tm.fsec > 0.0) {
		auto res = strings::format("{:04}-{:02}-{:02}{}{:02}:{:02}:{:02}",
			tm.year, tm.mon, tm.day, sep,
			tm.hour, tm.min, tm.sec);
		res += strings::format("{:.6f}", tm.fsec).erase(0, 1);
		if (tm.is_utc) {
				res.push_back('Z');
		}
		return res;
	}

	return strings::format("{:04}-{:02}-{:02}{}{:02}:{:02}:{:02}.000000{}",
		tm.year, tm.mon, tm.day, sep,
		tm.hour, tm.min, tm.sec, tm.is_utc ? "Z" : "");
}


/*
 * Transforms a timestamp in seconds with decimal fraction to ISO 8601 format.
 */
std::string
Datetime::iso8601(double timestamp, bool trim, char sep)
{
	auto tm = to_tm_t(timestamp);
	return iso8601(tm, trim, sep);
}


/*
 * Transforms a time_point in seconds with decimal fraction to ISO 8601 format.
 */
std::string
Datetime::iso8601(std::chrono::system_clock::time_point tp, bool trim, char sep)
{
	return iso8601(timestamp(tp), trim, sep);
}


bool
Datetime::isDate(std::string_view date)
{
	int errno_save;
	auto size = date.size();
	switch (size) {
		case 10: // 0000-00-00
			if (date[4] == '-' && date[7] == '-') {
				auto year  = strict_stoul(&errno_save, date.substr(0, 4));
				if (errno_save != 0) { return false; }
				auto mon   = strict_stoul(&errno_save, date.substr(5, 2));
				if (errno_save != 0) { return false; }
				auto day   = strict_stoul(&errno_save, date.substr(8, 2));
				if (errno_save != 0) { return false; }
				if (isValidDate(year, mon, day)) {
					return true;
				}
				return false;
			}
			[[fallthrough]];
		default:
			return false;
	}
}


bool
Datetime::isDatetime(std::string_view datetime)
{
	auto format = Iso8601Parser(datetime);
	switch (format) {
		case Format::VALID:
			return true;
		case Format::INVALID: {
			std::cmatch m;
			return std::regex_match(datetime.begin(), datetime.end(), m, date_re) && static_cast<std::size_t>(m.length(0)) == datetime.size();
		}
		default:
			return false;
	}
}


Datetime::clk_t
Datetime::TimeParser(std::string_view _time)
{
	int errno_save;
	clk_t clk;
	auto length = _time.length();
	switch (length) {
		case 5: // 00:00
			if (_time[2] == ':') {
				clk.hour = strict_stoul(&errno_save, _time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				clk.min = strict_stoul(&errno_save, _time.substr(3, 2));
				if (errno_save != 0) { goto error; }
				return clk;
			}
			break;
		case 8: // 00:00:00
			if (_time[2] == ':' && _time[5] == ':') {
				clk.hour = strict_stoul(&errno_save, _time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				clk.min = strict_stoul(&errno_save, _time.substr(3, 2));
				if (errno_save != 0) { goto error; }
				clk.sec = strict_stoul(&errno_save, _time.substr(6, 2));
				if (errno_save != 0) { goto error; }
				return clk;
			}
			break;
		default: //  00:00:00[+-]00:00  00:00:00.000...  00:00:00.000...[+-]00:00
			if (length > 9 && (_time[2] == ':' && _time[5] == ':')) {
				clk.hour = strict_stoul(&errno_save, _time.substr(0, 2));
				if (errno_save != 0) { goto error; }
				clk.min = strict_stoul(&errno_save, _time.substr(3, 2));
				if (errno_save != 0) { goto error; }
				clk.sec = strict_stoul(&errno_save, _time.substr(6, 2));
				if (errno_save != 0) { goto error; }
				switch (_time[8]) {
					case '-':
						clk.tz_s = '-';
						[[fallthrough]];
					case '+':
						if (length == 14 && _time[11] == ':') {
							clk.tz_h = strict_stoul(&errno_save, _time.substr(9, 2));
							if (errno_save != 0) { goto error; }
							clk.tz_m = strict_stoul(&errno_save, _time.substr(12, 2));
							if (errno_save != 0) { goto error; }
							return clk;
						}
						break;
					case '.': {
						auto it = _time.begin() + 8;
						const auto it_e = _time.end();
						for (auto aux = it + 1; aux != it_e; ++aux) {
							const auto& c = *aux;
							if (c < '0' || c > '9') {
								switch (c) {
									case '-':
										clk.tz_s = '-';
										[[fallthrough]];
									case '+':
										if ((it_e - aux) == 6) {
											auto aux_end = aux + 3;
											if (*aux_end == ':') {
												clk.tz_h = strict_stoul(&errno_save, std::string_view(aux + 1, aux_end - aux - 1));
												if (errno_save != 0) { goto error; }
												clk.tz_m = strict_stoul(&errno_save, std::string_view(aux_end + 1, it_e - aux_end - 1));
												if (errno_save != 0) { goto error; }
												auto fsec = strict_stod(&errno_save, std::string_view(it, aux - it));
												if (errno_save != 0) { goto error; }
												clk.fsec = Datetime::normalize_fsec(fsec);
												return clk;
											}
										}
										break;
									default:
										break;
								}
								goto error;
							}
						}
						auto fsec = strict_stod(&errno_save, std::string_view(it, it_e - it));
						if (errno_save != 0) { goto error; }
						clk.fsec = Datetime::normalize_fsec(fsec);
						return clk;
					}
					default:
						break;
				}
			}
			break;
	}

error:
	THROW(TimeError, "Error format in time: {}, the format must be '00:00(:00(.0...)([+-]00:00))'", _time);
}


/*
 * Transforms double time to a struct clk_t.
 */
Datetime::clk_t
Datetime::time_to_clk_t(double t)
{
	if (isvalidTime(t)) {
		clk_t clk;
		if (t < 0) {
			auto _time = static_cast<int>(-t);
			clk.fsec = t + _time;
			if (clk.fsec < 0.0) {
				++_time;
			}
			clk.tz_h = _time / 3600;
			int aux;
			if (clk.tz_h < 100) {
				aux = clk.tz_h * 3600;
			} else {
				clk.tz_h = 99;
				aux = clk.tz_h * 3600;
			}
			clk.tz_m = (_time - aux) / 60;
			clk.sec = _time - aux - clk.tz_m * 60;
			if (clk.sec != 0) {
				clk.sec = 60 - clk.sec;
				++clk.tz_m;
			}
			clk.tz_s = '+';
			if (clk.fsec < 0) {
				clk.fsec += 1.0;
			}
		} else {
			auto _time = static_cast<int>(t);
			clk.hour = _time / 3600;
			if (clk.hour < 100) {
				auto aux = _time - clk.hour * 3600;
				clk.min = aux / 60;
				clk.sec = aux - clk.min * 60;
			} else {
				if (clk.hour < 199) {
					auto aux = _time - clk.hour * 3600;
					clk.tz_h = clk.hour - 99;
					clk.hour = 99;
					clk.min = aux / 60;
					clk.sec = aux - clk.min * 60;
				} else {
					clk.tz_h = 99;
					clk.hour = 99;
					auto aux = _time - 712800; // 198 * 3600
					clk.min = aux / 60;
					if (clk.min < 100) {
						clk.sec = aux - clk.min * 60;
					} else {
						if (clk.min < 199) {
							clk.sec = aux - clk.min * 60;
							clk.tz_m = 198 - clk.min;
							clk.min = 99;
						} else {
							clk.tz_m = 99;
							clk.min = 99;
							clk.sec = aux - 11880; // 198 * 60
						}
					}
				}
			}
			clk.tz_s = '-';
			clk.fsec = t - _time;
		}

		return clk;
	}

	THROW(TimeError, "Bad serialised time value");
}


double
Datetime::time_to_double(const MsgPack& _time)
{
	switch (_time.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER: {
			double t_val = _time.u64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			THROW(TimeError, "Time: {} is out of range", t_val);
		}
		case MsgPack::Type::NEGATIVE_INTEGER: {
			double t_val = _time.i64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			THROW(TimeError, "Time: {} is out of range", t_val);
		}
		case MsgPack::Type::FLOAT: {
			double t_val = _time.f64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			THROW(TimeError, "Time: {} is out of range", t_val);
		}
		case MsgPack::Type::STR:
			return time_to_double(TimeParser(_time.str_view()));
		default:
			THROW(TimeError, "Time must be numeric or string");
	}
}


double
Datetime::time_to_double(const clk_t& clk)
{
	int hour, min;
	if (clk.tz_s == '-') {
		hour = clk.hour + clk.tz_h;
		min = clk.min + clk.tz_m;
	} else {
		hour = clk.hour - clk.tz_h;
		min = clk.min - clk.tz_m;
	}
	return clk.sec + clk.fsec + (hour * 60 + min) * 60;
}


std::string
Datetime::time_to_string(const clk_t& clk, bool trim)
{
	if (trim && clk.tz_h == 0 && clk.tz_m == 0) {
		auto res = strings::format("{:02}:{:02}:{:02}",
			clk.hour, clk.min, clk.sec);
		if (clk.fsec > 0) {
			res += strings::format("{:.6f}", clk.fsec).erase(0, 1);
			auto it_e = res.end();
			auto it = it_e - 1;
			for (; *it == '0'; --it) { }
			if (*it != '.') {
				++it;
			}
			res.erase(it, it_e);
		}
		return res;
	}

	if (clk.fsec > 0) {
		auto res = strings::format("{:02}:{:02}:{:02}",
			clk.hour, clk.min, clk.sec);
		res += strings::format("{:.6f}{}{:02}:{:02}", clk.fsec,
			clk.tz_s, clk.tz_h, clk.tz_m).erase(0, 1);
		return res;
	}

	return strings::format("{:02}:{:02}:{:02}.000000{}{:02}:{:02}",
		clk.hour, clk.min, clk.sec,
		clk.tz_s, clk.tz_h, clk.tz_m);
}


std::string
Datetime::time_to_string(double t, bool trim)
{
	return time_to_string(time_to_clk_t(t), trim);
}


bool
Datetime::isTime(std::string_view _time) {
	auto length = _time.length();
	switch (length) {
		case 5: // 00:00
			return (std::isdigit(_time[0]) != 0) && (std::isdigit(_time[1]) != 0) && _time[2] == ':' &&
				(std::isdigit(_time[3]) != 0) && (std::isdigit(_time[4]) != 0);
		case 8: // 00:00:00
			return (std::isdigit(_time[0]) != 0) && (std::isdigit(_time[1]) != 0) && _time[2] == ':' &&
				(std::isdigit(_time[3]) != 0) && (std::isdigit(_time[4]) != 0) && _time[5] == ':' &&
				(std::isdigit(_time[6]) != 0) && (std::isdigit(_time[7]) != 0);
		default: //  00:00:00[+-]00:00  00:00:00.000...  00:00:00.000...[+-]00:00
			if (length > 9 && (std::isdigit(_time[0]) != 0) && (std::isdigit(_time[1]) != 0) && _time[2] == ':' &&
				(std::isdigit(_time[3]) != 0) && (std::isdigit(_time[4]) != 0) && _time[5] == ':' &&
				(std::isdigit(_time[6]) != 0) && (std::isdigit(_time[7]) != 0)) {
				switch (_time[8]) {
					case '+':
					case '-':
						return length == 14 && (std::isdigit(_time[9]) != 0) && (std::isdigit(_time[10]) != 0) && _time[11] == ':' &&
							(std::isdigit(_time[12]) != 0) && (std::isdigit(_time[13]) != 0);
					case '.':
						for (size_t i = 9; i < length; ++i) {
							const auto c = _time[i];
							if (std::isdigit(c) == 0) {
								return (c == '+' || c == '-') && length == (i + 6) && (std::isdigit(_time[i + 1]) != 0) &&
									(std::isdigit(_time[i + 2]) != 0) && _time[i + 3] == ':' && (std::isdigit(_time[i + 4]) != 0) &&
									(std::isdigit(_time[i + 5]) != 0);
							}
						}
						return true;
				}
			}
			return false;
	}
}


Datetime::clk_t
Datetime::TimedeltaParser(std::string_view timedelta)
{
	int errno_save;
	clk_t clk;
	auto size = timedelta.size();
	switch (size) {
		case 6: // [+-]00:00
			switch (timedelta[0]) {
				case '-':
					clk.tz_s = '-';
					[[fallthrough]];
				case '+':
					if (timedelta[3] == ':') {
						clk.hour = strict_stoul(&errno_save, timedelta.substr(1, 2));
						if (errno_save != 0) { goto error; }
						clk.min = strict_stoul(&errno_save, timedelta.substr(4, 2));
						if (errno_save != 0) { goto error; }
						return clk;
					}
					[[fallthrough]];
				default:
					break;
			}
			break;

		case 9: // [+-]00:00:00
			switch (timedelta[0]) {
				case '-':
					clk.tz_s = '-';
					[[fallthrough]];
				case '+':
					if (timedelta[3] == ':' && timedelta[6] == ':') {
						clk.hour = strict_stoul(&errno_save, timedelta.substr(1, 2));
						if (errno_save != 0) { goto error; }
						clk.min = strict_stoul(&errno_save, timedelta.substr(4, 2));
						if (errno_save != 0) { goto error; }
						clk.sec = strict_stoul(&errno_save, timedelta.substr(7, 2));
						if (errno_save != 0) { goto error; }
						return clk;
					}
					[[fallthrough]];
				default:
					break;
			}
			break;

		default: //  [+-]00:00:00.000...
			switch (timedelta[0]) {
				case '-':
					clk.tz_s = '-';
					[[fallthrough]];
				case '+':
					if (size > 10 && (timedelta[3] == ':' && timedelta[6] == ':' && timedelta[9] == '.')) {
						clk.hour = strict_stoul(&errno_save, timedelta.substr(1, 2));
						if (errno_save != 0) { goto error; }
						clk.min = strict_stoul(&errno_save, timedelta.substr(4, 2));
						if (errno_save != 0) { goto error; }
						clk.sec = strict_stoul(&errno_save, timedelta.substr(7, 2));
						if (errno_save != 0) { goto error; }
						auto it = timedelta.begin() + 9;
						const auto it_e = timedelta.end();
						for (auto aux = it + 1; aux != it_e; ++aux) {
							const auto& c = *aux;
							if (c < '0' || c > '9') {
								goto error;
							}
						}
						auto fsec = strict_stod(&errno_save, std::string_view(it, it_e - it));
						if (errno_save != 0) { goto error; }
						clk.fsec = Datetime::normalize_fsec(fsec);
						return clk;
					}
					[[fallthrough]];
				default:
					break;
			}
			break;
	}

error:
	THROW(TimedeltaError, "Error format in timedelta: {}, the format must be '[+-]00:00(:00(.0...))'", timedelta);
}


/*
 * Transforms double timedelta to a struct clk_t.
 */
Datetime::clk_t
Datetime::timedelta_to_clk_t(double t)
{
	if (isvalidTimedelta(t)) {
		clk_t clk;
		if (t < 0) {
			t *= -1.0;
			clk.tz_s = '-';
		}

		auto _time = static_cast<int>(t);
		clk.hour = _time / 3600;
		if (clk.hour < 100) {
			auto aux = _time - clk.hour * 3600;
			clk.min = aux / 60;
			clk.sec = aux - clk.min * 60;
		} else {
			clk.hour = 99;
			auto aux = _time - clk.hour * 3600;
			clk.min = aux / 60;
			if (clk.min < 100) {
				clk.sec =  aux - clk.min * 60;
			} else {
				clk.min = 99;
				clk.sec =  aux - clk.min * 60;
			}
		}
		clk.fsec = t - _time;

		return clk;
	}

	THROW(TimedeltaError, "Bad serialised timedelta value");
}


double
Datetime::timedelta_to_double(const MsgPack& timedelta)
{
	switch (timedelta.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER: {
			double t_val = timedelta.u64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			THROW(TimedeltaError, "Timedelta: {} is out of range", t_val);
		}
		case MsgPack::Type::NEGATIVE_INTEGER: {
			double t_val = timedelta.i64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			THROW(TimedeltaError, "Timedelta: {} is out of range", t_val);
		}
		case MsgPack::Type::FLOAT: {
			double t_val = timedelta.f64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			THROW(TimedeltaError, "Timedelta: {} is out of range", t_val);
		}
		case MsgPack::Type::STR:
			return timedelta_to_double(TimedeltaParser(timedelta.str_view()));
		default:
			THROW(TimedeltaError, "Timedelta must be numeric or string");
	}
}


double
Datetime::timedelta_to_double(const clk_t& clk)
{
	double t = (clk.hour * 60 + clk.min) * 60 + clk.sec + clk.fsec;
	return clk.tz_s == '-' ? -t : t;
}


std::string
Datetime::timedelta_to_string(const clk_t& clk, bool trim)
{
	if (trim) {
		auto res = strings::format("{}{:02}:{:02}:{:02}",
			clk.tz_s, clk.hour, clk.min, clk.sec);
		if (clk.fsec > 0) {
			res += strings::format("{:.6f}", clk.fsec).erase(0, 1);
			auto it_e = res.end();
			auto it = it_e - 1;
			for (; *it == '0'; --it) { }
			if (*it != '.') {
				++it;
			}
			res.erase(it, it_e);
		}
		return res;
	}

	if (clk.fsec > 0) {
		return strings::format("{}{:02}:{:02}:{:02}",
			clk.tz_s, clk.hour, clk.min, clk.sec);
	}

	return strings::format("{}{:02}:{:02}:{:02}.000000",
		clk.tz_s, clk.hour, clk.min, clk.sec);
}


std::string
Datetime::timedelta_to_string(double t, bool trim)
{
	return timedelta_to_string(timedelta_to_clk_t(t), trim);
}


bool
Datetime::isTimedelta(std::string_view timedelta)
{
	auto size = timedelta.size();
	switch (size) {
		case 6: // [+-]00:00
			return (timedelta[0] == '+' || timedelta[0] == '-') && (std::isdigit(timedelta[1]) != 0) && (std::isdigit(timedelta[2]) != 0) &&
				timedelta[3] == ':' && (std::isdigit(timedelta[4]) != 0) && (std::isdigit(timedelta[5]) != 0);
		case 9: // [+-]00:00:00
			return (timedelta[0] == '+' || timedelta[0] == '-') && (std::isdigit(timedelta[1]) != 0) && (std::isdigit(timedelta[2]) != 0) &&
				timedelta[3] == ':' && (std::isdigit(timedelta[4]) != 0) && (std::isdigit(timedelta[5]) != 0) &&
				timedelta[6] == ':' && (std::isdigit(timedelta[7]) != 0) && (std::isdigit(timedelta[8]) != 0);
		default: // [+-]00:00:00  [+-]00:00:00.000...
			if (size > 10 && (timedelta[0] == '+' || timedelta[0] == '-') && (std::isdigit(timedelta[1]) != 0) &&
				(std::isdigit(timedelta[2]) != 0) && timedelta[3] == ':' && (std::isdigit(timedelta[4]) != 0) &&
				(std::isdigit(timedelta[5]) != 0) && timedelta[6] == ':' && (std::isdigit(timedelta[7]) != 0) &&
				(std::isdigit(timedelta[8]) != 0) && timedelta[9] == '.') {
				for (size_t i = 10; i < size; ++i) {
					if (std::isdigit(timedelta[i]) == 0) {
						return false;
					}
				}
				return true;
			}
			return false;
	}
}
