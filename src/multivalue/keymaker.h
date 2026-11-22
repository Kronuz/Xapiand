/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#pragma once

#include <cfloat>                                 // for DBL_MAX
#include <cmath>                                  // for fabs
#include <cstdlib>                                // for llabs
#include <memory>                                 // for default_delete, unique_ptr
#include <stdexcept>                              // for out_of_range
#include <string>                                 // for string, operator==, stod
#include <sys/types.h>                            // for int64_t, uint64_t
#include <vector>                                 // for std::vector

#include "database/schema.h"                      // for required_spc_t, required_sp...
#include "database/utils.h"                       // for query_field_t
#include "datetime.h"                             // for timestamp
#include "engine/callbacks.h"                    // for KeyMaker, DocView
#include "hashes.hh"                              // for fnv1ah32
#include "phf.hh"                                 // for phf
#include "phonetic.h"                             // for SoundexEnglish, SoundexFrench...
#include "serialize_list.h"                       // for StringList, ...
#include "sortable.hh"                             // for sortable::max_finite, sortable::min_finite
#include "strict_stox.hh"                         // for strict_stoull
#include "string_metric.h"                        // for Jaccard, Jaro, ..., Serializable* wrappers, DoubleMetaphone


// A missing value's key: at DBL_MAX (or -DBL_MAX), below +inf (or above -inf).
inline const std::string MAX_CMPVALUE(sortable::max_finite);
inline const std::string MIN_CMPVALUE(sortable::min_finite);

inline const std::string SERIALIZED_ZERO(Serialize::floating(0));
inline const std::string SERIALIZED_ONE(Serialize::floating(1));
inline const std::string SERIALIZED_M_PI(Serialize::floating(M_PI));

inline const std::string MAX_STR_CMPVALUE("\xff");
// The least byte string: a missing slot keys at or below every value.
inline const std::string MIN_STR_CMPVALUE;


class Multi_MultiValueKeyMaker;


using dispatch_str_metric = void (Multi_MultiValueKeyMaker::*)(const required_spc_t&, bool, std::string_view, const query_field_t&);


// Base class for creating keys.
class BaseKey {
protected:
	engine::valueno _slot;
	bool _reverse;

public:
	BaseKey() = default;

	BaseKey(engine::valueno slot, bool reverse)
		: _slot(slot),
		  _reverse(reverse) { }

	virtual ~BaseKey() = default;

	virtual std::string_view name() const noexcept = 0;
	virtual std::unique_ptr<BaseKey> clone() const = 0;
	virtual std::string serialize() const;
	virtual void unserialize(const char** p, const char* p_end);

	bool get_reverse() const noexcept {
		return _reverse;
	}

	virtual std::string findSmallest(const engine::DocView& doc) const = 0;
	virtual std::string findBiggest(const engine::DocView& doc) const = 0;
};


// Class for creating key from serialized value.
class SerializeKey : public BaseKey {
public:
	SerializeKey() = default;

	SerializeKey(engine::valueno slot, bool reverse)
		: BaseKey(slot, reverse) { }

	std::string_view name() const noexcept override {
		return "SerializeKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<SerializeKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a float value.
class FloatKey : public BaseKey {
	double _ref_val;
	std::string _ser_ref_val;

public:
	FloatKey() = default;

	FloatKey(engine::valueno slot, bool reverse, double val)
		: BaseKey(slot, reverse),
		  _ref_val(val),
		  _ser_ref_val(Serialize::floating(_ref_val)) { }

	std::string_view name() const noexcept override {
		return "FloatKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<FloatKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a integer value.
class IntegerKey : public BaseKey {
	int64_t _ref_val;
	std::string _ser_ref_val;

public:
	IntegerKey() = default;

	IntegerKey(engine::valueno slot, bool reverse, int64_t val)
		: BaseKey(slot, reverse),
		  _ref_val(val),
		  _ser_ref_val(Serialize::integer(_ref_val)) { }

	std::string_view name() const noexcept override {
		return "IntegerKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<IntegerKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a positive value.
class PositiveKey : public BaseKey {
	uint64_t _ref_val;
	std::string _ser_ref_val;

public:
	PositiveKey() = default;

	PositiveKey(engine::valueno slot, bool reverse, uint64_t val)
		: BaseKey(slot, reverse),
		  _ref_val(val),
		  _ser_ref_val(Serialize::positive(_ref_val)) { }

	std::string_view name() const noexcept override {
		return "PositiveKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<PositiveKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a date value.
class DateKey : public BaseKey {
	double _ref_val;
	std::string _ser_ref_val;

public:
	DateKey() = default;

	DateKey(engine::valueno slot, bool reverse, double val)
		: BaseKey(slot, reverse),
		  _ref_val(val),
		  _ser_ref_val(Serialize::timestamp(_ref_val)) { }

	std::string_view name() const noexcept override {
		return "DateKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<DateKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a boolean value.
class BoolKey : public BaseKey {
	std::string _ser_ref_val;

public:
	BoolKey() = default;

	BoolKey(engine::valueno slot, bool reverse, bool val)
		: BaseKey(slot, reverse),
		  _ser_ref_val(Serialize::boolean(val)) { }

	std::string_view name() const noexcept override {
		return "BoolKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<BoolKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


// Class for creating key using as reference a string value.
template <typename StringMetric>
class StringKey : public BaseKey {
	StringMetric _metric;

public:
	StringKey() = default;

	StringKey(engine::valueno slot, bool reverse, std::string_view value, bool icase)
		: BaseKey(slot, reverse),
		  _metric(value, icase) { }

	std::string_view name() const noexcept override {
		return _metric.name();
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<StringKey<StringMetric>>(*this);
	}

	std::string serialize() const override {
		return BaseKey::serialize() + _metric.serialize();
	}

	void unserialize(const char** p, const char* p_end) override {
		BaseKey::unserialize(p, p_end);
		_metric.unserialize(p, p_end);
	}

	std::string findSmallest(const engine::DocView& doc) const override {
		const auto multiValues = doc.value(_slot);
		if (multiValues.empty()) {
			return MAX_CMPVALUE;
		}

		StringList values(multiValues);

		double min_distance = DBL_MAX;
		for (const auto& value : values) {
			double distance = _metric.distance(value);
			if (distance < min_distance) {
				if (distance < DBL_TOLERANCE) {
					return SERIALIZED_ZERO;
				}
				min_distance = distance;
			}
		}

		return Serialize::floating(min_distance);
	}

	std::string findBiggest(const engine::DocView& doc) const override {
		const auto multiValues = doc.value(_slot);
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

		return Serialize::floating(max_distance);
	}
};


// Class for creating key using as reference a geospatial value.
class GeoKey : public BaseKey {
	std::vector<Cartesian> _centroids;

public:
	GeoKey() = default;

	template <typename T, typename = std::enable_if_t<std::is_same<std::vector<Cartesian>, std::decay_t<T>>::value>>
	GeoKey(engine::valueno slot, bool reverse, T&& centroids)
		: BaseKey(slot, reverse),
		  _centroids(std::forward<T>(centroids)) { }

	std::string_view name() const noexcept override {
		return "GeoKey";
	}

	std::unique_ptr<BaseKey> clone() const override {
		return std::make_unique<GeoKey>(*this);
	}

	std::string serialize() const override;
	void unserialize(const char** p, const char* p_end) override;

	std::string findSmallest(const engine::DocView& doc) const override;
	std::string findBiggest(const engine::DocView& doc) const override;
};


/*
 * KeyMaker subclass which combines several Multivalues.
 * This class only is used for sorting, there are two cases:
 * Case 1: Ascending (include in the query -> sort:+field_name or sort:field_name), of all values is selected the smallest.
 * Case 2: Descending (include in the query -> sort:-field_name), of all values is selected the largest.

 * Collapsing uses a slot (set_collapse_key), not this class.
 */
class Multi_MultiValueKeyMaker : public engine::KeyMaker {
	// Vector of slots
	std::vector<std::unique_ptr<BaseKey>> slots;

public:
	std::unique_ptr<engine::KeyMaker> clone() const override;
	std::string name() const override;
	std::string serialize() const override;
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view serialized) const override;

	std::string key(const engine::DocView& doc) const override;

	template <typename... Args>
	void add_serialize(Args... args) {
		slots.push_back(std::make_unique<SerializeKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_float(Args... args) {
		slots.push_back(std::make_unique<FloatKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_integer(Args... args) {
		slots.push_back(std::make_unique<IntegerKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_positive(Args... args) {
		slots.push_back(std::make_unique<PositiveKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_date(Args... args) {
		slots.push_back(std::make_unique<DateKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_bool(Args... args) {
		slots.push_back(std::make_unique<BoolKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_geo(Args... args) {
		slots.push_back(std::make_unique<GeoKey>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_levenshtein(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableLevenshtein>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_jaro(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableMetric<Jaro>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_jaro_winkler(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableJaroWinkler>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_sorensen_dice(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableMetric<Sorensen_Dice>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_jaccard(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableMetric<Jaccard>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_lcs(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableMetric<LCSubstr>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_lcsq(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializableMetric<LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_soundex_en(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializablePhoneticMetric<SoundexEnglish, LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_soundex_fr(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializablePhoneticMetric<SoundexFrench, LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_soundex_de(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializablePhoneticMetric<SoundexGerman, LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_soundex_es(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializablePhoneticMetric<SoundexSpanish, LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_double_metaphone(Args... args) {
		slots.push_back(std::make_unique<StringKey<SerializablePhoneticMetric<DoubleMetaphone, LCSubsequence>>>(std::forward<Args>(args)...));
	}

	template <typename... Args>
	void add_string_soundex(std::string_view language, Args... args) {
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

		switch (_.fhh(language)) {
			case _.fhh("english"):
			case _.fhh("en"):
			default:
				add_string_soundex_en(std::forward<Args>(args)...);
				break;
			case _.fhh("french"):
			case _.fhh("fr"):
				add_string_soundex_fr(std::forward<Args>(args)...);
				break;
			case _.fhh("german"):
			case _.fhh("de"):
				add_string_soundex_de(std::forward<Args>(args)...);
				break;
			case _.fhh("spanish"):
			case _.fhh("es"):
				add_string_soundex_es(std::forward<Args>(args)...);
				break;
		}
	}
};
