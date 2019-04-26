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

#include "bucket.h"

#include <algorithm>                              // for std::move, std::set_intersection
#include <iterator>                               // for std::inserter
#include <set>                                    // for std::set

#include "aggregations.h"                         // for Aggregation
#include "database/schema.h"                      // for Schema, required_spc_t
#include "metrics/basic_string_metric.h"          // for Counter
#include "serialise.h"                            // for Serialise::MsgPack


FilterAggregation::FilterAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
	: _agg(context, schema)
{
	if (!context.is_map()) {
		THROW(AggregationError, "{} must be object", repr(context.to_string()));
	}

	const auto filter_it = context.find(name);
	if (filter_it == context.end()) {
		THROW(AggregationError, "'{}' must be specified in {}", name, repr(context.to_string()));
	}
	const auto& filter_conf = filter_it.value();
	if (!filter_conf.is_map()) {
		THROW(AggregationError, "{} must be object", repr(filter_conf.to_string()));
	}

	const auto term_filter_it = filter_conf.find(RESERVED_AGGS_TERM);
	if (term_filter_it == filter_conf.end()) {
		THROW(AggregationError, "'{}' must be specified in {}", RESERVED_AGGS_TERM, repr(filter_conf.to_string()));
	}
	const auto& term_filter_conf = term_filter_it.value();
	if (!term_filter_conf.is_map()) {
		THROW(AggregationError, "{} must be object", repr(term_filter_conf.to_string()));
	}

	const auto it = term_filter_conf.begin();
	const auto it_end = term_filter_conf.end();
	for (; it != it_end; ++it) {
		auto field_name = it->str_view();
		auto field_spc = schema->get_slot_field(field_name);
		const auto& values = it.value();
		std::set<std::string> s_values;
		if (values.is_array()) {
			for (const auto& value : values) {
				s_values.insert(Serialise::MsgPack(field_spc, value));
			}
			func = &FilterAggregation::check_multiple;
		} else {
			s_values.insert(Serialise::MsgPack(field_spc, values));
			func = &FilterAggregation::check_single;
		}
		_filters.emplace_back(field_spc.slot, std::move(s_values));
	}
}


void
FilterAggregation::update()
{
	return _agg.update();
}


MsgPack
FilterAggregation::get_result() const
{
	return _agg.get_result();
}


std::string
FilterAggregation::serialise_results() const
{
	return _agg.serialise_results();
}


void
FilterAggregation::merge_results(const char* p, const char* p_end)
{
	L_CALL("FilterAggregation::merge_results({})", repr(std::string(p, p_end - p)));

	_agg.merge_results(p, p_end);
}


void
FilterAggregation::check_single(const Xapian::Document& doc)
{
	for (const auto& filter : _filters) {
		std::set<std::string> values;
		StringList::unserialise(doc.get_value(filter.first), std::inserter(values, values.begin()));
		if (values.find(*filter.second.begin()) != values.end()) {
			return _agg(doc);
		}
	}
}


void
FilterAggregation::check_multiple(const Xapian::Document& doc)
{
	for (const auto& filter : _filters) {
		std::set<std::string> values;
		StringList::unserialise(doc.get_value(filter.first), std::inserter(values, values.begin()));
		Counter c;
		std::set_intersection(values.begin(), values.end(), filter.second.begin(), filter.second.end(), std::back_inserter(c));
		if (c.count != 0u) {
			return _agg(doc);
		}
	}
}
