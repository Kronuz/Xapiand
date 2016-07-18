/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "stl_serialise.h"

#include "length.h"
#include "log.h"
#include "serialise.h"


#define STL_MAGIC '\0'


void
StringList::unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			clear();
			auto length = unserialise_length(&pos, end, true);
			reserve(length);
			while (pos != end) {
				length = unserialise_length(&pos, end, true);
				emplace_back(pos, length);
				pos += length;
			}
			return;
		} catch (const Xapian::SerialisationError&) { }
	}

	clear();
	pos = *ptr;
	if (pos != end) {
		emplace_back(pos, end - pos);
	}
}


void
StringList::add_unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto length = unserialise_length(&pos, end, true);
			reserve(length);
			while (pos != end) {
				length = unserialise_length(&pos, end, true);
				emplace_back(pos, length);
				pos += length;
			}
			return;
		} catch (const Xapian::SerialisationError&) { }
	}

	pos = *ptr;
	if (pos != end) {
		emplace_back(pos, end - pos);
	}
}


std::string
StringList::serialise() const
{
	std::string serialised;

	auto _size = size();
	if (_size > 1) {
		serialised.assign(1, STL_MAGIC);
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
StringSet::unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			clear();
			auto length = unserialise_length(&pos, end, true);
			while (pos != end) {
				length = unserialise_length(&pos, end, true);
				insert(std::string(pos, length));
				pos += length;
			}
			return;
		} catch (const Xapian::SerialisationError&) { }
	}

	clear();
	pos = *ptr;
	if (pos != end) {
		insert(std::string(pos, end - pos));
	}
}


void
StringSet::add_unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto length = unserialise_length(&pos, end, true);
			while (pos != end) {
				length = unserialise_length(&pos, end, true);
				insert(std::string(pos, length));
				pos += length;
			}
			return;
		} catch (const Xapian::SerialisationError&) { }
	}

	pos = *ptr;
	if (pos != end) {
		insert(std::string(pos, end - pos));
	}
}


std::string
StringSet::serialise() const
{
	std::string serialised;

	auto _size = size();
	if (_size > 1) {
		serialised.assign(1, STL_MAGIC);
		serialised.append(serialise_length(_size));
		for (const auto& str : *this) {
			serialised.append(serialise_length(str.size()));
			serialised.append(str);
		}
	} else if (_size == 1) {
		serialised.assign(*cbegin());
	}

	return serialised;
}


void
CartesianUSet::unserialise(const char** ptr, const char* end)
{
	clear();
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto _size = unserialise_length(&pos, end, true);
			reserve(_size);
			while (end - pos >= SIZE_SERIALISE_CARTESIAN) {
				insert(Unserialise::cartesian(std::string(pos, SIZE_SERIALISE_CARTESIAN)));
				pos += SIZE_SERIALISE_CARTESIAN;
			}
			if (pos != end || size() != _size) {
				clear();
			}
		} catch (const Xapian::SerialisationError&) {
			clear();
		}
	}
}


void
CartesianUSet::add_unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto _size = unserialise_length(&pos, end, true);
			reserve(_size);
			while (end - pos >= SIZE_SERIALISE_CARTESIAN) {
				insert(Unserialise::cartesian(std::string(pos, SIZE_SERIALISE_CARTESIAN)));
				pos += SIZE_SERIALISE_CARTESIAN;
			}
		} catch (const Xapian::SerialisationError&) { }
	}
}


std::string
CartesianUSet::serialise() const
{
	std::string serialised;
	auto _size = size();

	if (_size > 0) {
		serialised.assign(1, STL_MAGIC);
		serialised.append(serialise_length(_size));
		for (const auto& c : *this) {
			serialised.append(Serialise::cartesian(c));
		}
	}

	return serialised;
}


void
RangeList::unserialise(const char** ptr, const char* end)
{
	clear();
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto _size = unserialise_length(&pos, end, true);
			reserve(_size);
			long range_size = 2 * SIZE_BYTES_ID;
			while (end - pos >= range_size) {
				push_back({ Unserialise::trixel_id(std::string(pos, SIZE_BYTES_ID)), Unserialise::trixel_id(std::string(pos += SIZE_BYTES_ID, SIZE_BYTES_ID)) });
				pos += SIZE_BYTES_ID;
			}
			if (pos != end || size() != _size) {
				clear();
			}
		} catch (const Xapian::SerialisationError&) {
			clear();
		}
	}
}


void
RangeList::add_unserialise(const char** ptr, const char* end)
{
	const char* pos = *ptr;
	if (pos != end && *pos++ == STL_MAGIC) {
		try {
			auto _size = unserialise_length(&pos, end, true);
			reserve(_size);
			long range_size = 2 * SIZE_BYTES_ID;
			while (end - pos >= range_size) {
				push_back({ Unserialise::trixel_id(std::string(pos, SIZE_BYTES_ID)), Unserialise::trixel_id(std::string(pos += SIZE_BYTES_ID, SIZE_BYTES_ID)) });
				pos += SIZE_BYTES_ID;
			}
		} catch (const Xapian::SerialisationError&) { }
	}
}


std::string
RangeList::serialise() const
{
	std::string serialised;
	auto _size = size();

	if (_size) {
		serialised.assign(1, STL_MAGIC);
		serialised.append(serialise_length(_size));
		for (const auto& range : *this) {
			serialised.append(Serialise::trixel_id(range.start));
			serialised.append(Serialise::trixel_id(range.end));
		}
	}

	return serialised;
}
