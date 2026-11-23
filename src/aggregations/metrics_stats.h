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

// Stats and extended stats metrics.

#include "metrics_base.h"
#include "metrics_simple.h"
#include "metrics_variance.h"


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

	std::string serialize_results() const override {
		std::string results;
		results += MetricAvg::serialize_results();
		results += _min_metric.serialize_results();
		results += _max_metric.serialize_results();
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
		_min_metric.merge_results(&aggs->_min_metric);
		_max_metric.merge_results(&aggs->_max_metric);
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

	std::string serialize_results() const override {
		std::string results;
		results += MetricStdDeviation::serialize_results();
		results += _min_metric.serialize_results();
		results += _max_metric.serialize_results();
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
		_min_metric.merge_results(&aggs->_min_metric);
		_max_metric.merge_results(&aggs->_max_metric);
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
