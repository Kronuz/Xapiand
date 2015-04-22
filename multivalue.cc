/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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
StringListSerialiser::append(const std::string & value)
{
	if (items == 0) {
		values = value;
	} else {
		if (items == 1) {
			std::string new_values;
			new_values.append(encode_length(values.size()));
			new_values.append(values);
			values = new_values;
		}
		values.append(encode_length(value.size()));
		values.append(value);
	}
	items++;
}

const std::string
StringListSerialiser::get() const
{
	std::string serialised;
	if (items > 1) {
		serialised.append(MULTIVALUE_MAGIC);
		serialised.append(encode_length(values.size()));
	}
	serialised.append(values);
	return serialised;
}


StringListUnserialiser::StringListUnserialiser(const std::string & in)
		: serialised(in),
		  pos(serialised.data()),
		  is_list(false)
{
	is_list = serialised.compare(0, sizeof(MULTIVALUE_MAGIC) - 1, MULTIVALUE_MAGIC) == 0;
	if (is_list) {
		const char * old_pos = pos;
		pos += sizeof(MULTIVALUE_MAGIC) - 1;
		size_t length = decode_length(&pos, serialised.data() + serialised.size(), true);
		if (length == -1 || length != serialised.size() - (pos - serialised.data())) {
			pos = old_pos;
			is_list = false;
		}
	}
	read_next();
}


void
StringListUnserialiser::read_next()
{
	if (pos == NULL) {
		return;
	}
	if (pos == serialised.data() + serialised.size()) {
		pos = NULL;
		curritem.resize(0);
		return;
	}
	size_t currlen;
	if (is_list) {
		currlen = decode_length(&pos, serialised.data() + serialised.size(), true);
		if (currlen == -1) {
			// FIXME: throwing a NetworkError if the length is too long - should be a more appropriate error.
			throw Xapian::NetworkError("Decoding error of serialised MultiValueCountMatchSpy");
		}
	} else {
		pos = serialised.data();
		currlen = serialised.size();
	}
	curritem.assign(pos, currlen);
	pos += currlen;
}


void
MultiValueCountMatchSpy::operator()(const Xapian::Document &doc, double) {
	assert(internal.get());
	++(internal->total);
	StringListUnserialiser i(doc.get_value(internal->slot));
	StringListUnserialiser end;
	for (; i != end; ++i) {
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
									 const Xapian::Registry &) const{
	const char * p = s.data();
	const char * end = p + s.size();

	Xapian::valueno new_slot = decode_length(&p, end, false);
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
