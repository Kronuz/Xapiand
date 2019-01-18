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

#include "aggregation.h"

#include <stdexcept>                        // for out_of_range

#include "aggregation_bucket.h"             // for FilterAggregation, Histog...
#include "aggregation_metric.h"             // for AGGREGATION_AVG, AGGREGAT...
#include "database_utils.h"                 // for is_valid
#include "exception.h"                      // for AggregationError, MSG_Agg...
#include "msgpack.h"                        // for MsgPack, MsgPack::const_i...
#include "schema.h"                         // for Schema
#include "hashes.hh"                        // for fnv1ah32


Aggregation::Aggregation(MsgPack& result)
	: _result(result),
	  _doc_count(0)
{
	_result[AGGREGATION_DOC_COUNT] = _doc_count;  // Initialize here so it's at the start
}


Aggregation::Aggregation(MsgPack& result, const MsgPack& context, const std::shared_ptr<Schema>& schema)
	: _result(result),
	  _doc_count(0)
{
	constexpr static auto _ = phf::make_phf({
		hh(AGGREGATION_COUNT),
		// hh(AGGREGATION_CARDINALITY),
		hh(AGGREGATION_SUM),
		hh(AGGREGATION_AVG),
		hh(AGGREGATION_MIN),
		hh(AGGREGATION_MAX),
		hh(AGGREGATION_VARIANCE),
		hh(AGGREGATION_STD),
		hh(AGGREGATION_MEDIAN),
		hh(AGGREGATION_MODE),
		hh(AGGREGATION_STATS),
		hh(AGGREGATION_EXT_STATS),
		// hh(AGGREGATION_GEO_BOUNDS),
		// hh(AGGREGATION_GEO_CENTROID),
		// hh(AGGREGATION_PERCENTILES),
		// hh(AGGREGATION_PERCENTILES_RANK),
		// hh(AGGREGATION_SCRIPTED_METRIC),
		hh(AGGREGATION_FILTER),
		hh(AGGREGATION_VALUES),
		hh(AGGREGATION_VALUE),
		hh(AGGREGATION_TERMS),
		hh(AGGREGATION_TERM),
		// hh(AGGREGATION_DATE_HISTOGRAM),
		// hh(AGGREGATION_DATE_RANGE),
		// hh(AGGREGATION_GEO_DISTANCE),
		// hh(AGGREGATION_GEO_TRIXELS),
		hh(AGGREGATION_HISTOGRAM),
		// hh(AGGREGATION_MISSING),
		hh(AGGREGATION_RANGE),
		// hh(AGGREGATION_IP_RANGE),
		// hh(AGGREGATION_GEO_IP),
	});

	_result[AGGREGATION_DOC_COUNT] = _doc_count;  // Initialize here so it's at the start

	auto aggs_it = context.find(AGGREGATION_AGGREGATIONS);
	if (aggs_it == context.end()) {
		aggs_it = context.find(AGGREGATION_AGGS);
	}
	if (aggs_it != context.end()) {
		const auto& aggs = aggs_it.value();
		if (!aggs.is_map()) {
			THROW(AggregationError, "'%s' must be an object", AGGREGATION_AGGREGATIONS);
		}
		const auto it = aggs.begin();
		const auto it_end = aggs.end();
		for (; it != it_end; ++it) {
			auto sub_agg_name = it->str_view();
			if (is_valid(sub_agg_name)) {
				const auto& sub_agg = it.value();
				if (!sub_agg.is_map()) {
					THROW(AggregationError, "All aggregations must be objects");
				}
				auto sub_agg_type = sub_agg.begin()->str_view();
				switch (_.fhh(sub_agg_type)) {
					case _.fhh(AGGREGATION_COUNT):
						add_metric<MetricCount>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(AGGREGATION_CARDINALITY):
					// 	add_metric<MetricCardinality>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(AGGREGATION_SUM):
						add_metric<MetricSum>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_AVG):
						add_metric<MetricAvg>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_MIN):
						add_metric<MetricMin>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_MAX):
						add_metric<MetricMax>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_VARIANCE):
						add_metric<MetricVariance>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_STD):
						add_metric<MetricSTD>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_MEDIAN):
						add_metric<MetricMedian>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_MODE):
						add_metric<MetricMode>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_STATS):
						add_metric<MetricStats>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_EXT_STATS):
						add_metric<MetricExtendedStats>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(AGGREGATION_GEO_BOUNDS):
					// 	add_metric<MetricGeoBounds>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_GEO_CENTROID):
					// 	add_metric<MetricGeoCentroid>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_PERCENTILES):
					// 	add_metric<MetricPercentiles>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_PERCENTILES_RANK):
					// 	add_metric<MetricPercentilesRank>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_SCRIPTED_METRIC):
					// 	add_metric<MetricScripted>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;

					case _.fhh(AGGREGATION_FILTER):
						add_bucket<FilterAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_VALUE):
						L_WARNING_ONCE("Aggregation '%s' has been deprecated, use '%s' instead", AGGREGATION_VALUE, AGGREGATION_VALUES);
					case _.fhh(AGGREGATION_VALUES):
						add_bucket<ValuesAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(AGGREGATION_TERM):
						L_WARNING_ONCE("Aggregation '%s' has been deprecated, use '%s' instead", AGGREGATION_TERM, AGGREGATION_TERMS);
					case _.fhh(AGGREGATION_TERMS):
						add_bucket<TermsAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(AGGREGATION_DATE_HISTOGRAM):
					// 	add_bucket<DateHistogramAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_DATE_RANGE):
					// 	add_bucket<DateRangeAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_GEO_DISTANCE):
					// 	add_bucket<GeoDistanceAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_GEO_TRIXELS):
					// 	add_bucket<GeoTrixelsAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(AGGREGATION_HISTOGRAM):
						add_bucket<HistogramAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(AGGREGATION_MISSING):
					// 	add_bucket<MissingAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(AGGREGATION_RANGE):
						add_bucket<RangeAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(AGGREGATION_IP_RANGE):
					// 	add_bucket<IPRangeAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(AGGREGATION_GEO_IP):
					// 	add_bucket<GeoIPAggregation>(_result.put(sub_agg_name, MsgPack(MsgPack::Type::MAP)), sub_agg, sub_agg_type, schema);
					// 	break;
					default:
						THROW(AggregationError, "Aggregation type %s is not valid for %s", repr(sub_agg_type), repr(sub_agg_name));
				}
			} else {
				THROW(AggregationError, "Aggregation name %s is not valid", repr(sub_agg_name));
			}
		}
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
AggregationMatchSpy::operator()(const Xapian::Document& doc, double /*wt*/)
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
	std::vector<std::string> data = { _aggs.serialise(), _schema->get_const_schema()->serialise() };
	return StringList::serialise(data.begin(), data.end());
}


Xapian::MatchSpy*
AggregationMatchSpy::unserialise(const std::string& serialised, const Xapian::Registry& /*context*/) const
{
	try {
		StringList data(serialised);

		if (data.size() != 2) {
			throw Xapian::NetworkError("Bad serialised AggregationMatchSpy");
		}

		auto it = data.begin();
		return new AggregationMatchSpy(MsgPack::unserialise(*it), std::make_shared<Schema>(std::make_shared<const MsgPack>(MsgPack::unserialise(*++it)), nullptr, ""));
	} catch (const SerialisationError&) {
		throw Xapian::NetworkError("Bad serialised AggregationMatchSpy");
	}
}


std::string
AggregationMatchSpy::get_description() const
{
	std::string desc("AggregationMatchSpy(");
	desc.append(_aggs.to_string()).push_back(')');
	return desc;
}
