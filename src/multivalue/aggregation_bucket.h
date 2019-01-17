/*
 * Copyright (C) 2015-2019 Dubalu LLC. All rights reserved.
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

#include <cstdint>                          // for int64_t, uint64_t
#include <limits>                           // for numeric_limits
#include <math.h>                           // for fmod
#include <memory>                           // for shared_ptr, allocator
#include <stdexcept>                        // for out_of_range
#include <string>                           // for string, to_string, hash
#include <sys/types.h>                      // for int64_t, uint64_t
#include <tuple>                            // for forward_as_tuple
#include <unordered_map>                    // for unordered_map, __hash_map...
#include <utility>                          // for pair, make_pair, piecewis...
#include <vector>                           // for vector
#include <xapian.h>                         // for Document, valueno

#include "aggregation.h"                    // for Aggregation
#include "msgpack.h"                        // for MsgPack, object::object, ...
#include "aggregation_metric.h"             // for AGGREGATION_INTERVAL, AGG...
#include "exception.h"                      // for AggregationError, MSG_Agg...
#include "serialise.h"                      // for _float
#include "string.hh"                        // for string::format, string::Number
#include "hashes.hh"                        // for xxh64


class Schema;


class BucketAggregation : public HandledSubAggregation {
protected:
	std::unordered_map<std::string, Aggregation> _aggs;
	const std::shared_ptr<Schema> _schema;
	const MsgPack& _conf;

public:
	BucketAggregation(const char* name, MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema, bool use_terms)
		: HandledSubAggregation(result, conf.at(name), schema, use_terms),
		  _schema(schema),
		  _conf(conf) { }

	void update() override {
		for (auto& _agg : _aggs) {
			_agg.second.update();
		}
	}

	void aggregate(std::string_view bucket, const Xapian::Document& doc) {
		auto it = _aggs.find(std::string(bucket));
		if (it != _aggs.end()) {
			it->second(doc);
		} else {
			auto p = _aggs.emplace(std::piecewise_construct,
				std::forward_as_tuple(bucket),
				std::forward_as_tuple(_result[bucket], _conf, _schema));
			p.first->second(doc);
		}
	}
};


class ValuesAggregation : public BucketAggregation {
public:
	ValuesAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_VALUES, result, conf, schema, false) { }

	void aggregate_float(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		aggregate(std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const Xapian::Document& doc) override {
		aggregate(value, doc);
	}

	void aggregate_geo(const range_t& value, const Xapian::Document& doc) override {
		aggregate(value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const Xapian::Document& doc) override {
		aggregate(value, doc);
	}
};


class TermsAggregation : public BucketAggregation {
public:
	TermsAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_TERMS, result, conf, schema, true) { }

	void aggregate_float(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		aggregate(std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const Xapian::Document& doc) override {
		aggregate(value, doc);
	}

	void aggregate_geo(const range_t& value, const Xapian::Document& doc) override {
		aggregate(value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const Xapian::Document& doc) override {
		aggregate(value, doc);
	}
};


class HistogramAggregation : public BucketAggregation {
	uint64_t interval_u64;
	int64_t interval_i64;
	double interval_f64;

	std::string get_bucket(unsigned long value) {
		if (!interval_u64) {
			THROW(AggregationError, "'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = value % interval_u64;
		auto bucket_key = value - rem;
		return string::Number(bucket_key).str();
	}

	std::string get_bucket(long value) {
		if (!interval_i64) {
			THROW(AggregationError, "'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = value % interval_i64;
		if (rem < 0) {
			rem += interval_i64;
		}
		auto bucket_key = value - rem;
		return string::Number(bucket_key).str();
	}

	std::string get_bucket(double value) {
		if (!interval_f64) {
			THROW(AggregationError, "'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = fmod(value, interval_f64);
		if (rem < 0) {
			rem += interval_f64;
		}
		auto bucket_key = value - rem;
		return string::Number(bucket_key).str();
	}

public:
	HistogramAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_HISTOGRAM, result, conf, schema, false),
		  interval_u64(0),
		  interval_i64(0),
		  interval_f64(0.0)
	{
		const auto& histogram_conf = _conf.at(AGGREGATION_HISTOGRAM);
		if (!histogram_conf.is_map()) {
			THROW(AggregationError, "'%s' must be object", AGGREGATION_HISTOGRAM);
		}
		const auto it = histogram_conf.find(AGGREGATION_INTERVAL);
		if (it == histogram_conf.end()) {
			THROW(AggregationError, "'%s' must be object with '%s'", AGGREGATION_HISTOGRAM, AGGREGATION_INTERVAL);
		}
		const auto& interval_value = it.value();
		switch (interval_value.getType()) {
			case MsgPack::Type::POSITIVE_INTEGER:
				interval_u64 = interval_value.u64();
			case MsgPack::Type::NEGATIVE_INTEGER:
				interval_i64 = interval_value.i64();
			case MsgPack::Type::FLOAT:
				interval_f64 = interval_value.f64();
				break;
			default:
				THROW(AggregationError, "'%s' must be object", AGGREGATION_HISTOGRAM);
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

	void aggregate_time(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}
};


class RangeAggregation : public BucketAggregation {
	std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> ranges_u64;
	std::vector<std::pair<std::string, std::pair<int64_t, int64_t>>> ranges_i64;
	std::vector<std::pair<std::string, std::pair<double, double>>> ranges_f64;

	template <typename T>
	std::string _as_bucket(T start, T end) const {
		if (end == std::numeric_limits<T>::max()) {
			if (start == std::numeric_limits<T>::min()) {
				return "..";
			}
			return string::format("%s..", string::Number(start));
		}
		if (start == std::numeric_limits<T>::min()) {
			if (end == std::numeric_limits<T>::max()) {
				return "..";
			}
			return string::format("..%s", string::Number(end));
		}
		return string::format("%s..%s", string::Number(start), string::Number(end));
	}

public:
	RangeAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_RANGE, result, conf, schema, false)
	{
		const auto& range_conf = _conf.at(AGGREGATION_RANGE);
		if (!range_conf.is_map()) {
			THROW(AggregationError, "'%s' must be object", AGGREGATION_RANGE);
		}
		const auto it = range_conf.find(AGGREGATION_RANGES);
		if (it == range_conf.end()) {
			THROW(AggregationError, "'%s' must be object with '%s'", AGGREGATION_RANGE, AGGREGATION_RANGES);
		}
		const auto& ranges = it.value();
		if (!ranges.is_array()) {
			THROW(AggregationError, "'%s.%s' must be an array", AGGREGATION_RANGE, AGGREGATION_RANGES);
		}
		for (const auto& range : ranges) {
			std::string_view key;
			auto key_it = range.find(AGGREGATION_KEY);
			if (key_it != range.end()) {
				const auto& key_value = key_it.value();
				if (!key_value.is_string()) {
					THROW(AggregationError, "'%s' must be a string", AGGREGATION_KEY);
				}
				key = key_value.str_view();
			}

			bool is_u64 = false;
			bool is_i64 = false;
			bool is_f64 = false;

			uint64_t from_u64 = std::numeric_limits<uint64_t>::min();
			int64_t from_i64 = std::numeric_limits<int64_t>::min();
			double from_f64 = std::numeric_limits<double>::min();
			auto from_it = range.find(AGGREGATION_FROM);
			if (from_it != range.end()) {
				const auto& from_value = from_it.value();
				switch (from_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
						is_u64 = true;
						from_u64 = from_value.u64();
						break;
					case MsgPack::Type::NEGATIVE_INTEGER:
						is_i64 = true;
						from_i64 = from_value.i64();
						break;
					case MsgPack::Type::FLOAT:
						is_f64 = true;
						from_f64 = from_value.f64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_FROM);
				}
			}

			uint64_t to_u64 = std::numeric_limits<uint64_t>::max();
			int64_t to_i64 = std::numeric_limits<int64_t>::max();
			double to_f64 = std::numeric_limits<double>::max();
			auto to_it = range.find(AGGREGATION_TO);
			if (to_it != range.end()) {
				const auto& to_value = to_it.value();
				switch (to_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
						to_u64 = to_value.u64();
						if (is_i64) {
							to_i64 = static_cast<int64_t>(to_u64);
						} else if (is_f64) {
							to_f64 = static_cast<double>(to_u64);
						}
						break;
					case MsgPack::Type::NEGATIVE_INTEGER:
						to_i64 = to_value.i64();
						if (is_u64) {
							is_u64 = false;
							is_i64 = true;
							from_i64 = static_cast<int64_t>(from_u64);
						} else if (is_f64) {
							to_f64 = static_cast<double>(to_i64);
						}
						break;
					case MsgPack::Type::FLOAT:
						to_f64 = to_value.f64();
						if (is_u64) {
							is_u64 = false;
							is_f64 = true;
							from_f64 = static_cast<double>(from_u64);
						} else if (is_i64) {
							is_i64 = false;
							is_f64 = true;
							from_f64 = static_cast<double>(from_i64);
						}
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_TO);
				}
			}

			if (is_f64) {
				if (key.empty()) {
					key = _as_bucket(from_f64, to_f64);
				}
				ranges_f64.emplace_back(key, std::make_pair(from_f64, to_f64));
			} else if (is_i64) {
				if (key.empty()) {
					key = _as_bucket(from_i64, to_i64);
				}
				ranges_i64.emplace_back(key, std::make_pair(from_i64, to_i64));
			} else if (is_u64) {
				if (key.empty()) {
					key = _as_bucket(from_u64, to_u64);
				}
				ranges_u64.emplace_back(key, std::make_pair(from_u64, to_u64));
			}
		}
	}

	void aggregate_float(double value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_i64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_u64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}
};


class FilterAggregation : public SubAggregation {
	using func_filter = void (FilterAggregation::*)(const Xapian::Document&);

	std::vector<std::pair<Xapian::valueno, std::set<std::string>>> _filters;
	Aggregation _agg;
	func_filter func;

public:
	FilterAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
