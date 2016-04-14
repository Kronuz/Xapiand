/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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

#include <cmath>


static std::string findSmallest(const std::string& multiValues) {
	if (multiValues.empty()) return STR_FOR_EMPTY;
	StringList s;
	s.unserialise(multiValues);
	return *s.cbegin();
}


static std::string findLargest(const std::string& multiValues) {
	if (multiValues.empty()) return STR_FOR_EMPTY;
	StringList s;
	s.unserialise(multiValues);
	return *s.crbegin();
}


static std::string get_cmpvalue(const std::string& value_, const key_values_t& sort_value) {
	switch (sort_value.type) {
		case INTEGER_TYPE:
		case POSITIVE_TYPE:
		case FLOAT_TYPE:
		case DATE_TYPE: {
			double val = std::abs(Xapian::sortable_unserialise(value_) - sort_value.valuenumeric);
			return Xapian::sortable_serialise(val);
		}
		case BOOLEAN_TYPE:
			return value_[0] == sort_value.valuestring[0] ? Xapian::sortable_serialise(0) : Xapian::sortable_serialise(1);
		case STRING_TYPE:
			return Xapian::sortable_serialise(levenshtein_distance(value_, sort_value.valuestring));
		case GEO_TYPE: {
			auto geo_val = Unserialise::geo(value_);
			CartesianUSet centroids;
			centroids.unserialise(geo_val.second);
			double angle = M_PI;
			for (const auto& centroid_ : sort_value.valuegeo) {
				double aux = M_PI;
				for (const auto& centroid : centroids) {
					double rad_angle = acos(centroid_ * centroid);
					if (rad_angle < aux) aux = rad_angle;
				}
				if (aux < angle) angle = aux;
			}
			return Xapian::sortable_serialise(angle);
		}
		default:
			return std::string();
	}
}


static std::string findSmallest(const std::string& multiValues, const key_values_t& sort_value) {
	if (multiValues.empty()) return MAX_CMPVALUE;
	StringList s;
	s.unserialise(multiValues);

	StringList::const_iterator it(s.begin());
	std::string smallest(get_cmpvalue(*it, sort_value));
	const auto it_e = s.end();
	for (++it; it != it_e; ++it) {
		auto aux = get_cmpvalue(*it, sort_value);
		if (smallest > aux) smallest = aux;
	}

	return smallest;
}


static std::string findLargest(const std::string& multiValues, const key_values_t& sort_value) {
	if (multiValues.empty()) return MAX_CMPVALUE;
	StringList s;
	s.unserialise(multiValues);

	StringList::const_iterator it(s.begin());
	std::string largest(get_cmpvalue(*it, sort_value));
	const auto it_e = s.end();
	for (++it; it != it_e; ++it) {
		std::string aux(get_cmpvalue(*it, sort_value));
		if (aux > largest) largest = aux;
	}

	return largest;
}


std::string
Multi_MultiValueKeyMaker::operator()(const Xapian::Document& doc) const
{
	std::string result;

	auto i = slots.begin();
	// Don't crash if slots is empty.
	if (i == slots.end()) return result;

	while (true) {
		// All values (except for the last if it's sorted forwards) need to
		// be adjusted.
		bool reverse_sort = i->reverse;
		// Select The most representative value to create the key.
		std::string v = i->hasValue ? reverse_sort ? findLargest(doc.get_value(i->slot), *i) : findSmallest(doc.get_value(i->slot), *i) :
									  reverse_sort ? findLargest(doc.get_value(i->slot)) : findSmallest(doc.get_value(i->slot));
		// RULE: v is never empty, because if there is not value in the slot v is MAX_CMPVALUE or STR_FOR_EMPTY.

		if (++i == slots.end() && !reverse_sort) {
			// No need to adjust the last value if it's sorted forwards.
			result += v;
			break;
		}

		if (reverse_sort) {
			// For a reverse ordered value, we subtract each byte from '\xff',
			// except for '\0' which we convert to "\xff\0".  We insert
			// "\xff\xff" after the encoded value.
			for (const auto& ch_ : v) {
				unsigned char ch = static_cast<unsigned char>(ch_);
				result += char(255 - ch);
				if (ch == 0) result += '\0';
			}
			result.append("\xff\xff", 2);
			if (i == slots.end()) break;
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
			result.append("\0", 2);
		}
	}

	return result;
}
