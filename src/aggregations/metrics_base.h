/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Aggregation base for metrics: value and term handlers, and HandledSubAggregation.

#include <algorithm>                // for std::nth_element, std::max_element
#include <cmath>                    // for sqrt
#include <cstdio>
#include <cstring>                  // for size_t
#include <limits>                   // for std::numeric_limits
#include <map>                      // for std::map
#include <memory>                   // for std::shared_ptr
#include <string>                   // for std::string
#include <string_view>              // for std::string_view
#include <utility>                  // for std::pair
#include <vector>                   // for std::vector

#include "aggregations.h"           // for BaseAggregation
#include "exception.h"              // for AggregationError, MSG_AggregationError
#include "log.h"                    // for L_CALL (was transitive via htm.h -> strings.hh)
#include "msgpack.h"                // for MsgPack, object::object
#include "repr.hh"                  // for repr (was transitive via htm.h -> strings.hh)
#include "reserved/aggregations.h"  // for RESERVED_AGGS_*
#include "serialize_list.h"         // for StringList, RangeList
#include "engine/codec.h"          // for serialize_double, unserialize_double
#include "engine/doc_view.h"       // for DocView
#include "engine/types.h"          // for valueno


class Schema;


template <typename Handler>
class HandledSubAggregation;


class ValuesHandler {
	using func_value_handle = void (HandledSubAggregation<ValuesHandler>::*)(const engine::DocView&);

protected:
	FieldType _type;
	engine::valueno _slot;
	func_value_handle _func;

public:
	ValuesHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	std::vector<std::string> values(const engine::DocView& doc) const;

	FieldType get_type() {
		return _type;
	}

	void operator()(HandledSubAggregation<ValuesHandler>* agg, const engine::DocView& doc) const {
		(agg->*_func)(doc);
	}
};


class TermsHandler {
	using func_value_handle = void (HandledSubAggregation<TermsHandler>::*)(const engine::DocView&);

protected:
	FieldType _type;
	engine::valueno _slot;
	func_value_handle _func;

public:
	TermsHandler(const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	std::vector<std::string> values(const engine::DocView& doc) const;

	FieldType get_type() {
		return _type;
	}

	void operator()(HandledSubAggregation<TermsHandler>* agg, const engine::DocView& doc) const {
		(agg->*_func)(doc);
	}
};


template <typename Handler>
class HandledSubAggregation : public BaseAggregation {
protected:
	Handler _handler;
	const MsgPack& _conf;

public:
	HandledSubAggregation(const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: _handler(conf, schema),
		  _conf(conf) { }

	HandledSubAggregation(const MsgPack& context, std::string_view name, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation(context.at(name), schema) { }

	virtual void aggregate_float(long double, const engine::DocView&) {
		THROW(AggregationError, "float type is not supported");
	}

	virtual void aggregate_integer(int64_t, const engine::DocView&) {
		THROW(AggregationError, "integer type is not supported");
	}

	virtual void aggregate_positive(uint64_t, const engine::DocView&) {
		THROW(AggregationError, "positive type is not supported");
	}

	virtual void aggregate_date(double, const engine::DocView&) {
		THROW(AggregationError, "date type is not supported");
	}

	virtual void aggregate_time(double, const engine::DocView&) {
		THROW(AggregationError, "time type is not supported");
	}

	virtual void aggregate_timedelta(double, const engine::DocView&) {
		THROW(AggregationError, "timedelta type is not supported");
	}

	virtual void aggregate_boolean(bool, const engine::DocView&) {
		THROW(AggregationError, "boolean type is not supported");
	}

	virtual void aggregate_string(std::string_view, const engine::DocView&) {
		THROW(AggregationError, "string type is not supported");
	}

	virtual void aggregate_geo(const range_t&, const engine::DocView&) {
		THROW(AggregationError, "geo type is not supported");
	}

	virtual void aggregate_uuid(std::string_view, const engine::DocView&) {
		THROW(AggregationError, "uuid type is not supported");
	}

	void _aggregate_float(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_float(Unserialize::floating(value), doc);
		}
	}

	void _aggregate_integer(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_integer(Unserialize::integer(value), doc);
		}
	}

	void _aggregate_positive(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_positive(Unserialize::positive(value), doc);
		}
	}

	void _aggregate_date(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_date(Unserialize::timestamp(value), doc);
		}
	}

	void _aggregate_time(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_time(Unserialize::time_d(value), doc);
		}
	}

	void _aggregate_timedelta(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_timedelta(Unserialize::timedelta_d(value), doc);
		}
	}

	void _aggregate_boolean(const engine::DocView& doc)  {
		for (const auto& value : _handler.values(doc)) {
			aggregate_boolean(Unserialize::boolean(value), doc);
		}
	}

	void _aggregate_string(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_string(value, doc);
		}
	}

	void _aggregate_geo(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			for (const auto& range : RangeList(value)) {
				aggregate_geo(range, doc);
			}
		}
	}

	void _aggregate_uuid(const engine::DocView& doc) {
		for (const auto& value : _handler.values(doc)) {
			aggregate_uuid(Unserialize::uuid(value), doc);
		}
	}

	void _aggregate_nothing(const engine::DocView&) {
		/* do nothing */
	}

	void operator()(const engine::DocView& doc) override {
		_handler(this, doc);
	}
};
