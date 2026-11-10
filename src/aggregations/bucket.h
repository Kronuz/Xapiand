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
		  _shard_size(_conf_shard_size()) { }

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

	std::string serialise_results() const override {
		std::string results;
		if (_capped && _aggs.size() > _shard_size) {
			for (auto& it : _best(_shard_size)) {
				results += serialise_string(it->first);
				results += serialise_string(it->second.serialise_results());
			}
			return results;
		}
		for (auto& agg : _aggs) {
			results += serialise_string(agg.first);
			results += serialise_string(agg.second.serialise_results());
		}
		return results;
	}

	void merge_results(const char** p, const char* p_end) override {
		L_CALL("BucketAggregation::merge_results({})", repr(std::string(*p, p_end - *p)));

		while (*p != p_end) {
			auto bucket = unserialise_string(p, p_end);
			auto it = _aggs.find(std::string(bucket));  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
			if (it == _aggs.end()) {
				auto emplaced = _aggs.emplace(std::piecewise_construct,
					std::forward_as_tuple(bucket),
					std::forward_as_tuple(_context, _schema));
				it = emplaced.first;

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
			auto data = unserialise_string(p, p_end);
			const char* q = data.data();
			const char* q_end = q + data.size();
			it->second.merge_results(&q, q_end);
			if (q != q_end) {
				THROW(SerialisationError, "Junk found at end of serialised Aggregation");
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

		std::string key() const {
			if (!from && !to) {
				return "..";
			}
			return strings::format("{}..{}", from ? strings::format("{}", *from) : std::string(), to ? strings::format("{}", *to) : std::string());
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
					if constexpr (std::is_unsigned_v<T>) {
						return { Reach::below, T() };
					} else {
						return { Reach::inside, static_cast<T>(value.as_i64()) };
					}
				}
				default: {
					const auto d = std::ceil(value.as_f64());
					if (std::isnan(d)) {
						THROW(AggregationError, "bounds must be numbers");
					}
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
					case MsgPack::Type::POSITIVE_INTEGER:
					case MsgPack::Type::NEGATIVE_INTEGER:
					case MsgPack::Type::FLOAT:
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
			ranges.emplace_back(key.empty() ? bounds.key() : std::string(key), bounds);
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

	std::string serialise_results() const override;

	void merge_results(const char** p, const char* p_end) override;
	void merge_results(const BaseAggregation* other) override;

	void check_single(const engine::DocView& doc);
	void check_multiple(const engine::DocView& doc);
};
