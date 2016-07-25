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

#pragma once

#include "../serialise.h"
#include "../sortable_serialise.h"
#include "../string_metric.h"
#include "../wkt_parser.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>


const std::string MAX_CMPVALUE(sortable_serialise(DBL_MAX));
const std::string STR_FOR_EMPTY("\xff");


// Base class for create keys.
class BaseKey {
protected:
	Xapian::valueno _slot;
	bool _reverse;

public:
	BaseKey(Xapian::valueno slot, bool reverse)
		: _slot(slot),
		  _reverse(reverse) { }

	virtual std::string findSmallest(const Xapian::Document& doc) const;
	virtual std::string findLargest(const Xapian::Document& doc) const;
	virtual std::string get_cmpvalue(const std::string& serialise_val) const = 0;

	bool get_reverse() const {
		return _reverse;
	}
};


// Class for create the key from the serialized value.
class SerialiseKey : public BaseKey {
	std::string get_cmpvalue(const std::string&) const override {
		return STR_FOR_EMPTY;
	}

public:
	std::string findSmallest(const Xapian::Document& doc) const override;
	std::string findLargest(const Xapian::Document& doc) const override;

	SerialiseKey(Xapian::valueno slot, bool reverse)
		: BaseKey(slot, reverse) { }
};


// Class for create the key using as a reference a float value.
class FloatKey : public BaseKey {
	double _ref_val;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		return sortable_serialise(std::fabs(Unserialise::_float(serialise_val) - _ref_val));
	}

public:
	FloatKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _ref_val(strict(std::stod, value)) { }
};


// Class for create the key using as a reference a integer value.
class IntegerKey : public BaseKey {
	int64_t _ref_val;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		return sortable_serialise(std::llabs(Unserialise::integer(serialise_val) - _ref_val));
	}

public:
	IntegerKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _ref_val(strict(std::stoll, value)) { }
};


// Class for create the key using as a reference a positive value.
class PositiveKey : public BaseKey {
	uint64_t _ref_val;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		int64_t val = Unserialise::positive(serialise_val) - _ref_val;
		return sortable_serialise(std::llabs(val));
	}

public:
	PositiveKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _ref_val(strict(std::stoull, value)) { }
};


// Class for create the key using as a reference a timestamp value.
class DateKey : public BaseKey {
	double _ref_val;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		return sortable_serialise(std::fabs(Unserialise::timestamp(serialise_val) - _ref_val));
	}

public:
	DateKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _ref_val(Datetime::timestamp(value)) { }
};


// Class for create the key using as a reference a boolean value.
class BoolKey : public BaseKey {
	std::string _ref_val;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		return sortable_serialise(serialise_val[0] != _ref_val[0]);
	}

public:
	BoolKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _ref_val(Serialise::boolean(value)) { }
};


// Class for create the key using as a reference a boolean value.
template <typename Metric>
class StringKey : public BaseKey {
	Metric _metric;

	std::string get_cmpvalue(const std::string& serialise_val) const override {
		return sortable_serialise(_metric.distance(serialise_val));
	}

public:
	StringKey(Xapian::valueno slot, bool reverse, const std::string& value, bool icase)
		: BaseKey(slot, reverse),
		  _metric(value, icase) { }
};


// Class for create the key using as a reference a geospatial value.
class GeoKey : public BaseKey {
	CartesianUSet _centroids;

	std::string get_cmpvalue(const std::string& serialise_val) const override;

public:
	GeoKey(Xapian::valueno slot, bool reverse, const std::string& value)
		: BaseKey(slot, reverse),
		  _centroids(EWKT_Parser::getCentroids(value, true, HTM_MIN_ERROR)) { }
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
	Multi_MultiValueKeyMaker(Iterator begin, Iterator end) {
		slots.reserve(std::distance(begin, end));
		while (begin != end) add_value(*begin++);
	}

	virtual std::string operator()(const Xapian::Document& doc) const override;
	void add_value(Xapian::valueno slot, bool reverse, char type, const std::string& value, const std::string& metric, bool icase);

	void levenshtein(Xapian::valueno slot,  bool reverse, const std::string& value, bool icase) {
		slots.push_back(std::make_unique<StringKey<Levenshtein>>(slot, reverse, value, icase));
	}

	void jaro(Xapian::valueno slot, bool reverse, const std::string& value, bool icase) {
		slots.push_back(std::make_unique<StringKey<Jaro>>(slot, reverse, value, icase));
	}

	void jaro_winkler(Xapian::valueno slot, bool reverse, const std::string& value, bool icase) {
		slots.push_back(std::make_unique<StringKey<Jaro_Winkler>>(slot, reverse, value, icase));
	}

	void sorensen_dice(Xapian::valueno slot, bool reverse, const std::string& value, bool icase) {
		slots.push_back(std::make_unique<StringKey<Sorensen_Dice>>(slot, reverse, value, icase));
	}

	void jaccard(Xapian::valueno slot, bool reverse, const std::string& value, bool icase) {
		slots.push_back(std::make_unique<StringKey<Jaccard>>(slot, reverse, value, icase));
	}
};


using dispatch_str_metric = void (Multi_MultiValueKeyMaker::*)(Xapian::valueno, bool, const std::string&, bool);

extern const std::unordered_map<std::string, dispatch_str_metric> map_dispatch_str_metric;
extern const dispatch_str_metric def_str_metric;
