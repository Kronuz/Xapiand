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

#include "exception.h"
#include "length.h"

#include <assert.h>


constexpr char SL_MAGIC = '\0';


void
StringList::unserialise(const std::string& serialised)
{
	const char* ptr = serialised.data();
	const char* end = serialised.data() + serialised.size();
	unserialise(&ptr, end);
}


void
StringList::unserialise(const char** ptr, const char* end)
{
	ssize_t length = -1;

	clear();

	const char* pos = *ptr;
	if (*pos++ == SL_MAGIC && pos != end) {
		try {
			length = unserialise_length(&pos, end, true);
			reserve(length);
			while (pos != end) {
				length = unserialise_length(&pos, end, true);
				push_back(std::string(pos, length));
				pos += length;
			}
		} catch (const Xapian::SerialisationError&) {
			length = -1;
		}
	}

	if (length == -1) {
		pos = *ptr;
		push_back(std::string(pos, end - pos));
	}

	*ptr = pos;
}


std::string
StringList::serialise() const
{
	std::string serialised;
	auto _size = size();
	if (_size > 1) {
		serialised.assign(1, SL_MAGIC);
		serialised.append(serialise_length(_size));
		for (const auto& str : *this) {
			serialised.append(serialise_length(str.size()));
			serialised.append(str);
		}
	} else if (_size == 1) {
		serialised.assign(front());
	}

	return serialised;
}


void
MultiValueCountMatchSpy::operator()(const Xapian::Document &doc, double)
{
	assert(internal.get());
	++(internal->total);
	StringList list;
	list.unserialise(doc.get_value(internal->slot));
	if (is_geo) {
		const auto it_e = list.end();
		for (auto i = list.begin(); i != it_e; ++i) {
			if (!i->empty()) {
				StringList s;
				s.push_back(*i);
				s.push_back(*(++i));
				++(internal->values[s.serialise()]);
			}
		}
	} else {
		for (const auto& val : list) {
			if (!val.empty()) ++(internal->values[val]);
		}
	}
}


Xapian::MatchSpy*
MultiValueCountMatchSpy::clone() const
{
	assert(internal.get());
	return new MultiValueCountMatchSpy(internal->slot);
}


std::string
MultiValueCountMatchSpy::name() const
{
	return "Xapian::MultiValueCountMatchSpy";
}


std::string
MultiValueCountMatchSpy::serialise() const
{
	assert(internal.get());
	std::string result;
	result += serialise_length(internal->slot);
	return result;
}


Xapian::MatchSpy*
MultiValueCountMatchSpy::unserialise(const std::string& s, const Xapian::Registry&) const
{
	const char* p = s.data();
	const char* end = p + s.size();

	Xapian::valueno new_slot = (Xapian::valueno)unserialise_length(&p, end, false);
	if (new_slot == Xapian::BAD_VALUENO) {
		throw MSG_NetworkError("Decoding error of serialised MultiValueCountMatchSpy");
	}
	if (p != end) {
		throw MSG_NetworkError("Junk at end of serialised MultiValueCountMatchSpy");
	}

	return new MultiValueCountMatchSpy(new_slot);
}


std::string
MultiValueCountMatchSpy::get_description() const
{
	char buffer[20];
	std::string d("MultiValueCountMatchSpy(");
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
