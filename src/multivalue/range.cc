/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <cmath>                                  // for std::isnan
#include <cstdint>                                // for std::int64_t, std::uint64_t
#include <limits>                                 // for std::numeric_limits
#include <stdexcept>                              // for out_of_range
#include <sys/types.h>                            // for int64_t, uint64_t
#include <type_traits>                            // for decay_t, enable_if_t, is_i...
#include <utility>                                // for std::pair, std::cmp_less, std::cmp_greater
#include <vector>                                 // for vector

#include "database/schema.h"                      // for required_spc_t, FieldType
#include "datetime.h"                             // for timestamp
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads
#include "engine/error.h"                       // for engine::Error
#include "exception_engine.h"                   // for QueryParserError, TermsOnlyRangeError
#include "xapiand_exception.h"                            // for MSG_QueryParserError, Quer...
#include "generate_terms.h"                       // for datetime, geo, numeric
#include "geospatialrange.h"                      // for GeoSpatialRange
#include "length.h"                               // for serialize_length
#include "math.hh"                          // for max, min
#include "reserved/query_dsl.h"                   // for RESERVED_QUERYDSL_FROM, RESERVED_QUERYDSL_TO
#include "serialize_list.h"                       // for StringList


// Terms-only fields (no value slot) have no exact check behind the terms, so they get a larger budget.
constexpr size_t TERMS_ONLY_MAX_TERMS = 4096;

// A terms-only field has no value to check a range against, so a range its terms cannot cover exactly is
// an error: no terms means the range is too wide for the budget, and an open end has no finite cover.
static void
require_terms_only_cover(const required_spc_t& field_spc, bool open_ended, const engine::Query& query)
{
	if (field_spc.slot != engine::BAD_VALUENO) {
		return;
	}
	if (open_ended) {
		THROW(TermsOnlyRangeError, "the range is open-ended, and a terms-only field cannot answer an open-ended range exactly");
	}
	if (query.empty()) {
		THROW(TermsOnlyRangeError, "the range is too wide for the terms of a terms-only field, which cannot answer it exactly");
	}
}


// An integer as the nearest T, saturating at T's limits.
template <typename T, typename I>
static T
saturate(I v)
{
	if (std::cmp_less(v, std::numeric_limits<T>::min())) {
		return std::numeric_limits<T>::min();
	}
	if (std::cmp_greater(v, std::numeric_limits<T>::max())) {
		return std::numeric_limits<T>::max();
	}
	return static_cast<T>(v);
}


// A numeric range bound: its bytes in its own type, and the T its accuracy terms cover. QueryDSL reads each bound as a
// value of the field's type (Serialize::exact, D92), so it arrives a number; one number has one encoding whatever its
// type, so 2^53 + 1 on a floating field compares exactly against the slot.
template <typename T>
static std::pair<std::string, T>
numeric_bound(const MsgPack& bound)
{
	switch (bound.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			return { Serialize::positive(bound.u64()), saturate<T>(bound.u64()) };
		case MsgPack::Type::NEGATIVE_INTEGER:
			return { Serialize::integer(bound.i64()), saturate<T>(bound.i64()) };
		case MsgPack::Type::FLOAT:
			return { Serialize::floating(bound.f64()), GenerateTerms::truncate<T>(bound.f64()) };
		default:
			THROW(SerializationError, "A range bound must be a number, got {}", bound.to_string());
	}
}


template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
engine::Query
getNumericQuery(const required_spc_t& field_spc, const MsgPack* start, const MsgPack* end)
{
	// An absent bound is unbounded: -inf or +inf, whatever the field's type (design D6).
	auto [ser_start, value_s] = start ? numeric_bound<T>(*start)
		: std::pair<std::string, T>(Serialize::floating(-std::numeric_limits<double>::infinity()), std::numeric_limits<T>::min());
	auto [ser_end, value_e] = end ? numeric_bound<T>(*end)
		: std::pair<std::string, T>(Serialize::floating(std::numeric_limits<double>::infinity()), std::numeric_limits<T>::max());

	if (ser_start > ser_end) {
		return engine::Query();
	}

	auto query = GenerateTerms::numeric(value_s, value_e, field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);
	require_terms_only_cover(field_spc, !start || !end, query);

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
		std::string start_s = start ? Serialize::MsgPack(field_spc, *start) : "";
		std::string end_s = end ? Serialize::MsgPack(field_spc, *end) : "";

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
		ser_start = Serialize::timestamp(timestamp_s);
	}
	if (end) {
		ser_end = Serialize::timestamp(timestamp_e);
	}

	auto query = start && end ? GenerateTerms::datetime(timestamp_s, timestamp_e, field_spc.accuracy, field_spc.acc_prefix) : engine::Query();
	require_terms_only_cover(field_spc, !start || !end, query);

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
		ser_start = Serialize::timestamp(time_s);
	}
	if (end) {
		ser_end = Serialize::timestamp(time_e);
	}

	auto query = GenerateTerms::numeric(GenerateTerms::truncate<int64_t>(time_s), GenerateTerms::truncate<int64_t>(time_e), field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);
	require_terms_only_cover(field_spc, !start || !end, query);

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
		ser_start = Serialize::timestamp(timedelta_s);
	}
	if (end) {
		ser_end = Serialize::timestamp(timedelta_e);
	}

	auto query = GenerateTerms::numeric(GenerateTerms::truncate<int64_t>(timedelta_s), GenerateTerms::truncate<int64_t>(timedelta_e), field_spc.accuracy, field_spc.acc_prefix, 1, field_spc.slot != engine::BAD_VALUENO ? MAX_TERMS : TERMS_ONLY_MAX_TERMS);
	require_terms_only_cover(field_spc, !start || !end, query);

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


// A NaN bound selects no well-defined range, whatever the field's numeric or date type.
static void
refuse_nan(const char* name, const MsgPack* bound)
{
	if (bound && bound->get_type() == MsgPack::Type::FLOAT && std::isnan(bound->f64())) {
		THROW(SerializationError, "A range bound '{}' cannot be NaN", name);
	}
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
			case FieldType::positive:
			case FieldType::date:
			case FieldType::datetime:
			case FieldType::time:
			case FieldType::timedelta:
				refuse_nan(RESERVED_QUERYDSL_FROM, start);
				refuse_nan(RESERVED_QUERYDSL_TO, end);
				break;
			default:
				break;
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
	} catch (const TermsOnlyRangeError&) {
		throw;
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
	} catch (const SerializationError&) {
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
MultipleValueRange::serialize() const
{
	std::vector<std::string> data = { serialize_length(slot_), start, end };
	return StringList::serialize(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueRange::unserialize(std::string_view serialized) const
{
	try {
		StringList data(serialized);

		if (data.size() != 3) {
			throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueRange");
		}

		auto it = data.begin();
		const auto slot = unserialize_length(*it);
		std::string start(*++it);
		std::string end(*++it);
		return std::make_unique<MultipleValueRange>(slot, std::move(start), std::move(end));
	} catch (const SerializationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueRange");
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
	} catch (const SerializationError&) {
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
MultipleValueGE::serialize() const
{
	std::vector<std::string> data = { serialize_length(slot_), start };
	return StringList::serialize(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueGE::unserialize(std::string_view serialized) const
{
	try {
		StringList data(serialized);

		if (data.size() != 2) {
			throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueGE");
		}

		auto it = data.begin();
		const auto slot = unserialize_length(*it);
		std::string start(*++it);
		return std::make_unique<MultipleValueGE>(slot, std::move(start));
	} catch (const SerializationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueGE");
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
	} catch (const SerializationError&) {
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
MultipleValueLE::serialize() const
{
	std::vector<std::string> data = { serialize_length(slot_), end };
	return StringList::serialize(data.begin(), data.end());
}


std::unique_ptr<engine::PostingSource>
MultipleValueLE::unserialize(std::string_view serialized) const
{
	try {
		StringList data(serialized);

		if (data.size() != 2) {
			throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueLE");
		}

		auto it = data.begin();
		const auto slot = unserialize_length(*it);
		std::string end(*++it);
		return std::make_unique<MultipleValueLE>(slot, std::move(end));
	} catch (const SerializationError& er) {
		throw engine::Error(engine::errc::invalid, "Bad serialized MultipleValueLE");
	}
}


std::string
MultipleValueLE::description() const
{
	std::string result("MultipleValueLE ");
	result += std::to_string(slot_) + " " + end;
	return result;
}
