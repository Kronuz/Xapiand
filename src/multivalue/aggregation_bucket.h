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
#include <math.h>                           // for fmodl
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
#include "schema.h"                         // for FieldType
#include "serialise.h"                      // for _float
#include "string.hh"                        // for string::format, string::Number
#include "hashes.hh"                        // for xxh64

class Schema;


template <typename Handler>
class BucketAggregation : public HandledSubAggregation<Handler> {
protected:
	std::unordered_map<std::string, Aggregation> _aggs;
	const std::shared_ptr<Schema> _schema;
	const MsgPack& _context;

public:
	BucketAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation<Handler>(result, context, name, schema),
		  _schema(schema),
		  _context(context) { }

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
				std::forward_as_tuple(this->_result.put(bucket, MsgPack(MsgPack::Type::MAP)), _context, _schema));
			p.first->second(doc);
		}
	}
};


class ValuesAggregation : public BucketAggregation<ValuesHandler> {
public:
	ValuesAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(result, context, name, schema) { }

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
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


class TermsAggregation : public BucketAggregation<TermsHandler> {
public:
	TermsAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<TermsHandler>(result, context, name, schema) { }

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		aggregate(string::Number(value), doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
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


class HistogramAggregation : public BucketAggregation<ValuesHandler> {
	uint64_t interval_u64;
	int64_t interval_i64;
	long double interval_f64;

	std::string get_bucket(uint64_t value) {
		if (!interval_u64) {
			THROW(AggregationError, "'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = value % interval_u64;
		auto bucket_key = value - rem;
		return string::Number(bucket_key).str();
	}

	std::string get_bucket(int64_t value) {
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

	std::string get_bucket(long double value) {
		if (!interval_f64) {
			THROW(AggregationError, "'%s' must be a non-zero number", AGGREGATION_INTERVAL);
		}
		auto rem = fmodl(value, interval_f64);
		if (rem < 0) {
			rem += interval_f64;
		}
		auto bucket_key = value - rem;
		return string::Number(bucket_key).str();
	}

public:
	HistogramAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(result, context, name, schema),
		  interval_u64(0),
		  interval_i64(0),
		  interval_f64(0.0)
	{
		const auto it = _conf.find(AGGREGATION_INTERVAL);
		if (it == _conf.end()) {
			THROW(AggregationError, "'%s' must be object with '%s'", name, AGGREGATION_INTERVAL);
		}
		const auto& interval_value = it.value();
		switch (_handler.get_type()) {
			case FieldType::POSITIVE:
				switch (interval_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						interval_u64 = interval_value.as_u64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_INTERVAL);
				}
				break;
			case FieldType::INTEGER:
				switch (interval_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						interval_i64 = interval_value.as_i64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_INTERVAL);
				}
				break;
			case FieldType::FLOAT:
			case FieldType::DATE:
			case FieldType::TIME:
			case FieldType::TIMEDELTA:
				switch (interval_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						interval_f64 = interval_value.as_f64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_INTERVAL);
				}
				break;
			default:
				THROW(AggregationError, "Histogram aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(bucket, doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(bucket, doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(bucket, doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(bucket, doc);
	}
};


class RangeAggregation : public BucketAggregation<ValuesHandler> {
	std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> ranges_u64;
	std::vector<std::pair<std::string, std::pair<int64_t, int64_t>>> ranges_i64;
	std::vector<std::pair<std::string, std::pair<long double, long double>>> ranges_f64;

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

	void configure_u64(const MsgPack& ranges) {
		for (const auto& range : ranges) {
			std::string default_key;
			std::string_view key;
			auto key_it = range.find(AGGREGATION_KEY);
			if (key_it != range.end()) {
				const auto& key_value = key_it.value();
				if (!key_value.is_string()) {
					THROW(AggregationError, "'%s' must be a string", AGGREGATION_KEY);
				}
				key = key_value.str_view();
			}

			long double from_u64 = std::numeric_limits<long double>::min();
			auto from_it = range.find(AGGREGATION_FROM);
			if (from_it != range.end()) {
				const auto& from_value = from_it.value();
				switch (from_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						from_u64 = from_value.as_u64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_FROM);
				}
			}

			long double to_u64 = std::numeric_limits<long double>::max();
			auto to_it = range.find(AGGREGATION_TO);
			if (to_it != range.end()) {
				const auto& to_value = to_it.value();
				switch (to_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						to_u64 = to_value.as_u64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_TO);
				}
			}

			if (key.empty()) {
				default_key = _as_bucket(from_u64, to_u64);
				key = default_key;
			}
			ranges_u64.emplace_back(key, std::make_pair(from_u64, to_u64));
		}
	}

	void configure_i64(const MsgPack& ranges) {
		for (const auto& range : ranges) {
			std::string default_key;
			std::string_view key;
			auto key_it = range.find(AGGREGATION_KEY);
			if (key_it != range.end()) {
				const auto& key_value = key_it.value();
				if (!key_value.is_string()) {
					THROW(AggregationError, "'%s' must be a string", AGGREGATION_KEY);
				}
				key = key_value.str_view();
			}

			long double from_i64 = std::numeric_limits<long double>::min();
			auto from_it = range.find(AGGREGATION_FROM);
			if (from_it != range.end()) {
				const auto& from_value = from_it.value();
				switch (from_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						from_i64 = from_value.as_i64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_FROM);
				}
			}

			long double to_i64 = std::numeric_limits<long double>::max();
			auto to_it = range.find(AGGREGATION_TO);
			if (to_it != range.end()) {
				const auto& to_value = to_it.value();
				switch (to_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						to_i64 = to_value.as_i64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_TO);
				}
			}

			if (key.empty()) {
				default_key = _as_bucket(from_i64, to_i64);
				key = default_key;
			}
			ranges_i64.emplace_back(key, std::make_pair(from_i64, to_i64));
		}
	}

	void configure_f64(const MsgPack& ranges) {
		for (const auto& range : ranges) {
			std::string default_key;
			std::string_view key;
			auto key_it = range.find(AGGREGATION_KEY);
			if (key_it != range.end()) {
				const auto& key_value = key_it.value();
				if (!key_value.is_string()) {
					THROW(AggregationError, "'%s' must be a string", AGGREGATION_KEY);
				}
				key = key_value.str_view();
			}

			long double from_f64 = std::numeric_limits<long double>::min();
			auto from_it = range.find(AGGREGATION_FROM);
			if (from_it != range.end()) {
				const auto& from_value = from_it.value();
				switch (from_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						from_f64 = from_value.as_f64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_FROM);
				}
			}

			long double to_f64 = std::numeric_limits<long double>::max();
			auto to_it = range.find(AGGREGATION_TO);
			if (to_it != range.end()) {
				const auto& to_value = to_it.value();
				switch (to_value.getType()) {
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
						to_f64 = to_value.as_f64();
						break;
					default:
						THROW(AggregationError, "'%s' must be numeric", AGGREGATION_TO);
				}
			}

			if (key.empty()) {
				default_key = _as_bucket(from_f64, to_f64);
				key = default_key;
			}
			ranges_f64.emplace_back(key, std::make_pair(from_f64, to_f64));
		}
	}

public:
	RangeAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(result, context, name, schema)
	{
		const auto it = _conf.find(AGGREGATION_RANGES);
		if (it == _conf.end()) {
			THROW(AggregationError, "'%s' must be object with '%s'", name, AGGREGATION_RANGES);
		}
		const auto& ranges = it.value();
		if (!ranges.is_array()) {
			THROW(AggregationError, "'%s.%s' must be an array", name, AGGREGATION_RANGES);
		}

		switch (_handler.get_type()) {
			case FieldType::POSITIVE:
				configure_u64(ranges);
				break;
			case FieldType::INTEGER:
				configure_i64(ranges);
				break;
			case FieldType::FLOAT:
			case FieldType::DATE:
			case FieldType::TIME:
			case FieldType::TIMEDELTA:
				configure_f64(ranges);
				break;
			default:
				THROW(AggregationError, "Range aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		for (const auto& range : ranges_i64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.first, doc);
			}
		}
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
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
	FilterAggregation(MsgPack& result, const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
