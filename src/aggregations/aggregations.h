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
#include "string_view.hh"                         // for std::string_view
#include <type_traits>                            // for decay_t, enable_if_t, forward
#include <utility>                                // for std::pair, std::make_pair
#include <vector>                                 // for vector

#include "msgpack.h"                              // for MsgPack
#include "reserved.h"                             // for RESERVED__
#include "xapian.h"                               // for MatchSpy, doccount


constexpr const char RESERVED_AGGS_AGGS[]                   = RESERVED__ "aggs";
constexpr const char RESERVED_AGGS_AGGREGATIONS[]           = RESERVED__ "aggregations";
constexpr const char RESERVED_AGGS_DOC_COUNT[]              = RESERVED__ "doc_count";
constexpr const char RESERVED_AGGS_FIELD[]                  = RESERVED__ "field";
constexpr const char RESERVED_AGGS_FROM[]                   = RESERVED__ "from";
constexpr const char RESERVED_AGGS_INTERVAL[]               = RESERVED__ "interval";
constexpr const char RESERVED_AGGS_SHIFT[]                  = RESERVED__ "shift";
constexpr const char RESERVED_AGGS_KEY[]                    = RESERVED__ "key";
constexpr const char RESERVED_AGGS_RANGES[]                 = RESERVED__ "ranges";
constexpr const char RESERVED_AGGS_SUM_OF_SQ[]              = RESERVED__ "sum_of_squares";
constexpr const char RESERVED_AGGS_TO[]                     = RESERVED__ "to";

constexpr const char RESERVED_AGGS_AVG[]                    = RESERVED__ "avg";
constexpr const char RESERVED_AGGS_CARDINALITY[]            = RESERVED__ "cardinality";
constexpr const char RESERVED_AGGS_COUNT[]                  = RESERVED__ "count";
constexpr const char RESERVED_AGGS_EXT_STATS[]              = RESERVED__ "extended_stats";
constexpr const char RESERVED_AGGS_GEO_BOUNDS[]             = RESERVED__ "geo_bounds";
constexpr const char RESERVED_AGGS_GEO_CENTROID[]           = RESERVED__ "geo_centroid";
constexpr const char RESERVED_AGGS_MAX[]                    = RESERVED__ "max";
constexpr const char RESERVED_AGGS_MEDIAN[]                 = RESERVED__ "median";
constexpr const char RESERVED_AGGS_MIN[]                    = RESERVED__ "min";
constexpr const char RESERVED_AGGS_MODE[]                   = RESERVED__ "mode";
constexpr const char RESERVED_AGGS_PERCENTILES[]            = RESERVED__ "percentiles";
constexpr const char RESERVED_AGGS_PERCENTILES_RANK[]       = RESERVED__ "percentiles_rank";
constexpr const char RESERVED_AGGS_SCRIPTED_METRIC[]        = RESERVED__ "scripted_metric";
constexpr const char RESERVED_AGGS_STATS[]                  = RESERVED__ "stats";
constexpr const char RESERVED_AGGS_STD[]                    = RESERVED__ "std_deviation";
constexpr const char RESERVED_AGGS_STD_BOUNDS[]             = RESERVED__ "std_deviation_bounds";
constexpr const char RESERVED_AGGS_SUM[]                    = RESERVED__ "sum";
constexpr const char RESERVED_AGGS_VARIANCE[]               = RESERVED__ "variance";

constexpr const char RESERVED_AGGS_DATE_HISTOGRAM[]         = RESERVED__ "date_histogram";
constexpr const char RESERVED_AGGS_DATE_RANGE[]             = RESERVED__ "date_range";
constexpr const char RESERVED_AGGS_FILTER[]                 = RESERVED__ "filter";
constexpr const char RESERVED_AGGS_GEO_DISTANCE[]           = RESERVED__ "geo_distance";
constexpr const char RESERVED_AGGS_GEO_IP[]                 = RESERVED__ "geo_ip";
constexpr const char RESERVED_AGGS_GEO_TRIXELS[]            = RESERVED__ "geo_trixels";
constexpr const char RESERVED_AGGS_HISTOGRAM[]              = RESERVED__ "histogram";
constexpr const char RESERVED_AGGS_IP_RANGE[]               = RESERVED__ "ip_range";
constexpr const char RESERVED_AGGS_MISSING[]                = RESERVED__ "missing";
constexpr const char RESERVED_AGGS_RANGE[]                  = RESERVED__ "range";
constexpr const char RESERVED_AGGS_VALUES[]                 = RESERVED__ "values";
constexpr const char RESERVED_AGGS_TERMS[]                  = RESERVED__ "terms";

constexpr const char RESERVED_AGGS_UPPER[]                  = RESERVED__ "upper";
constexpr const char RESERVED_AGGS_LOWER[]                  = RESERVED__ "lower";
constexpr const char RESERVED_AGGS_SIGMA[]                  = RESERVED__ "sigma";

constexpr const char RESERVED_AGGS_VALUE[]                  = RESERVED__ "_value";
constexpr const char RESERVED_AGGS_TERM[]                   = RESERVED__ "_term";
constexpr const char RESERVED_AGGS_SORT[]                   = RESERVED__ "_sort";
constexpr const char RESERVED_AGGS_ORDER[]                  = RESERVED__ "_order";
constexpr const char RESERVED_AGGS_MIN_DOC_COUNT[]          = RESERVED__ "_min_doc_count";
constexpr const char RESERVED_AGGS_LIMIT[]                  = RESERVED__ "_limit";
constexpr const char RESERVED_AGGS_KEYED[]                  = RESERVED__ "_keyed";


class Schema;


class BaseAggregation {
public:
	virtual ~BaseAggregation() = default;

	virtual void operator()(const Xapian::Document&) = 0;

	virtual void update() { }

	virtual MsgPack get_result() = 0;

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

	MsgPack get_result() override;

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
		_result[RESERVED_AGGS_AGGREGATIONS] = _aggregation.get_result();
		return _result;
	}
};
