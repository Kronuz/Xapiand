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

#include "multivalue.h"

#include "length.h"
#include "utils.h"

#include <assert.h>


void
StringList::unserialise(const std::string & serialised)
{
	const char * ptr = serialised.data();
	const char * end = serialised.data() + serialised.size();
	unserialise(&ptr, end);
}


void
StringList::unserialise(const char ** ptr, const char * end)
{
	const char *pos = *ptr;
	clear();
	size_t length = decode_length(&pos, end, true);
	if (length == -1 || length != end - pos) {
		push_back(std::string(pos, end - pos));
	} else {
		size_t currlen;
		while (pos != end) {
			currlen = decode_length(&pos, end, true);
			if (currlen == -1) {
				// FIXME: throwing a NetworkError if the length is too long - should be a more appropriate error.
				throw Xapian::NetworkError("Decoding error of serialised MultiValueCountMatchSpy");
			}
			push_back(std::string(pos, currlen));
			pos += currlen;
		}
	}
	*ptr = pos;
}


std::string
StringList::serialise() const
{
	std::string serialised, values;
	StringList::const_iterator i(begin());
	if (size() > 1) {
		for (; i != end(); i++) {
			values.append(encode_length((*i).size()));
			values.append(*i);
		}
		serialised.append(encode_length(values.size()));
	} else if (i != end()) {
		values.assign(*i);
	}
	serialised.append(values);
	return serialised;
}


void
CartesianList::unserialise(const std::string & serialised)
{
	for (size_t i = 0, j =  SIZE_SERIALISE_CARTESIAN; i < serialised.size(); i = j, j += SIZE_SERIALISE_CARTESIAN) {
		push_back(unserialise_cartesian(serialised.substr(i, j)));
	}
}


std::string
CartesianList::serialise() const
{
	std::string serialised;
	CartesianList::const_iterator i(begin());
	for ( ; i != end(); i++) {
		serialised.append(serialise_cartesian(*i));
	}

	return serialised;
}


void
uInt64List::unserialise(const std::string & serialised)
{
	for (size_t i = 0, j = SIZE_BYTES_ID; i < serialised.size(); i = j, j += SIZE_BYTES_ID) {
		push_back(unserialise_geo(serialised.substr(i, j)));
	}
}


std::string
uInt64List::serialise() const
{
	std::string serialised;
	uInt64List::const_iterator i(begin());
	for ( ; i != end(); i++) {
		serialised.append(serialise_geo(*i));
	}

	return serialised;
}


void
MultiValueCountMatchSpy::operator()(const Xapian::Document &doc, double) {
	assert(internal.get());
	++(internal->total);
	StringList list;
	list.unserialise(doc.get_value(internal->slot));
	StringList::const_iterator i(list.begin());
	for (; i != list.end(); i++) {
		std::string val(*i);
		if (!val.empty()) ++(internal->values[val]);
	}
}


Xapian::MatchSpy *
MultiValueCountMatchSpy::clone() const {
	assert(internal.get());
	return new MultiValueCountMatchSpy(internal->slot);
}


std::string
MultiValueCountMatchSpy::name() const {
	return "Xapian::MultiValueCountMatchSpy";
}


std::string
MultiValueCountMatchSpy::serialise() const {
	assert(internal.get());
	std::string result;
	result += encode_length(internal->slot);
	return result;
}


Xapian::MatchSpy *
MultiValueCountMatchSpy::unserialise(const std::string & s,
									 const Xapian::Registry &) const {
	const char * p = s.data();
	const char * end = p + s.size();

	Xapian::valueno new_slot = (Xapian::valueno)decode_length(&p, end, false);
	if (new_slot == -1) {
		throw Xapian::NetworkError("Decoding error of serialised MultiValueCountMatchSpy");
	}
	if (p != end) {
		throw Xapian::NetworkError("Junk at end of serialised MultiValueCountMatchSpy");
	}

	return new MultiValueCountMatchSpy(new_slot);
}


std::string
MultiValueCountMatchSpy::get_description() const {
	char buffer[20];
	std::string d = "MultiValueCountMatchSpy(";
	if (internal.get()) {
		snprintf(buffer, sizeof(buffer), "%u", internal->total);
		d += buffer;
		d += " docs seen, looking in ";
		snprintf(buffer, sizeof(buffer), "%lu", internal->values.size());
		d += buffer;
		d += " slots)";
	} else {
		d += ")";
	}
	return d;
}


MultipleValueRange::MultipleValueRange(Xapian::valueno slot_, const std::string &start_, const std::string &end_)
	: ValuePostingSource(slot_), slot(slot_), start(start_), end(end_) {
	set_maxweight(1.0);
}


// Receive start and end did not serialize.
Xapian::Query
MultipleValueRange::getQuery(Xapian::valueno slot_, char field_type, std::string start_, std::string end_, const std::string &field_name) {
	if (start_.empty() && end_.empty()){
		return Xapian::Query::MatchAll;
	} else if (start_.empty()) {
		end_ = ::serialise(field_type, end_);
		if (end_.empty()) throw Xapian::QueryParserError("Failed to serialize '" + field_name + "'");
		MultipleValueLE mvle(slot_, end_);
		return Xapian::Query(&mvle);
	} else if (end_.empty()) {
		start_ = ::serialise(field_type, start_);
		if (start_.empty()) throw Xapian::QueryParserError("Failed to serialize '" + field_name + "'");
		MultipleValueGE mvge(slot_, start_);
		return Xapian::Query(&mvge);
	}

	// Multiple Value Range
	start_ = ::serialise(field_type, start_);
	if (start_.empty()) throw Xapian::QueryParserError("Failed to serialize '" + field_name + "'");
	end_ = ::serialise(field_type, end_);
	if (end_.empty()) throw Xapian::QueryParserError("Failed to serialize '" + field_name + "'");
	if (start_ > end_) return Xapian::Query::MatchNothing;
	MultipleValueRange mvr(slot_, start_, end_);
	return Xapian::Query(&mvr);
}


bool
MultipleValueRange::insideRange() {
	StringList list;
	list.unserialise(*value_it);
	StringList::const_iterator i(list.begin());
	for ( ; i != list.end(); i++) {
		if (*i >= start && *i <= end) {
			return true;
		}
	}
	return false;
}


void
MultipleValueRange::next(double min_wt) {
	Xapian::ValuePostingSource::next(min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


void
MultipleValueRange::skip_to(Xapian::docid min_docid, double min_wt) {
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


bool
MultipleValueRange::check(Xapian::docid min_docid, double min_wt) {
	if (!ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (value_it == db.valuestream_end(slot)) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueRange::get_weight() const {
	return 1.0;
}


MultipleValueRange*
MultipleValueRange::clone() const {
	return new MultipleValueRange(slot, start, end);
}


std::string
MultipleValueRange::name() const {
	return "MultipleValueRange";
}


std::string
MultipleValueRange::serialise() const {
	std::string serialised, values, s_slot(Xapian::sortable_serialise(slot));
	values.append(encode_length(s_slot.size()));
	values.append(s_slot);
	values.append(encode_length(start.size()));
	values.append(start);
	values.append(encode_length(end.size()));
	values.append(end);
	serialised.append(encode_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueRange*
MultipleValueRange::unserialise_with_registry(const std::string &s, const Xapian::Registry &) const {
	StringList data;
	data.unserialise(s);
	return new MultipleValueRange(Xapian::sortable_unserialise(data.at(0)), data.at(1), data.at(2));
}


void
MultipleValueRange::init(const Xapian::Database &db_) {
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	termfreq_min = 0;
}


std::string
MultipleValueRange::get_description() const {
	std::string result("MultipleValueRange ");
	result += std::to_string(slot) + " " + start;
	result += " " + end;
	return result;
}


MultipleValueGE::MultipleValueGE(Xapian::valueno slot_, const std::string &start_)
	: ValuePostingSource(slot_), slot(slot_), start(start_) {
	set_maxweight(1.0);
}


bool
MultipleValueGE::insideRange() {
	StringList list;
	list.unserialise(*value_it);
	StringList::const_iterator i(list.begin());
	for ( ; i != list.end(); i++) {
		if (*i >= start) {
			return true;
		}
	}
	return false;
}


void
MultipleValueGE::next(double min_wt) {
	Xapian::ValuePostingSource::next(min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


void
MultipleValueGE::skip_to(Xapian::docid min_docid, double min_wt) {
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


bool
MultipleValueGE::check(Xapian::docid min_docid, double min_wt) {
	if (!ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (value_it == db.valuestream_end(slot)) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueGE::get_weight() const {
	return 1.0;
}


MultipleValueGE*
MultipleValueGE::clone() const {
	return new MultipleValueGE(slot, start);
}


std::string
MultipleValueGE::name() const {
	return "MultipleValueGE";
}


std::string
MultipleValueGE::serialise() const {
	std::string serialised, values, s_slot(Xapian::sortable_serialise(slot));
	values.append(encode_length(s_slot.size()));
	values.append(s_slot);
	values.append(encode_length(start.size()));
	values.append(start);
	serialised.append(encode_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueGE*
MultipleValueGE::unserialise_with_registry(const std::string &s, const Xapian::Registry &) const {
	StringList data;
	data.unserialise(s);
	return new MultipleValueGE(Xapian::sortable_unserialise(data.at(0)), data.at(1));
}


void
MultipleValueGE::init(const Xapian::Database &db_) {
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	termfreq_min = 0;
}


std::string
MultipleValueGE::get_description() const {
	std::string result("MultipleValueGE ");
	result += std::to_string(slot) + " " + start + ")";
	return result;
}


MultipleValueLE::MultipleValueLE(Xapian::valueno slot_, const std::string &end_)
	: ValuePostingSource(slot_), slot(slot_), end(end_) {
	set_maxweight(1.0);
}


bool
MultipleValueLE::insideRange() {
	StringList list;
	list.unserialise(*value_it);
	StringList::const_iterator i(list.begin());
	for ( ; i != list.end(); i++) {
		if (*i <= end) {
			return true;
		}
	}
	return false;
}


void
MultipleValueLE::next(double min_wt) {
	Xapian::ValuePostingSource::next(min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


void
MultipleValueLE::skip_to(Xapian::docid min_docid, double min_wt) {
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRange()) break;
		++value_it;
	}
}


bool
MultipleValueLE::check(Xapian::docid min_docid, double min_wt) {
	if (!ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (value_it == db.valuestream_end(slot)) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRange();
}


double
MultipleValueLE::get_weight() const {
	return 1.0;
}


MultipleValueLE*
MultipleValueLE::clone() const {
	return new MultipleValueLE(slot, end);
}


std::string
MultipleValueLE::name() const {
	return "MultipleValueLE";
}


std::string
MultipleValueLE::serialise() const {
	std::string serialised, values, s_slot(Xapian::sortable_serialise(slot));
	values.append(encode_length(s_slot.size()));
	values.append(s_slot);
	values.append(encode_length(end.size()));
	values.append(end);
	serialised.append(encode_length(values.size()));
	serialised.append(values);
	return serialised;
}


MultipleValueLE*
MultipleValueLE::unserialise_with_registry(const std::string &s, const Xapian::Registry &) const {
	StringList data;
	data.unserialise(s);
	return new MultipleValueLE(Xapian::sortable_unserialise(data.at(0)), data.at(1));
}


void
MultipleValueLE::init(const Xapian::Database &db_) {
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	termfreq_min = 0;
}


std::string
MultipleValueLE::get_description() const {
	std::string result("MultipleValueLE ");
	result += std::to_string(slot) + " " + end + ")";
	return result;
}


GeoSpatialRange::GeoSpatialRange(Xapian::valueno slot_, const std::vector<range_t> &ranges_)
	: ValuePostingSource(slot_), slot(slot_) {
	ranges.reserve(ranges_.size());
	std::vector<range_t>::const_iterator it(ranges_.begin());
	for ( ; it != ranges_.end(); it++) {
		ranges.push_back({serialise_geo(it->start), serialise_geo(it->end)});
	}
}


GeoSpatialRange::GeoSpatialRange(Xapian::valueno slot_, const std::vector<srange_t> &ranges_)
	: ValuePostingSource(slot_), slot(slot_) {
	ranges.reserve(ranges_.size());
	ranges.insert(ranges.begin(), ranges_.begin(), ranges_.end());
}


// Receive start and end did not serialize.
Xapian::Query
GeoSpatialRange::getQuery(Xapian::valueno slot_, const std::vector<range_t> &ranges_) {
	if (ranges_.empty()){
		return Xapian::Query::MatchNothing;
	}

	// GeoSpatial Range
	GeoSpatialRange gsr(slot_, ranges_);
	return Xapian::Query(&gsr);
}


bool
GeoSpatialRange::insideRanges() {
	StringList list;
	list.unserialise(*value_it);
	StringList::const_iterator i(list.begin());
	for ( ; i != list.end(); i += 2) {
		StringList::const_iterator o(i + 1);
		std::vector<srange_t>::const_iterator it(ranges.begin());
		for ( ; it != ranges.end(); it++) {
			if (*i <= it->end && *o >= it->start) {
				return true;
			}
		}
	}
	return false;
}


void
GeoSpatialRange::next(double min_wt) {
	Xapian::ValuePostingSource::next(min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRanges()) break;
		++value_it;
	}
}


void
GeoSpatialRange::skip_to(Xapian::docid min_docid, double min_wt) {
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (value_it != db.valuestream_end(slot)) {
		if (insideRanges()) break;
		++value_it;
	}
}


bool
GeoSpatialRange::check(Xapian::docid min_docid, double min_wt) {
	if (!ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (value_it == db.valuestream_end(slot)) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	return insideRanges();
}


double
GeoSpatialRange::get_weight() const
{
	return 1.0;
}


GeoSpatialRange*
GeoSpatialRange::clone() const {
	return new GeoSpatialRange(slot, ranges);
}


std::string
GeoSpatialRange::name() const {
	return "GeoSpatialRange";
}


std::string
GeoSpatialRange::serialise() const {
	std::string serialised, values, s_slot(Xapian::sortable_serialise(slot));
	values.append(encode_length(s_slot.size()));
	values.append(s_slot);
	std::vector<srange_t>::const_iterator it(ranges.begin());
	for ( ; it != ranges.end(); it++) {
		values.append(encode_length(it->start.size()));
		values.append(it->start);
		values.append(encode_length(it->end.size()));
		values.append(it->end);
	}
	serialised.append(encode_length(values.size()));
	serialised.append(values);
	return serialised;
}


GeoSpatialRange*
GeoSpatialRange::unserialise_with_registry(const std::string &s, const Xapian::Registry &) const {
	StringList data;
	std::vector<srange_t> ranges_;
	data.unserialise(s);
	std::vector<std::string>::const_iterator it(data.begin() + 1);
	for ( ; it != data.end(); ++it) {
		ranges_.push_back({*it, *(++it)});
	}
	return new GeoSpatialRange(Xapian::sortable_unserialise(data.at(0)), ranges_);
}


void
GeoSpatialRange::init(const Xapian::Database &db_) {
	Xapian::ValuePostingSource::init(db_);

	// Possible that no documents are in range.
	termfreq_min = 0;
}


std::string
GeoSpatialRange::get_description() const {
	std::string result("GeoSpatialRange ");
	result += std::to_string(slot);
	return result;
}