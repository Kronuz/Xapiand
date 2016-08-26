/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "../serialise.h"
#include "../stl_serialise.h"
#include "../fields.h"
#include "../utils.h"
#include "generate_terms.h"
#include "geospatialrange.h"


template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
Xapian::Query filterNumericQuery(const data_field_t& field_spc, T start, T end, const std::string& start_s, const std::string& end_s) {
	auto mvr = new MultipleValueRange(field_spc.slot, start_s, end_s);

	auto filter_terms = GenerateTerms::numeric(start, end, field_spc.accuracy, field_spc.acc_prefix);
	if (filter_terms.first.empty()) {
		return Xapian::Query(mvr->release());
	} else {
		Xapian::QueryParser queryparser;
		for (const auto& prefix : filter_terms.second) {
			auto nfp = new NumericFieldProcessor(field_spc.type, prefix);
			queryparser.add_prefix(prefix, nfp->release());
		}
		return Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_terms.first), Xapian::Query(mvr->release()));
	}
}


MultipleValueRange::MultipleValueRange(Xapian::valueno slot_, const std::string& start_, const std::string& end_)
	: Xapian::ValuePostingSource(slot_), start(start_), end(end_)
{
	set_maxweight(1.0);
}


// Receive start and end, they are not serialized.
Xapian::Query
MultipleValueRange::getQuery(const data_field_t& field_spc, const std::string& field_name, const std::string& start, const std::string& end)
{
	try {
		if (start.empty()) {
			if (end.empty()) {
				return Xapian::Query::MatchAll;
			}

			if (field_spc.type == GEO_TYPE) {
				auto geo = EWKT_Parser::getGeoSpatial(end, field_spc.accuracy[0], field_spc.accuracy[1]);

				if (geo.ranges.empty()) {
					return Xapian::Query::MatchNothing;
				}

				auto GeoQuery = GeoSpatialRange::getQuery(field_spc.slot, geo.ranges, geo.centroids);

				auto filter_term = GenerateTerms::geo(geo.ranges, field_spc.accuracy, field_spc.acc_prefix);
				if (filter_term.first.empty()) {
					return GeoQuery;
				} else {
					Xapian::QueryParser queryparser;
					for (const auto& prefix : filter_term.second) {
						auto gfp = new GeoFieldProcessor(prefix);
						queryparser.add_prefix(prefix, gfp->release());
					}
					return Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term.first), GeoQuery);
				}
			} else {
				auto mvle = new MultipleValueLE(field_spc.slot, Serialise::serialise(field_spc.type, end));
				return Xapian::Query(mvle->release());
			}
		} else if (end.empty()) {
			if (field_spc.type == GEO_TYPE) {
				throw MSG_SerialisationError("The format for Geo Spatial range is: field_name:\"..EWKT\"");
			}

			auto mvge = new MultipleValueGE(field_spc.slot, Serialise::serialise(field_spc.type, start));
			return Xapian::Query(mvge->release());
		}

		switch (field_spc.type) {
			case FLOAT_TYPE: {
				auto start_v = strict(std::stod, start);
				auto end_v   = strict(std::stod, end);
				if (start_v > end_v) {
					return Xapian::Query::MatchNothing;
				}
				return filterNumericQuery(field_spc, (int64_t)start_v, (int64_t)end_v, Serialise::_float(start_v), Serialise::_float(end_v));
			}
			case INTEGER_TYPE: {
				auto start_v = strict(std::stoll, start);
				auto end_v   = strict(std::stoll, end);
				if (start_v > end_v) {
					return Xapian::Query::MatchNothing;
				}
				return filterNumericQuery(field_spc, start_v, end_v, Serialise::integer(start_v), Serialise::integer(end_v));
			}
			case POSITIVE_TYPE: {
				auto start_v = strict(std::stoull, start);
				auto end_v   = strict(std::stoull, end);
				if (start_v > end_v) {
					return Xapian::Query::MatchNothing;
				}
				return filterNumericQuery(field_spc, start_v, end_v, Serialise::positive(start_v), Serialise::positive(end_v));
			}
			case TEXT_TYPE:
			case STRING_TYPE: {
				if (start > end) {
					return Xapian::Query::MatchNothing;
				}

				auto mvr = new MultipleValueRange(field_spc.slot, start, end);
				return Xapian::Query(mvr->release());
			}
			case DATE_TYPE: {
				auto timestamp_s = Datetime::timestamp(start);
				auto timestamp_e = Datetime::timestamp(end);

				if (timestamp_s > timestamp_e) {
					return Xapian::Query::MatchNothing;
				}

				auto start_s = Serialise::timestamp(timestamp_s);
				auto end_s   = Serialise::timestamp(timestamp_e);

				auto mvr = new MultipleValueRange(field_spc.slot, start_s, end_s);

				auto filter_terms = GenerateTerms::date(timestamp_s, timestamp_e, field_spc.accuracy, field_spc.acc_prefix);
				if (filter_terms.first.empty()) {
					return Xapian::Query(mvr->release());
				} else {
					Xapian::QueryParser queryparser;
					for (const auto& prefix : filter_terms.second) {
						auto dfp = new DateFieldProcessor(prefix);
						queryparser.add_prefix(prefix, dfp->release());
					}
					return Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_terms.first), Xapian::Query(mvr->release()));
				}
			}
			case GEO_TYPE:
				throw MSG_QueryParserError("The format for Geo Spatial range is: field_name:\"..EWKT\"");
			default:
				return Xapian::Query::MatchNothing;
		}
	} catch (const Exception& exc) {
		throw MSG_QueryParserError("Failed to serialize: %s:%s..%s like %s (%s)", field_name.c_str(), start.c_str(), end.c_str(),
			Serialise::type(field_spc.type).c_str(), exc.what());
	}
}


bool
MultipleValueRange::insideRange() const noexcept
{
	StringList list;
	list.unserialise(get_value());
	for (const auto& value_ : list) {
		if (value_ >= start && value_ <= end) {
			return true;
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
	std::string serialised, values, s_slot(sortable_serialise(get_slot()));
	values.append(serialise_length(s_slot.size()));
	values.append(s_slot);
	values.append(serialise_length(start.size()));
	values.append(start);
	values.append(serialise_length(end.size()));
	values.append(end);
	serialised.append(serialise_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueRange*
MultipleValueRange::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	StringList data;
	data.unserialise(s);
	return new MultipleValueRange(sortable_unserialise(data[0]), data[1], data[2]);
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


MultipleValueGE::MultipleValueGE(Xapian::valueno slot_, const std::string& start_)
	: Xapian::ValuePostingSource(slot_), start(start_)
{
	set_maxweight(1.0);
}


bool
MultipleValueGE::insideRange() const noexcept
{
	StringList list;
	list.unserialise(get_value());
	for (const auto& value_ : list) {
		if (value_ >= start) {
			return true;
		}
	}
	return false;
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
	std::string serialised, values, s_slot(sortable_serialise(get_slot()));
	values.append(serialise_length(s_slot.size()));
	values.append(s_slot);
	values.append(serialise_length(start.size()));
	values.append(start);
	serialised.append(serialise_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueGE*
MultipleValueGE::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	StringList data;
	data.unserialise(s);
	return new MultipleValueGE(sortable_unserialise(data[0]), data[1]);
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
	result += std::to_string(get_slot()) + " " + start + ")";
	return result;
}


MultipleValueLE::MultipleValueLE(Xapian::valueno slot_, const std::string& end_)
	: Xapian::ValuePostingSource(slot_), end(end_)
{
	set_maxweight(1.0);
}


bool
MultipleValueLE::insideRange() const noexcept
{
	StringList list;
	list.unserialise(get_value());
	for (const auto& value_ : list) {
		if (value_ <= end) {
			return true;
		}
	}
	return false;
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
	std::string serialised, values, s_slot(sortable_serialise(get_slot()));
	values.append(serialise_length(s_slot.size()));
	values.append(s_slot);
	values.append(serialise_length(end.size()));
	values.append(end);
	serialised.append(serialise_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueLE*
MultipleValueLE::unserialise_with_registry(const std::string& s, const Xapian::Registry&) const
{
	StringList data;
	data.unserialise(s);
	return new MultipleValueLE(sortable_unserialise(data[0]), data[1]);
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
	result += std::to_string(get_slot()) + " " + end + ")";
	return result;
}
