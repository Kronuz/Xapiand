/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "engine/codec.h"

#include <cstdint>                 // for std::uint64_t
#include <cstring>                 // for std::memcpy

#include "engine/error.h"          // for Error, errc


std::string
engine::serialise_double(double value)
{
	std::uint64_t bits;
	std::memcpy(&bits, &value, sizeof bits);
	std::string out(sizeof bits, '\0');
	for (std::size_t i = 0; i < sizeof bits; ++i) {
		out[i] = static_cast<char>(bits >> (8 * i));  // little endian, whatever the host
	}
	return out;
}


double
engine::unserialise_double(const char** p, const char* end)
{
	if (end - *p < 8) {
		throw engine::Error(engine::errc::invalid, "Bad serialised double: too short");
	}
	std::uint64_t bits = 0;
	for (std::size_t i = 0; i < sizeof bits; ++i) {
		bits |= static_cast<std::uint64_t>(static_cast<unsigned char>((*p)[i])) << (8 * i);
	}
	*p += 8;
	double value;
	std::memcpy(&value, &bits, sizeof value);
	return value;
}
