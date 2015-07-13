/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "fields.h"
#include <xapian/query.h>


NumericFieldProcessor::NumericFieldProcessor(const std::string &prefix_): prefix(prefix_) {}


Xapian::Query
NumericFieldProcessor::operator()(const std::string &str)
{
	// For negative number, we receive _# and we serialise -#.
	std::string serialise(str.c_str());
	if (serialise.at(0) == '_') serialise.at(0) = '-';
	LOG(this, "Numeric FP %s!!\n", serialise.c_str());
	serialise = serialise_numeric(serialise);
	if (serialise.size() == 0) {
		throw Xapian::QueryParserError("Didn't understand numeric specification '" + str + "'");
	}
	return Xapian::Query(prefix + serialise);
}


BooleanFieldProcessor::BooleanFieldProcessor(const std::string &prefix_): prefix(prefix_) {}


Xapian::Query BooleanFieldProcessor::operator()(const std::string &str)
{
	LOG(this, "Boolean FP!!\n");
	std::string serialise = serialise_bool(str);
	if (serialise.size() == 0) {
		throw Xapian::QueryParserError("Didn't understand bool specification '" + str + "'");
	}
	return Xapian::Query(prefix + serialise);
}


DateFieldProcessor::DateFieldProcessor(const std::string &prefix_): prefix(prefix_) {}


Xapian::Query DateFieldProcessor::operator()(const std::string &str)
{
	std::string serialise(str.c_str());
	if (serialise.at(0) == '_') serialise.at(0) = '-';
	LOG(this, "Date FP %s!!\n", serialise.c_str());
	serialise = serialise_date(serialise);
	if (serialise.size() == 0) {
		throw Xapian::QueryParserError("Didn't understand date specification '" + str + "'");
	}
	return Xapian::Query(prefix + serialise);
}


DateTimeValueRangeProcessor::DateTimeValueRangeProcessor(Xapian::valueno slot_, const std::string &prefix_): valno(slot_), prefix(prefix_) {}


Xapian::valueno
DateTimeValueRangeProcessor::operator()(std::string &begin, std::string &end)
{
	std::string buf;
	LOG(this, "Inside of DateTimeValueRangeProcessor\n");

	// Verify the prefix.
	if (std::string(begin.c_str(), 0, prefix.size()).compare(prefix) == 0) {
		begin.assign(begin.c_str(), prefix.size(), begin.size());
	} else return valno;

	if (begin.size() != 0) {
		std::string serialise = serialise_date(begin);
		printf("Begin: %s End: %s\n", begin.c_str(), end.c_str());
		if (serialise.size() == 0) return Xapian::BAD_VALUENO;
		buf = prefix + serialise;
		begin.assign(buf.c_str(), buf.size());
		LOG(this, "Serialise of begin %s\n", buf.c_str());
	}
	buf = "";

	if (end.size() != 0) {
		std::string serialise = serialise_date(end);
		if (serialise.size() == 0) return Xapian::BAD_VALUENO;
		buf = serialise;
		end.assign(buf.c_str(), buf.size());
		LOG(this, "Serialise of end %s\n", buf.c_str());
	}
	LOG(this, "DateTimeValueRangeProcessor process\n");

	return valno;
}