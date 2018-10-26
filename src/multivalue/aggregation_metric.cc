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

#include "aggregation_metric.h"

#include "msgpack/object_fwd.hpp"  // for type_error
#include "multivalue/exception.h"  // for AggregationError, MSG_AggregationE...
#include "repr.hh"                 // for repr
#include "schema.h"                // for FieldType, required_spc_t, FieldTy...
#include "utype.hh"                // for toUType


static func_value_handle get_func_value_handle(FieldType type, std::string_view field_name) {
	switch (type) {
		case FieldType::FLOAT:
			return &SubAggregation::_aggregate_float;
		case FieldType::INTEGER:
			return &SubAggregation::_aggregate_integer;
		case FieldType::POSITIVE:
			return &SubAggregation::_aggregate_positive;
		case FieldType::DATE:
			return &SubAggregation::_aggregate_date;
		case FieldType::TIME:
			return &SubAggregation::_aggregate_time;
		case FieldType::TIMEDELTA:
			return &SubAggregation::_aggregate_timedelta;
		case FieldType::BOOLEAN:
			return &SubAggregation::_aggregate_boolean;
		case FieldType::KEYWORD:
		case FieldType::TEXT:
		case FieldType::STRING:
			return &SubAggregation::_aggregate_string;
		case FieldType::GEO:
			return &SubAggregation::_aggregate_geo;
		case FieldType::UUID:
			return &SubAggregation::_aggregate_uuid;
		case FieldType::EMPTY:
			THROW(AggregationError, "Field: %s has not been indexed", repr(field_name));
		default:
			THROW(AggregationError, "Type: '%c' is not supported", toUType(type));
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
	if (!conf.is_map()) {
		THROW(AggregationError, "%s must be object", repr(conf.to_string()));
	}
	const auto field_it = conf.find(AGGREGATION_FIELD);
	if (field_it == conf.end()) {
		THROW(AggregationError, "'%s' must be specified in %s", AGGREGATION_FIELD, repr(conf.to_string()));
	}
	const auto& field_conf = field_it.value();
	if (!field_conf.is_string()) {
		THROW(AggregationError, "'%s' must be string", AGGREGATION_FIELD);
	}
	auto field_name = field_conf.str_view();
	auto field_spc = schema->get_slot_field(field_name);
	_handle.set(field_spc.slot, get_func_value_handle(field_spc.get_type(), field_name));
}
