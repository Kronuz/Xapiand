/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <utility>          // for pair

#include "exception.h"      // for InvalidArgumentError, MSG_I...
#include "geo/ewkt.h"       // for EWKT


const dispatch_str_metric def_str_metric     = &Multi_MultiValueKeyMaker::jaro;
const dispatch_str_metric def_soundex_metric = &Multi_MultiValueKeyMaker::soundex_en;


const std::unordered_map<std::string, dispatch_str_metric> map_dispatch_soundex_metric({
	{ "english",  &Multi_MultiValueKeyMaker::soundex_en     },
	{ "en",       &Multi_MultiValueKeyMaker::soundex_en     },
	{ "french",   &Multi_MultiValueKeyMaker::soundex_fr     },
	{ "fr",       &Multi_MultiValueKeyMaker::soundex_fr     },
	{ "german",   &Multi_MultiValueKeyMaker::soundex_de     },
	{ "de",       &Multi_MultiValueKeyMaker::soundex_de     },
	{ "spanish",  &Multi_MultiValueKeyMaker::soundex_es     },
	{ "es",       &Multi_MultiValueKeyMaker::soundex_es     }
});


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
	{ "lcsubstr",      &Multi_MultiValueKeyMaker::lcs             },
	{ "lcs",           &Multi_MultiValueKeyMaker::lcs             },
	{ "lcsubsequence", &Multi_MultiValueKeyMaker::lcsq            },
	{ "lcsq",          &Multi_MultiValueKeyMaker::lcsq            },
	{ "soundex",       &Multi_MultiValueKeyMaker::soundex         },
	{ "sound",         &Multi_MultiValueKeyMaker::soundex         }
});


std::string
SerialiseKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_STR_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	return values.front();
}


std::string
SerialiseKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_STR_CMPVALUE;
	}

	StringList values(doc.get_value(_slot));

	return values.back();
}


std::string
FloatKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		return Serialise::_float(std::fabs(Unserialise::_float(*it) - _ref_val));
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::_float(std::fabs(Unserialise::_float(*last) - _ref_val));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	double distance1 = std::fabs(Unserialise::_float(*it_p) - _ref_val);
	double distance2 = std::fabs(Unserialise::_float(*it) - _ref_val);
	return Serialise::_float(distance1 < distance2 ? distance1 : distance2);
}


std::string
FloatKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		return Serialise::_float(std::fabs(Unserialise::_float(values.back()) - _ref_val));
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::_float(std::fabs(Unserialise::_float(*last) - _ref_val));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		return Serialise::_float(std::fabs(Unserialise::_float(*it) - _ref_val));
	}

	double distance1 = std::fabs(Unserialise::_float(*it_p) - _ref_val);
	double distance2 = std::fabs(Unserialise::_float(*it) - _ref_val);
	return Serialise::_float(distance1 > distance2 ? distance1 : distance2);
}


std::string
IntegerKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		return Serialise::integer(std::llabs(Unserialise::integer(*it) - _ref_val));
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::integer(std::llabs(Unserialise::integer(*last) - _ref_val));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	int64_t distance1 = std::llabs(Unserialise::integer(*it_p) - _ref_val);
	int64_t distance2 = std::llabs(Unserialise::integer(*it) - _ref_val);
	return Serialise::integer(distance1 < distance2 ? distance1 : distance2);
}


std::string
IntegerKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		return Serialise::integer(std::llabs(Unserialise::integer(values.back()) - _ref_val));
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		return Serialise::integer(std::llabs(Unserialise::integer(*last) - _ref_val));
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		return Serialise::integer(std::llabs(Unserialise::integer(*it) - _ref_val));
	}

	int64_t distance1 = std::llabs(Unserialise::integer(*it_p) - _ref_val);
	int64_t distance2 = std::llabs(Unserialise::integer(*it) - _ref_val);
	return Serialise::integer(distance1 > distance2 ? distance1 : distance2);
}


std::string
PositiveKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		uint64_t val = Unserialise::positive(*it);
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		uint64_t val = Unserialise::positive(*last);
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		return SERIALISED_ZERO;
	}

	uint64_t val = Unserialise::positive(*it_p);
	uint64_t distance1 = val > _ref_val ? val - _ref_val : _ref_val - val;
	val = Unserialise::positive(*it);
	uint64_t distance2 = val > _ref_val ? val - _ref_val : _ref_val - val;
	return Serialise::positive(distance1 < distance2 ? distance1 : distance2);
}


std::string
PositiveKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	auto it = values.cbegin();
	if (values.single() || it.compare(_ser_ref_val) >= 0) {
		uint64_t val = Unserialise::positive(values.back());
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto last = values.clast();
	if (last.compare(_ser_ref_val) <= 0) {
		uint64_t val = Unserialise::positive(*it);
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	auto it_p = it++;
	for ( ; it != last && it.compare(_ser_ref_val) <= 0; it_p = it++);

	if (it_p.compare(_ser_ref_val) == 0) {
		uint64_t val = Unserialise::positive(*it);
		return Serialise::positive(val > _ref_val ? val - _ref_val : _ref_val - val);
	}

	uint64_t val = Unserialise::positive(*it_p);
	uint64_t distance1 = val > _ref_val ? val - _ref_val : _ref_val - val;
	val = Unserialise::positive(*it);
	uint64_t distance2 = val > _ref_val ? val - _ref_val : _ref_val - val;
	return Serialise::positive(distance1 > distance2 ? distance1 : distance2);
}


std::string
BoolKey::findSmallest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	if (values.front().at(0) == _ref_val.at(0) || values.back().at(0) == _ref_val.at(0)) {
		return SERIALISED_ZERO;
	} else {
		return SERIALISED_ONE;
	}
}


std::string
BoolKey::findBiggest(const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_CMPVALUE;
	}

	StringList values(std::move(multiValues));

	if (values.front().at(0) != _ref_val.at(0) || values.back().at(0) != _ref_val.at(0)) {
		return SERIALISED_ONE;
	} else {
		return SERIALISED_ZERO;
	}
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
			double rad_angle = std::acos(_centroid * centroid);
			if (rad_angle < min_angle) {
				min_angle = rad_angle;
			}
		}
	}

	return Serialise::_float(min_angle);
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
			double rad_angle = std::acos(_centroid * centroid);
			if (rad_angle > max_angle) {
				max_angle = rad_angle;
			}
		}
	}

	return Serialise::_float(max_angle);
}


void
Multi_MultiValueKeyMaker::add_value(const required_spc_t& field_spc, bool reverse, const std::string& value, const query_field_t& qf)
{
	if (value.empty()) {
		if (field_spc.get_type() != FieldType::GEO) {
			slots.push_back(std::make_unique<SerialiseKey>(field_spc.slot, reverse));
		}
	} else {
		switch (field_spc.get_type()) {
			case FieldType::FLOAT:
				slots.push_back(std::make_unique<FloatKey>(field_spc.slot, reverse, value));
				return;
			case FieldType::INTEGER:
				slots.push_back(std::make_unique<IntegerKey>(field_spc.slot, reverse, value));
				return;
			case FieldType::POSITIVE:
				slots.push_back(std::make_unique<PositiveKey>(field_spc.slot, reverse, value));
				return;
			case FieldType::DATE:
				slots.push_back(std::make_unique<DateKey>(field_spc.slot, reverse, value));
				return;
			case FieldType::BOOLEAN:
				slots.push_back(std::make_unique<BoolKey>(field_spc.slot, reverse, value));
				return;
			case FieldType::UUID:
			case FieldType::TERM:
			case FieldType::TEXT:
			case FieldType::STRING:
				try {
					auto func = map_dispatch_str_metric.at(qf.metric);
					(this->*func)(field_spc, reverse, value, qf);
				} catch (const std::out_of_range&) {
					(this->*def_str_metric)(field_spc, reverse, value, qf);
				}
				return;
			case FieldType::GEO: {
				EWKT ewkt(value);
				auto centroids = ewkt.getGeometry()->getCentroids();
				if (!centroids.empty()) {
					slots.push_back(std::make_unique<GeoKey>(field_spc, reverse, std::move(centroids)));
				}
				return;
			}
			default:
				THROW(InvalidArgumentError, "Type '%c' is not supported", field_spc.get_type());
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
