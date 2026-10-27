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

#include "range.h"

#include <limits>                                 // for std::numeric_limits
#include <stdexcept>                              // for out_of_range
#include <sys/types.h>                            // for int64_t, uint64_t
#include <type_traits>                            // for decay_t, enable_if_t, is_i...
#include <vector>                                 // for vector

#include "cast.h"                                 // for Cast
#include "database/schema.h"                      // for required_spc_t, FieldType
#include "datetime.h"                             // for timestamp
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads
#include "engine/error.h"                       // for engine::Error
#include "xapiand_exception.h"                            // for MSG_QueryParserError, Quer...
#include "generate_terms.h"                       // for datetime, geo, numeric
#include "geospatialrange.h"                      // for GeoSpatialRange
#include "length.h"                               // for serialise_length
#include "math.hh"                          // for max, min
#include "reserved/query_dsl.h"                   // for RESERVED_QUERYDSL_FROM, RESERVED_QUERYDSL_TO
#include "serialise_list.h"                       // for StringList


// Terms-only fields (no value slot) have no exact check behind the terms, so they get a larger budget.
constexpr size_t TERMS_ONLY_MAX_TERMS = 4096;

// The truncated integer a double is indexed as (static_cast truncates), saturating at T's limits.
template <typename T>
static T
truncate_saturating(double d)
{
	constexpr double lowest = static_cast<double>(std::numeric_limits<T>::min());
	constexpr double highest = static_cast<double>(std::numeric_limits<T>::max());  // 2^63 or 2^64: first out of range
	if (!(d > lowest)) {
		return std::numeric_limits<T>::min();
	}
	if (d >= highest) {
		return std::numeric_limits<T>::max();
	}
	return static_cast<T>(d);
}


template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
engine::Query
getNumericQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end)
{
	std::string ser_start, ser_end;
	T value_s, value_e;
	switch (field_spc.get_type()) {
		case FieldType::floating: {
			double val_s = start ? start->is_map() ? Cast::cast(*start).f64() : Cast::floating(*start) : std::numeric_limits<double>::lowest();
			double val_e = end ? end->is_map() ? Cast::cast(*end).f64() : Cast::floating(*end) : std::numeric_limits<double>::max();

			if (val_s > val_e) {
				return engine::Query();
			}

			ser_start = Serialise::floating(val_s);
			ser_end = Serialise::floating(val_e);
			value_s = truncate_saturating<T>(val_s);
			value_e = truncate_saturating<T>(val_e);
			break;
		}
		case FieldType::integer: {
			int64_t val_s = start ? start->is_map() ? Cast::cast(*start).i64() : Cast::integer(*start) : std::numeric_limits<int64_t>::min();
			int64_t val_e = end ? end->is_map() ? Cast::cast(*end).i64() : Cast::integer(*end) : std::numeric_limits<int64_t>::max();

			if (val_s > val_e) {
				return engine::Query();
			}

			ser_start = Serialise::integer(val_s);
			ser_end = Serialise::integer(val_e);
			value_s = val_s;
			value_e = val_e;
			break;
		}
		case FieldType::positive: {
			uint64_t val_s = start ? start->is_map() ? Cast::cast(*start).u64() : Cast::positive(*start) : std::numeric_limits<uint64_t>::min();
			uint64_t val_e = end ? end->is_map() ? Cast::cast(*end).u64() : Cast::positive(*end) : std::numeric_limits<uint64_t>::max();

			if (val_s > val_e) {
				return engine::Query();
			}

			ser_start = Serialise::positive(val_s);
			ser_end = Serialise::positive(val_e);
			value_s = val_s;
			value_e = val_e;
			break;
		}
		default:
			THROW(QueryParserError, "Expected numeric type for query range");
	}

	auto query = GenerateTerms::numeric(value_s, value_e, field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);

	if (field_spc.slot != engine::BAD_VALUENO) {
		if (!start) {
			auto mvle = std::make_unique<MultipleValueLE>(field_spc.slot, std::move(ser_end));
			if (query.empty()) {
				return engine::Query::source(std::move(mvle));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvle)), query);
		}

		if (!end) {
			auto mvge = std::make_unique<MultipleValueGE>(field_spc.slot, std::move(ser_start));
			if (query.empty()) {
				return engine::Query::source(std::move(mvge));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvge)), query);
		}

		auto mvr = std::make_unique<MultipleValueRange>(field_spc.slot, std::move(ser_start), std::move(ser_end));
		if (query.empty()) {
			return engine::Query::source(std::move(mvr));
		}
		return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvr)), query);
	}

	return query;
}


engine::Query
getStringQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end)
{
	if (field_spc.slot != engine::BAD_VALUENO) {
		std::string start_s = start ? Serialise::MsgPack(field_spc, *start) : "";
		std::string end_s = end ? Serialise::MsgPack(field_spc, *end) : "";

		if (start_s > end_s) {
			return engine::Query();
		}

		if (!start) {
			auto mvle = std::make_unique<MultipleValueLE>(field_spc.slot, std::move(end_s));
			return engine::Query::source(std::move(mvle));
		}

		if (!end) {
			auto mvge = std::make_unique<MultipleValueGE>(field_spc.slot, std::move(start_s));
			return engine::Query::source(std::move(mvge));
		}

		auto mvr = std::make_unique<MultipleValueRange>(field_spc.slot, std::move(start_s), std::move(end_s));
		return engine::Query::source(std::move(mvr));
	}

	return engine::Query();
}


engine::Query
getDateQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end)
{
	// An open end has no finite timestamp: the check below only compares the ends given, and the value
	// source alone decides (a finite term cover would drop the dates outside it).
	double timestamp_s = start ? Datetime::timestamp(Datetime::DatetimeParser(*start)) : 0;
	double timestamp_e = end ? Datetime::timestamp(Datetime::DatetimeParser(*end)) : 0;

	if (start && end && timestamp_s > timestamp_e) {
		return engine::Query();
	}

	std::string ser_start, ser_end;
	if (start) {
		ser_start = Serialise::timestamp(timestamp_s);
	}
	if (end) {
		ser_end = Serialise::timestamp(timestamp_e);
	}

	auto query = start && end ? GenerateTerms::datetime(timestamp_s, timestamp_e, field_spc.accuracy, field_spc.acc_prefix) : engine::Query();

	if (field_spc.slot != engine::BAD_VALUENO) {
		if (!start) {
			auto mvle = std::make_unique<MultipleValueLE>(field_spc.slot, std::move(ser_end));
			if (query.empty()) {
				return engine::Query::source(std::move(mvle));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvle)), query);
		}

		if (!end) {
			auto mvge = std::make_unique<MultipleValueGE>(field_spc.slot, std::move(ser_start));
			if (query.empty()) {
				return engine::Query::source(std::move(mvge));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvge)), query);
		}

		auto mvr = std::make_unique<MultipleValueRange>(field_spc.slot, std::move(ser_start), std::move(ser_end));
		if (query.empty()) {
			return engine::Query::source(std::move(mvr));
		}
		return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvr)), query);
	}

	return query;
}


engine::Query
getTimeQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end)
{
	double time_s = start ? Datetime::time_to_double(*start) : std::numeric_limits<double>::lowest();
	double time_e = end ? Datetime::time_to_double(*end) : std::numeric_limits<double>::max();

	if (time_s > time_e) {
		return engine::Query();
	}

	std::string ser_start, ser_end;
	if (start) {
		ser_start = Serialise::timestamp(time_s);
	}
	if (end) {
		ser_end = Serialise::timestamp(time_e);
	}

	auto query = GenerateTerms::numeric(truncate_saturating<int64_t>(time_s), truncate_saturating<int64_t>(time_e), field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);

	if (field_spc.slot != engine::BAD_VALUENO) {
		if (!start) {
			auto mvle = std::make_unique<MultipleValueLE>(field_spc.slot, std::move(ser_end));
			if (query.empty()) {
				return engine::Query::source(std::move(mvle));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvle)), query);
		}

		if (!end) {
			auto mvge = std::make_unique<MultipleValueGE>(field_spc.slot, std::move(ser_start));
			if (query.empty()) {
				return engine::Query::source(std::move(mvge));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvge)), query);
		}

		auto mvr = std::make_unique<MultipleValueRange>(field_spc.slot, std::move(ser_start), std::move(ser_end));
		if (query.empty()) {
			return engine::Query::source(std::move(mvr));
		}
		return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvr)), query);
	}

	return query;
}


engine::Query
getTimedeltaQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end) {
	double timedelta_s = start ? Datetime::timedelta_to_double(*start) : std::numeric_limits<double>::lowest();
	double timedelta_e = end ? Datetime::timedelta_to_double(*end) : std::numeric_limits<double>::max();

	if (timedelta_s > timedelta_e) {
		return engine::Query();
	}

	std::string ser_start, ser_end;
	if (start) {
		ser_start = Serialise::timestamp(timedelta_s);
	}
	if (end) {
		ser_end = Serialise::timestamp(timedelta_e);
	}

	auto query = GenerateTerms::numeric(truncate_saturating<int64_t>(timedelta_s), truncate_saturating<int64_t>(timedelta_e), field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);

	if (field_spc.slot != engine::BAD_VALUENO) {
		if (!start) {
			auto mvle = std::make_unique<MultipleValueLE>(field_spc.slot, std::move(ser_end));
			if (query.empty()) {
				return engine::Query::source(std::move(mvle));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvle)), query);
		}

		if (!end) {
			auto mvge = std::make_unique<MultipleValueGE>(field_spc.slot, std::move(ser_start));
			if (query.empty()) {
				return engine::Query::source(std::move(mvge));
			}
			return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvge)), query);
		}

		auto mvr = std::make_unique<MultipleValueRange>(field_spc.slot, std::move(ser_start), std::move(ser_end));
		if (query.empty()) {
			return engine::Query::source(std::move(mvr));
		}
		return engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(mvr)), query);
	}

	return query;
}


engine::Query
MultipleValueRange::getQuery(const required_spc_t& field_spc, const MsgPack& obj)
{
	const MsgPack* start = nullptr;
	const MsgPack* end = nullptr;

	auto it = obj.find(RESERVED_QUERYDSL_FROM);
	if (it != obj.end()) {
		start = &it.value();
	}

	it = obj.find(RESERVED_QUERYDSL_TO);
	if (it != obj.end()) {
		end = &it.value();
	}

	try {
		if (!start && !end) {
			return engine::Query::match_all();
		}

		switch (field_spc.get_type()) {
			case FieldType::integer:
			case FieldType::floating:
				return getNumericQuery<int64_t>(field_spc, start, end);
			case FieldType::positive:
				return getNumericQuery<uint64_t>(field_spc, start, end);
			case FieldType::uuid:
			case FieldType::boolean:
			case FieldType::keyword:
			case FieldType::text:
				return getStringQuery(field_spc, start, end);
			case FieldType::date:
			case FieldType::datetime:
				return getDateQuery(field_spc, start, end);
			case FieldType::time:
				return getTimeQuery(field_spc, start, end);
			case FieldType::timedelta:
				return getTimedeltaQuery(field_spc, start, end);
			case FieldType::geo:
				if (!start) {
					return GeoSpatialRange::getQuery(field_spc, *end);
				}
				if (!end) {
					return GeoSpatialRange::getQuery(field_spc, *start);
				}
				THROW(QueryParserError, "The format for Geo Spatial range is: <field>: [\"EWKT\"]");
			default:
				return engine::Query();
		}
	} catch (const Exception& exc) {
		THROW(QueryParserError, "Failed to serialize: {} - {} like {} ({})", start ? start->to_string() : "", end ? end->to_string() : "",
			enum_name(field_spc.get_type()), exc.what());
	}
}


template <typename T, typename>
MultipleValueRange::MultipleValueRange(engine::valueno slot, T&& start_, T&& end_)
	: slot_(slot),
	  start(std::forward<T>(start_)),
	  end(std::forward<T>(end_)) { }


bool
MultipleValueRange::insideRange(std::string_view value) const noexcept
{
	// A value that does not decode is not accepted.
	try {
		StringList data(value);

		if (data.empty() || end < data.front() || start > data.back()) {
			return false;
		}

		for (const auto& value_ : data) {
			if (value_ >= start) {
				return value_ <= end;
			}
		}

		return false;
	} catch (const SerialisationError&) {
		return false;
	}
}


double
MultipleValueRange::weight(std::string_view /*value*/) const
{
	return 1.0;
}


std::unique_ptr<engine::PostingSource>
MultipleValueRange::clone() const
{
	return std::make_unique<MultipleValueRange>(slot_, start, end);
}


std::string
MultipleValueRange::name() const
{
	return "MultipleValueRange";
}


std::string
MultipleValueRange::serialise() const
{
	std::vector<std::string> data = { serialise_length(slot_), start, end };
	return StringList::serialise(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueRange::unserialise(std::string_view serialised) const
{
	try {
		StringList data(serialised);

		if (data.size() != 3) {
			throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueRange");
		}

		auto it = data.begin();
		const auto slot = unserialise_length(*it);
		std::string start(*++it);
		std::string end(*++it);
		return std::make_unique<MultipleValueRange>(slot, std::move(start), std::move(end));
	} catch (const SerialisationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueRange");
	}
}


std::string
MultipleValueRange::description() const
{
	std::string result("MultipleValueRange ");
	result += std::to_string(slot_) + " " + start;
	result += " " + end;
	return result;
}


template <typename T, typename>
MultipleValueGE::MultipleValueGE(engine::valueno slot, T&& start_)
	: slot_(slot),
	  start(std::forward<T>(start_)) { }


bool
MultipleValueGE::insideRange(std::string_view value) const noexcept
{
	// A value that does not decode is not accepted.
	try {
		StringList data(value);

		if (data.empty()) {
			return false;
		}

		return data.back() >= start;
	} catch (const SerialisationError&) {
		return false;
	}
}


double
MultipleValueGE::weight(std::string_view /*value*/) const
{
	return 1.0;
}


std::unique_ptr<engine::PostingSource>
MultipleValueGE::clone() const
{
	return std::make_unique<MultipleValueGE>(slot_, start);
}


std::string
MultipleValueGE::name() const
{
	return "MultipleValueGE";
}


std::string
MultipleValueGE::serialise() const
{
	std::vector<std::string> data = { serialise_length(slot_), start };
	return StringList::serialise(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueGE::unserialise(std::string_view serialised) const
{
	try {
		StringList data(serialised);

		if (data.size() != 2) {
			throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueGE");
		}

		auto it = data.begin();
		const auto slot = unserialise_length(*it);
		std::string start(*++it);
		return std::make_unique<MultipleValueGE>(slot, std::move(start));
	} catch (const SerialisationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueGE");
	}
}


std::string
MultipleValueGE::description() const
{
	std::string result("MultipleValueGE ");
	result += std::to_string(slot_) + " " + start;
	return result;
}


template <typename T, typename>
MultipleValueLE::MultipleValueLE(engine::valueno slot, T&& end_)
	: slot_(slot),
	  end(std::forward<T>(end_)) { }


bool
MultipleValueLE::insideRange(std::string_view value) const noexcept
{
	// A value that does not decode is not accepted.
	try {
		StringList data(value);

		if (data.empty()) {
			return false;
		}

		return data.front() <= end;
	} catch (const SerialisationError&) {
		return false;
	}
}


double
MultipleValueLE::weight(std::string_view /*value*/) const
{
	return 1.0;
}


std::unique_ptr<engine::PostingSource>
MultipleValueLE::clone() const
{
	return std::make_unique<MultipleValueLE>(slot_, end);
}


std::string
MultipleValueLE::name() const
{
	return "MultipleValueLE";
}


std::string
MultipleValueLE::serialise() const
{
	std::vector<std::string> data = { serialise_length(slot_), end };
	return StringList::serialise(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueLE::unserialise(std::string_view serialised) const
{
	try {
		StringList data(serialised);

		if (data.size() != 2) {
			throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueLE");
		}

		auto it = data.begin();
		const auto slot = unserialise_length(*it);
		std::string end(*++it);
		return std::make_unique<MultipleValueLE>(slot, std::move(end));
	} catch (const SerialisationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialised MultipleValueLE");
	}
}


std::string
MultipleValueLE::description() const
{
	std::string result("MultipleValueLE ");
	result += std::to_string(slot_) + " " + end;
	return result;
}
