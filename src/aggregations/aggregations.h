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

#include <memory>                                 // for shared_ptr, make_shared
#include <stddef.h>                               // for size_t
#include <string>                                 // for string
#include <string_view>                            // for std::string_view
#include <type_traits>                            // for decay_t, enable_if_t, forward
#include <utility>                                // for std::pair, std::make_pair
#include <vector>                                 // for vector

#include "msgpack.h"                              // for MsgPack
#include "xapian.h"                               // for MatchSpy, doccount


class Schema;


class BaseAggregation {
public:
	virtual ~BaseAggregation() = default;

	virtual void operator()(const Xapian::Document&) = 0;

	virtual void update() { }

	virtual MsgPack get_result() const = 0;

	virtual std::string serialise_results() const = 0;
	virtual void merge_results(const char** p, const char* end) = 0;

	virtual BaseAggregation* get_agg(std::string_view) {
		return nullptr;
	}

	virtual const long double* get_value_ptr(std::string_view) const {
		return nullptr;
	}
};


class Aggregation : public BaseAggregation {
	size_t _doc_count;

	std::map<std::string_view, std::unique_ptr<BaseAggregation>> _sub_aggs;

public:
	const long double* value_ptr;
	long double slot;
	size_t idx;

	Aggregation();

	Aggregation(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override;

	void update() override;

	MsgPack get_result() const override;

	std::string serialise_results() const override;
	void merge_results(const char** p, const char* p_end) override;

	BaseAggregation* get_agg(std::string_view field) override;

	size_t doc_count() const {
		return _doc_count;
	}

	template <typename MetricAggregation, typename... Args>
	void add_metric(std::string_view name, Args&&... args) {
		_sub_aggs[name] = std::make_unique<MetricAggregation>(std::forward<Args>(args)...);
	}

	template <typename BucketAggregation, typename... Args>
	void add_bucket(std::string_view name, Args&&... args) {
		_sub_aggs[name] = std::make_unique<BucketAggregation>(std::forward<Args>(args)...);
	}
};


// Class for calculating aggregations in the matching documents.
class AggregationMatchSpy : public Xapian::MatchSpy {
	// Total number of documents seen by the match spy.
	Xapian::doccount _total;

	// Result for aggregations.
	MsgPack _result;

	MsgPack _aggs;
	std::shared_ptr<Schema> _schema;

	// Aggregation seen so far.
	Aggregation _aggregation;

public:
	// Construct an empty AggregationMatchSpy.
	AggregationMatchSpy()
		: _total(0),
		  _result(MsgPack::MAP()),
		  _aggs(MsgPack::MAP()) { }

	/*
	 * Construct a AggregationMatchSpy which aggregates the values.
	 *
	 * Further Aggregations can be added by calling add_aggregation().
	 */
	template <typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<T>>::value>>
	AggregationMatchSpy(T&& aggs, const std::shared_ptr<Schema>& schema)
		: _total(0),
		  _result(MsgPack::MAP()),
		  _aggs(std::forward<T>(aggs)),
		  _schema(schema),
		  _aggregation(_aggs, _schema) { }

	/*
	 * Implementation of virtual operator().
	 *
	 * This implementation aggregates values for a matching document.
	 */
	void operator()(const Xapian::Document &doc, double wt) override;

	Xapian::MatchSpy* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	Xapian::MatchSpy* unserialise(const std::string& serialised, const Xapian::Registry& context) const override;
	std::string serialise_results() const override;
	void merge_results(const std::string& serialised) override;

	std::string get_description() const override;
	const MsgPack& get_aggregation() noexcept;
};
