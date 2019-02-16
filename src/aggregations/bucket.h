/*
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

#include <algorithm>                              // for std::sort, std::sort_heap
#include <cstdint>                                // for int64_t, uint64_t
#include <limits>                                 // for std::numeric_limits
#include <map>                                    // for std::map
#include <math.h>                                 // for fmodl
#include <memory>                                 // for std::shared_ptr
#include <string>                                 // for std::string
#include <sys/types.h>                            // for int64_t, uint64_t
#include <tuple>                                  // for std::forward_as_tuple
#include <utility>                                // for std::pair, std::make_pair
#include <vector>                                 // for std::vector

#include "aggregations.h"                         // for Aggregation, RESERVED_AGGS_*
#include "metrics.h"                              // for HandledSubAggregation
#include "msgpack.h"                              // for MsgPack, object::object, ...
#include "exception.h"                            // for AggregationError, MSG_Agg...
#include "schema.h"                               // for FieldType
#include "string.hh"                              // for string::format
#include "hashes.hh"                              // for xxh64
#include "xapian.h"                               // for Document, valueno


class Schema;


template <typename Handler>
class BucketAggregation : public HandledSubAggregation<Handler> {
protected:
	enum class Sort {
		by_index,
		by_key_asc,
		by_key_desc,
		by_count_asc,
		by_count_desc,
		by_field_asc,
		by_field_desc,
	};

	std::map<std::string, Aggregation> _aggs;

	const std::shared_ptr<Schema> _schema;
	const MsgPack& _context;
	const std::string_view _name;

	Sort _default_sort;
	Split<std::string_view> _sort_field;
	Sort _sort;
	size_t _limit;
	bool _keyed;
	size_t _min_doc_count;

private:
	friend struct CmpByIndex;
	friend struct CmpByKeyAsc;
	friend struct CmpByKeyDesc;
	friend struct CmpByCountAsc;
	friend struct CmpByCountDesc;
	friend struct CmpByFieldAsc;
	friend struct CmpByFieldDesc;

	struct CmpByIndex {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			return a->second.idx < b->second.idx;
		}
	};

	struct CmpByKeyAsc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			if (a->second.slot != b->second.slot) {
				return a->second.slot < b->second.slot;
			}
			return a->first < b->first;
		}
	};

	struct CmpByKeyDesc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			if (a->second.slot != b->second.slot) {
				return a->second.slot > b->second.slot;
			}
			return a->first > b->first;
		}
	};

	struct CmpByCountAsc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			if (a->second.doc_count() != b->second.doc_count()) {
				return a->second.doc_count() < b->second.doc_count();
			}
			if (a->second.slot != b->second.slot) {
				return a->second.slot < b->second.slot;
			}
			return a->first < b->first;
		}
	};

	struct CmpByCountDesc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			if (a->second.doc_count() != b->second.doc_count()) {
				return a->second.doc_count() > b->second.doc_count();
			}
			if (a->second.slot != b->second.slot) {
				return a->second.slot > b->second.slot;
			}
			return a->first > b->first;
		}
	};

	struct CmpByFieldAsc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			ASSERT(a->second.value_ptr);
			ASSERT(b->second.value_ptr);
			if (*a->second.value_ptr != *b->second.value_ptr) {
				return *a->second.value_ptr < *b->second.value_ptr;
			}
			if (a->second.slot != b->second.slot) {
				return a->second.slot < b->second.slot;
			}
			return a->first < b->first;
		}
	};

	struct CmpByFieldDesc {
		bool operator()(const std::map<std::string, Aggregation>::iterator& a, const std::map<std::string, Aggregation>::iterator& b) const {
			ASSERT(a->second.value_ptr);
			ASSERT(b->second.value_ptr);
			if (*a->second.value_ptr != *b->second.value_ptr) {
				return *a->second.value_ptr > *b->second.value_ptr;
			}
			if (a->second.slot != b->second.slot) {
				return a->second.slot > b->second.slot;
			}
			return a->first > b->first;
		}
	};

	template <typename Cmp>
	MsgPack _get_result() {
		Cmp cmp;
		bool is_heap = false;
		std::vector<std::map<std::string, Aggregation>::iterator> ordered;
		for (auto it = _aggs.begin(); it != _aggs.end(); ++it) {
			if (it->second.doc_count() >= _min_doc_count) {
				it->second.update();
				ordered.push_back(it);
				if (ordered.size() > _limit) {
					if (is_heap) {
						std::push_heap(ordered.begin(), ordered.end(), cmp);
					} else {
						std::make_heap(ordered.begin(), ordered.end(), cmp);
						is_heap = true;
					}
					std::pop_heap(ordered.begin(), ordered.end(), cmp);
					ordered.pop_back();
				}
			}
		}
		if (is_heap) {
			std::sort_heap(ordered.begin(), ordered.end(), cmp);
		} else {
			std::sort(ordered.begin(), ordered.end(), cmp);
		}

		if (_keyed) {
			MsgPack result(MsgPack::Type::MAP);
			for (auto& agg : ordered) {
				result[agg->first] = agg->second.get_result();
			}
			return result;
		}

		MsgPack result(MsgPack::Type::ARRAY);
		for (auto& agg : ordered) {
			result.append(agg->second.get_result())[RESERVED_AGGS_KEY] = agg->first;
		}
		return result;
	}

	Sort _conf_sort() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_SORT);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.getType()) {
				case MsgPack::Type::STR: {
					auto str = value.str_view();
					if (str == RESERVED_AGGS_DOC_COUNT) {
						return Sort::by_count_asc;
					}
					if (str == RESERVED_AGGS_KEY) {
						return Sort::by_key_asc;
					}
					return Sort::by_field_asc;
				}

				case MsgPack::Type::MAP: {
					it = value.find(RESERVED_AGGS_DOC_COUNT);
					if (it != value.end()) {
						const auto& sorter = it.value();
						switch (sorter.getType()) {
							case MsgPack::Type::STR: {
								auto order_str = sorter.str_view();
								if (order_str == "desc") {
									return Sort::by_count_desc;
								}
								if (order_str == "asc") {
									return Sort::by_count_asc;
								}
								THROW(AggregationError, "'{}.{}' must use either 'desc' or 'asc'", RESERVED_AGGS_SORT, RESERVED_AGGS_DOC_COUNT);
							}
							case MsgPack::Type::MAP: {
								it = sorter.find(RESERVED_AGGS_ORDER);
								if (it != sorter.end()) {
									const auto& order = it.value();
									switch (order.getType()) {
										case MsgPack::Type::STR: {
											auto order_str = order.str_view();
											if (order_str == "desc") {
												return Sort::by_count_desc;
											}
											if (order_str == "asc") {
												return Sort::by_count_asc;
											}
											THROW(AggregationError, "'{}.{}.{}' must be either 'desc' or 'asc'", RESERVED_AGGS_SORT, RESERVED_AGGS_DOC_COUNT, RESERVED_AGGS_ORDER);
										}
										default:
											THROW(AggregationError, "'{}.{}.{}' must be a string", RESERVED_AGGS_SORT, RESERVED_AGGS_DOC_COUNT, RESERVED_AGGS_ORDER);
									}
									break;
								}
								THROW(AggregationError, "'{}.{}' must contain '{}'", RESERVED_AGGS_SORT, RESERVED_AGGS_DOC_COUNT, RESERVED_AGGS_ORDER);
							}
							default:
								THROW(AggregationError, "'{}.{}' must be a string or an object", RESERVED_AGGS_SORT, RESERVED_AGGS_DOC_COUNT);
						}
					}

					it = value.find(RESERVED_AGGS_KEY);
					if (it != value.end()) {
						const auto& sorter = it.value();
						switch (sorter.getType()) {
							case MsgPack::Type::STR: {
								auto order_str = sorter.str_view();
								if (order_str == "desc") {
									return Sort::by_key_desc;
								}
								if (order_str == "asc") {
									return Sort::by_key_asc;
								}
								THROW(AggregationError, "'{}.{}' must use either 'desc' or 'asc'", RESERVED_AGGS_SORT, RESERVED_AGGS_KEY);
							}
							case MsgPack::Type::MAP: {
								it = sorter.find(RESERVED_AGGS_ORDER);
								if (it != sorter.end()) {
									const auto& order = it.value();
									switch (order.getType()) {
										case MsgPack::Type::STR: {
											auto order_str = order.str_view();
											if (order_str == "desc") {
												return Sort::by_key_desc;
											}
											if (order_str == "asc") {
												return Sort::by_key_asc;
											}
											THROW(AggregationError, "'{}.{}.{}' must be either 'desc' or 'asc'", RESERVED_AGGS_SORT, RESERVED_AGGS_KEY, RESERVED_AGGS_ORDER);
										}
										default:
											THROW(AggregationError, "'{}.{}.{}' must be a string", RESERVED_AGGS_SORT, RESERVED_AGGS_KEY, RESERVED_AGGS_ORDER);
									}
									break;
								}
								THROW(AggregationError, "'{}.{}' must contain '{}'", RESERVED_AGGS_SORT, RESERVED_AGGS_KEY, RESERVED_AGGS_ORDER);
							}
							default:
								THROW(AggregationError, "'{}.{}' must be a string or an object", RESERVED_AGGS_SORT, RESERVED_AGGS_KEY);
						}
					}

					it = value.begin();
					if (it != value.end()) {
						const auto& field = it->str_view();
						if (field.empty()) {
							THROW(AggregationError, "'{}' must have a valid field name", RESERVED_AGGS_SORT);
						}
						const auto& sorter = it.value();
						switch (sorter.getType()) {
							case MsgPack::Type::STR: {
								auto order_str = sorter.str_view();
								if (order_str == "desc") {
									_sort_field = Split<std::string_view>(field, '.');
									return Sort::by_field_desc;
								}
								if (order_str == "asc") {
									_sort_field = Split<std::string_view>(field, '.');
									return Sort::by_field_asc;
								}
								THROW(AggregationError, "'{}.{}' must use either 'desc' or 'asc'", RESERVED_AGGS_SORT, field);
							}
							case MsgPack::Type::MAP: {
								it = sorter.find(RESERVED_AGGS_ORDER);
								if (it != sorter.end()) {
									const auto& order = it.value();
									switch (order.getType()) {
										case MsgPack::Type::STR: {
											auto order_str = order.str_view();
											if (order_str == "desc") {
												_sort_field = Split<std::string_view>(field, '.');
												return Sort::by_field_desc;
											}
											if (order_str == "asc") {
												_sort_field = Split<std::string_view>(field, '.');
												return Sort::by_field_asc;
											}
											THROW(AggregationError, "'{}.{}.{}' must be either 'desc' or 'asc'", RESERVED_AGGS_SORT, field, RESERVED_AGGS_ORDER);
										}
										default:
											THROW(AggregationError, "'{}.{}.{}' must be a string", RESERVED_AGGS_SORT, field, RESERVED_AGGS_ORDER);
									}
									break;
								}
								THROW(AggregationError, "'{}.{}' must contain '{}'", RESERVED_AGGS_SORT, field, RESERVED_AGGS_ORDER);
							}
							default:
								THROW(AggregationError, "'{}.{}' must be a string or an object", RESERVED_AGGS_SORT, field);
						}
					}

					THROW(AggregationError, "'{}' must contain a field name", RESERVED_AGGS_SORT);
				}

				default:
					THROW(AggregationError, "'{}' must be a string or an object", RESERVED_AGGS_SORT);
			}
		}
		return _default_sort;
	}

	size_t _conf_limit() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_LIMIT);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.getType()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER: {
					auto val = value.as_i64();
					if (val >= 0) {
						return val;
					}
				}
				default:
					THROW(AggregationError, "'{}' must be a positive integer", RESERVED_AGGS_LIMIT);
			}
		}
		return 10000;
	}

	size_t _conf_keyed() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_KEYED);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.getType()) {
				case MsgPack::Type::BOOLEAN: {
					return value.as_boolean();
				}
				default:
					THROW(AggregationError, "'{}' must be a boolean", RESERVED_AGGS_KEYED);
			}
		}
		return false;
	}

	size_t _conf_min_doc_count() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_MIN_DOC_COUNT);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.getType()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER: {
					auto val = value.as_i64();
					if (val >= 0) {
						return val;
					}
				}
				default:
					THROW(AggregationError, "'{}' must be a positive number", RESERVED_AGGS_MIN_DOC_COUNT);
			}
		}
		return 1;
	}

public:
	BucketAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema, Sort default_sort)
		: HandledSubAggregation<Handler>(context, name, schema),
		  _schema(schema),
		  _context(context),
		  _name(name),
		  _default_sort(default_sort),
		  _sort(_conf_sort()),
		  _limit(_conf_limit()),
		  _keyed(_conf_keyed()),
		  _min_doc_count(_conf_min_doc_count()) { }

	MsgPack get_result() override {
		switch (_sort) {
			case Sort::by_index:
				return _get_result<CmpByIndex>();
			case Sort::by_key_asc:
				return _get_result<CmpByKeyAsc>();
			case Sort::by_key_desc:
				return _get_result<CmpByKeyDesc>();
			case Sort::by_count_asc:
				return _get_result<CmpByCountAsc>();
			case Sort::by_count_desc:
				return _get_result<CmpByCountDesc>();
			case Sort::by_field_asc:
				return _get_result<CmpByFieldAsc>();
			case Sort::by_field_desc:
				return _get_result<CmpByFieldDesc>();
		}
	}

	BaseAggregation* get_agg(std::string_view field) override {
		auto it = _aggs.find(std::string(field));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
		if (it != _aggs.end()) {
			return &it->second;
		}
		return nullptr;
	}

	void aggregate(long double slot, std::string_view bucket, const Xapian::Document& doc, size_t idx = 0) {
		auto it = _aggs.find(std::string(bucket));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
		if (it != _aggs.end()) {
			it->second(doc);
			return;
		}

		auto emplaced = _aggs.emplace(std::piecewise_construct,
			std::forward_as_tuple(bucket),
			std::forward_as_tuple(_context, _schema));

		emplaced.first->second(doc);

		emplaced.first->second.slot = slot;
		emplaced.first->second.idx = idx;

		// Find and store the Aggregation the value should be recovered from:
		if (!_sort_field.empty()) {
			BaseAggregation* agg = &emplaced.first->second;
			for (const auto& field : _sort_field) {
				if (emplaced.first->second.value_ptr) {
					THROW(AggregationError, "Bad field path!");
				}
				auto agg_ = agg->get_agg(field);
				if (agg_) {
					agg = agg_;
				} else {
					emplaced.first->second.value_ptr = agg->get_value_ptr(field);
					if (!emplaced.first->second.value_ptr) {
						THROW(AggregationError, "Field not found! (1)");
					}
				}
			}
			if (!emplaced.first->second.value_ptr) {
				THROW(AggregationError, "Field not found! (2)");
			}
		}
	}
};


class ValuesAggregation : public BucketAggregation<ValuesHandler> {
public:
	ValuesAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(context, name, schema, Sort::by_count_desc) { }

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		aggregate(value ? 1.0 : 0.0, std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const Xapian::Document& doc) override {
		aggregate(0.0, value, doc);
	}

	void aggregate_geo(const range_t& value, const Xapian::Document& doc) override {
		aggregate(0.0, value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const Xapian::Document& doc) override {
		aggregate(0.0, value, doc);
	}
};


class TermsAggregation : public BucketAggregation<TermsHandler> {
public:
	TermsAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<TermsHandler>(context, name, schema, Sort::by_count_desc) { }

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		aggregate(value, string::format("{}", value), doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		aggregate(value ? 1.0 : 0.0, std::string_view(value ? "true" : "false"), doc);
	}

	void aggregate_string(std::string_view value, const Xapian::Document& doc) override {
		aggregate(0.0, value, doc);
	}

	void aggregate_geo(const range_t& value, const Xapian::Document& doc) override {
		aggregate(0.0, value.to_string(), doc);
	}

	void aggregate_uuid(std::string_view value, const Xapian::Document& doc) override {
		aggregate(0.0, value, doc);
	}
};


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
			switch (interval_value.getType()) {
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
			switch (shift_value.getType()) {
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
			switch (interval_value.getType()) {
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
			switch (shift_value.getType()) {
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
			switch (interval_value.getType()) {
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
			switch (shift_value.getType()) {
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
			case FieldType::POSITIVE:
				configure_u64();
				break;
			case FieldType::INTEGER:
				configure_i64();
				break;
			case FieldType::FLOAT:
			case FieldType::DATE:
			case FieldType::TIME:
			case FieldType::TIMEDELTA:
				configure_f64();
				break;
			default:
				THROW(AggregationError, "Histogram aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, string::format("{}", bucket), doc);
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, string::format("{}", bucket), doc);
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(value);
		aggregate(value, string::format("{}", bucket), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, string::format("{}", bucket), doc);
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, string::format("{}", bucket), doc);
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		auto bucket = get_bucket(static_cast<long double>(value));
		aggregate(value, string::format("{}", bucket), doc);
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
			return string::format("{}..", start);
		}
		if (start == std::numeric_limits<T>::min()) {
			if (end == std::numeric_limits<T>::max()) {
				return "..";
			}
			return string::format("..{}", end);
		}
		return string::format("{}..{}", start, end);
	}

	void configure_u64() {
		const auto it = _conf.find(RESERVED_AGGS_RANGES);
		if (it != _conf.end()) {
			const auto& ranges = it.value();
			if (!ranges.is_array()) {
				THROW(AggregationError, "'{}.{}' must be an array", _name, RESERVED_AGGS_RANGES);
			}

			for (const auto& range : ranges) {
				std::string default_key;
				std::string_view key;
				auto key_it = range.find(RESERVED_AGGS_KEY);
				if (key_it != range.end()) {
					const auto& key_value = key_it.value();
					if (!key_value.is_string()) {
						THROW(AggregationError, "'{}' must be a string", RESERVED_AGGS_KEY);
					}
					key = key_value.str_view();
				}

				long double from_u64 = std::numeric_limits<long double>::min();
				auto from_it = range.find(RESERVED_AGGS_FROM);
				if (from_it != range.end()) {
					const auto& from_value = from_it.value();
					switch (from_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							from_u64 = from_value.as_u64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_FROM);
					}
				}

				long double to_u64 = std::numeric_limits<long double>::max();
				auto to_it = range.find(RESERVED_AGGS_TO);
				if (to_it != range.end()) {
					const auto& to_value = to_it.value();
					switch (to_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							to_u64 = to_value.as_u64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_TO);
					}
				}

				if (key.empty()) {
					default_key = _as_bucket(from_u64, to_u64);
					key = default_key;
				}
				ranges_u64.emplace_back(key, std::make_pair(from_u64, to_u64));
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_RANGES);
		}
	}

	void configure_i64() {
		const auto it = _conf.find(RESERVED_AGGS_RANGES);
		if (it != _conf.end()) {
			const auto& ranges = it.value();
			if (!ranges.is_array()) {
				THROW(AggregationError, "'{}.{}' must be an array", _name, RESERVED_AGGS_RANGES);
			}

			for (const auto& range : ranges) {
				std::string default_key;
				std::string_view key;
				auto key_it = range.find(RESERVED_AGGS_KEY);
				if (key_it != range.end()) {
					const auto& key_value = key_it.value();
					if (!key_value.is_string()) {
						THROW(AggregationError, "'{}' must be a string", RESERVED_AGGS_KEY);
					}
					key = key_value.str_view();
				}

				long double from_i64 = std::numeric_limits<long double>::min();
				auto from_it = range.find(RESERVED_AGGS_FROM);
				if (from_it != range.end()) {
					const auto& from_value = from_it.value();
					switch (from_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							from_i64 = from_value.as_i64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_FROM);
					}
				}

				long double to_i64 = std::numeric_limits<long double>::max();
				auto to_it = range.find(RESERVED_AGGS_TO);
				if (to_it != range.end()) {
					const auto& to_value = to_it.value();
					switch (to_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							to_i64 = to_value.as_i64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_TO);
					}
				}

				if (key.empty()) {
					default_key = _as_bucket(from_i64, to_i64);
					key = default_key;
				}
				ranges_i64.emplace_back(key, std::make_pair(from_i64, to_i64));
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_RANGES);
		}
	}

	void configure_f64() {
		const auto it = _conf.find(RESERVED_AGGS_RANGES);
		if (it != _conf.end()) {
			const auto& ranges = it.value();
			if (!ranges.is_array()) {
				THROW(AggregationError, "'{}.{}' must be an array", _name, RESERVED_AGGS_RANGES);
			}
			for (const auto& range : ranges) {
				std::string default_key;
				std::string_view key;
				auto key_it = range.find(RESERVED_AGGS_KEY);
				if (key_it != range.end()) {
					const auto& key_value = key_it.value();
					if (!key_value.is_string()) {
						THROW(AggregationError, "'{}' must be a string", RESERVED_AGGS_KEY);
					}
					key = key_value.str_view();
				}

				long double from_f64 = std::numeric_limits<long double>::min();
				auto from_it = range.find(RESERVED_AGGS_FROM);
				if (from_it != range.end()) {
					const auto& from_value = from_it.value();
					switch (from_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							from_f64 = from_value.as_f64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_FROM);
					}
				}

				long double to_f64 = std::numeric_limits<long double>::max();
				auto to_it = range.find(RESERVED_AGGS_TO);
				if (to_it != range.end()) {
					const auto& to_value = to_it.value();
					switch (to_value.getType()) {
						case MsgPack::Type::POSITIVE_INTEGER:
						case MsgPack::Type::NEGATIVE_INTEGER:
						case MsgPack::Type::FLOAT:
							to_f64 = to_value.as_f64();
							break;
						default:
							THROW(AggregationError, "'{}' must be a number", RESERVED_AGGS_TO);
					}
				}

				if (key.empty()) {
					default_key = _as_bucket(from_f64, to_f64);
					key = default_key;
				}
				ranges_f64.emplace_back(key, std::make_pair(from_f64, to_f64));
			}
		} else {
			THROW(AggregationError, "'{}' must be object with '{}'", _name, RESERVED_AGGS_RANGES);
		}
	}

public:
	RangeAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: BucketAggregation<ValuesHandler>(context, name, schema, Sort::by_index)
	{
		switch (_handler.get_type()) {
			case FieldType::POSITIVE:
				configure_u64();
				break;
			case FieldType::INTEGER:
				configure_i64();
				break;
			case FieldType::FLOAT:
			case FieldType::DATE:
			case FieldType::TIME:
			case FieldType::TIMEDELTA:
				configure_f64();
				break;
			default:
				THROW(AggregationError, "Range aggregation can work only on numeric fields");
		}
	}

	void aggregate_float(long double value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}

	void aggregate_integer(int64_t value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_i64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}

	void aggregate_positive(uint64_t value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_u64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}

	void aggregate_time(double value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}

	void aggregate_timedelta(double value, const Xapian::Document& doc) override {
		size_t idx = 0;
		for (const auto& range : ranges_f64) {
			if (value >= range.second.first && value < range.second.second) {
				aggregate(range.second.first, range.first, doc, idx);
			}
			++idx;
		}
	}
};


class FilterAggregation : public BaseAggregation {
	using func_filter = void (FilterAggregation::*)(const Xapian::Document&);

	std::vector<std::pair<Xapian::valueno, std::set<std::string>>> _filters;
	Aggregation _agg;
	func_filter func;

public:
	FilterAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	MsgPack get_result() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
