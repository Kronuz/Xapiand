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

// BucketAggregation, the base of the aggregations that group documents into buckets.

#include <algorithm>                              // for std::sort, std::sort_heap
#include <cassert>                                // for assert
#include <cmath>                                  // for std::ceil, std::isnan
#include <cstdint>                                // for int64_t, uint64_t
#include <limits>                                 // for std::numeric_limits
#include <map>                                    // for std::map
#include <math.h>                                 // for fmodl
#include <memory>                                 // for std::shared_ptr
#include <optional>                               // for std::optional
#include <string>                                 // for std::string
#include <sys/types.h>                            // for int64_t, uint64_t
#include <tuple>                                  // for std::forward_as_tuple
#include <type_traits>                            // for std::is_floating_point_v, std::is_unsigned_v
#include <utility>                                // for std::pair, std::make_pair
#include <vector>                                 // for std::vector

#include "aggregations.h"                         // for Aggregation, RESERVED_AGGS_*
#include "database/schema.h"                      // for FieldType
#include "metrics.h"                              // for HandledSubAggregation
#include "msgpack.h"                              // for MsgPack, object::object, ...
#include "exception_engine.h"                     // for AggregationError, MSG_Agg...
#include "xapiand_strings.hh"                             // for strings::format
#include "hashes.hh"                              // for xxh64
#include "engine/codec.h"                        // for serialize_double, unserialize_double
#include "engine/doc_view.h"                     // for DocView
#include "engine/types.h"                        // for valueno


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
	// A capped aggregation (distinct values) keeps at most about `_shard_size` buckets while it collects.
	bool _capped;
	size_t _shard_size;

private:
	friend struct CmpByIndex;
	friend struct CmpByKeyAsc;
	friend struct CmpByKeyDesc;
	friend struct CmpByCountAsc;
	friend struct CmpByCountDesc;
	friend struct CmpByFieldAsc;
	friend struct CmpByFieldDesc;

	struct CmpByIndex {
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
			return a->second.idx < b->second.idx;
		}
	};

	struct CmpByKeyAsc {
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
			if (a->second.slot != b->second.slot) {
				return a->second.slot < b->second.slot;
			}
			return a->first < b->first;
		}
	};

	struct CmpByKeyDesc {
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
			if (a->second.slot != b->second.slot) {
				return a->second.slot > b->second.slot;
			}
			return a->first > b->first;
		}
	};

	struct CmpByCountAsc {
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
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
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
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
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
			assert(a->second.value_ptr);
			assert(b->second.value_ptr);
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
		bool operator()(const std::map<std::string, Aggregation>::const_iterator& a, const std::map<std::string, Aggregation>::const_iterator& b) const {
			assert(a->second.value_ptr);
			assert(b->second.value_ptr);
			if (*a->second.value_ptr != *b->second.value_ptr) {
				return *a->second.value_ptr > *b->second.value_ptr;
			}
			if (a->second.slot != b->second.slot) {
				return a->second.slot > b->second.slot;
			}
			return a->first > b->first;
		}
	};

	using BucketIterator = std::map<std::string, Aggregation>::const_iterator;

	// The `keep` buckets that rank first in the aggregation's order (counts stand in for an order by a sub-aggregation's value).
	std::vector<BucketIterator> _best(size_t keep) const {
		std::vector<BucketIterator> all;
		all.reserve(_aggs.size());
		for (auto it = _aggs.cbegin(); it != _aggs.cend(); ++it) {
			all.push_back(it);
		}
		if (all.size() > keep) {
			switch (_sort) {
				case Sort::by_key_asc:
					std::nth_element(all.begin(), all.begin() + keep, all.end(), CmpByKeyAsc());
					break;
				case Sort::by_key_desc:
					std::nth_element(all.begin(), all.begin() + keep, all.end(), CmpByKeyDesc());
					break;
				case Sort::by_count_asc:
					std::nth_element(all.begin(), all.begin() + keep, all.end(), CmpByCountAsc());
					break;
				default:
					std::nth_element(all.begin(), all.begin() + keep, all.end(), CmpByCountDesc());
					break;
			}
			all.resize(keep);
		}
		return all;
	}

	// Drops the buckets beyond the best `_shard_size` once twice that many exist: the memory is bounded and the
	// work amortised. A dropped value that comes back starts over, so counts can fall short past the cap.
	void _prune() {
		if (!_capped || _aggs.size() < 2 * _shard_size) {
			return;
		}
		const auto before = bucket_count();
		const auto kept = _best(_shard_size);
		std::map<std::string, Aggregation> trimmed;
		for (auto& it : kept) {
			trimmed.insert(_aggs.extract(it));
		}
		_aggs = std::move(trimmed);
		if (auto* budget = current_bucket_budget) {
			budget->used -= std::min(budget->used, before - bucket_count());
		}
	}

	size_t bucket_count() const override {
		size_t count = _aggs.size();
		for (const auto& agg : _aggs) {
			count += agg.second.bucket_count();
		}
		return count;
	}

	void update() override {
		for (auto it = _aggs.begin(); it != _aggs.end(); ++it) {
			if (it->second.doc_count() >= _min_doc_count) {
				it->second.update();
			}
		}
	}

	template <typename Cmp>
	MsgPack _get_result() const {
		Cmp cmp;
		bool is_heap = false;
		std::vector<std::map<std::string, Aggregation>::const_iterator> ordered;
		for (auto it = _aggs.begin(); it != _aggs.end(); ++it) {
			if (it->second.doc_count() >= _min_doc_count) {
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
			auto result = MsgPack::MAP();
			for (auto& agg : ordered) {
				result[agg->first] = agg->second.get_result();
			}
			return result;
		}

		auto result = MsgPack::ARRAY();
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
			switch (value.get_type()) {
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
						switch (sorter.get_type()) {
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
									switch (order.get_type()) {
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
						switch (sorter.get_type()) {
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
									switch (order.get_type()) {
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
						switch (sorter.get_type()) {
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
									switch (order.get_type()) {
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
			switch (value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER: {
					auto val = value.as_i64();
					if (val >= 0) {
						return val;
					}
				}
				[[fallthrough]];
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
			switch (value.get_type()) {
				case MsgPack::Type::BOOLEAN: {
					return value.as_boolean();
				}
				default:
					THROW(AggregationError, "'{}' must be a boolean", RESERVED_AGGS_KEYED);
			}
		}
		return false;
	}

	// Elasticsearch's default: a little more than the size, so the merged top is mostly exact.
	size_t _conf_shard_size() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_SHARD_SIZE);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER: {
					auto val = value.as_i64();
					if (val > 0) {
						return std::max(static_cast<size_t>(val), _limit);
					}
				}
				[[fallthrough]];
				default:
					THROW(AggregationError, "'{}' must be a positive integer", RESERVED_AGGS_SHARD_SIZE);
			}
		}
		return _limit > (std::numeric_limits<size_t>::max() - 10) / 2 ? std::numeric_limits<size_t>::max() / 4 : _limit + _limit / 2 + 10;
	}

	size_t _conf_min_doc_count() {
		const auto& conf = this->_conf;
		auto it = conf.find(RESERVED_AGGS_MIN_DOC_COUNT);
		if (it != conf.end()) {
			const auto& value = it.value();
			switch (value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
				case MsgPack::Type::NEGATIVE_INTEGER: {
					auto val = value.as_i64();
					if (val >= 0) {
						return val;
					}
				}
				[[fallthrough]];
				default:
					THROW(AggregationError, "'{}' must be a positive number", RESERVED_AGGS_MIN_DOC_COUNT);
			}
		}
		return 1;
	}

public:
	BucketAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema, Sort default_sort, bool capped = false)
		: HandledSubAggregation<Handler>(context, name, schema),
		  _schema(schema),
		  _context(context),
		  _name(name),
		  _default_sort(default_sort),
		  _sort(_conf_sort()),
		  _limit(_conf_limit()),
		  _keyed(_conf_keyed()),
		  _min_doc_count(_conf_min_doc_count()),
		  _capped(capped),
		  _shard_size(_conf_shard_size())
	{
		// Each bucket builds its sub-aggregations lazily at match time; build them once now so a bad
		// tree is refused with a 400 before the search runs, an empty index included.
		Aggregation validate(_context, _schema);
	}

	MsgPack get_result() const override {
		switch (_sort) {
			default:
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

	// A bucket's order keys (`slot`, `idx`), which the merge cannot recompute: the long double as two doubles that
	// add up to it (exact on x86, the second one is 0 where long double is double), then the index.
	static std::string serialize_order(const Aggregation& agg) {
		const double hi = static_cast<double>(agg.slot);
		const double lo = std::isfinite(hi) ? static_cast<double>(agg.slot - hi) : 0.0;
		return engine::serialize_double(hi) + engine::serialize_double(lo) + serialize_length(agg.idx);
	}

	static void unserialize_order(std::string_view order, Aggregation& agg) {
		const char* p = order.data();
		const char* p_end = p + order.size();
		const long double hi = engine::unserialize_double(&p, p_end);
		const long double lo = engine::unserialize_double(&p, p_end);
		agg.slot = hi + lo;
		agg.idx = unserialize_length(&p, p_end);
		if (p != p_end) {
			THROW(SerializationError, "Junk found at end of serialized bucket order");
		}
	}

	std::string serialize_results() const override {
		std::string results;
		auto put = [&](const std::string& bucket, const Aggregation& agg) {
			results += serialize_string(bucket);
			results += serialize_string(serialize_order(agg));
			results += serialize_string(agg.serialize_results());
		};
		if (_capped && _aggs.size() > _shard_size) {
			for (auto& it : _best(_shard_size)) {
				put(it->first, it->second);
			}
			return results;
		}
		for (auto& agg : _aggs) {
			put(agg.first, agg.second);
		}
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("BucketAggregation::merge_results({})", repr(std::string(*p, p_end - *p)));

		while (*p != p_end) {
			auto bucket = unserialize_string(p, p_end);
			auto order = unserialize_string(p, p_end);
			auto it = _aggs.find(std::string(bucket));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
			if (it == _aggs.end()) {
				auto emplaced = _aggs.emplace(std::piecewise_construct,
					std::forward_as_tuple(bucket),
					std::forward_as_tuple(_context, _schema));
				it = emplaced.first;
				unserialize_order(order, it->second);

				// Find and store the Aggregation the value should be recovered from:
				if (!_sort_field.empty()) {
					BaseAggregation* agg = &it->second;
					for (const auto& field : _sort_field) {
						if (it->second.value_ptr) {
							THROW(AggregationError, "Bad field path!");
						}
						auto agg_ = agg->get_agg(field);
						if (agg_) {
							agg = agg_;
						} else {
							it->second.value_ptr = agg->get_value_ptr(field);
							if (!it->second.value_ptr) {
								THROW(AggregationError, "Field {} not found", repr(field));
							}
						}
					}
					if (!it->second.value_ptr) {
						THROW(AggregationError, "Field not found");
					}
				}
			}
			auto data = unserialize_string(p, p_end);
			const char* q = data.data();
			const char* q_end = q + data.size();
			it->second.merge_results(&q, q_end);
			if (q != q_end) {
				THROW(SerializationError, "Junk found at end of serialized Aggregation");
			}
		}
	}

	void merge_results(const BaseAggregation* other) override {
		L_CALL("BucketAggregation::merge_results(<aggs>)");

		auto aggs = static_cast<const BucketAggregation*>(other);

		for (auto& _agg : aggs->_aggs) {
			auto& bucket = _agg.first;
			auto it = _aggs.find(bucket);
			if (it == _aggs.end()) {
				auto emplaced = _aggs.emplace(std::piecewise_construct,
					std::forward_as_tuple(bucket),
					std::forward_as_tuple(_context, _schema));
				it = emplaced.first;
				it->second.slot = _agg.second.slot;
				it->second.idx = _agg.second.idx;

				// Find and store the Aggregation the value should be recovered from:
				if (!_sort_field.empty()) {
					BaseAggregation* agg = &it->second;
					for (const auto& field : _sort_field) {
						if (it->second.value_ptr) {
							THROW(AggregationError, "Bad field path!");
						}
						auto agg_ = agg->get_agg(field);
						if (agg_) {
							agg = agg_;
						} else {
							it->second.value_ptr = agg->get_value_ptr(field);
							if (!it->second.value_ptr) {
								THROW(AggregationError, "Field {} not found", repr(field));
							}
						}
					}
					if (!it->second.value_ptr) {
						THROW(AggregationError, "Field not found");
					}
				}
			}
			it->second.merge_results(&_agg.second);
		}
	}

	BaseAggregation* get_agg(std::string_view field) override {
		auto it = _aggs.find(std::string(field));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
		if (it != _aggs.end()) {
			return &it->second;
		}
		return nullptr;
	}

	void aggregate(long double slot, std::string_view bucket, const engine::DocView& doc, size_t idx = 0) {
		auto it = _aggs.find(std::string(bucket));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
		if (it != _aggs.end()) {
			it->second(doc);
			return;
		}

		_prune();

		if (auto* budget = current_bucket_budget) {
			if (++budget->used > budget->max) {
				THROW(AggregationError, "Aggregations would make more than {} buckets in one search; nest fewer levels or aggregate fewer values", budget->max);
			}
		}

		auto emplaced = _aggs.emplace(std::piecewise_construct,
			std::forward_as_tuple(bucket),
			std::forward_as_tuple(_context, _schema));
		it = emplaced.first;

		it->second(doc);

		it->second.slot = slot;
		it->second.idx = idx;

		// Find and store the Aggregation the value should be recovered from:
		if (!_sort_field.empty()) {
			BaseAggregation* agg = &it->second;
			for (const auto& field : _sort_field) {
				if (it->second.value_ptr) {
					THROW(AggregationError, "Bad field path!");
				}
				auto agg_ = agg->get_agg(field);
				if (agg_) {
					agg = agg_;
				} else {
					it->second.value_ptr = agg->get_value_ptr(field);
					if (!it->second.value_ptr) {
						THROW(AggregationError, "Field {} not found", repr(field));
					}
				}
			}
			if (!it->second.value_ptr) {
				THROW(AggregationError, "Field not found");
			}
		}
	}
};
