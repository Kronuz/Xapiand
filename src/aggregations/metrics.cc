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

#include "metrics.h"

#include "database/schema.h"       // for FieldType, required_spc_t, FieldTy...
#include "exception.h"             // for AggregationError, MSG_AggregationE...
#include "msgpack/object_fwd.hpp"  // for type_error
#include "repr.hh"                 // for repr
#include "strings.hh"              // for strings::startswith
#include "utype.hh"                // for toUType


template <typename Handler>
static auto
get_func_value_handle(FieldType type, std::string_view field_name)
{
	switch (type) {
		case FieldType::floating:
			return &HandledSubAggregation<Handler>::_aggregate_float;
		case FieldType::integer:
			return &HandledSubAggregation<Handler>::_aggregate_integer;
		case FieldType::positive:
			return &HandledSubAggregation<Handler>::_aggregate_positive;
		case FieldType::date:
		case FieldType::datetime:
			return &HandledSubAggregation<Handler>::_aggregate_date;
		case FieldType::time:
			return &HandledSubAggregation<Handler>::_aggregate_time;
		case FieldType::timedelta:
			return &HandledSubAggregation<Handler>::_aggregate_timedelta;
		case FieldType::boolean:
			return &HandledSubAggregation<Handler>::_aggregate_boolean;
		case FieldType::keyword:
		case FieldType::text:
		case FieldType::string:
			return &HandledSubAggregation<Handler>::_aggregate_string;
		case FieldType::geo:
			return &HandledSubAggregation<Handler>::_aggregate_geo;
		case FieldType::uuid:
			return &HandledSubAggregation<Handler>::_aggregate_uuid;
		case FieldType::empty:
			THROW(AggregationError, "Field: {} has not been indexed", repr(field_name));
		default:
			THROW(AggregationError, "Type: '{}' is not supported", toUType(type));
	}
}


ValuesHandler::ValuesHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema) {
	if (!conf.is_map()) {
		THROW(AggregationError, "{} must be object", repr(conf.to_string()));
	}
	const auto field_it = conf.find(RESERVED_AGGS_FIELD);
	if (field_it == conf.end()) {
		THROW(AggregationError, "'{}' must be specified in {}", RESERVED_AGGS_FIELD, repr(conf.to_string()));
	}
	const auto& field_conf = field_it.value();
	if (!field_conf.is_string()) {
		THROW(AggregationError, "'{}' must be string", RESERVED_AGGS_FIELD);
	}
	auto field_name = field_conf.str_view();
	auto field_spc = schema->get_slot_field(field_name);

	_type = field_spc.get_type();
	_slot = field_spc.slot;
	_func = get_func_value_handle<ValuesHandler>(_type, field_name);
}


std::vector<std::string>
ValuesHandler::values(const Xapian::Document& doc) const
{
	std::vector<std::string> values;
	auto doc_value = doc.get_value(_slot);
	for (const auto& value : StringList(doc_value)) {
		values.push_back(std::string(value));
	}
	return values;
}


TermsHandler::TermsHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema) {
	if (!conf.is_map()) {
		THROW(AggregationError, "{} must be object", repr(conf.to_string()));
	}
	const auto field_it = conf.find(RESERVED_AGGS_FIELD);
	if (field_it == conf.end()) {
		THROW(AggregationError, "'{}' must be specified in {}", RESERVED_AGGS_FIELD, repr(conf.to_string()));
	}
	const auto& field_conf = field_it.value();
	if (!field_conf.is_string()) {
		THROW(AggregationError, "'{}' must be string", RESERVED_AGGS_FIELD);
	}
	auto field_name = field_conf.str_view();
	auto field_spc = schema->get_data_field(field_name).first;

	_type = field_spc.get_type();
	_prefix = field_spc.prefix();
	_func = get_func_value_handle<TermsHandler>(_type, field_name);
}


std::vector<std::string>
TermsHandler::values(const Xapian::Document& doc) const
{
	std::vector<std::string> values;

	auto it = doc.termlist_begin();
	it.skip_to(_prefix);
	auto it_e = doc.termlist_end();
	for (; it != it_e; ++it) {
		const auto& term = *it;
		if (strings::startswith(term, _prefix)) {
			if (term.size() > _prefix.size() + 1) {
				values.push_back(term.substr(_prefix.size() + 1));
			}
		} else {
			break;
		}
	}
	return values;
}
