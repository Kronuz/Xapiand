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

// Count, sum, average, min and max metrics.

#include "metrics_base.h"


class MetricCount : public HandledSubAggregation<ValuesHandler> {
protected:
	long double _count;

public:
	MetricCount(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _count(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_COUNT, static_cast<uint64_t>(_count) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += engine::serialize_double(_count);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricCount::merge_results({})", repr(std::string(*p, p_end - *p)));

		_count += engine::unserialize_double(p, p_end);
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

	void aggregate_float(long double, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_integer(int64_t, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_positive(uint64_t, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_date(double, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_time(double, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_timedelta(double, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_boolean(bool, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_string(std::string_view, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_geo(const range_t&, const engine::DocView&) override {
		_aggregate();
	}

	void aggregate_uuid(std::string_view, const engine::DocView&) override {
		_aggregate();
	}
};


class MetricSum : public HandledSubAggregation<ValuesHandler> {
protected:
	long double _sum;

public:
	MetricSum(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _sum(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_SUM, static_cast<double>(_sum) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += engine::serialize_double(_sum);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetriSum::merge_results({})", repr(std::string_view(p, p_end - p)));

		_sum += engine::unserialize_double(p, p_end);
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

	void aggregate_float(long double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const engine::DocView&) override {
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
		  _count(0),
		  _avg(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_AVG, static_cast<double>(_avg) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += MetricSum::serialize_results();
		results += engine::serialize_double(_count);
		// results += engine::serialize_double(_avg);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricAvg::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricSum::merge_results(p, p_end);

		_count += engine::unserialize_double(p, p_end);
		// _avg = engine::unserialize_double(p, p_end);  // calculated during update()
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

	void aggregate_float(long double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const engine::DocView&) override {
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
		  _min(std::numeric_limits<double>::max()) { }

	MetricMin(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _min(std::numeric_limits<double>::max()) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MIN, static_cast<double>(_min) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += engine::serialize_double(_min);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMin::merge_results({})", repr(std::string(*p, p_end - *p)));

		auto value = engine::unserialize_double(p, p_end);
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

	void aggregate_float(long double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const engine::DocView&) override {
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
		  _max(std::numeric_limits<double>::min()) { }

	MetricMax(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _max(std::numeric_limits<double>::min()) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MAX, static_cast<double>(_max) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += engine::serialize_double(_max);
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMax::merge_results({})", repr(std::string(*p, p_end - *p)));

		auto value = engine::unserialize_double(p, p_end);
		if (_max < value) {
			_max = value;
		}
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("MetricMax::merge_results(<aggs>)");

		auto aggs = static_cast<const MetricMax*>(other);

		if (_max < aggs->_max) {
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

	void aggregate_float(long double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_integer(int64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_positive(uint64_t value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_date(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_time(double value, const engine::DocView&) override {
		_aggregate(value);
	}

	void aggregate_timedelta(double value, const engine::DocView&) override {
		_aggregate(value);
	}
};
