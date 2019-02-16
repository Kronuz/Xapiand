/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "keymaker.h"

#include <utility>              // for pair

#include "exception.h"          // for InvalidArgumentError, MSG_I...
#include "geospatial/ewkt.h"    // for EWKT


std::string
SerialiseKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_STR_CMPVALUE;
	}

	StringList values(multiValues);

	return values.front();
}


std::string
SerialiseKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_STR_CMPVALUE;
	}

	StringList values(multiValues);

	return values.back();
}


std::string
FloatKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::floating(std::fabs(Unserialise::floating(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::floating(Unserialise::floating(*it) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::floating(_ref_val - Unserialise::floating(*last));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) < 0; it_p = it++);

	if (it.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	double distance1 = _ref_val - Unserialise::floating(*it_p);
	double distance2 = Unserialise::floating(*it) - _ref_val;
	return Serialise::floating(distance1 < distance2 ? distance1 : distance2);
}


std::string
FloatKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::floating(std::fabs(Unserialise::floating(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::floating(Unserialise::floating(values.back()) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::floating(_ref_val - Unserialise::floating(*it));
	}

	double distance1 = _ref_val - Unserialise::floating(*it);
	double distance2 = Unserialise::floating(*last) - _ref_val;
	return Serialise::floating(distance1 > distance2 ? distance1 : distance2);
}


std::string
IntegerKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::integer(std::llabs(Unserialise::integer(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::integer(Unserialise::integer(*it) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::integer(_ref_val - Unserialise::integer(*last));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) < 0; it_p = it++);

	if (it.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	int64_t distance1 = _ref_val - Unserialise::integer(*it_p);
	int64_t distance2 = Unserialise::integer(*it) - _ref_val;
	return Serialise::integer(distance1 < distance2 ? distance1 : distance2);
}


std::string
IntegerKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::integer(std::llabs(Unserialise::integer(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::integer(Unserialise::integer(values.back()) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::integer(_ref_val - Unserialise::integer(*it));
	}

	int64_t distance1 = _ref_val - Unserialise::integer(*it);
	int64_t distance2 = Unserialise::integer(*last) - _ref_val;
	return Serialise::integer(distance1 > distance2 ? distance1 : distance2);
}


std::string
PositiveKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		uint64_t val = Unserialise::positive(values.front());
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::positive(Unserialise::positive(*it) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::positive(_ref_val - Unserialise::positive(*last));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) < 0; it_p = it++);

	if (it.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	uint64_t distance1 = _ref_val - Unserialise::positive(*it_p);
	uint64_t distance2 = Unserialise::positive(*it) - _ref_val;
	return Serialise::positive(distance1 < distance2 ? distance1 : distance2);
}


std::string
PositiveKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		uint64_t val = Unserialise::positive(values.front());
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::positive(Unserialise::positive(values.back()) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::positive(_ref_val - Unserialise::positive(*it));
	}

	uint64_t distance1 = _ref_val - Unserialise::positive(*it);
	uint64_t distance2 = Unserialise::positive(*last) - _ref_val;
	return Serialise::positive(distance1 > distance2 ? distance1 : distance2);
}


std::string
DateKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::timestamp(std::fabs(Unserialise::timestamp(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::timestamp(Unserialise::timestamp(*it) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::timestamp(_ref_val - Unserialise::timestamp(*last));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) < 0; it_p = it++);

	if (it.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	double distance1 = _ref_val - Unserialise::timestamp(*it_p);
	double distance2 = Unserialise::timestamp(*it) - _ref_val;
	return Serialise::timestamp(distance1 < distance2 ? distance1 : distance2);
}


std::string
DateKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.single()) {
		return Serialise::timestamp(std::fabs(Unserialise::timestamp(values.front()) - _ref_val));
	}

	auto it = values.cbegin();
	if (it.compare(_ser_ref_val) >= 0) {
		return Serialise::timestamp(Unserialise::timestamp(values.back()) - _ref_val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::timestamp(_ref_val - Unserialise::timestamp(*it));
	}

	double distance1 = _ref_val - Unserialise::timestamp(*it);
	double distance2 = Unserialise::timestamp(*last) - _ref_val;
	return Serialise::timestamp(distance1 > distance2 ? distance1 : distance2);
}


std::string
BoolKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.front().at(0) == _ser_ref_val.at(0) || values.back().at(0) == _ser_ref_val.at(0)) {
		return SERIALISED_ZERO;
	}
	return SERIALISED_ONE;
}


std::string
BoolKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(multiValues);

	if (values.front().at(0) != _ser_ref_val.at(0) || values.back().at(0) != _ser_ref_val.at(0)) {
		return SERIALISED_ONE;
	}
	return SERIALISED_ZERO;
}


std::string
GeoKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	auto centroids = Unserialise::centroids(std::move(multiValues));

	if (centroids.empty()) {
		return SERIALISED_M_PI;
	}

	double min_angle = M_PI;
	for (const auto& centroid : centroids) {
		for (const auto& _centroid : _centroids) {
			double rad_angle = _centroid.distance(centroid);
			if (rad_angle < min_angle) {
				min_angle = rad_angle;
			}
		}
	}

	return Serialise::floating(min_angle);
}


std::string
GeoKey::findBiggest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	auto centroids = Unserialise::centroids(std::move(multiValues));

	if (centroids.empty()) {
		return SERIALISED_ZERO;
	}

	double max_angle = 0;
	for (const auto& centroid : centroids) {
		for (const auto& _centroid : _centroids) {
			double rad_angle = _centroid.distance(centroid);
			if (rad_angle > max_angle) {
				max_angle = rad_angle;
			}
		}
	}

	return Serialise::floating(max_angle);
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
		auto v = reverse_sort ? (*i)->findBiggest(doc) : (*i)->findSmallest(doc);
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
				auto ch = static_cast<unsigned char>(ch_);
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
