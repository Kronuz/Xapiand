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

#include "datetime_msgpack.h"

#include <string_view>                            // for std::string_view

#include "log.h"                                  // for L_CALL
#include "msgpack.h"                              // for MsgPack
#include "phf.hh"                                 // for phf
#include "repr.hh"                                // for repr
#include "reserved/types.h"                       // for RESERVED_TIME
#include "reserved/datetime.h"                    // for RESERVED_
#include "strict_stox.hh"                         // for strict_stoul
#include "strings.hh"                             // for strings::format

// NOTE on exceptions: the datetime library's DatetimeError / TimeError /
// TimedeltaError now derive from std::runtime_error and take a single message
// string, so the located-exception THROW(X, "fmt", ...) macro does NOT apply to
// them (it needs a BaseException-shaped __func__/__FILE__/__LINE__ ctor). We
// therefore `throw DatetimeError(strings::format(...))` directly for those. The
// generic `Error` type (one site) is still a located-exception type, so it keeps
// using THROW.


static inline void process_date_year(Datetime::tm_t& tm, const MsgPack& year) {
	switch (year.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			tm.year = year.u64();
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			tm.year = year.i64();
			return;
		default:
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_YEAR));
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
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_MONTH));
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
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_DAY));
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
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_HOUR));
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
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_MIN));
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
			throw DatetimeError(strings::format("'{}' must be a positive integer value", RESERVED_SEC));
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
			throw DatetimeError(strings::format("'{}' must be a numeric value", RESERVED_FSEC));
	}
}


// String-time helper. The library folds the string *datetime* path into
// Datetime::DatetimeParser(string_view), but the string *time* path (HH:MM:SS
// with optional fraction/zone, used by the MsgPack time/datetime STR cases) is
// only needed here, so it stays in the glue. It uses the library's
// Datetime::computeTimeZone and Datetime::normalize_fsec.
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
												Datetime::computeTimeZone(tm, str_time[8], tz_h, tz_m);
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
																Datetime::computeTimeZone(tm, c, tz_h, tz_m);
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
	throw DatetimeError(strings::format("Error format in _time: {}, the format must be '00:00(:00(.0...)([+-]00:00))'", str_time));

error_out_of_range:
	throw DatetimeError(strings::format("Time: {} is out of range", str_time));
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
						throw DatetimeError(strings::format("Unsupported Key: {} in time", repr(str_key)));
				}
			}
			return;
		}

		case MsgPack::Type::STR:
			process_date_time(tm, time.str_view());
			return;

		default:
			throw DatetimeError(strings::format("'{}' must be a map or string value", RESERVED_TIME));
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
						throw DatetimeError(strings::format("Unsupported Key: {} in date", repr(str_key)));
				}
			}
			return;
		}

		default:
			throw DatetimeError(strings::format("'{}' must be a map value", RESERVED_DATE));
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
						throw DatetimeError(strings::format("Unsupported Key: {} in datetime", repr(str_key)));
				}
			}
			return;
		}

		case MsgPack::Type::STR:
			process_date_time(tm, datetime.str_view());
			return;

		default:
			throw DatetimeError(strings::format("'{}' must be a map value", RESERVED_DATETIME));
	}
}


/*
 * Returns struct tm according to the datetime specified by value.
 *
 * Scalars and strings delegate straight to the library: a numeric timestamp goes
 * through Datetime::to_tm_t, a string through Datetime::DatetimeParser(string).
 * Only the MsgPack MAP (structured object) case is handled here.
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
				throw DatetimeError("Datetime is out of range");
			}
			return tm;
		}
		default:
			throw DatetimeError("Datetime value must be numeric or string");
	}
}


/*
 * Pull a time out of a MsgPack scalar or string. Numerics are validated and
 * returned; strings delegate to the library's Datetime::TimeParser + the
 * clk_t->double conversion.
 */
double
Datetime::time_to_double(const MsgPack& _time)
{
	switch (_time.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER: {
			double t_val = _time.u64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			throw TimeError(strings::format("Time: {} is out of range", t_val));
		}
		case MsgPack::Type::NEGATIVE_INTEGER: {
			double t_val = _time.i64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			throw TimeError(strings::format("Time: {} is out of range", t_val));
		}
		case MsgPack::Type::FLOAT: {
			double t_val = _time.f64();
			if (isvalidTime(t_val)) {
				return t_val;
			}
			throw TimeError(strings::format("Time: {} is out of range", t_val));
		}
		case MsgPack::Type::STR:
			return time_to_double(TimeParser(_time.str_view()));
		default:
			throw TimeError("Time must be numeric or string");
	}
}


/*
 * Pull a timedelta out of a MsgPack scalar or string. Numerics are validated and
 * returned; strings delegate to the library's Datetime::TimedeltaParser + the
 * clk_t->double conversion.
 */
double
Datetime::timedelta_to_double(const MsgPack& timedelta)
{
	switch (timedelta.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER: {
			double t_val = timedelta.u64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			throw TimedeltaError(strings::format("Timedelta: {} is out of range", t_val));
		}
		case MsgPack::Type::NEGATIVE_INTEGER: {
			double t_val = timedelta.i64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			throw TimedeltaError(strings::format("Timedelta: {} is out of range", t_val));
		}
		case MsgPack::Type::FLOAT: {
			double t_val = timedelta.f64();
			if (isvalidTimedelta(t_val)) {
				return t_val;
			}
			throw TimedeltaError(strings::format("Timedelta: {} is out of range", t_val));
		}
		case MsgPack::Type::STR:
			return timedelta_to_double(TimedeltaParser(timedelta.str_view()));
		default:
			throw TimedeltaError("Timedelta must be numeric or string");
	}
}
