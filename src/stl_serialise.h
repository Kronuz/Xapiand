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

#pragma once

#include "htm.h"

#include <string>
#include <vector>
#include <set>
#include <unordered_set>


/*
 * Class for serialise a vector of strings.
 * i.e
 * vector = {a, ..., b}
 * serialised = SC_MAGIC + serialise_length(size vector) + serialise_length(a.size()) + a + ... + serialise_length(b.size()) + b
 * symbol '+' means concatenate
 */
class StringList : public std::vector<std::string> {
public:
	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.size();
		unserialise(&ptr, end);
	}

	void unserialise(const char** ptr, const char* end);
	std::string serialise() const;
};


/*
 * Class for serialise a set of strings.
 * i.e
 * set = {a, ..., b}
 * serialised = SC_MAGIC + serialise_length(size set) + serialise_length(a.size()) + a + ... + serialise_length(b.size()) + b
 * symbol '+' means concatenate
 */
class StringSet : public std::set<std::string> {
public:
	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.size();
		unserialise(&ptr, end);
	}

	void unserialise(const char** ptr, const char* end);
	std::string serialise() const;
};


/*
 * This class serializes a unordered set of Cartesian.
 * i.e
 * CartesianUSet = {a, ..., b}
 * serialised = GC_MAGIC + serialise_length(size unordered_set) + serialise_cartesian(a) + ... + serialise_cartesian(b)
 * symbol '+' means concatenate.
 * It is not necessary to save the size because it's SIZE_SERIALISE_CARTESIAN for all.
 */
class CartesianUSet : public std::unordered_set<Cartesian> {
public:
	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.size();
		unserialise(&ptr, end);
	}

	void unserialise(const char** ptr, const char* end);
	std::string serialise() const;
};


/*
 * This class serializes a vector of range_t.
 * i.e
 * RangeList = {{a,b}, ..., {c,d}}
 * serialised = RC_MAGIC + serialise_length(size vector) + serialise_geo(a) + serialise_geo(b) ... + serialise_geo(d)
 * symbol '+' means concatenate.
 * It is not necessary to save the size because it's SIZE_BYTES_ID for all.
 */
class RangeList : public std::vector<range_t> {
public:
	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.size();
		unserialise(&ptr, end);
	}

	void unserialise(const char** ptr, const char* end);
	std::string serialise() const;
};
