/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "../schema.h"
#include "exception.h"

#include <cfloat>
#include <cstdio>

#include <xapian.h>

#define AGGREGATION_AGGS            "_aggregations"
#define AGGREGATION_COUNT           "_count"
#define AGGREGATION_SUM             "_sum"
#define AGGREGATION_AVG             "_avg"
#define AGGREGATION_MIN             "_min"
#define AGGREGATION_MAX             "_max"
#define AGGREGATION_VARIANCE        "_variance"
#define AGGREGATION_STD             "_std"
#define AGGREGATION_MEDIAN          "_median"
#define AGGREGATION_MODE            "_mode"
#define AGGREGATION_STATS           "_stats"
#define AGGREGATION_EXT_STATS       "_extended_stats"
#define AGGREGATION_GEO_BOUNDS      "_geo_bounds"
#define AGGREGATION_GEO_CENTROID    "_geo_centroid"
#define AGGREGATION_PERCENTILE      "_percentile"
#define AGGREGATION_SUM_OF_SQ       "_sum_of_squares"
#define AGGREGATION_FIELD           "_field"


#define AGGREGATION_TERM            "_term"
#define AGGREGATION_FILTER          "_filter"


class MetricAggregation;


using func_value_handle = void (MetricAggregation::*)(const std::string&);


class ValueHandle {
protected:
	Xapian::valueno _slot;
	func_value_handle _func;

public:
	ValueHandle() = default;

	inline void set(Xapian::valueno slot, func_value_handle func) {
		_slot = slot;
		_func = func;
	}

	void operator()(MetricAggregation* agg, const Xapian::Document& doc) const;
};


class SubAggregation {
public:
	virtual void operator()(const Xapian::Document&) = 0;
	virtual void update() = 0;
};


class MetricAggregation : public SubAggregation {
protected:
	MsgPack& _result;

public:
	MetricAggregation(MsgPack& result)
		: _result(result) { }

	virtual void add(double)  { }

	virtual void _float(const std::string& s) {
		StringList l;
		l.unserialise(s);
		for (const auto& value : l) {
			add(Unserialise::_float(value));
		}
	}

	virtual void integer(const std::string& s) {
		StringList l;
		l.unserialise(s);
		for (const auto& value : l) {
			add(Unserialise::integer(value));
		}
	}

	virtual void positive(const std::string& s) {
		StringList l;
		l.unserialise(s);
		for (const auto& value : l) {
			add(Unserialise::positive(value));
		}
	}

	virtual void date(const std::string& s) {
		StringList l;
		l.unserialise(s);
		for (const auto& value : l) {
			add(Unserialise::timestamp(value));
		}
	}

	virtual void boolean(const std::string& s)  {
		StringList l;
		l.unserialise(s);
		for (const auto& value : l) {
			add(Unserialise::boolean(value));
		}
	}

	virtual void string(const std::string&) {
		throw MSG_AggregationError("string type is not supported");
	}

	virtual void geo(const std::string&) {
		throw MSG_AggregationError("geo type is not supported");
	}

	virtual void uuid(const std::string&) {
		throw MSG_AggregationError("uuis type is not supported");
	}
};


class MetricCount : public MetricAggregation {
	size_t _doc_count;

public:
	MetricCount(MsgPack& result, const MsgPack& data)
		: MetricAggregation(result),
		  _doc_count(0) { }

	void operator()(const Xapian::Document&) override {
		++_doc_count;
	}

	void update() override {
		_result = _doc_count;
	}
};


class MetricSum : public MetricAggregation {
protected:
	ValueHandle _handle;
	long double _sum;

public:
	MetricSum(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		_handle(this, doc);
	}

	void update() override {
		_result = static_cast<double>(_sum);
	}

	void add(double value) override {
		_sum += value;
	}
};


class MetricAvg : public MetricSum {
protected:
	size_t _count = 0;

public:
	MetricAvg(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema)
		: MetricSum(result, data, schema),
		  _count(0) { }

	void update() override {
		_result = static_cast<double>(avg());
	}

	void add(double value) override {
		++_count;
		_sum += value;
	}

	inline long double avg() const {
		return _sum ? _sum / _count : _sum;
	}
};


class MetricStats;
class MetricExtendedStats;


class MetricMin : public MetricAggregation {
	ValueHandle _handle;
	double _min;

	MetricMin(MsgPack& result)
		: MetricAggregation(result),
		  _min(DBL_MAX) { }

	friend class MetricStats;
	friend class MetricExtendedStats;

public:
	MetricMin(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		_handle(this, doc);
	}

	void update() override {
		_result = _min;
	}

	void add(double value) override {
		if (value < _min) {
			_min = value;
		}
	}
};


class MetricMax : public MetricAggregation {
	ValueHandle _handle;
	double _max;

	MetricMax(MsgPack& result)
		: MetricAggregation(result),
		  _max(DBL_MIN) { }

	friend class MetricStats;
	friend class MetricExtendedStats;

public:
	MetricMax(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		_handle(this, doc);
	}

	void update() override {
		_result = _max;
	}

	void add(double value) override {
		if (value > _max) {
			_max = value;
		}
	}
};


class MetricVariance : public MetricAvg {
protected:
	long double _sq_sum;

public:
	MetricVariance(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema)
		: MetricAvg(result, data, schema),
		  _sq_sum(0) { }

	void update() override {
		_result = static_cast<double>(variance());
	}

	void add(double value) override {
		++_count;
		_sum += value;
		_sq_sum += value * value;
	}

	inline long double variance() const {
		auto _avg = avg();
		return (_sq_sum - _count * _avg * _avg) / (_count - 1);
	}
};


// Standard deviation.
class MetricSTD : public MetricVariance {
public:
	MetricSTD(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema)
		: MetricVariance(result, data, schema) { }

	void update() override {
		_result = static_cast<double>(std());
	}

	inline long double std() const {
		return std::sqrt(variance());
	}
};


class MetricMedian : public MetricAggregation {
	ValueHandle _handle;
	std::vector<double> values;

public:
	MetricMedian(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		_handle(this, doc);
	}

	void update() override {
		if (values.empty()) {
			_result = 0;
			return;
		}
		size_t median_pos = values.size();
		if (median_pos % 2 == 0) {
			median_pos /= 2;
			std::nth_element(values.begin(), values.begin() + median_pos, values.end());
			auto val1 = values[median_pos];
			std::nth_element(values.begin(), values.begin() + median_pos - 1, values.end());
			_result = (val1 + values[median_pos - 1]) / 2;
		} else {
			median_pos /= 2;
			std::nth_element(values.begin(), values.begin() + median_pos, values.end());
			_result = values[median_pos];
		}
	}

	void add(double value) override {
		values.push_back(value);
	}
};


class MetricMode : public MetricAggregation {
	ValueHandle _handle;
	std::unordered_map<double, size_t> _histogram;

public:
	MetricMode(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		_handle(this, doc);
	}

	void update() override {
		if (_histogram.empty()) {
			_result = 0;
			return;
		}
		auto it = std::max_element(_histogram.begin(), _histogram.end(), [](const std::pair<double, size_t>& a, const std::pair<double, size_t>& b) { return a.second < b.second; });
		_result = it->first;
	}

	void add(double value) override {
		try {
			++_histogram.at(value);
		} catch (const std::out_of_range&) {
			_histogram[value] = 1;
		}
	}
};


class MetricStats : public MetricAvg {
	MetricMin _min_metric;
	MetricMax _max_metric;
	size_t _doc_count;

public:
	MetricStats(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema)
		: MetricAvg(result, data, schema),
		  _min_metric(_result),
		  _max_metric(_result),
		  _doc_count(0) { }

	void operator()(const Xapian::Document& doc) override {
		++_doc_count;
		_handle(this, doc);
	}

	void update() override {
		_result[AGGREGATION_COUNT] = _doc_count;
		_result[AGGREGATION_MIN]   = _min_metric._min;
		_result[AGGREGATION_MAX]   = _max_metric._max;
		_result[AGGREGATION_AVG]   = static_cast<double>(avg());
		_result[AGGREGATION_SUM]   = static_cast<double>(_sum);
	}

	void add(double value) override {
		_min_metric.add(value);
		_max_metric.add(value);
		MetricAvg::add(value);
	}
};


class MetricExtendedStats : public MetricSTD {
	MetricMin _min_metric;
	MetricMax _max_metric;
	size_t _doc_count;

public:
	MetricExtendedStats(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema)
		: MetricSTD(result, data, schema),
		  _min_metric(_result),
		  _max_metric(_result),
		  _doc_count(0) { }

	void operator()(const Xapian::Document& doc) override {
		++_doc_count;
		_handle(this, doc);
	}

	void update() override {
		_result[AGGREGATION_COUNT]      = _doc_count;
		_result[AGGREGATION_MIN]        = _min_metric._min;
		_result[AGGREGATION_MAX]        = _max_metric._max;
		_result[AGGREGATION_AVG]        = static_cast<double>(avg());
		_result[AGGREGATION_SUM]        = static_cast<double>(_sum);
		_result[AGGREGATION_SUM_OF_SQ]  = static_cast<double>(_sq_sum);
		_result[AGGREGATION_VARIANCE]   = static_cast<double>(variance());
		_result[AGGREGATION_STD]        = static_cast<double>(std());
	}

	void add(double value) override {
		_min_metric.add(value);
		_max_metric.add(value);
		MetricSTD::add(value);
	}
};
