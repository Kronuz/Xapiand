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

#pragma once

#include "aggregation.h"
#include "string_metric.h"


class ValueAggregation : public HandledSubAggregation {
protected:
	const MsgPack _conf;
	const std::shared_ptr<Schema> _schema;

	std::unordered_map<std::string, Aggregation> _aggs;

public:
	ValueAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema)
		: HandledSubAggregation(result, conf.at(AGGREGATION_VALUE), schema),
		  _conf(conf),
		  _schema(schema)
		{ }

	void update() override {
		for (auto& _agg : _aggs) {
			_agg.second.update();
		}
	}

	void _aggregate(const std::string& value, const Xapian::Document& doc) {
		try {
			_aggs.at(value)(doc);
		} catch (const std::out_of_range&) {
			auto p = _aggs.emplace(std::piecewise_construct, std::forward_as_tuple(value), std::forward_as_tuple(_result[value], _conf, _schema));
			p.first->second(doc);
		}
	}

	void aggregate_float(double value, const Xapian::Document& doc) override {
		_aggregate(std::to_string(value), doc);
	}

	void aggregate_integer(long value, const Xapian::Document& doc) override {
		_aggregate(std::to_string(value), doc);
	}

	void aggregate_positive(unsigned long value, const Xapian::Document& doc) override {
		_aggregate(std::to_string(value), doc);
	}

	void aggregate_date(double value, const Xapian::Document& doc) override {
		_aggregate(std::to_string(value), doc);
	}

	void aggregate_boolean(bool value, const Xapian::Document& doc) override {
		_aggregate(std::to_string(value), doc);
	}

	void aggregate_string(const std::string& value, const Xapian::Document& doc) override {
		_aggregate(value, doc);
	}

	// void aggregate_geo(const std::pair<std::string, std::string>& value, const Xapian::Document& doc) override {
	// }

	// void aggregate_uuid(const std::string& value, const Xapian::Document& doc) override {
	// }
};


class FilterAggregation : public SubAggregation {
	using func_filter = void (FilterAggregation::*)(const Xapian::Document&);

	std::vector<std::pair<Xapian::valueno, StringSet>> _filters;
	Aggregation _agg;
	func_filter func;

public:
	FilterAggregation(MsgPack& result, const MsgPack& conf, const std::shared_ptr<Schema>& schema);

	inline void operator()(const Xapian::Document& doc) override {
		(this->*func)(doc);
	}

	void update() override;

	void check_single(const Xapian::Document& doc);
	void check_multiple(const Xapian::Document& doc);
};
