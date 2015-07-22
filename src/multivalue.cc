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