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

#include "range.h"

#include <stdexcept>                       // for out_of_range
#include <sys/types.h>                     // for int64_t, uint64_t
#include <type_traits>                     // for decay_t, enable_if_t, is_i...
#include <vector>                          // for vector

#include "datetime.h"                      // for timestamp
#include "exception.h"                     // for MSG_QueryParserError, Quer...
#include "generate_terms.h"                // for date, geo, numeric
#include "geo/ewkt.h"                      // for EWKT
#include "geospatialrange.h"               // for GeoSpatialRange
#include "length.h"                        // for serialise_length
#include "query_dsl.h"                     // for QUERYDSL_FROM, QUERYDSL_TO
#include "schema.h"                        // for required_spc_t, FieldType
#include "serialise.h"                     // for MsgPack, cast, _float, int...
#include "utils.h"                         // for stox


template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
Xapian::Query filterNumericQuery(const required_spc_t& field_spc, const MsgPack& start, const MsgPack& end) {
	std::string ser_start, ser_end;
	T value_s, value_e;
	switch (field_spc.get_type()) {
		case FieldType::FLOAT: {
			double val_s = start.is_map() ? Cast::cast(start).as_f64() : Cast::_float(start);
			double val_e = end.is_map() ? Cast::cast(end).as_f64() : Cast::_float(end);
			if (val_s > val_e) {
				return Xapian::Query::MatchNothing;
			}

			ser_start = Serialise::_float(val_s);
			ser_end = Serialise::_float(val_e);
			value_s = val_s;
			value_e = val_e;
			break;
		}
		case FieldType::INTEGER: {
			int64_t val_s = start.is_map() ? Cast::cast(start).as_i64() : Cast::integer(start);
			int64_t val_e = end.is_map() ? Cast::cast(end).as_i64() : Cast::integer(end);
			if (val_s > val_e) {
				return Xapian::Query::MatchNothing;
			}

			ser_start = Serialise::integer(val_s);
			ser_end = Serialise::integer(val_e);
			value_s = val_s;
			value_e = val_e;
			break;
		}
		case FieldType::POSITIVE: {
			uint64_t val_s = start.is_map() ? Cast::cast(start).as_u64() : Cast::positive(start);
			uint64_t val_e = end.is_map() ? Cast::cast(end).as_u64() : Cast::positive(end);
			if (val_s > val_e) {
				return Xapian::Query::MatchNothing;
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

	auto query = GenerateTerms::numeric(value_s, value_e, field_spc.accuracy, field_spc.acc_prefix);
	auto mvr = new MultipleValueRange(field_spc.slot, std::move(ser_start), std::move(ser_end));
	if (query.empty()) {
		return Xapian::Query(mvr->release());
	} else {
		return Xapian::Query(Xapian::Query::OP_AND, query, Xapian::Query(mvr->release()));
	}
}


Xapian::Query filterStringQuery(const required_spc_t& field_spc, std::string&& start_s, std::string&& end_s) {
	if (start_s > end_s) {
		return Xapian::Query::MatchNothing;
	}

	auto mvr = new MultipleValueRange(field_spc.slot, std::move(start_s), std::move(end_s));
	return Xapian::Query(mvr->release());
}


Xapian::Query filterDateQuery(const required_spc_t& field_spc, const MsgPack& start, const MsgPack& end) {
	auto timestamp_s = Datetime::timestamp(start);
	auto timestamp_e = Datetime::timestamp(end);

	if (timestamp_s > timestamp_e) {
		return Xapian::Query::MatchNothing;
	}

	auto query = GenerateTerms::date(timestamp_s, timestamp_e, field_spc.accuracy, field_spc.acc_prefix);
	auto mvr = new MultipleValueRange(field_spc.slot, Serialise::timestamp(timestamp_s), Serialise::timestamp(timestamp_e));
	if (query.empty()) {
		return Xapian::Query(mvr->release());
	} else {
		return Xapian::Query(Xapian::Query::OP_AND, query, Xapian::Query(mvr->release()));
	}
}


Xapian::Query filterGeoQuery(const required_spc_t& field_spc, const MsgPack& obj) {
	EWKT ewkt(obj.as_string());

	auto ranges = ewkt.geometry->getRanges(field_spc.flags.partials, field_spc.error);

	if (ranges.empty()) {
		return Xapian::Query::MatchNothing;
	}

	auto query = GenerateTerms::geo(ranges, field_spc.accuracy, field_spc.acc_prefix);
	auto geoQ = GeoSpatialRange::getQuery(field_spc.slot, std::move(ranges));
	if (query.empty()) {
		return geoQ;
	} else {
		return Xapian::Query(Xapian::Query::OP_AND, query, geoQ);
	}
}


Xapian::Query
MultipleValueRange::getQuery(const required_spc_t& field_spc, const MsgPack& obj)
{
	const MsgPack* start = nullptr;
	const MsgPack* end = nullptr;

	auto it = obj.find(QUERYDSL_FROM);
	if (it != obj.end()) {
		start = &it.value();
	}

	it = obj.find(QUERYDSL_TO);
	if (it != obj.end()) {
		end = &it.value();
	}

	try {
		if (!start) {
			if (!end) {
				return Xapian::Query::MatchAll;
			}
			if (field_spc.get_type() == FieldType::GEO) {
				return filterGeoQuery(field_spc, *end);
			}
			auto mvle = new MultipleValueLE(field_spc.slot, Serialise::MsgPack(field_spc, *end));
			return Xapian::Query(mvle->release());
		}

		if (!end) {
			if (field_spc.get_type() == FieldType::GEO) {
				return filterGeoQuery(field_spc, *start);
			}
			auto mvge = new MultipleValueGE(field_spc.slot, Serialise::MsgPack(field_spc, *start));
			return Xapian::Query(mvge->release());
		}

		switch (field_spc.get_type()) {
			case FieldType::INTEGER:
			case FieldType::FLOAT:
				return filterNumericQuery<int64_t>(field_spc, *start, *end);
			case FieldType::POSITIVE:
				return filterNumericQuery<uint64_t>(field_spc, *start, *end);
			case FieldType::UUID:
			case FieldType::BOOLEAN:
			case FieldType::TERM:
			case FieldType::TEXT:
			case FieldType::STRING:
				return filterStringQuery(field_spc, Serialise::MsgPack(field_spc, *start), Serialise::MsgPack(field_spc, *end));
			case FieldType::DATE:
				return filterDateQuery(field_spc, *start, *end);
			case FieldType::GEO:
				THROW(QueryParserError, "The format for Geo Spatial range is: <field>:[\"EWKT\"]");
			default:
				return Xapian::Query::MatchNothing;
		}
	} catch (const Exception& exc) {
		THROW(QueryParserError, "Failed to serialize: %s - %s like %s (%s)", start->to_string().c_str(), end->to_string().c_str(),
			Serialise::type(field_spc.get_type()).c_str(), exc.what());
	}
}


template <typename T, typename>
MultipleValueRange::MultipleValueRange(Xapian::valueno slot_, T&& start_, T&& end_)
	: Xapian::ValuePostingSource(slot_),
	  start(std::forward<T>(start_)),
	  end(std::forward<T>(end_)) { }


bool
MultipleValueRange::insideRange() const noexcept
{
	std::vector<std::string> data;
	Unserialise::STLString(get_value(), std::back_inserter(data));

	if (data.empty() || start > data.back() || end < data.front()) {
		return false;
	}

	for (const auto& value_ : data) {
		if (value_ >= start) {
			return value_ <= end;
		}
	}

	return false;
}


void
MultipleValueRange::next(double min_wt)
{
	Xapian::ValuePostingSource::next(min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
MultipleValueRange::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


bool
MultipleValueRange::check(Xapian::docid min_docid, double min_wt)
{
	if (!Xapian::ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (at_end()) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueRange::get_weight() const
{
	return 1.0;
}


MultipleValueRange*
MultipleValueRange::clone() const
{
	return new MultipleValueRange(get_slot(), start, end);
}


std::string
MultipleValueRange::name() const
{
	return "MultipleValueRange";
}


std::string
MultipleValueRange::serialise() const
{
	std::vector<std::string> data = { serialise_length(get_slot()), start, end };
	return Serialise::STLString(data.begin(), data.end());
}


MultipleValueRange*
MultipleValueRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	std::vector<std::string> data;
	Unserialise::STLString(s, std::back_inserter(data));
	return new MultipleValueRange(unserialise_length(data.at(0)), std::move(data.at(1)), std::move(data.at(2)));
}


void
MultipleValueRange::init(const Xapian::Database& db_)
{
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	set_termfreq_min(0);
}


std::string
MultipleValueRange::get_description() const
{
	std::string result("MultipleValueRange ");
	result += std::to_string(get_slot()) + " " + start;
	result += " " + end;
	return result;
}


template <typename T, typename>
MultipleValueGE::MultipleValueGE(Xapian::valueno slot_, T&& start_)
	: Xapian::ValuePostingSource(slot_),
	  start(std::forward<T>(start_)) { }


bool
MultipleValueGE::insideRange() const noexcept
{
	std::vector<std::string> data;
	Unserialise::STLString(get_value(), std::back_inserter(data));

	if (data.empty()) {
		return false;
	}

	return data.back() >= start;
}


void
MultipleValueGE::next(double min_wt)
{
	Xapian::ValuePostingSource::next(min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
MultipleValueGE::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


bool
MultipleValueGE::check(Xapian::docid min_docid, double min_wt)
{
	if (!Xapian::ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (at_end()) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueGE::get_weight() const
{
	return 1.0;
}


MultipleValueGE*
MultipleValueGE::clone() const
{
	return new MultipleValueGE(get_slot(), start);
}


std::string
MultipleValueGE::name() const
{
	return "MultipleValueGE";
}


std::string
MultipleValueGE::serialise() const
{
	std::vector<std::string> data = { serialise_length(get_slot()), start };
	return Serialise::STLString(data.begin(), data.end());
}


MultipleValueGE*
MultipleValueGE::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	std::vector<std::string> data;
	Unserialise::STLString(s, std::back_inserter(data));
	return new MultipleValueGE(unserialise_length(data.at(0)), std::move(data.at(1)));
}


void
MultipleValueGE::init(const Xapian::Database& db_)
{
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	set_termfreq_min(0);
}


std::string
MultipleValueGE::get_description() const
{
	std::string result("MultipleValueGE ");
	result += std::to_string(get_slot()) + " " + start;
	return result;
}


template <typename T, typename>
MultipleValueLE::MultipleValueLE(Xapian::valueno slot_, T&& end_)
	: Xapian::ValuePostingSource(slot_),
	  end(std::forward<T>(end_)) { }


bool
MultipleValueLE::insideRange() const noexcept
{
	std::vector<std::string> data;
	Unserialise::STLString(get_value(), std::back_inserter(data));

	if (data.empty()) {
		return false;
	}

	return data.front() <= end;
}


void
MultipleValueLE::next(double min_wt)
{
	Xapian::ValuePostingSource::next(min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
MultipleValueLE::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		if (insideRange()) break;
		Xapian::ValuePostingSource::next(min_wt);
	}
}


bool
MultipleValueLE::check(Xapian::docid min_docid, double min_wt)
{
	if (!Xapian::ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (at_end()) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueLE::get_weight() const
{
	return 1.0;
}


MultipleValueLE*
MultipleValueLE::clone() const
{
	return new MultipleValueLE(get_slot(), end);
}


std::string
MultipleValueLE::name() const
{
	return "MultipleValueLE";
}


std::string
MultipleValueLE::serialise() const
{
	std::vector<std::string> data = { serialise_length(get_slot()), end };
	return Serialise::STLString(data.begin(), data.end());
}


MultipleValueLE*
MultipleValueLE::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	std::vector<std::string> data;
	Unserialise::STLString(s, std::back_inserter(data));
	return new MultipleValueLE(unserialise_length(data.at(0)), std::move(data.at(1)));
}


void
MultipleValueLE::init(const Xapian::Database& db_)
{
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	set_termfreq_min(0);
}


std::string
MultipleValueLE::get_description() const
{
	std::string result("MultipleValueLE ");
	result += std::to_string(get_slot()) + " " + end;
	return result;
}
