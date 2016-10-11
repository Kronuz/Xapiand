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

#include "aggregation.h"
#include "string_metric.h"


class BucketAggregation : public HandledSubAggregation {
protected:
	std::unordered_map<std::string, Aggregation> _aggs;
	const std::shared_ptr<Schema> _schema;
	const MsgPack& _conf;

public:
	BucketAggregation(const char* name, MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation(result, conf.at(name), schema),
		  _schema(schema),
		  _conf(conf) { }

	void update() override {
		for (auto& _agg : _aggs) {
			_agg.second.update();
		}
	}

	void aggregate(const std::string& bucket, const Xapian::Document& doc) {
		try {
			_aggs.at(bucket)(doc);
		} catch (const std::out_of_range&) {
			auto p = _aggs.emplace(std::piecewise_construct, std::forward_as_tuple(bucket), std::forward_as_tuple(_result[bucket], _conf, _schema));
			p.first->second(doc);
		}
	}
};


class ValueAggregation : public BucketAggregation {
public:
	ValueAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_VALUE, result, conf, schema) { }

	void aggregate_float(double value, const Xapian::Document& doc) override {
		auto bucket = std::to_string(value);
		aggregate(bucket, doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		auto bucket = std::to_string(value);
		aggregate(bucket, doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		auto bucket = std::to_string(value);
		aggregate(bucket, doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		auto bucket = std::to_string(value);
		aggregate(bucket, doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		auto bucket = std::string(value ? "true" : "false");
		aggregate(bucket, doc);
	}

	void aggregate_string(const std::string& value, const Xapian::Document& doc) override {
		auto& bucket = value;
		aggregate(bucket, doc);
	}

	void aggregate_geo(const std::pair<std::string, std::string>& value, const Xapian::Document& doc) override {
		auto bucket = format_string("(%f, %f)", Unserialise::_float(value.first), Unserialise::_float(value.second));
		aggregate(bucket, doc);
	}

	void aggregate_uuid(const std::string& value, const Xapian::Document& doc) override {
		auto& bucket = value;
		aggregate(bucket, doc);
	}
};


class HistogramAggregation : public BucketAggregation {
	MsgPack interval;
	uint64_t interval_u64;
	int64_t interval_i64;
	double interval_f64;

	std::string get_bucket(unsigned long value) {
		if (!interval_u64) {
			throw MSG_AggregationError("'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = value % interval_u64;
		auto bucket_key = value - rem;
		return std::to_string(bucket_key);
	}

	std::string get_bucket(long value) {
		if (!interval_i64) {
			throw MSG_AggregationError("'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = value % interval_i64;
		if (rem < 0) {
			rem += interval_i64;
		}
		auto bucket_key = value - rem;
		return std::to_string(bucket_key);
	}

	std::string get_bucket(double value) {
		if (!interval_f64) {
			throw MSG_AggregationError("'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = fmod(value, interval_f64);
		if (rem < 0) {
			rem += interval_f64;
		}
		auto bucket_key = value - rem;
		return std::to_string(bucket_key);
	}

public:
	HistogramAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_HISTOGRAM, result, conf, schema),
		  interval_u64(0),
		  interval_i64(0),
		  interval_f64(0.0)
	{
		auto histogram_conf = _conf.at(AGGREGATION_HISTOGRAM);
		try {
			interval = histogram_conf.at(AGGREGATION_INTERVAL);
			try {
				interval_u64 = interval.as_u64();
			} catch (const msgpack::type_error&) { }
			try {
				interval_i64 = interval.as_i64();
			} catch (const msgpack::type_error&) { }
			try {
				interval_f64 = interval.as_f64();
			} catch (const msgpack::type_error&) { }
		} catch (const std::out_of_range&) {
			throw MSG_AggregationError("'%s' must be specified must be specified in '%s'", AGGREGATION_INTERVAL, AGGREGATION_HISTOGRAM);
		} catch (const msgpack::type_error&) {
			throw MSG_AggregationError("'%s' must be object", AGGREGATION_HISTOGRAM);
		}
	}

	void aggregate_float(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}
};


class FilterAggregation : public SubAggregation {
	using func_filter = void (FilterAggregation::*)(const Xapian::Document&);

	std::vector<std::pair<Xapian::valueno, StringSet>> _filters;
	Aggregation _agg;
	func_filter func;

public:
	FilterAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	inline void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
