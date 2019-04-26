/*
 * Copyright (c) 2015-2019 Dubalu LLC
 * Copyright (c) 2006,2007,2008,2009,2010,2011,2012 Olly Betts
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

#include "length.h"

#include <algorithm>                              // for min
#include <cassert>                                // for assert
#include <cfloat>                                 // for FLT_RADIX, DBL_MANT_DIG, DBL_MAX_EXP, DBL_MAX
#include <cmath>                                  // for scalbn, frexp, HUGE_VAL
#include <functional>                             // for std::reference_wrapper

#include "io.hh"                                  // for io::read and io::write
#include "xapian/net/length.h"                    // for encode_length, decode_length


constexpr int max_length_size = sizeof(unsigned long long) * 8 / 7;


std::string
serialise_length(unsigned long long len)
{
	return encode_length(len);
}


unsigned long long
unserialise_length(const char** p, const char* end, bool check_remaining)
{
	unsigned long long decoded;
	if (check_remaining) {
		decode_length_and_check(p, end, decoded);
	} else {
		decode_length(p, end, decoded);
	}
	return decoded;
}


std::string
serialise_bool(bool value)
{
	return value ? "1" : "0";
}


bool
unserialise_bool(const char** p, const char* end)
{
	const char *ptr = *p;
	assert(ptr);
	assert(ptr <= end);

	if unlikely(ptr == end) {
		THROW(SerialisationError, "Bad encoded boolean: no data");
	}

	if (*ptr < '0' || *ptr > '1') {
		THROW(SerialisationError, "Bad encoded boolean: invalid");
	}
	bool value(*ptr++ != '0');

	*p = ptr;
	return value;
}


std::string
serialise_char(char value)
{
	return std::string(1, value);
}


char
unserialise_char(const char** p, const char* end)
{
	const char *ptr = *p;
	assert(ptr);
	assert(ptr <= end);

	if unlikely(ptr == end) {
		THROW(SerialisationError, "Bad encoded char: no data");
	}

	char value = *ptr++;

	*p = ptr;
	return value;
}


std::string
serialise_string(std::string_view input) {
	std::string output;
	unsigned long long input_size = input.size();
	output.reserve(max_length_size + input_size);
	output.append(serialise_length(input_size));
	output.append(input);
	return output;
}


std::string_view
unserialise_string(const char** p, const char* end) {
	const char *ptr = *p;
	assert(ptr);
	assert(ptr <= end);

	unsigned long long length = unserialise_length(&ptr, end, true);
	std::string_view string(ptr, length);
	ptr += length;

	*p = ptr;

	return string;
}


void
serialise_length(int fd, unsigned long long len)
{
	ssize_t w;

	auto length = serialise_length(len);
	w = io::write(fd, length.data(), length.size());
	if (w < 0) { THROW(Error, "Cannot write to file [{}]", fd); }
}


unsigned long long
unserialise_length(int fd, std::string &buffer, std::size_t& off, std::size_t& acc)
{
	ssize_t r;
	if (buffer.size() - off < 10) {
		char buf[1024];
		r = io::read(fd, buf, sizeof(buf));
		if (r < 0) { THROW(Error, "Cannot read from file [{}]", fd); }
		acc += r;
		buffer.append(buf, r);
	}

	const char* start = buffer.data();
	auto end = start + buffer.size();
	start += off;
	auto pos = start;
	auto length = unserialise_length(&pos, end, false);
	off += (pos - start);

	return length;
}


void
serialise_string(int fd, std::string_view input)
{
	serialise_length(fd, input.size());

	ssize_t w = io::write(fd, input.data(), input.size());
	if (w < 0) { THROW(Error, "Cannot write to file [{}]", fd); }
}


std::string
unserialise_string(int fd, std::string &buffer, std::size_t& off, std::size_t& acc)
{
	ssize_t length = unserialise_length(fd, buffer, off, acc);

	auto pos = buffer.data();
	auto end = pos + buffer.size();
	pos += off;

	std::string str;
	auto available = end - pos;
	if (available >= length) {
		str.append(pos, pos + length);
		buffer.erase(0, off + length);
		off = 0;
	} else {
		str.reserve(length);
		str.append(pos, end);
		str.resize(length);
		ssize_t r = io::read(fd, &str[available], length - available);
		if (r < 0) { THROW(Error, "Cannot read from file [{}]", fd); }
		acc += r;
		if (r != length - available) {
			THROW(SerialisationError, "Invalid input: insufficient data (needed {}, read {})", length - available, r);
		}
		buffer.clear();
		off = 0;
	}

	if (off > 10 * 1024) {
		buffer.erase(0, off);
		off = 0;
	}

	return str;
}

void
serialise_char(int fd, char ch)
{
	ssize_t w;

	w = io::write(fd, &ch, 1);
	if (w < 0) { THROW(Error, "Cannot write to file [{}]", fd); }
}


char
unserialise_char(int fd, std::string &buffer, std::size_t& off, std::size_t& acc)
{
	ssize_t r;
	if (buffer.size() - off < 1) {
		char buf[1024];
		r = io::read(fd, buf, sizeof(buf));
		if (r < 0) { THROW(Error, "Cannot read from file [{}]", fd); }
		acc += r;
		buffer.append(buf, r);
	}

	const char* start = buffer.data();
	auto end = start + buffer.size();
	start += off;
	auto pos = start;
	if (pos == end) {
		THROW(SerialisationError, "Invalid input: insufficient data");
	}
	char ch = *pos;
	++pos;
	off += (pos - start);

	return ch;
}


std::string
serialise_strings(const std::vector<std::string_view>& strings)
{
	std::string output;
	for (const auto& s : strings) {
		output.append(serialise_string(s));
	}
	return output;
}


std::string_view
unserialise_string_at(size_t at, const char** p, const char* end)
{
	const char *ptr = *p;
	assert(ptr);
	assert(ptr <= end);

	unsigned long long length = 0;

	++at;
	do {
		ptr += length;
		if (ptr >= end) { break; }
		length = unserialise_length(&ptr, end, true);
	} while (--at != 0u);

	std::string_view string = "";

	if (at == 0) {
		string = std::string_view(ptr, length);
		ptr += length;
		*p = ptr;
	}

	return string;
}
