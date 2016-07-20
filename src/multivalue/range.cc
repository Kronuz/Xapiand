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

#include "../length.h"
#include "../serialise.h"
#include "../stl_serialise.h"
#include "../utils.h"


MultipleValueRange::MultipleValueRange(Xapian::valueno slot_, const std::string& start_, const std::string& end_)
	: Xapian::ValuePostingSource(slot_), start(start_), end(end_)
{
	set_maxweight(1.0);
}


// Receive start and end did not serialize.
Xapian::Query
MultipleValueRange::getQuery(Xapian::valueno slot_, char field_type, const std::string& start_, const std::string& end_, const std::string& field_name)
{
	if (start_.empty()) {
		if (end_.empty()){
			return Xapian::Query::MatchAll;
		}
		try {
			auto mvle = new MultipleValueLE(slot_, Serialise::serialise(field_type, end_));
			return Xapian::Query(mvle->release());
		} catch (const Exception& exc) {
			throw MSG_QueryParserError("Failed to serialize: " + field_name + ":" + start_ + ".." + end_ + " like " + Serialise::type(field_type) + " (" + exc.what() +")");
		}
	} else if (end_.empty()) {
		try {
			auto mvge = new MultipleValueGE(slot_, Serialise::serialise(field_type, start_));
			return Xapian::Query(mvge->release());
		} catch (const Exception& exc) {
			throw MSG_QueryParserError("Failed to serialize: " + field_name + ":" + start_ + ".." + end_ + " like " + Serialise::type(field_type) + " (" + exc.what() +")");
		}
	}

	// Multiple Value Range
	if (start_ > end_) {
		return Xapian::Query::MatchNothing;
	}

	try {
		auto mvr = new MultipleValueRange(slot_, Serialise::serialise(field_type, start_), Serialise::serialise(field_type, end_));
		return Xapian::Query(mvr->release());
	} catch (const Exception& exc) {
		throw MSG_QueryParserError("Failed to serialize: " + field_name + ":" + start_ + ".." + end_ + " like " + Serialise::type(field_type) + " (" + exc.what() +")");
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
	std::string serialised, values, s_slot(Xapian::sortable_serialise(get_slot()));
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
	return new MultipleValueRange(Xapian::sortable_unserialise(data[0]), data[1], data[2]);
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
	std::string serialised, values, s_slot(Xapian::sortable_serialise(get_slot()));
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
	return new MultipleValueGE(Xapian::sortable_unserialise(data[0]), data[1]);
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
	std::string serialised, values, s_slot(Xapian::sortable_serialise(get_slot()));
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
	return new MultipleValueLE(Xapian::sortable_unserialise(data[0]), data[1]);
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
