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

#include "aggregation_bucket.h"

#include <algorithm>                      // for move, set_intersection
#include <iterator>                       // for back_insert_iterator, back_...
#include <map>                            // for __tree_const_iterator, oper...

#include "metrics/basic_string_metric.h"  // for Counter
#include "multivalue/aggregation.h"       // for Aggregation
#include "schema.h"                       // for Schema, required_spc_t


FilterAggregation::FilterAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
	: SubAggregation(result),
	  _agg(result, conf, schema)
{
	try {
		const auto& field_term = conf.at(AGGREGATION_FILTER).at(AGGREGATION_TERM);
		try {
			for (const auto& field : field_term) {
				auto field_name = field.str_view();
				auto field_spc = schema->get_slot_field(field_name);
				const auto& values = field_term.at(field_name);
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
		} catch (const msgpack::type_error&) {
			THROW(AggregationError, "'%s' must be object of objects", AGGREGATION_TERM);
		}
	} catch (const std::out_of_range&) {
		THROW(AggregationError, "'%s' must be specified must be specified in '%s'", AGGREGATION_TERM, AGGREGATION_FILTER);
	} catch (const msgpack::type_error&) {
		THROW(AggregationError, "'%s' must be object", AGGREGATION_FILTER);
	}
}


void
FilterAggregation::update()
{
	_agg.update();
}


void
FilterAggregation::check_single(const Xapian::Document& doc)
{
	for (const auto& filter : _filters) {
		std::unordered_set<std::string> values;
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
		if (c.count) {
			return _agg(doc);
		}
	}
}
