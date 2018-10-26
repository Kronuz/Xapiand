/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#pragma once

#include <cfloat>                         // for DBL_MAX
#include <cmath>                          // for fabs
#include <cstdlib>                        // for llabs
#include <memory>                         // for default_delete, unique_ptr
#include <stdexcept>                      // for out_of_range
#include <string>                         // for string, operator==, stod
#include <sys/types.h>                    // for int64_t, uint64_t
#include <xapian.h>                       // for valueno, KeyMaker

#include "database_utils.h"               // for query_field_t
#include "datetime.h"                     // for timestamp
#include "hashes.hh"                      // for fnv1ah32
#include "phonetic.h"                     // for SoundexEnglish, SoundexFrench...
#include "schema.h"                       // for required_spc_t, required_sp...
#include "serialise_list.h"               // for StringList, ...
#include "strict_stox.hh"                 // for strict_stoull
#include "string_metric.h"                // for Jaccard, Jaro, Jaro_Winkler...


const std::string MAX_CMPVALUE(Serialise::_float(DBL_MAX));
const std::string MIN_CMPVALUE(Serialise::_float(-DBL_MAX));

const std::string SERIALISED_ZERO(Serialise::_float(0));
const std::string SERIALISED_ONE(Serialise::_float(1));
const std::string SERIALISED_M_PI(Serialise::_float(M_PI));

const std::string MAX_STR_CMPVALUE("\xff");
const std::string MIN_STR_CMPVALUE("\x00");


class Multi_MultiValueKeyMaker;


using dispatch_str_metric = void (Multi_MultiValueKeyMaker::*)(const required_spc_t&, bool, std::string_view, const query_field_t&);


// Base class for creating keys.
class BaseKey {
protected:
	Xapian::valueno _slot;
	bool _reverse;

public:
	BaseKey(Xapian::valueno slot, bool reverse)
		: _slot(slot),
		  _reverse(reverse) { }

	virtual ~BaseKey() = default;

	bool get_reverse() const noexcept {
		return _reverse;
	}

	virtual std::string findSmallest(const Xapian::Document& doc) const = 0;
	virtual std::string findBiggest(const Xapian::Document& doc) const = 0;
};


// Class for creating key from serialised value.
class SerialiseKey : public BaseKey {
public:
	SerialiseKey(Xapian::valueno slot, bool reverse)
		: BaseKey(slot, reverse) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a float value.
class FloatKey : public BaseKey {
	double _ref_val;
	std::string _ser_ref_val;

public:
	FloatKey(Xapian::valueno slot, bool reverse, std::string_view value)
		: BaseKey(slot, reverse),
		  _ref_val(strict_stod(value)),
		  _ser_ref_val(Serialise::_float(_ref_val)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a integer value.
class IntegerKey : public BaseKey {
	int64_t _ref_val;
	std::string _ser_ref_val;

public:
	IntegerKey(Xapian::valueno slot, bool reverse, std::string_view value)
		: BaseKey(slot, reverse),
		  _ref_val(strict_stoll(value)),
		  _ser_ref_val(Serialise::integer(_ref_val)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a positive value.
class PositiveKey : public BaseKey {
	uint64_t _ref_val;
	std::string _ser_ref_val;

public:
	PositiveKey(Xapian::valueno slot, bool reverse, std::string_view value)
		: BaseKey(slot, reverse),
		  _ref_val(strict_stoull(value)),
		  _ser_ref_val(Serialise::positive(_ref_val)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a date value.
class DateKey : public BaseKey {
	double _ref_val;
	std::string _ser_ref_val;

public:
	DateKey(Xapian::valueno slot, bool reverse, std::string_view value)
		: BaseKey(slot, reverse),
		  _ref_val(Datetime::timestamp(Datetime::DateParser(value))),
		  _ser_ref_val(Serialise::timestamp(_ref_val)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a boolean value.
class BoolKey : public BaseKey {
	std::string _ref_val;

public:
	BoolKey(Xapian::valueno slot, bool reverse, std::string_view value)
		: BaseKey(slot, reverse),
		  _ref_val(Serialise::boolean(value)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


// Class for creating key using as reference a string value.
template <typename StringMetric>
class StringKey : public BaseKey {
	StringMetric _metric;

public:
	StringKey(Xapian::valueno slot, bool reverse, std::string_view value, bool icase)
		: BaseKey(slot, reverse),
		  _metric(value, icase) { }

	std::string findSmallest(const Xapian::Document& doc) const override {
		const auto multiValues = doc.get_value(_slot);
		if (multiValues.empty()) {
			return MAX_CMPVALUE;
		}

		StringList values(multiValues);

		double min_distance = DBL_MAX;
		for (const auto& value : values) {
			double distance = _metric.distance(value);
			if (distance < min_distance) {
				if (distance < DBL_TOLERANCE) {
					return SERIALISED_ZERO;
				}
				min_distance = distance;
			}
		}

		return Serialise::_float(min_distance);
	}

	std::string findBiggest(const Xapian::Document& doc) const override {
		const auto multiValues = doc.get_value(_slot);
		if (multiValues.empty()) {
			return MIN_CMPVALUE;
		}

		StringList values(multiValues);

		double max_distance = -DBL_MAX;
		for (const auto& value : values) {
			double distance = _metric.distance(value);
			if (distance > max_distance) {
				max_distance = distance;
			}
		}

		return Serialise::_float(max_distance);
	}
};


// Class for creating key using as reference a geospatial value.
class GeoKey : public BaseKey {
	std::vector<Cartesian> _centroids;

public:
	template <typename T, typename = std::enable_if_t<std::is_same<std::vector<Cartesian>, std::decay_t<T>>::value>>
	GeoKey(const required_spc_t& field_spc, bool reverse, T&& centroids)
		: BaseKey(field_spc.slot, reverse),
		  _centroids(std::forward<T>(centroids)) { }

	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findBiggest(const Xapian::Document& doc) const override;
};


/*
 * KeyMaker subclass which combines several Multivalues.
 * This class only is used for sorting, there are two cases:
 * Case 1: Ascending (include in the query -> sort:+field_name or sort:field_name), of all values is selected the smallest.
 * Case 2: Descending (include in the query -> sort:-field_name), of all values is selected the largest.

 * For collapsing, it is used Xapian::MultiValueKeyMaker.
 */
class Multi_MultiValueKeyMaker : public Xapian::KeyMaker {
	// Vector of slots
	std::vector<std::unique_ptr<BaseKey>> slots;

public:
	Multi_MultiValueKeyMaker() = default;

	template <class Iterator>
	Multi_MultiValueKeyMaker(Iterator begin, Iterator end)
		: slots(begin, end) { }

	virtual std::string operator()(const Xapian::Document& doc) const override;
	void add_value(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf);

	void levenshtein(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<Levenshtein>>(field_spc.slot, reverse, value, qf.icase));
	}

	void jaro(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<Jaro>>(field_spc.slot, reverse, value, qf.icase));
	}

	void jaro_winkler(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<Jaro_Winkler>>(field_spc.slot, reverse, value, qf.icase));
	}

	void sorensen_dice(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<Sorensen_Dice>>(field_spc.slot, reverse, value, qf.icase));
	}

	void jaccard(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<Jaccard>>(field_spc.slot, reverse, value, qf.icase));
	}

	void lcs(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<LCSubstr>>(field_spc.slot, reverse, value, qf.icase));
	}

	void lcsq(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<LCSubsequence>>(field_spc.slot, reverse, value, qf.icase));
	}

	void soundex_en(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<SoundexMetric<SoundexEnglish, LCSubsequence>>>(field_spc.slot, reverse, value, qf.icase));
	}

	void soundex_fr(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<SoundexMetric<SoundexFrench, LCSubsequence>>>(field_spc.slot, reverse, value, qf.icase));
	}

	void soundex_de(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<SoundexMetric<SoundexGerman, LCSubsequence>>>(field_spc.slot, reverse, value, qf.icase));
	}

	void soundex_es(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		slots.push_back(std::make_unique<StringKey<SoundexMetric<SoundexSpanish, LCSubsequence>>>(field_spc.slot, reverse, value, qf.icase));
	}

	void soundex(const required_spc_t& field_spc, bool reverse, std::string_view value, const query_field_t& qf) {
		constexpr static auto _ = phf::make_phf({
			hh("english"),
			hh("en"),
			hh("french"),
			hh("fr"),
			hh("german"),
			hh("de"),
			hh("spanish"),
			hh("es"),
		});

		switch (_.fhh(field_spc.language)) {
			case _.fhh("english"):
			case _.fhh("en"):
			default:
				soundex_en(field_spc, reverse, value, qf);
				break;
			case _.fhh("french"):
			case _.fhh("fr"):
				soundex_fr(field_spc, reverse, value, qf);
				break;
			case _.fhh("german"):
			case _.fhh("de"):
				soundex_de(field_spc, reverse, value, qf);
				break;
			case _.fhh("spanish"):
			case _.fhh("es"):
				soundex_es(field_spc, reverse, value, qf);
				break;
		}
	}
};
