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

// Median and mode metrics.

#include "metrics_base.h"


class MetricMedian : public HandledSubAggregation<ValuesHandler> {
	std::vector<long double> _values;

	long double _median;

public:
	MetricMedian(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _median(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MEDIAN, static_cast<double>(_median) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += serialize_length(_values.size());
		for (auto& value : _values) {
			results += engine::serialize_double(value);
		}
		// results += engine::serialize_double(_median);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMedian::merge_results({})", repr(std::string(*p, p_end - *p)));

		size_t size = unserialize_length(p, p_end);
		while (size--) {
			auto value = engine::unserialize_double(p, p_end);
			_values.push_back(value);
		}
		// _median = engine::unserialize_double(p, p_end);  // calculated during update()
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


class MetricMode : public HandledSubAggregation<ValuesHandler> {
	std::map<long double, size_t> _histogram;

	long double _mode;

public:
	MetricMode(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<ValuesHandler>(context, name, schema),
		  _mode(0) { }

	MsgPack get_result() const override {
		return {
			{ RESERVED_AGGS_MODE, static_cast<double>(_mode) },
		};
	}

	std::string serialize_results() const override {
		std::string results;
		results += serialize_length(_histogram.size());
		for (auto& item : _histogram) {
			results += engine::serialize_double(item.first);
			results += serialize_length(item.second);
		}
		// results += engine::serialize_double(_mode);  // calculated during update()
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("MetricMode::merge_results({})", repr(std::string(*p, p_end - *p)));

		size_t size = unserialize_length(p, p_end);
		while (size--) {
			auto key = engine::unserialize_double(p, p_end);
			auto value = unserialize_length(p, p_end);
			_histogram[key] += value;
		}
		// _mode = engine::unserialize_double(p, p_end);  // calculated during update()
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
