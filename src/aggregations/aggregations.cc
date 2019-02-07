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

#include "aggregations.h"

#include <stdexcept>                        // for out_of_range

#include "bucket.h"                         // for FilterAggregation, Histog...
#include "database_utils.h"                 // for is_valid
#include "exception.h"                      // for AggregationError, MSG_Agg...
#include "metrics.h"                        // for RESERVED_AGGS_*
#include "msgpack.h"                        // for MsgPack, MsgPack::const_i...
#include "schema.h"                         // for Schema
#include "hashes.hh"                        // for fnv1ah32
#include "phf.hh"                           // for phf


Aggregation::Aggregation()
	: _doc_count{0},
	  value_ptr(nullptr),
	  slot{0.0},
	  idx{0}
{
}


Aggregation::Aggregation(const MsgPack& context, const std::shared_ptr<Schema>& schema)
	: _doc_count{0},
	  value_ptr(nullptr),
	  slot{0.0},
	  idx{0}
{
	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_AGGS_COUNT),
		// hh(RESERVED_AGGS_CARDINALITY),
		hh(RESERVED_AGGS_SUM),
		hh(RESERVED_AGGS_AVG),
		hh(RESERVED_AGGS_MIN),
		hh(RESERVED_AGGS_MAX),
		hh(RESERVED_AGGS_VARIANCE),
		hh(RESERVED_AGGS_STD),
		hh(RESERVED_AGGS_MEDIAN),
		hh(RESERVED_AGGS_MODE),
		hh(RESERVED_AGGS_STATS),
		hh(RESERVED_AGGS_EXT_STATS),
		// hh(RESERVED_AGGS_GEO_BOUNDS),
		// hh(RESERVED_AGGS_GEO_CENTROID),
		// hh(RESERVED_AGGS_PERCENTILES),
		// hh(RESERVED_AGGS_PERCENTILES_RANK),
		// hh(RESERVED_AGGS_SCRIPTED_METRIC),
		hh(RESERVED_AGGS_FILTER),
		hh(RESERVED_AGGS_VALUES),
		hh(RESERVED_AGGS_VALUE),
		hh(RESERVED_AGGS_TERMS),
		hh(RESERVED_AGGS_TERM),
		// hh(RESERVED_AGGS_DATE_HISTOGRAM),
		// hh(RESERVED_AGGS_DATE_RANGE),
		// hh(RESERVED_AGGS_GEO_DISTANCE),
		// hh(RESERVED_AGGS_GEO_TRIXELS),
		hh(RESERVED_AGGS_HISTOGRAM),
		// hh(RESERVED_AGGS_MISSING),
		hh(RESERVED_AGGS_RANGE),
		// hh(RESERVED_AGGS_IP_RANGE),
		// hh(RESERVED_AGGS_GEO_IP),
	});

	auto aggs_it = context.find(RESERVED_AGGS_AGGREGATIONS);
	if (aggs_it == context.end()) {
		aggs_it = context.find(RESERVED_AGGS_AGGS);
	}
	if (aggs_it != context.end()) {
		const auto& aggs = aggs_it.value();
		if (!aggs.is_map()) {
			THROW(AggregationError, "'%s' must be an object", RESERVED_AGGS_AGGREGATIONS);
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
					case _.fhh(RESERVED_AGGS_COUNT):
						add_metric<MetricCount>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(RESERVED_AGGS_CARDINALITY):
					// 	add_metric<MetricCardinality>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(RESERVED_AGGS_SUM):
						add_metric<MetricSum>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_AVG):
						add_metric<MetricAvg>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_MIN):
						add_metric<MetricMin>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_MAX):
						add_metric<MetricMax>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_VARIANCE):
						add_metric<MetricVariance>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_STD):
						add_metric<MetricStdDeviation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_MEDIAN):
						add_metric<MetricMedian>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_MODE):
						add_metric<MetricMode>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_STATS):
						add_metric<MetricStats>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_EXT_STATS):
						add_metric<MetricExtendedStats>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(RESERVED_AGGS_GEO_BOUNDS):
					// 	add_metric<MetricGeoBounds>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_GEO_CENTROID):
					// 	add_metric<MetricGeoCentroid>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_PERCENTILES):
					// 	add_metric<MetricPercentiles>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_PERCENTILES_RANK):
					// 	add_metric<MetricPercentilesRank>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_SCRIPTED_METRIC):
					// 	add_metric<MetricScripted>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;

					case _.fhh(RESERVED_AGGS_FILTER):
						add_bucket<FilterAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_VALUE):
						L_WARNING_ONCE("Aggregation '%s' has been deprecated, use '%s' instead", RESERVED_AGGS_VALUE, RESERVED_AGGS_VALUES);
					case _.fhh(RESERVED_AGGS_VALUES):
						add_bucket<ValuesAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					case _.fhh(RESERVED_AGGS_TERM):
						L_WARNING_ONCE("Aggregation '%s' has been deprecated, use '%s' instead", RESERVED_AGGS_TERM, RESERVED_AGGS_TERMS);
					case _.fhh(RESERVED_AGGS_TERMS):
						add_bucket<TermsAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(RESERVED_AGGS_DATE_HISTOGRAM):
					// 	add_bucket<DateHistogramAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_DATE_RANGE):
					// 	add_bucket<DateRangeAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_GEO_DISTANCE):
					// 	add_bucket<GeoDistanceAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_GEO_TRIXELS):
					// 	add_bucket<GeoTrixelsAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(RESERVED_AGGS_HISTOGRAM):
						add_bucket<HistogramAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(RESERVED_AGGS_MISSING):
					// 	add_bucket<MissingAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					case _.fhh(RESERVED_AGGS_RANGE):
						add_bucket<RangeAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
						break;
					// case _.fhh(RESERVED_AGGS_IP_RANGE):
					// 	add_bucket<IPRangeAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
					// 	break;
					// case _.fhh(RESERVED_AGGS_GEO_IP):
					// 	add_bucket<GeoIPAggregation>(sub_agg_name, sub_agg, sub_agg_type, schema);
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
	for (auto& sub_agg : _sub_aggs) {
		(*sub_agg.second)(doc);
	}
}


void
Aggregation::update()
{
	for (auto& sub_agg : _sub_aggs) {
		sub_agg.second->update();
	}
}


MsgPack
Aggregation::get_result()
{
	MsgPack result = {
		{ RESERVED_AGGS_DOC_COUNT, _doc_count },
	};
	for (auto& sub_agg : _sub_aggs) {
		result[sub_agg.first] = sub_agg.second->get_result();
	}
	return result;
}


BaseAggregation*
Aggregation::get_agg(std::string_view field)
{
	auto it = _sub_aggs.find(field);  // FIXME: This copies bucket as std::map cannot find std::string_view directly!
	if (it != _sub_aggs.end()) {
		return it->second.get();
	}
	return nullptr;
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
