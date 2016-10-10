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

#include "aggregation_metric.h"

#include <xapian.h>

#include <string>
#include <vector>


class Aggregation {
	std::vector<std::shared_ptr<SubAggregation>> _sub_aggregations;

public:
	Aggregation() = default;

	Aggregation(MsgPack& result, const MsgPack& aggs, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc);

	void update();

	void add_count(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricCount>(result[AGGREGATION_COUNT], data[AGGREGATION_COUNT], schema));
	}

	void add_sum(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricSum>(result[AGGREGATION_SUM], data[AGGREGATION_SUM], schema));
	}

	void add_avg(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricAvg>(result[AGGREGATION_AVG], data[AGGREGATION_AVG], schema));
	}

	void add_min(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricMin>(result[AGGREGATION_MIN], data[AGGREGATION_MIN], schema));
	}

	void add_max(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricMax>(result[AGGREGATION_MAX], data[AGGREGATION_MAX], schema));
	}

	void add_variance(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricVariance>(result[AGGREGATION_VARIANCE], data[AGGREGATION_VARIANCE], schema));
	}

	void add_std(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricSTD>(result[AGGREGATION_STD], data[AGGREGATION_STD], schema));
	}

	void add_median(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricMedian>(result[AGGREGATION_MEDIAN], data[AGGREGATION_MEDIAN], schema));
	}

	void add_mode(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricMode>(result[AGGREGATION_MODE], data[AGGREGATION_MODE], schema));
	}

	void add_stats(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricStats>(result, data[AGGREGATION_STATS], schema));
	}

	void add_ext_stats(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<MetricExtendedStats>(result, data[AGGREGATION_EXT_STATS], schema));
	}

	void add_geo_bounds(MsgPack&, const MsgPack&, const std::shared_ptr<Schema>&) { }

	void add_geo_centroid(MsgPack&, const MsgPack&, const std::shared_ptr<Schema>&) { }

	void add_percentile(MsgPack&, const MsgPack&, const std::shared_ptr<Schema>&) { }

	template <typename BucketAggregation>
	void add_bucket(MsgPack& result, const MsgPack& data, const std::shared_ptr<Schema>& schema) {
		_sub_aggregations.push_back(std::make_shared<BucketAggregation>(result, data, schema));
	}
};


using dispatch_aggregations = void (Aggregation::*)(MsgPack&, const MsgPack&, const std::shared_ptr<Schema>&);


extern const std::unordered_map<std::string, dispatch_aggregations> map_dispatch_aggregations;


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
		: _total(0) { }

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
