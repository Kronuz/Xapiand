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

// Values, terms and filter bucket aggregations.

#include "bucket_base.h"


class ValuesAggregation : public BucketAggregation<ValuesHandler> {
public:
	ValuesAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(context, name, schema, Sort::by_count_desc, true) {
		if (this->_handler.get_type() == FieldType::geo) {
			THROW(AggregationError, "values over geo fields are not supported");
		}
	}

	void aggregate_float(long double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_integer(int64_t value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_positive(uint64_t value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_date(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_time(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_timedelta(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_boolean(bool value, const engine::DocView& doc) override {
		aggregate(value ? 1.0 : 0.0, std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const engine::DocView& doc) override {
		aggregate(0.0, value, doc);
	}

	void aggregate_geo(const range_t& value, const engine::DocView& doc) override {
		aggregate(0.0, value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const engine::DocView& doc) override {
		aggregate(0.0, value, doc);
	}
};


class TermsAggregation : public BucketAggregation<TermsHandler> {
public:
	TermsAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<TermsHandler>(context, name, schema, Sort::by_count_desc, true) {
		if (this->_handler.get_type() == FieldType::geo) {
			THROW(AggregationError, "terms over geo fields are not supported");
		}
	}

	void aggregate_float(long double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_integer(int64_t value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_positive(uint64_t value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_date(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_time(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_timedelta(double value, const engine::DocView& doc) override {
		aggregate(value, strings::format("{}", value), doc);
	}

	void aggregate_boolean(bool value, const engine::DocView& doc) override {
		aggregate(value ? 1.0 : 0.0, std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const engine::DocView& doc) override {
		aggregate(0.0, value, doc);
	}

	void aggregate_geo(const range_t& value, const engine::DocView& doc) override {
		aggregate(0.0, value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const engine::DocView& doc) override {
		aggregate(0.0, value, doc);
	}
};


class FilterAggregation : public BaseAggregation {
	using func_filter = void (FilterAggregation::*)(const engine::DocView&);

	std::vector<std::pair<engine::valueno, std::set<std::string>>> _filters;
	Aggregation _agg;
	func_filter func;

public:
	FilterAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema);

	void operator()(const engine::DocView& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	size_t bucket_count() const override {
		return _agg.bucket_count();
	}

	MsgPack get_result() const override;

	std::string serialize_results() const override;

	void merge_results(const char** p, const char* p_end) override;
	void merge_results(const BaseAggregation* other) override;

	void check_single(const engine::DocView& doc);
	void check_multiple(const engine::DocView& doc);
};
