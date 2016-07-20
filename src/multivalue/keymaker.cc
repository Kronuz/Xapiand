/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "keymaker.h"

#include "../stl_serialise.h"


const dispatch_str_metric def_str_metric = &Multi_MultiValueKeyMaker::levenshtein;


const std::unordered_map<std::string, dispatch_str_metric> map_dispatch_str_metric({
	{ "levenshtein",   &Multi_MultiValueKeyMaker::levenshtein     },
	{ "leven",         &Multi_MultiValueKeyMaker::levenshtein     },
	{ "jaro",          &Multi_MultiValueKeyMaker::jaro            },
	{ "jarowinkler",   &Multi_MultiValueKeyMaker::jaro_winkler    },
	{ "jarow",         &Multi_MultiValueKeyMaker::jaro_winkler    },
	{ "sorensendice",  &Multi_MultiValueKeyMaker::sorensen_dice   },
	{ "sorensen",      &Multi_MultiValueKeyMaker::sorensen_dice   },
	{ "dice",          &Multi_MultiValueKeyMaker::sorensen_dice   },
	{ "jaccard",       &Multi_MultiValueKeyMaker::jaccard         },
	{ "",              def_str_metric                             },
});


std::string
BaseKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) return MAX_CMPVALUE;
	StringList s;
	s.unserialise(multiValues);

	StringList::const_iterator it(s.begin());
	std::string smallest(get_cmpvalue(*it));
	const auto it_e = s.end();
	for (++it; it != it_e; ++it) {
		auto aux = get_cmpvalue(*it);
		if (smallest > aux) smallest = aux;
	}

	return smallest;
}


std::string
BaseKey::findLargest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) return MAX_CMPVALUE;
	StringList s;
	s.unserialise(multiValues);

	StringList::const_iterator it(s.begin());
	std::string largest(get_cmpvalue(*it));
	const auto it_e = s.end();
	for (++it; it != it_e; ++it) {
		std::string aux(get_cmpvalue(*it));
		if (aux > largest) largest = aux;
	}

	return largest;
}


std::string
SerialiseKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) return STR_FOR_EMPTY;
	StringList s;
	s.unserialise(multiValues);
	return *s.cbegin();
}


std::string
SerialiseKey::findLargest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) return STR_FOR_EMPTY;
	StringList s;
	s.unserialise(multiValues);
	return *s.crbegin();
}


std::string
GeoKey::get_cmpvalue(const std::string& serialise_val) const
{
	auto geo_val = Unserialise::geo(serialise_val);
	CartesianUSet centroids;
	centroids.unserialise(geo_val.second);
	double angle = M_PI;
	for (const auto& _centroid : _centroids) {
		double aux = M_PI;
		for (const auto& centroid : centroids) {
			double rad_angle = std::acos(_centroid * centroid);
			if (rad_angle < aux) aux = rad_angle;
		}
		if (aux < angle) angle = aux;
	}
	return Serialise::_float(angle);
}


void
Multi_MultiValueKeyMaker::add_value(Xapian::valueno slot, bool reverse, char type, const std::string& value, const std::string& metric, bool icase)
{
	if (value.empty()) {
		if (type != GEO_TYPE) {
			slots.push_back(std::make_unique<SerialiseKey>(slot, reverse));
		}
	} else {
		switch (type) {
			case FLOAT_TYPE:
				slots.push_back(std::make_unique<FloatKey>(slot, reverse, value));
				return;
			case INTEGER_TYPE:
				slots.push_back(std::make_unique<IntegerKey>(slot, reverse, value));
				return;
			case POSITIVE_TYPE:
				slots.push_back(std::make_unique<PositiveKey>(slot, reverse, value));
				return;
			case DATE_TYPE:
				slots.push_back(std::make_unique<DateKey>(slot, reverse, value));
				return;
			case BOOLEAN_TYPE:
				slots.push_back(std::make_unique<BoolKey>(slot, reverse, value));
				return;
			case STRING_TYPE:
				try {
					auto func = map_dispatch_str_metric.at(metric);
					(this->*func)(slot, reverse, value, icase);
				} catch (const std::out_of_range&) {
					(this->*def_str_metric)(slot, reverse, value, icase);
				}
				return;
			case GEO_TYPE:
				slots.push_back(std::make_unique<GeoKey>(slot, reverse, value));
				return;
		}
	}
}


std::string
Multi_MultiValueKeyMaker::operator()(const Xapian::Document& doc) const
{
	std::string result;

	// Don't crash if slots is empty.
	if (slots.empty()) {
		return result;
	}

	auto i = slots.begin();
	while (true) {
		// All values (except for the last if it's sorted forwards) need to
		// be adjusted.
		auto reverse_sort = (*i)->get_reverse();
		// Select The most representative value to create the key.
		auto v = reverse_sort ? (*i)->findLargest(doc) : (*i)->findSmallest(doc);
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
