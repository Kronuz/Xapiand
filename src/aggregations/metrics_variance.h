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

// Variance and standard deviation metrics.

#include "metrics_base.h"
#include "metrics_simple.h"


class MetricVariance : public MetricAvg {
protected:
	long double _sq_sum;
	long double _variance;

public:
	MetricVariance(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: MetricAvg(context, name, schema),
		  _sq_sum(0),
		  _variance(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_VARIANCE, static_cast<double>(_variance) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += MetricAvg::serialize_results();
		results += engine::serialize_double(_sq_sum);
		// results += engine::serialize_double(_variance);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricVariance::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricAvg::merge_results(p, p_end);

		_sq_sum += engine::unserialize_double(p, p_end);
		// _variance = engine::unserialize_double(p, p_end);  // calculated during update()
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
		  _sigma(2),
		  _upper(0),
		  _lower(0) {
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


	std::string serialize_results() const override {
		std::string results;
		results += MetricVariance::serialize_results();
		results += engine::serialize_double(_sigma);
		// results += engine::serialize_double(_std);  // calculated during update()
		// results += engine::serialize_double(_upper);  // calculated during update()
		// results += engine::serialize_double(_lower);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricStdDeviation::merge_results({})", repr(std::string(*p, p_end - *p)));

		MetricVariance::merge_results(p, p_end);
		_sigma = engine::unserialize_double(p, p_end);
		// _std = engine::unserialize_double(p, p_end);  // calculated during update()
		// _upper = engine::unserialize_double(p, p_end);  // calculated during update()
		// _lower = engine::unserialize_double(p, p_end);  // calculated during update()
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
