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

#include "aggregation_bucket.h"


FilterAggregation::FilterAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
	: SubAggregation(result),
	  _agg(result, conf, schema)
{
	try {
		const auto& field_term = conf.at(AGGREGATION_FILTER).at(AGGREGATION_TERM);
		try {
			for (const auto& field : field_term) {
				auto field_name = field.as_string();
				auto field_spc = schema->get_slot_field(field_name);
				const auto& values = field_term.at(field_name);
				StringSet s_values;
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
			throw MSG_AggregationError("'%s' must be object of objects", AGGREGATION_TERM);
		}
	} catch (const std::out_of_range&) {
		throw MSG_AggregationError("'%s' must be specified must be specified in '%s'", AGGREGATION_TERM, AGGREGATION_FILTER);
	} catch (const msgpack::type_error&) {
		throw MSG_AggregationError("'%s' must be object", AGGREGATION_FILTER);
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
	for (const auto&  filter : _filters) {
		StringUSet us;
		us.unserialise(doc.get_value(filter.first));
		if (us.find(*filter.second.begin()) != us.end()) {
			return _agg(doc);
		}
	}
}


void
FilterAggregation::check_multiple(const Xapian::Document& doc)
{
	for (const auto&  filter : _filters) {
		StringSet s;
		s.unserialise(doc.get_value(filter.first));
		Counter c;
		std::set_intersection(s.begin(), s.end(), filter.second.begin(), filter.second.end(), std::back_inserter(c));
		if (c.count) {
			return _agg(doc);
		}
	}
}
