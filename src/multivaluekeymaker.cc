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

#include "multivaluekeymaker.h"
#include "multivalue.h"


static std::string findSmallest(const std::string &multiValues) {
	StringList s;
	s.unserialise(multiValues);
	StringList::const_iterator it(s.begin());
	std::string smallest(*it);
	for (it++; it != s.end(); it++) {
		if (smallest > *it) smallest = *it;
	}
	return smallest;
}


static std::string findLargest(const std::string &multiValues) {
	StringList s;
	s.unserialise(multiValues);
	StringList::const_iterator it(s.begin());
	std::string largest(*it);
	for (it++; it != s.end(); it++) {
		if (*it > largest) largest = *it;
	}
	return largest;
}


std::string
Multi_MultiValueKeyMaker::operator()(const Xapian::Document & doc) const
{
	std::string result;

	std::vector<std::pair<Xapian::valueno, bool> >::const_iterator i = slots.begin();
	// Don't crash if slots is empty.
	if (i == slots.end()) return result;

	size_t last_not_empty_forwards = 0;
	while (true) {
		// All values (except for the last if it's sorted forwards) need to
		// be adjusted.
		//
		// FIXME: allow Xapian::BAD_VALUENO to mean "relevance?"
		bool reverse_sort = i->second;
		// Select The most representative value to create the key.
		std::string v = (reverse_sort) ? findLargest(doc.get_value(i->first)) : findSmallest(doc.get_value(i->first));

		if (reverse_sort || !v.empty())
			last_not_empty_forwards = result.size();

		if (++i == slots.end() && !reverse_sort) {
			if (v.empty()) {
				// Trim off all the trailing empty forwards values.
				result.resize(last_not_empty_forwards);
			} else {
				// No need to adjust the last value if it's sorted forwards.
				result += v;
			}
			break;
		}

		if (reverse_sort) {
			// For a reverse ordered value, we subtract each byte from '\xff',
			// except for '\0' which we convert to "\xff\0".  We insert
			// "\xff\xff" after the encoded value.
			for (std::string::const_iterator j = v.begin(); j != v.end(); ++j) {
				unsigned char ch = static_cast<unsigned char>(*j);
				result += char(255 - ch);
				if (ch == 0) result += '\0';
			}
			result.append("\xff\xff", 2);
			if (i == slots.end()) break;
			last_not_empty_forwards = result.size();
		} else {
			// For a forward ordered value (unless it's the last value), we
			// convert any '\0' to "\0\xff".  We insert "\0\0" after the
			// encoded value.
			std::string::size_type j = 0, nul;
			while ((nul = v.find('\0', j)) != std::string::npos) {
				++nul;
				result.append(v, j, nul - j);
				result += '\xff';
				j = nul;
			}
			result.append(v, j, std::string::npos);
			if (!v.empty())
				last_not_empty_forwards = result.size();
			result.append("\0", 2);
		}
	}

	return result;
}