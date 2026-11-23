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

// Histogram and range bucket aggregations.

#include "bucket_base.h"


class HistogramAggregation : public BucketAggregation<ValuesHandler> {
	uint64_t interval_u64;
	int64_t interval_i64;
	long double interval_f64;

	uint64_t shift_u64;
	int64_t shift_i64;
	long double shift_f64;

	auto get_bucket(uint64_t value) {
		if (!interval_u64) {
			THROW(AggregationError, "'{}' must be a non-zero number", RESERVED_AGGS_INTERVAL);
		}
		return ((value - shift_u64) / interval_u64) * interval_u64 + shift_u64;
	}

	auto get_bucket(int64_t value) {
		if (!interval_i64) {
			THROW(AggregationError, "'{}' must be a non-zero number", RESERVED_AGGS_INTERVAL);
		}
		return ((value - shift_i64) / interval_i64) * interval_i64 + shift_i64;
	}

	auto get_bucket(long double value) {
		if (!interval_f64) {
			THROW(AggregationError, "'{}' must be a non-zero number", RESERVED_AGGS_INTERVAL);
		}
		return floorl((value - shift_f64) / interval_f64) * interval_f64 + shift_f64;
	}

	void configure_u64() {
		const auto interval_it = _conf.find(RESERVED_AGGS_INTERVAL);
		if (interval_it != _conf.end()) {
			const auto& interval_value = interval_it.value();
			switch (interval_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					interval_u64 = interval_value.as_u64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_INTERVAL);
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_INTERVAL);
		}

		const auto shift_it = _conf.find(RESERVED_AGGS_SHIFT);
		if (shift_it != _conf.end()) {
			const auto& shift_value = shift_it.value();
			switch (shift_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					shift_u64 = shift_value.as_u64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_SHIFT);
			}
		}
	}

	void configure_i64() {
		const auto interval_it = _conf.find(RESERVED_AGGS_INTERVAL);
		if (interval_it != _conf.end()) {
			const auto& interval_value = interval_it.value();
			switch (interval_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					interval_i64 = interval_value.as_i64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_INTERVAL);
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_INTERVAL);
		}

		const auto shift_it = _conf.find(RESERVED_AGGS_SHIFT);
		if (shift_it != _conf.end()) {
			const auto& shift_value = shift_it.value();
			switch (shift_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					shift_i64 = shift_value.as_i64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_SHIFT);
			}
		}
	}

	void configure_f64() {
		const auto interval_it = _conf.find(RESERVED_AGGS_INTERVAL);
		if (interval_it != _conf.end()) {
			const auto& interval_value = interval_it.value();
			switch (interval_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					interval_f64 = interval_value.as_f64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_INTERVAL);
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_INTERVAL);
		}

		const auto shift_it = _conf.find(RESERVED_AGGS_SHIFT);
		if (shift_it != _conf.end()) {
			const auto& shift_value = shift_it.value();
			switch (shift_value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER:
				case MsgPack::Type::FLOAT:
					shift_f64 = shift_value.as_f64();
					break;
				default:
					THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_SHIFT);
			}
		}
	}

public:
	HistogramAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(context, name, schema, Sort::by_key_asc),
		  interval_u64{0},
		  interval_i64{0},
		  interval_f64{0.0},
		  shift_u64{0},
		  shift_i64{0},
		  shift_f64{0.0}
	{
		switch (_handler.get_type()) {
			case FieldType::positive:
				configure_u64();
				break;
			case FieldType::integer:
				configure_i64();
				break;
			case FieldType::floating:
			case FieldType::date:
			case FieldType::datetime:
			case FieldType::time:
			case FieldType::timedelta:
				configure_f64();
				break;
			default:
				THROW(AggregationError, "Histogram aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const engine::DocView& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, strings::format("{}", bucket), doc);
	}

	void aggregate_integer(int64_t value, const engine::DocView& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, strings::format("{}", bucket), doc);
	}

	void aggregate_positive(uint64_t value, const engine::DocView& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, strings::format("{}", bucket), doc);
	}

	void aggregate_date(double value, const engine::DocView& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, strings::format("{}", bucket), doc);
	}

	void aggregate_time(double value, const engine::DocView& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, strings::format("{}", bucket), doc);
	}

	void aggregate_timedelta(double value, const engine::DocView& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, strings::format("{}", bucket), doc);
	}
};


class RangeAggregation : public BucketAggregation<ValuesHandler> {
	// The [from, to) of a range in the field's own type; an end that is not set is open.
	template <typename T>
	struct Bounds {
		std::optional<T> from;
		std::optional<T> to;
		bool empty = false;  // an end beyond every value of T leaves nothing inside

		bool contains(T value) const {
			return !empty && (!from || value >= *from) && (!to || value < *to);
		}

		long double slot() const {
			return from ? static_cast<long double>(*from) : -std::numeric_limits<long double>::infinity();
		}
	};

	template <typename T>
	using Ranges = std::vector<std::pair<std::string, Bounds<T>>>;

	Ranges<uint64_t> ranges_u64;
	Ranges<int64_t> ranges_i64;
	Ranges<long double> ranges_f64;

	// Where a bound lies relative to the values of T, and its value when inside them.
	enum class Reach { below, inside, above };

	template <typename T>
	static std::pair<Reach, T> reach_of(const MsgPack& value) {
		if constexpr (std::is_floating_point_v<T>) {
			return { Reach::inside, static_cast<T>(value.as_f64()) };
		} else {
			// Over integers, "value >= bound" and "value < bound" hold exactly when they hold for the bound rounded up.
			constexpr auto lowest = static_cast<double>(std::numeric_limits<T>::lowest());
			constexpr auto beyond = static_cast<double>(std::numeric_limits<T>::max());  // the first double above T's range
			switch (value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER: {
					const auto u = value.as_u64();
					if (u > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
						return { Reach::above, T() };
					}
					return { Reach::inside, static_cast<T>(u) };
				}
				case MsgPack::Type::NEGATIVE_INTEGER: {
					// Reads the value, not the tag: a signed integer of either sign may carry this tag.
					const auto i = value.as_i64();
					if constexpr (std::is_unsigned_v<T>) {
						if (i < 0) {
							return { Reach::below, T() };
						}
					}
					return { Reach::inside, static_cast<T>(i) };
				}
				default: {
					const auto d = std::ceil(value.as_f64());
					if (d < lowest) {
						return { Reach::below, T() };
					}
					if (d >= beyond) {
						return { Reach::above, T() };
					}
					return { Reach::inside, static_cast<T>(d) };
				}
			}
		}
	}

	// A bound as the request wrote it.
	static std::string bound_text(const MsgPack& value) {
		switch (value.get_type()) {
			case MsgPack::Type::POSITIVE_INTEGER:
				return strings::format("{}", value.as_u64());
			case MsgPack::Type::NEGATIVE_INTEGER:
				return strings::format("{}", value.as_i64());
			default:
				return strings::format("{}", value.as_f64());
		}
	}

	// The default key of a range: its bounds as written, ".." when it has none.
	static std::string default_key(const MsgPack& range) {
		const auto from = range.find(RESERVED_AGGS_FROM);
		const auto to = range.find(RESERVED_AGGS_TO);
		return strings::format("{}..{}", from != range.end() ? bound_text(from.value()) : std::string(), to != range.end() ? bound_text(to.value()) : std::string());
	}

	template <typename T>
	void configure(Ranges<T>& ranges) {
		const auto it = _conf.find(RESERVED_AGGS_RANGES);
		if (it == _conf.end()) {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_RANGES);
		}
		const auto& list = it.value();
		if (!list.is_array()) {
			THROW(AggregationError, "'{}.{}' must be an array", _name, RESERVED_AGGS_RANGES);
		}

		for (const auto& range : list) {
			std::string_view key;
			auto key_it = range.find(RESERVED_AGGS_KEY);
			if (key_it != range.end()) {
				const auto& key_value = key_it.value();
				if (!key_value.is_string()) {
					THROW(AggregationError, "'{}' must be a string", RESERVED_AGGS_KEY);
				}
				key = key_value.str_view();
			}

			auto bound = [&](const char* name) -> std::optional<std::pair<Reach, T>> {
				auto bound_it = range.find(name);
				if (bound_it == range.end()) {
					return std::nullopt;
				}
				const auto& bound_value = bound_it.value();
				switch (bound_value.get_type()) {
					case MsgPack::Type::FLOAT:
						// NaN has no place in any field's order: refused alike on every type.
						if (std::isnan(bound_value.as_f64())) {
							THROW(AggregationError, "Field {} range bound '{}' cannot be NaN", repr(_conf.at(RESERVED_AGGS_FIELD).str_view()), name);
						}
						return reach_of<T>(bound_value);
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
						return reach_of<T>(bound_value);
					default:
						THROW(AggregationError, "'{}' must be a number", name);
				}
			};

			Bounds<T> bounds;
			if (const auto from = bound(RESERVED_AGGS_FROM)) {
				switch (from->first) {
					case Reach::below: break;  // open
					case Reach::inside: bounds.from = from->second; break;
					case Reach::above: bounds.empty = true; break;
				}
			}
			if (const auto to = bound(RESERVED_AGGS_TO)) {
				switch (to->first) {
					case Reach::below: bounds.empty = true; break;
					case Reach::inside: bounds.to = to->second; break;
					case Reach::above: break;  // open
				}
			}
			ranges.emplace_back(key.empty() ? default_key(range) : std::string(key), bounds);
		}
	}

	template <typename T>
	void aggregate_ranges(const Ranges<T>& ranges, T value, const engine::DocView& doc) {
		size_t idx = 0;
		for (const auto& [key, bounds] : ranges) {
			if (bounds.contains(value)) {
				aggregate(bounds.slot(), key, doc, idx);
			}
			++idx;
		}
	}

public:
	RangeAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(context, name, schema, Sort::by_index)
	{
		switch (_handler.get_type()) {
			case FieldType::positive:
				configure(ranges_u64);
				break;
			case FieldType::integer:
				configure(ranges_i64);
				break;
			case FieldType::floating:
			case FieldType::date:
			case FieldType::datetime:
			case FieldType::time:
			case FieldType::timedelta:
				configure(ranges_f64);
				break;
			default:
				THROW(AggregationError, "Range aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const engine::DocView& doc) override {
		aggregate_ranges(ranges_f64, value, doc);
	}

	void aggregate_integer(int64_t value, const engine::DocView& doc) override {
		aggregate_ranges(ranges_i64, value, doc);
	}

	void aggregate_positive(uint64_t value, const engine::DocView& doc) override {
		aggregate_ranges(ranges_u64, value, doc);
	}

	void aggregate_date(double value, const engine::DocView& doc) override {
		aggregate_ranges<long double>(ranges_f64, value, doc);
	}

	void aggregate_time(double value, const engine::DocView& doc) override {
		aggregate_ranges<long double>(ranges_f64, value, doc);
	}

	void aggregate_timedelta(double value, const engine::DocView& doc) override {
		aggregate_ranges<long double>(ranges_f64, value, doc);
	}
};
