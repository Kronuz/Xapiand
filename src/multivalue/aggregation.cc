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

#include "aggregation.h"

#include <stdexcept>                        // for out_of_range

#include "./aggregation_metric.h"           // for AGGREGATION_AVG, AGGREGAT...
#include "./exception.h"                    // for AggregationError, MSG_Agg...
#include "aggregation_bucket.h"             // for FilterAggregation, Histog...
#include "database_utils.h"                 // for is_valid
#include "msgpack.h"                        // for MsgPack, MsgPack::const_i...
#include "schema.h"                         // for Schema
#include "stl_serialise.h"                  // for StringList


const std::unordered_map<std::string, dispatch_aggregations> map_dispatch_aggregations({
	{ AGGREGATION_COUNT,            &Aggregation::add_metric<AGGREGATION_COUNT, MetricCount>                       },
	// { AGGREGATION_CARDINALITY,      &Aggregation::add_metric<AGGREGATION_CARDINALITY, MetricCardinality>           },
	{ AGGREGATION_SUM,              &Aggregation::add_metric<AGGREGATION_SUM, MetricSum>                           },
	{ AGGREGATION_AVG,              &Aggregation::add_metric<AGGREGATION_AVG, MetricAvg>                           },
	{ AGGREGATION_MIN,              &Aggregation::add_metric<AGGREGATION_MIN, MetricMin>                           },
	{ AGGREGATION_MAX,              &Aggregation::add_metric<AGGREGATION_MAX, MetricMax>                           },
	{ AGGREGATION_VARIANCE,         &Aggregation::add_metric<AGGREGATION_VARIANCE, MetricVariance>                 },
	{ AGGREGATION_STD,              &Aggregation::add_metric<AGGREGATION_STD, MetricSTD>                           },
	{ AGGREGATION_MEDIAN,           &Aggregation::add_metric<AGGREGATION_MEDIAN, MetricMedian>                     },
	{ AGGREGATION_MODE,             &Aggregation::add_metric<AGGREGATION_MODE, MetricMode>                         },
	{ AGGREGATION_STATS,            &Aggregation::add_metric<AGGREGATION_STATS, MetricStats>                       },
	{ AGGREGATION_EXT_STATS,        &Aggregation::add_metric<AGGREGATION_EXT_STATS, MetricExtendedStats>           },
	// { AGGREGATION_GEO_BOUNDS,       &Aggregation::add_metric<AGGREGATION_GEO_BOUNDS, MetricGeoBounds>              },
	// { AGGREGATION_GEO_CENTROID,     &Aggregation::add_metric<AGGREGATION_GEO_CENTROID, MetricGeoCentroid>          },
	// { AGGREGATION_PERCENTILES,      &Aggregation::add_metric<AGGREGATION_PERCENTILES, MetricPercentiles>           },
	// { AGGREGATION_PERCENTILES_RANK, &Aggregation::add_metric<AGGREGATION_PERCENTILES_RANK, MetricPercentilesRank>  },
	// { AGGREGATION_SCRIPTED_METRIC,  &Aggregation::add_metric<AGGREGATION_SCRIPTED_METRIC, MetricScripted>          },

	{ AGGREGATION_FILTER,           &Aggregation::add_bucket<FilterAggregation>                                    },
	{ AGGREGATION_VALUE,            &Aggregation::add_bucket<ValueAggregation>                                     },
	// { AGGREGATION_DATE_HISTOGRAM,   &Aggregation::add_bucket<DateHistogramAggregation>                             },
	// { AGGREGATION_DATE_RANGE,       &Aggregation::add_bucket<DateRangeAggregation>                                 },
	// { AGGREGATION_GEO_DISTANCE,     &Aggregation::add_bucket<GeoDistanceAggregation>                               },
	// { AGGREGATION_GEO_TRIXELS,      &Aggregation::add_bucket<GeoTrixelsAggregation>                                },
	{ AGGREGATION_HISTOGRAM,        &Aggregation::add_bucket<HistogramAggregation>                                 },
	// { AGGREGATION_MISSING,          &Aggregation::add_bucket<MissingAggregation>                                   },
	{ AGGREGATION_RANGE,            &Aggregation::add_bucket<RangeAggregation>                                     },
	// { AGGREGATION_IP_RANGE,         &Aggregation::add_bucket<IPRangeAggregation>                                   },
	// { AGGREGATION_GEO_IP,           &Aggregation::add_bucket<GeoIPAggregation>                                     },
});


Aggregation::Aggregation(MsgPack& result)
	: _result(result),
	  _doc_count(0)
{
	_result[AGGREGATION_DOC_COUNT] = _doc_count;  // Initialize here so it's at the start
}


Aggregation::Aggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
	: _result(result),
	  _doc_count(0)
{
	_result[AGGREGATION_DOC_COUNT] = _doc_count;  // Initialize here so it's at the start

	try {
		const auto& aggs = conf.at(AGGREGATION_AGGS);
		for (const auto& agg : aggs) {
			auto sub_agg_name = agg.as_string();
			if (is_valid(sub_agg_name)) {
				const auto& sub_agg = aggs.at(sub_agg_name);
				auto sub_agg_type = (*sub_agg.begin()).as_string();
				try {
					auto func = map_dispatch_aggregations.at(sub_agg_type);
					(this->*func)(_result[sub_agg_name], sub_agg, schema);
				} catch (const std::out_of_range&) {
					THROW(AggregationError, "Aggregation type: %s is not valid", sub_agg_name.c_str());
				}
			} else {
				THROW(AggregationError, "Aggregation sub_agg_name: %s is not valid", sub_agg_name.c_str());
			}
		}
	} catch (const msgpack::type_error) {
		THROW(AggregationError, "Aggregations must be an object");
	} catch (const std::out_of_range&) {
	}
}


void
Aggregation::operator()(const Xapian::Document& doc)
{
	++_doc_count;
	for (auto& sub_agg : _sub_aggregations) {
		(*sub_agg)(doc);
	}
};


void
Aggregation::update()
{
	for (auto& sub_agg : _sub_aggregations) {
		sub_agg->update();
	}
	_result[AGGREGATION_DOC_COUNT] = _doc_count;
}


void
AggregationMatchSpy::operator()(const Xapian::Document& doc, double)
{
	++_total;
	_aggregation(doc);
}


Xapian::MatchSpy*
AggregationMatchSpy::clone() const
{
	return new AggregationMatchSpy(_aggs, _schema);
}


std::string
AggregationMatchSpy::name() const
{
	return "AggregationMatchSpy";
}


std::string
AggregationMatchSpy::serialise() const
{
	StringList l;
	l.push_back(_aggs.serialise());
	l.push_back(_schema->get_const_schema()->serialise());
	return l.serialise();
}


Xapian::MatchSpy*
AggregationMatchSpy::unserialise(const std::string& s, const Xapian::Registry&) const
{
	StringList l;
	l.unserialise(s);
	std::shared_ptr<const MsgPack> internal_schema = std::make_shared<const MsgPack>(MsgPack::unserialise(l.at(1)));
	return new AggregationMatchSpy(MsgPack::unserialise(l.at(0)), std::make_shared<Schema>(internal_schema));
}


std::string
AggregationMatchSpy::get_description() const
{
	std::string desc("AggregationMatchSpy(");
	desc.append(_aggs.to_string()).push_back(')');
	return desc;
}
