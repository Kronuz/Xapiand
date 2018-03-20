/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "xapiand.h"

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
#include "fmt/format.h"                     // for fmt::FormatInt
#include "msgpack.h"                        // for MsgPack, object::object, ...
#include "multivalue/aggregation_metric.h"  // for AGGREGATION_INTERVAL, AGG...
#include "multivalue/exception.h"           // for AggregationError, MSG_Agg...
#include "serialise.h"                      // for _float
#include "string.hh"                        // for string::format
#include "hashes.hh"                        // for xxh64
#include "milo.h"                           // for fmt::FormatDouble


class Schema;


class BucketAggregation : public HandledSubAggregation {
protected:
	std::unordered_map<uint64_t, Aggregation> _aggs;
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

	void aggregate(std::string_view bucket, const Xapian::Document& doc) {
		auto bucket_hash = xxh64::hash(bucket);
		auto it = _aggs.find(bucket_hash);
		if (it != _aggs.end()) {
			it->second(doc);
		} else {
			auto p = _aggs.emplace(std::piecewise_construct,
				std::forward_as_tuple(bucket_hash),
				std::forward_as_tuple(_result[bucket], _conf, _schema));
			p.first->second(doc);
		}
	}
};


class ValueAggregation : public BucketAggregation {
public:
	ValueAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_VALUE, result, conf, schema) { }

	void aggregate_float(double value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatDouble(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatInt(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatInt(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatDouble(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatDouble(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		auto format_value = fmt::FormatDouble(value);
		aggregate(std::string_view(format_value.data(), format_value.size()), doc);
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
		return fmt::FormatInt(bucket_key).str();
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
		return fmt::FormatInt(bucket_key).str();
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
		return fmt::FormatDouble(bucket_key).str();
	}

public:
	HistogramAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_HISTOGRAM, result, conf, schema),
		  interval_u64(0),
		  interval_i64(0),
		  interval_f64(0.0)
	{
		const auto& histogram_conf = _conf.at(AGGREGATION_HISTOGRAM);
		try {
			const auto& interval = histogram_conf.at(AGGREGATION_INTERVAL);
			try {
				interval_u64 = interval.u64();
			} catch (const msgpack::type_error&) { }
			try {
				interval_i64 = interval.i64();
			} catch (const msgpack::type_error&) { }
			try {
				interval_f64 = interval.f64();
			} catch (const msgpack::type_error&) { }
		} catch (const std::out_of_range&) {
			THROW(AggregationError, "'%s' must be specified must be specified in '%s'", AGGREGATION_INTERVAL, AGGREGATION_HISTOGRAM);
		} catch (const msgpack::type_error&) {
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
			return string::format("%s..", std::to_string(start));
		}
		if (start == std::numeric_limits<T>::min()) {
			if (end == std::numeric_limits<T>::max()) {
				return "..";
			}
			return string::format("..%s", std::to_string(end));
		}
		return string::format("%s..%s", std::to_string(start), std::to_string(end));
	}

public:
	RangeAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: BucketAggregation(AGGREGATION_RANGE, result, conf, schema)
	{
		const auto& range_conf = _conf.at(AGGREGATION_RANGE);
		try {
			const auto& ranges = range_conf.at(AGGREGATION_RANGES);
			for (const auto& range : ranges) {
				std::string key;
				uint64_t from_u64, to_u64;
				int64_t from_i64, to_i64;
				double from_f64, to_f64;

				bool err_u64 = false;
				bool err_i64 = false;
				bool err_f64 = false;

				try {
					key = range.at(AGGREGATION_KEY).str();
				} catch (const std::out_of_range&) { }

				try {
					const auto& from = range.at(AGGREGATION_FROM);
					try {
						from_u64 = from.u64();
					} catch (const msgpack::type_error&) {
						err_u64 = true;
					}
					try {
						from_i64 = from.i64();
					} catch (const msgpack::type_error&) {
						err_i64 = true;
					}
					try {
						from_f64 = from.f64();
					} catch (const msgpack::type_error&) {
						err_f64 = true;
					}
				} catch (const std::out_of_range&) {
					from_u64 = std::numeric_limits<uint64_t>::min();
					from_i64 = std::numeric_limits<int64_t>::min();
					from_f64 = std::numeric_limits<double>::min();
				}

				try {
					const auto& to = range.at(AGGREGATION_TO);
					try {
						to_u64 = to.u64();
					} catch (const msgpack::type_error&) {
						err_u64 = true;
					}
					try {
						to_i64 = to.i64();
					} catch (const msgpack::type_error&) {
						err_i64 = true;
					}
					try {
						to_f64 = to.f64();
					} catch (const msgpack::type_error&) {
						err_f64 = true;
					}
				} catch (const std::out_of_range&) {
					to_u64 = std::numeric_limits<uint64_t>::max();
					to_i64 = std::numeric_limits<int64_t>::max();
					to_f64 = std::numeric_limits<double>::max();
				}

				if (!err_u64) ranges_u64.emplace_back(key.empty() ? _as_bucket(from_u64, to_u64) : key, std::make_pair(from_u64, to_u64));
				if (!err_i64) ranges_i64.emplace_back(key.empty() ? _as_bucket(from_i64, to_i64) : key, std::make_pair(from_i64, to_i64));
				if (!err_f64) ranges_f64.emplace_back(key.empty() ? _as_bucket(from_f64, to_f64) : key, std::make_pair(from_f64, to_f64));
			}
		} catch (const std::out_of_range&) {
			THROW(AggregationError, "'%s' must be specified must be specified in '%s'", AGGREGATION_RANGES, AGGREGATION_RANGE);
		} catch (const msgpack::type_error&) {
			THROW(AggregationError, "'%s' must be object", AGGREGATION_RANGE);
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

	inline void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
