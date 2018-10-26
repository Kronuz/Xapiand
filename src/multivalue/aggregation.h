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

#include <memory>                   // for shared_ptr, make_shared
#include <stddef.h>                 // for size_t
#include <string>                   // for string
#include "string_view.hh"           // for std::string_view
#include <type_traits>              // for decay_t, enable_if_t, forward
#include <unordered_map>            // for unordered_map
#include <vector>                   // for vector
#include <xapian.h>                 // for MatchSpy, doccount

#include "aggregation_metric.h"     // for AGGREGATION_AGGS
#include "msgpack.h"                // for MsgPack


class Schema;


class Aggregation {
	MsgPack& _result;
	size_t _doc_count;

	std::vector<std::shared_ptr<SubAggregation>> _sub_aggregations;


public:
	explicit Aggregation(MsgPack& result);
	Aggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc);

	void update();

	template <const char* name, typename MetricAggregation>
	void add_metric(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricAggregation>(result, conf[name], schema));
	}

	template <typename BucketAggregation>
	void add_bucket(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<BucketAggregation>(result, conf, schema));
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
		  _result(),
		  _aggregation(_result[AGGREGATION_AGGS]) { }

	/*
	 * Construct a AggregationMatchSpy which aggregates the values.
	 *
	 * Further Aggregations can be added by calling add_aggregation().
	 */
	template <typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<T>>::value>>
	AggregationMatchSpy(T&& aggs, const std::shared_ptr<Schema>& schema)
		: _total(0),
		  _result(),
		  _aggs(std::forward<T>(aggs)),
		  _schema(schema),
		  _aggregation(_result[AGGREGATION_AGGS], _aggs, _schema) { }

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
	std::string get_description() const override;

	const auto& get_aggregation() noexcept {
		_aggregation.update();
		return _result;
	}
};
