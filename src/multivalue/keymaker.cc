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

#include <utility>                                // for std::pair

#include "exception.h"                            // for InvalidArgumentError, MSG_I...
#include "geospatial/ewkt.h"                      // for EWKT
#include "length.h"                               // for serialise_length, unserialise_length
#include "xapian/common/serialise-double.h"       // for serialise_double, unserialise_double


std::string
BaseKey::serialise() const
{
	std::string serialised;
	serialised += serialise_length(_slot);
	serialised += serialise_length(_reverse);
	return serialised;
}


void
BaseKey::unserialise(const char** p, const char* p_end)
{
	_slot = unserialise_length(p, p_end);
	_reverse = unserialise_length(p, p_end);
}


std::string
SerialiseKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	return serialised;
}


void
SerialiseKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
}


std::string
SerialiseKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_STR_CMPVALUE;
	}

	StringList values(multiValues);

	return std::string(values.front());
}


std::string
SerialiseKey::findBiggest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MIN_STR_CMPVALUE;
	}

	StringList values(multiValues);

	return std::string(values.back());
}


std::string
FloatKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_double(_ref_val);
	serialised += serialise_string(_ser_ref_val);
	return serialised;
}


void
FloatKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	_ref_val = unserialise_double(p, p_end);
	_ser_ref_val = unserialise_string(p, p_end);
}


std::string
FloatKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
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
	const auto multiValues = doc.get_value(_slot);
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
IntegerKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_length(_ref_val);
	serialised += serialise_string(_ser_ref_val);
	return serialised;
}


void
IntegerKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	_ref_val = unserialise_length(p, p_end);
	_ser_ref_val = unserialise_string(p, p_end);
}


std::string
IntegerKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
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
	const auto multiValues = doc.get_value(_slot);
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
PositiveKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_length(_ref_val);
	serialised += serialise_string(_ser_ref_val);
	return serialised;
}


void
PositiveKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	_ref_val = unserialise_length(p, p_end);
	_ser_ref_val = unserialise_string(p, p_end);
}


std::string
PositiveKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
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
	const auto multiValues = doc.get_value(_slot);
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
DateKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_double(_ref_val);
	serialised += serialise_string(_ser_ref_val);
	return serialised;
}


void
DateKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	_ref_val = unserialise_double(p, p_end);
	_ser_ref_val = unserialise_string(p, p_end);
}


std::string
DateKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
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
	const auto multiValues = doc.get_value(_slot);
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
BoolKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_string(_ser_ref_val);
	return serialised;
}


void
BoolKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	_ser_ref_val = unserialise_string(p, p_end);
}


std::string
BoolKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
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
	const auto multiValues = doc.get_value(_slot);
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
GeoKey::serialise() const
{
	std::string serialised;
	serialised += BaseKey::serialise();
	serialised += serialise_string(Serialise::centroids(_centroids));
	return serialised;
}


void
GeoKey::unserialise(const char** p, const char* p_end)
{
	BaseKey::unserialise(p, p_end);
	for (const auto& centroid : Unserialise::centroids(unserialise_string(p, p_end))) {
		_centroids.emplace_back(std::move(centroid));
	}
}


std::string
GeoKey::findSmallest(const Xapian::Document& doc) const
{
	const auto multiValues = doc.get_value(_slot);
	if (multiValues.empty()) {
		return MAX_CMPVALUE;
	}

	auto centroids = Unserialise::centroids(multiValues);

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

	auto centroids = Unserialise::centroids(multiValues);

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


Multi_MultiValueKeyMaker*
Multi_MultiValueKeyMaker::clone() const
{
	auto sorter = std::make_unique<Multi_MultiValueKeyMaker>();
	for (const auto& slot : slots) {
		sorter->slots.push_back(slot->clone());
	}
	return sorter.release();
}


std::string
Multi_MultiValueKeyMaker::name() const
{
	return "Multi_MultiValueKeyMaker";
}


std::string
Multi_MultiValueKeyMaker::serialise() const
{
	std::string serialised;
	for (const auto& slot : slots) {
		serialised += serialise_string(slot->name());
		serialised += serialise_string(slot->serialise());
	}
	return serialised;
}


Multi_MultiValueKeyMaker*
Multi_MultiValueKeyMaker::unserialise(const std::string& serialised, const Xapian::Registry& /*registry*/) const
{
	constexpr static auto _ = phf::make_phf({
		hh("SerialiseKey"),
		hh("FloatKey"),
		hh("IntegerKey"),
		hh("PositiveKey"),
		hh("DateKey"),
		hh("BoolKey"),
		hh("GeoKey"),
		hh("Jaccard"),
		hh("Jaro"),
		hh("Jaro_Winkler"),
		hh("Sorensen_Dice"),
		hh("LCSubsequence"),
		hh("LCSubstr"),
		hh("Levenshtein"),
		hh("SoundexEnglish"),
		hh("SoundexFrench"),
		hh("SoundexGerman"),
		hh("SoundexSpanish"),
	});

	auto sorter = std::make_unique<Multi_MultiValueKeyMaker>();
	const char *p = serialised.data();
	const char *p_end = p + serialised.size();
	while (p != p_end) {
		auto name = unserialise_string(&p, p_end);
		auto data = unserialise_string(&p, p_end);
		const char *q = data.data();
		const char *q_end = q + data.size();
		std::unique_ptr<BaseKey> slot;
		switch (_.fhh(name)) {
			case _.fhh("SerialiseKey"):
				slot = std::make_unique<SerialiseKey>();
				break;
			case _.fhh("FloatKey"):
				slot = std::make_unique<FloatKey>();
				break;
			case _.fhh("IntegerKey"):
				slot = std::make_unique<IntegerKey>();
				break;
			case _.fhh("PositiveKey"):
				slot = std::make_unique<PositiveKey>();
				break;
			case _.fhh("DateKey"):
				slot = std::make_unique<DateKey>();
				break;
			case _.fhh("BoolKey"):
				slot = std::make_unique<BoolKey>();
				break;
			case _.fhh("GeoKey"):
				slot = std::make_unique<GeoKey>();
				break;
			case _.fhh("Levenshtein"):
				slot = std::make_unique<StringKey<Levenshtein>>();
				break;
			case _.fhh("Jaro"):
				slot = std::make_unique<StringKey<Jaro>>();
				break;
			case _.fhh("Jaro_Winkler"):
				slot = std::make_unique<StringKey<Jaro_Winkler>>();
				break;
			case _.fhh("Sorensen_Dice"):
				slot = std::make_unique<StringKey<Sorensen_Dice>>();
				break;
			case _.fhh("Jaccard"):
				slot = std::make_unique<StringKey<Jaccard>>();
				break;
			case _.fhh("LCSubstr"):
				slot = std::make_unique<StringKey<LCSubstr>>();
				break;
			case _.fhh("LCSubsequence"):
				slot = std::make_unique<StringKey<LCSubsequence>>();
				break;
			case _.fhh("SoundexEnglish"):
				slot = std::make_unique<StringKey<SoundexMetric<SoundexEnglish, LCSubsequence>>>();
				break;
			case _.fhh("SoundexFrench"):
				slot = std::make_unique<StringKey<SoundexMetric<SoundexFrench, LCSubsequence>>>();
				break;
			case _.fhh("SoundexGerman"):
				slot = std::make_unique<StringKey<SoundexMetric<SoundexGerman, LCSubsequence>>>();
				break;
			case _.fhh("SoundexSpanish"):
				slot = std::make_unique<StringKey<SoundexMetric<SoundexSpanish, LCSubsequence>>>();
				break;
			default:
				THROW(SerialisationError, "{} not implemented");
		}
		slot->unserialise(&q, q_end);
		if (q != q_end) {
			THROW(SerialisationError, "Bad serialised {} - junk at end", name);
		}
		sorter->slots.push_back(std::move(slot));
	}
	return sorter.release();
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
