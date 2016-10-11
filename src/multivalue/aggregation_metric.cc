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

#include "aggregation_metric.h"


static func_value_handle get_func_value_handle(FieldType type, const std::string& field_name) {
	switch (type) {
		case FieldType::FLOAT:
			return &SubAggregation::_aggregate_float;
		case FieldType::INTEGER:
			return &SubAggregation::_aggregate_integer;
		case FieldType::POSITIVE:
			return &SubAggregation::_aggregate_positive;
		case FieldType::DATE:
			return &SubAggregation::_aggregate_date;
		case FieldType::BOOLEAN:
			return &SubAggregation::_aggregate_boolean;
		case FieldType::STRING:
		case FieldType::TEXT:
			return &SubAggregation::_aggregate_string;
		case FieldType::GEO:
			return &SubAggregation::_aggregate_geo;
		case FieldType::UUID:
			return &SubAggregation::_aggregate_uuid;
		case FieldType::EMPTY:
			throw MSG_AggregationError("Field: %s has not been indexed", field_name.c_str());
		default:
			throw MSG_AggregationError("Type: '%c' is not supported", toUType(type));
	}
}


void
ValueHandle::operator()(SubAggregation* agg, const Xapian::Document& doc) const
{
	auto multiValues = doc.get_value(_slot);

	if (!multiValues.empty()) {
		(agg->*_func)(multiValues, doc);
	}
}


HandledSubAggregation::HandledSubAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
	: SubAggregation(result)
{
	try {
		const auto& agg = conf.at(AGGREGATION_FIELD);
		try {
			auto field_name = agg.as_string();
			auto field_spc = schema->get_slot_field(field_name);
			_handle.set(field_spc.slot, get_func_value_handle(field_spc.get_type(), field_name));
		}  catch (const msgpack::type_error&) {
			throw MSG_AggregationError("'%s' must be string", AGGREGATION_FIELD);
		}
	} catch (const std::out_of_range&) {
		throw MSG_AggregationError("'%s' must be specified in '%s'", AGGREGATION_FIELD, conf.to_string().c_str());
	} catch (const msgpack::type_error&) {
		throw MSG_AggregationError("'%s' must be object", conf.to_string().c_str());
	}
}
