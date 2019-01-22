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

#include <memory>                   // for shared_ptr, make_shared
#include <stddef.h>                 // for size_t
#include <string>                   // for string
#include "string_view.hh"           // for std::string_view
#include <type_traits>              // for decay_t, enable_if_t, forward
#include <utility>                  // for std::pair, std::make_pair
#include <vector>                   // for vector
#include <xapian.h>                 // for MatchSpy, doccount

#include "msgpack.h"                // for MsgPack


constexpr const char AGGREGATION_AGGS[]             = "_aggs";
constexpr const char AGGREGATION_AGGREGATIONS[]     = "_aggregations";
constexpr const char AGGREGATION_DOC_COUNT[]        = "_doc_count";
constexpr const char AGGREGATION_FIELD[]            = "_field";
constexpr const char AGGREGATION_FROM[]             = "_from";
constexpr const char AGGREGATION_INTERVAL[]         = "_interval";
constexpr const char AGGREGATION_KEY[]              = "_key";
constexpr const char AGGREGATION_RANGES[]           = "_ranges";
constexpr const char AGGREGATION_SUM_OF_SQ[]        = "_sum_of_squares";
constexpr const char AGGREGATION_TO[]               = "_to";

constexpr const char AGGREGATION_AVG[]              = "_avg";
constexpr const char AGGREGATION_CARDINALITY[]      = "_cardinality";
constexpr const char AGGREGATION_COUNT[]            = "_count";
constexpr const char AGGREGATION_EXT_STATS[]        = "_extended_stats";
constexpr const char AGGREGATION_GEO_BOUNDS[]       = "_geo_bounds";
constexpr const char AGGREGATION_GEO_CENTROID[]     = "_geo_centroid";
constexpr const char AGGREGATION_MAX[]              = "_max";
constexpr const char AGGREGATION_MEDIAN[]           = "_median";
constexpr const char AGGREGATION_MIN[]              = "_min";
constexpr const char AGGREGATION_MODE[]             = "_mode";
constexpr const char AGGREGATION_PERCENTILES[]      = "_percentiles";
constexpr const char AGGREGATION_PERCENTILES_RANK[] = "_percentiles_rank";
constexpr const char AGGREGATION_SCRIPTED_METRIC[]  = "_scripted_metric";
constexpr const char AGGREGATION_STATS[]            = "_stats";
constexpr const char AGGREGATION_STD[]              = "_std_deviation";
constexpr const char AGGREGATION_STD_BOUNDS[]       = "_std_deviation_bounds";
constexpr const char AGGREGATION_SUM[]              = "_sum";
constexpr const char AGGREGATION_VARIANCE[]         = "_variance";

constexpr const char AGGREGATION_DATE_HISTOGRAM[]   = "_date_histogram";
constexpr const char AGGREGATION_DATE_RANGE[]       = "_date_range";
constexpr const char AGGREGATION_FILTER[]           = "_filter";
constexpr const char AGGREGATION_GEO_DISTANCE[]     = "_geo_distance";
constexpr const char AGGREGATION_GEO_IP[]           = "_geo_ip";
constexpr const char AGGREGATION_GEO_TRIXELS[]      = "_geo_trixels";
constexpr const char AGGREGATION_HISTOGRAM[]        = "_histogram";
constexpr const char AGGREGATION_IP_RANGE[]         = "_ip_range";
constexpr const char AGGREGATION_MISSING[]          = "_missing";
constexpr const char AGGREGATION_RANGE[]            = "_range";
constexpr const char AGGREGATION_VALUES[]           = "_values";
constexpr const char AGGREGATION_TERMS[]            = "_terms";

constexpr const char AGGREGATION_UPPER[]            = "_upper";
constexpr const char AGGREGATION_LOWER[]            = "_lower";
constexpr const char AGGREGATION_SIGMA[]            = "_sigma";

constexpr const char AGGREGATION_VALUE[]            = "_value";
constexpr const char AGGREGATION_TERM[]             = "_term";
constexpr const char AGGREGATION_SORT[]             = "_sort";
constexpr const char AGGREGATION_ORDER[]            = "_order";
constexpr const char AGGREGATION_MIN_DOC_COUNT[]    = "_min_doc_count";
constexpr const char AGGREGATION_LIMIT[]            = "_limit";


class Schema;


class BaseAggregation {
public:
	virtual ~BaseAggregation() = default;

	virtual void operator()(const Xapian::Document&) = 0;

	virtual void update() { }

	virtual MsgPack get_aggregation() = 0;
};


class Aggregation : public BaseAggregation {
	size_t _doc_count;

	std::vector<std::pair<std::string_view, std::unique_ptr<BaseAggregation>>> _sub_aggs;

public:
	explicit Aggregation();

	Aggregation(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	void operator()(const Xapian::Document& doc) override;

	void update() override;

	MsgPack get_aggregation() override;

	size_t doc_count() const {
		return _doc_count;
	}

	template <typename MetricAggregation, typename... Args>
	void add_metric(std::string_view name, Args&&... args) {
		_sub_aggs.push_back(std::make_pair(name, std::make_unique<MetricAggregation>(std::forward<Args>(args)...)));
	}

	template <typename BucketAggregation, typename... Args>
	void add_bucket(std::string_view name, Args&&... args) {
		_sub_aggs.push_back(std::make_pair(name, std::make_unique<BucketAggregation>(std::forward<Args>(args)...)));
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
		  _result(MsgPack::Type::MAP) { }

	/*
	 * Construct a AggregationMatchSpy which aggregates the values.
	 *
	 * Further Aggregations can be added by calling add_aggregation().
	 */
	template <typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<T>>::value>>
	AggregationMatchSpy(T&& aggs, const std::shared_ptr<Schema>& schema)
		: _total(0),
		  _result(MsgPack::Type::MAP),
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
	std::string get_description() const override;

	const auto& get_aggregation() noexcept {
		_aggregation.update();
		_result[AGGREGATION_AGGREGATIONS] = _aggregation.get_aggregation();
		return _result;
	}
};
