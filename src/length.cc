/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include <algorithm>    // for min
#include <cfloat>       // for FLT_RADIX, DBL_MANT_DIG, DBL_MAX_EXP, DBL_MAX
#include <cmath>        // for scalbn, frexp, HUGE_VAL
#include <functional>   // for std::reference_wrapper

#include "cassert.h"    // for ASSERT
#include "io.hh"        // for io::read and io::write


// The serialisation we use for doubles is inspired by a comp.lang.c post
// by Jens Moeller:
//
// http://groups.google.com/group/comp.lang.c/browse_thread/thread/6558d4653f6dea8b/75a529ec03148c98
//
// The clever part is that the mantissa is encoded as a base-256 number which
// means there's no rounding error provided both ends have FLT_RADIX as some
// power of two.
//
// FLT_RADIX == 2 seems to be ubiquitous on modern UNIX platforms, while
// some older platforms used FLT_RADIX == 16 (IBM machines for example).
// FLT_RADIX == 10 seems to be very rare (the only instance Google finds
// is for a cross-compiler to some TI calculators).

#if FLT_RADIX == 2
# define MAX_MANTISSA_BYTES ((DBL_MANT_DIG + 7 + 7) / 8)
# define MAX_EXP ((DBL_MAX_EXP + 1) / 8)
# define MAX_MANTISSA (1 << (DBL_MAX_EXP & 7))
#elif FLT_RADIX == 16
# define MAX_MANTISSA_BYTES ((DBL_MANT_DIG + 1 + 1) / 2)
# define MAX_EXP ((DBL_MAX_EXP + 1) / 2)
# define MAX_MANTISSA (1 << ((DBL_MAX_EXP & 1) * 4))
#else
# error FLT_RADIX is a value not currently handled (not 2 or 16)
// # define MAX_MANTISSA_BYTES (sizeof(double) + 1)
#endif

constexpr int max_mantissa_bytes = std::min(MAX_MANTISSA_BYTES, 8);
constexpr int max_length_size = sizeof(unsigned long long) * 8 / 7;


static int
base256ify_double(double &v)
{
	int exp;
	v = frexp(v, &exp);
	// v is now in the range [0.5, 1.0)
	--exp;
#if FLT_RADIX == 2
	v = scalbn(v, (exp & 7) + 1);
#else
	v = ldexp(v, (exp & 7) + 1);
#endif
	// v is now in the range [1.0, 256.0)
	exp >>= 3;
	return exp;
}


std::string
serialise_double(double v)
{
	/* First byte:
	 *   bit 7 Negative flag
	 *   bit 4..6 Mantissa length - 1
	 *   bit 0..3 --- 0-13 -> Exponent + 7
	 *               \- 14 -> Exponent given by next byte
	 *                - 15 -> Exponent given by next 2 bytes
	 *
	 * Then optional medium (1 byte) or large exponent (2 bytes, lsb first)
	 *
	 * Then mantissa (0 iff value is 0)
	 */

	bool negative = (v < 0.0);

	if (negative) { v = -v; }

	int exp = base256ify_double(v);

	std::string result;
	result.reserve(3 + max_mantissa_bytes);

	if (exp <= 6 && exp >= -7) {
		auto b = static_cast<unsigned char>(exp + 7);
		if (negative) { b |= static_cast<unsigned char>(0x80); }
		result.push_back(static_cast<char>(b));
	} else {
		if (exp >= -128 && exp < 127) {
			result.push_back(negative ? static_cast<char>(0x8e) : static_cast<char>(0x0e));
			result.push_back(static_cast<char>(exp + 128));
		} else {
			if (exp < -32768 || exp > 32767) {
				THROW(InternalError, "Insane exponent in floating point number");
			}
			result.push_back(negative ? static_cast<char>(0x8f) : static_cast<char>(0x0f));
			result.push_back(static_cast<char>(static_cast<unsigned>(exp + 32768) & 0xff));
			result.push_back(static_cast<char>(static_cast<unsigned>(exp + 32768) >> 8));
		}
	}

	size_t n = result.size();

	int max_bytes = max_mantissa_bytes;
	do {
		auto byte = static_cast<unsigned char>(v);
		result.push_back(static_cast<char>(byte));
		v -= static_cast<double>(byte);
		v *= 256.0;
	} while (v != 0.0 && (--max_bytes != 0));

	n = result.size() - n;
	if (n > 1) {
		ASSERT(n <= 8);
		result[0] = static_cast<unsigned char>(result[0] | ((n - 1) << 4));
	}

	return result;
}


double
unserialise_double(const char** p, const char* end)
{
	if (end - *p < 2) {
		THROW(SerialisationError, "Bad encoded double: insufficient data");
	}
	unsigned char first = *(*p)++;
	if (first == 0 && *(*p) == 0) {
		++*p;
		return 0.0;
	}

	bool negative = (first & 0x80) != 0;
	size_t mantissa_len = ((first >> 4) & 0x07) + 1;

	int exp = first & 0x0f;
	if (exp >= 14) {
		int bigexp = static_cast<unsigned char>(*(*p)++);
		if (exp == 15) {
			if unlikely(*p == end) {
				*p = nullptr;
				THROW(SerialisationError, "Bad encoded double: short large exponent");
			}
			exp = bigexp | (static_cast<unsigned char>(*(*p)++) << 8);
			exp -= 32768;
		} else {
			exp = bigexp - 128;
		}
	} else {
		exp -= 7;
	}

	if (static_cast<std::size_t>(end - *p) < mantissa_len) {
		THROW(SerialisationError, "Bad encoded double: short mantissa");
	}

	double v = 0.0;

	static double dbl_max_mantissa = DBL_MAX;
	static int dbl_max_exp = base256ify_double(dbl_max_mantissa);
	*p += mantissa_len;
	if (exp > dbl_max_exp ||
		(exp == dbl_max_exp &&
		 static_cast<double>(static_cast<unsigned char>((*p)[-1])) > dbl_max_mantissa)) {
		// The mantissa check should be precise provided that FLT_RADIX
		// is a power of 2.
		v = HUGE_VAL;
	} else {
		const char *q = *p;
		while ((mantissa_len--) != 0u) {
			v *= 0.00390625; // 1/256
			v += static_cast<double>(static_cast<unsigned char>(*--q));
		}

#if FLT_RADIX == 2
		if (exp != 0) { v = scalbn(v, exp * 8); }
#elif FLT_RADIX == 16
		if (exp != 0) { v = scalbn(v, exp * 2); }
#else
		if (exp != 0) { v = ldexp(v, exp * 8); }
#endif

#if 0
		if (v == 0.0) {
			// FIXME: handle underflow
		}
#endif
	}

	if (negative) { v = -v; }

	return v;
}


std::string
serialise_length(unsigned long long len)
{
	std::string result;
	result.reserve(max_length_size);
	if (len < 255) {
		result.push_back(static_cast<unsigned char>(len));
	} else {
		result.push_back('\xff');
		len -= 255;
		while (true) {
			auto b = static_cast<unsigned char>(len & 0x7f);
			len >>= 7;
			if (len == 0u) {
				result.push_back(b | static_cast<unsigned char>(0x80));
				break;
			}
			result.push_back(b);
		}
	}
	return result;
}


unsigned long long
unserialise_length(const char** p, const char* end, bool check_remaining)
{
	const char *ptr = *p;
	ASSERT(ptr);
	ASSERT(ptr <= end);

	if unlikely(ptr == end) {
		// Out of data.
		*p = nullptr;
		THROW(SerialisationError, "Bad encoded length: no data");
	}

	unsigned long long len = static_cast<unsigned char>(*ptr++);
	if (len == 0xff) {
		len = 0;
		unsigned char ch;
		unsigned shift = 0;
		do {
			if unlikely(ptr == end || shift > (max_length_size * 7)) {
				*p = nullptr;
				THROW(SerialisationError, "Bad encoded length: insufficient data");
			}
			ch = *ptr++;
			len |= static_cast<unsigned long long>(ch & 0x7f) << shift;
			shift += 7;
		} while ((ch & 0x80) == 0);
		len += 255;
	}
	if (check_remaining && len > static_cast<unsigned long long>(end - ptr)) {
		THROW(SerialisationError, "Bad encoded length: length greater than data");
	}
	*p = ptr;
	return len;
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
	ASSERT(ptr);
	ASSERT(ptr <= end);

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
	if (w < 0) { THROW(Error, "Cannot write to file [%d]", fd); }
}


unsigned long long
unserialise_length(int fd, std::string &buffer, std::size_t& off, std::size_t& acc)
{
	ssize_t r;
	if (buffer.size() - off < 10) {
		char buf[1024];
		r = io::read(fd, buf, sizeof(buf));
		if (r < 0) { THROW(Error, "Cannot read from file [%d]", fd); }
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
	if (w < 0) { THROW(Error, "Cannot write to file [%d]", fd); }
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
		if (r < 0) { THROW(Error, "Cannot read from file [%d]", fd); }
		acc += r;
		if (r != length - available) {
			THROW(SerialisationError, "Invalid input: insufficient data (needed %zd, read %zd)", length - available, r);
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
	if (w < 0) { THROW(Error, "Cannot write to file [%d]", fd); }
}


char
unserialise_char(int fd, std::string &buffer, std::size_t& off, std::size_t& acc)
{
	ssize_t r;
	if (buffer.size() - off < 1) {
		char buf[1024];
		r = io::read(fd, buf, sizeof(buf));
		if (r < 0) { THROW(Error, "Cannot read from file [%d]", fd); }
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
	ASSERT(ptr);
	ASSERT(ptr <= end);

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
