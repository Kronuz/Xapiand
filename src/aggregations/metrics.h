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

#pragma once

#include <algorithm>                // for std::nth_element, std::max_element
#include <cmath>                    // for sqrt
#include <cstdio>
#include <cstring>                  // for size_t
#include <limits>                   // for std::numeric_limits
#include <map>                      // for std::map
#include <memory>                   // for std::shared_ptr
#include <string>                   // for std::string
#include <string_view>              // for std::string_view
#include <utility>                  // for std::pair
#include <vector>                   // for std::vector

#include "aggregations.h"           // for BaseAggregation
#include "exception.h"              // for AggregationError, MSG_AggregationError
#include "msgpack.h"                // for MsgPack, object::object
#include "reserved/aggregations.h"  // for RESERVED_AGGS_*
#include "serialise_list.h"         // for StringList, RangeList
#include "xapian.h"                 // for valueno
#include "xapian/common/serialise-double.h"       // for serialise_double, unserialise_double


class Schema;


template <typename Handler>
class HandledSubAggregation;


class ValuesHandler {
	using func_value_handle = void (HandledSubAggregation<ValuesHandler>::*)(const Xapian::Document&);

protected:
	FieldType _type;
	Xapian::valueno _slot;
	func_value_handle _func;

public:
	ValuesHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	std::vector<std::string> values(const Xapian::Document& doc) const;

	FieldType get_type() {
		return _type;
	}

	void operator()(HandledSubAggregation<ValuesHandler>* agg, const Xapian::Document& doc) const {
		(agg->*_func)(doc);
	}
};


class TermsHandler {
	using func_value_handle = void (HandledSubAggregation<TermsHandler>::*)(const Xapian::Document&);

protected:
	FieldType _type;
	std::string _prefix;
	func_value_handle _func;

public:
	TermsHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	std::vector<std::string> values(const Xapian::Document& doc) const;

	FieldType get_type() {
		return _type;
	}

	void operator()(HandledSubAggregation<TermsHandler>* agg, const Xapian::Document& doc) const {
		(agg->*_func)(doc);
	}
};


template <typename Handler>
class HandledSubAggregation : public BaseAggregation {
protected:
	Handler _handler;
	const MsgPack& _conf;

public:
	HandledSubAggregation(const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: _handler(conf, schema),
		  _conf(conf) { }

	HandledSubAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation(context.at(name), schema) { }

	virtual void aggregate_float(long double, const Xapian::Document&) {
		THROW(AggregationError, "float type is not supported");
	}

	virtual void aggregate_integer(int64_t, const Xapian::Document&) {
		THROW(AggregationError, "integer type is not supported");
	}

	virtual void aggregate_positive(uint64_t, const Xapian::Document&) {
		THROW(AggregationError, "positive type is not supported");
	}

	virtual void aggregate_date(double, const Xapian::Document&) {
		THROW(AggregationError, "date type is not supported");
	}

	virtual void aggregate_time(double, const Xapian::Document&) {
		THROW(AggregationError, "time type is not supported");
	}

	virtual void aggregate_timedelta(double, const Xapian::Document&) {
		THROW(AggregationError, "timedelta type is not supported");
	}

	virtual void aggregate_boolean(bool, const Xapian::Document&) {
		THROW(AggregationError, "boolean type is not supported");
	}

	virtual void aggregate_string(std::string_view, const Xapian::Document&) {
		THROW(AggregationError, "string type is not supported");
	}

	virtual void aggregate_geo(const range_t&, const Xapian::Document&) {
		THROW(AggregationError, "geo type is not supported");
	}

	virtual void aggregate_uuid(std::string_view, const Xapian::Document&) {
		THROW(AggregationError, "uuid type is not supported");
	}

	void _aggregate_float(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_float(Unserialise::floating(value), doc);
		}
	}

	void _aggregate_integer(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_integer(Unserialise::integer(value), doc);
		}
	}

	void _aggregate_positive(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_positive(Unserialise::positive(value), doc);
		}
	}

	void _aggregate_date(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_date(Unserialise::timestamp(value), doc);
		}
	}

	void _aggregate_time(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_time(Unserialise::time_d(value), doc);
		}
	}

	void _aggregate_timedelta(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_timedelta(Unserialise::timedelta_d(value), doc);
		}
	}

	void _aggregate_boolean(const Xapian::Document& doc)  {
		for (const auto& value : _handler.values(doc)) {
			aggregate_boolean(Unserialise::boolean(value), doc);
		}
	}

	void _aggregate_string(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_string(value, doc);
		}
	}

	void _aggregate_geo(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			for (const auto& range : RangeList(value)) {
				aggregate_geo(range, doc);
			}
		}
	}

	void _aggregate_uuid(const Xapian::Document& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_uuid(Unserialise::uuid(value), doc);
		}
	}

	void operator()(const Xapian::Document& doc) override {
		_handler(this, doc);
	}
};


class MetricCount : public HandledSubAggregation<ValuesHandler> {
protected:
	long double _count;

public:
	MetricCount(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _count{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_COUNT, static_cast<uint64_t>(_count) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_double(_count);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricCount::merge_results({})", repr(std::string(*p, p_end - *p)));

		_count += unserialise_double(p, p_end);
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricCount::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricCount*>(other);

		_count += aggs->_count;
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_COUNT) {
			return &_count;
		}
		return nullptr;
	}

	void _aggregate() {
		++_count;
	}

	void aggregate_float(long double, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_integer(int64_t, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_positive(uint64_t, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_date(double, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_time(double, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_timedelta(double, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_boolean(bool, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_string(std::string_view, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_geo(const range_t&, const Xapian::Document&) override {
		_aggregate();
	}

	void aggregate_uuid(std::string_view, const Xapian::Document&) override {
		_aggregate();
	}
};


class MetricSum : public HandledSubAggregation<ValuesHandler> {
protected:
	long double _sum;

public:
	MetricSum(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _sum{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_SUM, static_cast<double>(_sum) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_double(_sum);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetriSum::merge_results({})", repr(std::string_view(p, p_end - p)));

		_sum += unserialise_double(p, p_end);
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricSum::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricSum*>(other);

		_sum += aggs->_sum;
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_SUM) {
			return &_sum;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		_sum += value;
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricAvg : public MetricSum {
protected:
	long double _count;

	long double _avg;

public:
	MetricAvg(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricSum(context, name, schema),
		  _count{0.0},
		  _avg{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_AVG, static_cast<double>(_avg) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += MetricSum::serialise_results();
		results += serialise_double(_count);
		// results += serialise_double(_avg);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricAvg::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricSum::merge_results(p, p_end);

		_count += unserialise_double(p, p_end);
		// _avg = unserialise_double(p, p_end);  // calculated during update()
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricAvg::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricAvg*>(other);

		MetricSum::merge_results(aggs);

		_count += aggs->_count;
		// _avg += aggs->_avg;  // calculated during update()
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_AVG) {
			return &_avg;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		MetricSum::_aggregate(value);
		++_count;
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void update() override {
		_avg = _sum ? _sum / _count : _sum;
	}
};


class MetricExtendedStats;
class MetricStats;


class MetricMin : public HandledSubAggregation<ValuesHandler> {
	friend class MetricStats;
	friend class MetricExtendedStats;

protected:
	long double _min;

public:
	MetricMin(const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(conf, schema),
		  _min(std::numeric_limits<long double>::max()) { }

	MetricMin(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _min(std::numeric_limits<long double>::max()) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MIN, static_cast<double>(_min) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_double(_min);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMin::merge_results({})", repr(std::string(*p, p_end - *p)));

		auto value = unserialise_double(p, p_end);
		if (_min > value) {
			_min = value;
		}
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricMin::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricMin*>(other);

		if (_min > aggs->_min) {
			_min = aggs->_min;
		}
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_MIN) {
			return &_min;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		if (_min > value) {
			_min = value;
		}
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricMax : public HandledSubAggregation<ValuesHandler> {
	friend class MetricStats;
	friend class MetricExtendedStats;

protected:
	long double _max;

public:
	MetricMax(const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(conf, schema),
		  _max(std::numeric_limits<long double>::min()) { }

	MetricMax(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _max(std::numeric_limits<long double>::min()) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MAX, static_cast<double>(_max) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_double(_max);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMax::merge_results({})", repr(std::string(*p, p_end - *p)));

		auto value = unserialise_double(p, p_end);
		if (_max < value) {
			_max = value;
		}
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricMax::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricMax*>(other);

		if (_max > aggs->_max) {
			_max = aggs->_max;
		}
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_MAX) {
			return &_max;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		if (_max < value) {
			_max = value;
		}
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricVariance : public MetricAvg {
protected:
	long double _sq_sum;
	long double _variance;

public:
	MetricVariance(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricAvg(context, name, schema),
		  _sq_sum{0.0},
		  _variance{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_VARIANCE, static_cast<double>(_variance) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += MetricAvg::serialise_results();
		results += serialise_double(_sq_sum);
		// results += serialise_double(_variance);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricVariance::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricAvg::merge_results(p, p_end);

		_sq_sum += unserialise_double(p, p_end);
		// _variance = unserialise_double(p, p_end);  // calculated during update()
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricVariance::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricVariance*>(other);

		MetricAvg::merge_results(aggs);

		_sq_sum += aggs->_sq_sum;
		// _variance = aggs->_variance;  // calculated during update()
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_VARIANCE) {
			return &_variance;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		MetricAvg::_aggregate(value);
		_sq_sum += value * value;
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void update() override {
		MetricAvg::update();
		_variance = (_sq_sum - _count * _avg * _avg) / (_count - 1);
	}
};


// Standard deviation.
class MetricStdDeviation : public MetricVariance {
protected:
	long double _sigma;

	long double _std;
	long double _upper;
	long double _lower;

public:
	MetricStdDeviation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricVariance(context, name, schema),
		  _sigma{2.0},
		  _upper{0.0},
		  _lower{0.0} {
		const auto it = _conf.find(RESERVED_AGGS_SIGMA);
		if (it != _conf.end()) {
			const auto& sigma_value = it.value();
			switch (sigma_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					_sigma = sigma_value.as_f64();
					if (_sigma >= 0.0) {
						break;
					}
					[[fallthrough]];
				default:
					THROW(AggregationError, "'{}' must be a positive number", RESERVED_AGGS_SIGMA);
			}
		}
	}

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_STD, static_cast<double>(_std) },
			{ RESERVED_AGGS_STD_BOUNDS, {
				{ RESERVED_AGGS_UPPER, static_cast<double>(_upper) },
				{ RESERVED_AGGS_LOWER, static_cast<double>(_lower) },
			}},
		};
	}


	std::string serialise_results() const override {
		std::string results;
		results += MetricVariance::serialise_results();
		results += serialise_double(_sigma);
		// results += serialise_double(_std);  // calculated during update()
		// results += serialise_double(_upper);  // calculated during update()
		// results += serialise_double(_lower);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricStdDeviation::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricVariance::merge_results(p, p_end);
		_sigma = unserialise_double(p, p_end);
		// _std = unserialise_double(p, p_end);  // calculated during update()
		// _upper = unserialise_double(p, p_end);  // calculated during update()
		// _lower = unserialise_double(p, p_end);  // calculated during update()
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricStdDeviation::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricStdDeviation*>(other);

		MetricVariance::merge_results(aggs);
		_sigma = aggs->_sigma;
		// _std = aggs->_std;  // calculated during update()
		// _upper = aggs->_upper;  // calculated during update()
		// _lower = aggs->_lower;  // calculated during update()
	}

	BaseAggregation* get_agg(std::string_view field) override {
		if (field == RESERVED_AGGS_STD_BOUNDS) {
			return this;  // FIXME: This is an ugly hack to allow getting fields inside _std_deviation_bounds
		}
		return nullptr;
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_STD) {
			return &_std;
		}
		if (field == RESERVED_AGGS_UPPER) {
			return &_upper;
		}
		if (field == RESERVED_AGGS_LOWER) {
			return &_lower;
		}
		return nullptr;
	}

	void update() override {
		MetricVariance::update();
		_std = std::sqrt(_variance);
		_upper = _avg + _std * _sigma;
		_lower = _avg - _std * _sigma;
	}
};


class MetricMedian : public HandledSubAggregation<ValuesHandler> {
	std::vector<long double> _values;

	long double _median;

public:
	MetricMedian(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _median{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MEDIAN, static_cast<double>(_median) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_length(_values.size());
		for (auto& value : _values) {
			results += serialise_double(value);
		}
		// results += serialise_double(_median);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMedian::merge_results({})", repr(std::string(*p, p_end - *p)));

		size_t size = unserialise_length(p, p_end);
		while (size--) {
			auto value = unserialise_double(p, p_end);
			_values.push_back(value);
		}
		// _median = unserialise_double(p, p_end);  // calculated during update()
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricMedian::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricMedian*>(other);

		for (auto& value : aggs->_values) {
			_values.push_back(value);
		}
		// _median = aggs->_median;  // calculated during update()
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_MEDIAN) {
			return &_median;
		}
		return nullptr;
	}

	void update() override {
		if (!_values.empty()) {
			std::sort(_values.begin(), _values.end());
			size_t median_pos = _values.size();
			if (median_pos % 2 == 0) {
				median_pos /= 2;
				std::nth_element(_values.begin(), _values.begin() + median_pos, _values.end());
				auto val1 = _values[median_pos];
				std::nth_element(_values.begin(), _values.begin() + median_pos - 1, _values.end());
				_median = (val1 + _values[median_pos - 1]) / 2;
			} else {
				median_pos /= 2;
				std::nth_element(_values.begin(), _values.begin() + median_pos, _values.end());
				_median = _values[median_pos];
			}
		}
	}

	void _aggregate(long double value) {
		_values.push_back(value);
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricMode : public HandledSubAggregation<ValuesHandler> {
	std::map<long double, size_t> _histogram;

	long double _mode;

public:
	MetricMode(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _mode{0.0} { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MODE, static_cast<double>(_mode) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += serialise_length(_histogram.size());
		for (auto& item : _histogram) {
			results += serialise_double(item.first);
			results += serialise_length(item.second);
		}
		// results += serialise_double(_mode);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMode::merge_results({})", repr(std::string(*p, p_end - *p)));

		size_t size = unserialise_length(p, p_end);
		while (size--) {
			auto key = unserialise_double(p, p_end);
			auto value = unserialise_length(p, p_end);
			_histogram[key] += value;
		}
		// _mode = unserialise_double(p, p_end);  // calculated during update()
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricMode::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricMode*>(other);

		for (auto& hist : aggs->_histogram) {
			_histogram[hist.first] += hist.second;
		}
		// _mode = aggs->_mode;  // calculated during update()
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_MODE) {
			return &_mode;
		}
		return nullptr;
	}

	void update() override {
		if (!_histogram.empty()) {
			auto it = std::max_element(_histogram.begin(), _histogram.end(), [](const std::pair<double, size_t>& a, const std::pair<double, size_t>& b) { return a.second < b.second; });
			_mode = static_cast<double>(it->first);
		}
	}

	void _aggregate(long double value) {
		++_histogram[value];
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricStats : public MetricAvg {
protected:
	MetricMin _min_metric;
	MetricMax _max_metric;

public:
	MetricStats(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricAvg(context, name, schema),
		  _min_metric(_conf, schema),
		  _max_metric(_conf, schema) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_COUNT, static_cast<uint64_t>(_count) },
			{ RESERVED_AGGS_MIN, static_cast<double>(_min_metric._min) },
			{ RESERVED_AGGS_MAX, static_cast<double>(_max_metric._max) },
			{ RESERVED_AGGS_AVG, static_cast<double>(_avg) },
			{ RESERVED_AGGS_SUM, static_cast<double>(_sum) },
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += MetricAvg::serialise_results();
		results += _min_metric.serialise_results();
		results += _max_metric.serialise_results();
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricStats::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricAvg::merge_results(p, p_end);
		_min_metric.merge_results(p, p_end);
		_max_metric.merge_results(p, p_end);
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricStats::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricStats*>(other);

		MetricAvg::merge_results(aggs);
		_min_metric.merge_results(aggs);
		_max_metric.merge_results(aggs);
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_COUNT) {
			return &_count;
		}
		if (field == RESERVED_AGGS_MIN) {
			return &_min_metric._min;
		}
		if (field == RESERVED_AGGS_MAX) {
			return &_max_metric._max;
		}
		if (field == RESERVED_AGGS_AVG) {
			return &_avg;
		}
		if (field == RESERVED_AGGS_SUM) {
			return &_sum;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		MetricAvg::_aggregate(value);
		_min_metric._aggregate(value);
		_max_metric._aggregate(value);
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};


class MetricExtendedStats : public MetricStdDeviation {
protected:
	MetricMin _min_metric;
	MetricMax _max_metric;

public:
	MetricExtendedStats(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricStdDeviation(context, name, schema),
		  _min_metric(_conf, schema),
		  _max_metric(_conf, schema) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_COUNT, static_cast<uint64_t>(_count) },
			{ RESERVED_AGGS_MIN, static_cast<double>(_min_metric._min) },
			{ RESERVED_AGGS_MAX, static_cast<double>(_max_metric._max) },
			{ RESERVED_AGGS_AVG, static_cast<double>(_avg) },
			{ RESERVED_AGGS_SUM, static_cast<double>(_sum) },
			{ RESERVED_AGGS_SUM_OF_SQ, static_cast<double>(_sq_sum) },
			{ RESERVED_AGGS_VARIANCE, static_cast<double>(_variance) },
			{ RESERVED_AGGS_STD, static_cast<double>(_std) },
			{ RESERVED_AGGS_STD_BOUNDS, {
				{ RESERVED_AGGS_UPPER, static_cast<double>(_upper) },
				{ RESERVED_AGGS_LOWER, static_cast<double>(_lower) },
			}},
		};
	}

	std::string serialise_results() const override {
		std::string results;
		results += MetricStdDeviation::serialise_results();
		results += _min_metric.serialise_results();
		results += _max_metric.serialise_results();
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricExtendedStats::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricStdDeviation::merge_results(p, p_end);
		_min_metric.merge_results(p, p_end);
		_max_metric.merge_results(p, p_end);
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricExtendedStats::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricExtendedStats*>(other);

		MetricStdDeviation::merge_results(aggs);
		_min_metric.merge_results(aggs);
		_max_metric.merge_results(aggs);
	}

	BaseAggregation* get_agg(std::string_view field) override {
		if (field == RESERVED_AGGS_STD_BOUNDS) {
			return this;  // FIXME: This is an ugly hack to allow getting fields inside _std_deviation_bounds
		}
		return nullptr;
	}

	const long double* get_value_ptr(std::string_view field) const override {
		if (field == RESERVED_AGGS_COUNT) {
			return &_count;
		}
		if (field == RESERVED_AGGS_MIN) {
			return &_min_metric._min;
		}
		if (field == RESERVED_AGGS_MAX) {
			return &_max_metric._max;
		}
		if (field == RESERVED_AGGS_AVG) {
			return &_avg;
		}
		if (field == RESERVED_AGGS_SUM) {
			return &_sum;
		}
		if (field == RESERVED_AGGS_SUM_OF_SQ) {
			return &_sq_sum;
		}
		if (field == RESERVED_AGGS_VARIANCE) {
			return &_variance;
		}
		if (field == RESERVED_AGGS_STD) {
			return &_std;
		}
		if (field == RESERVED_AGGS_UPPER) {
			return &_upper;
		}
		if (field == RESERVED_AGGS_LOWER) {
			return &_lower;
		}
		return nullptr;
	}

	void _aggregate(long double value) {
		MetricStdDeviation::_aggregate(value);
		_min_metric._aggregate(value);
		_max_metric._aggregate(value);
	}

	void aggregate_float(long double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const Xapian::Document&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const Xapian::Document&) override {
		_aggregate(value);
	}
};
