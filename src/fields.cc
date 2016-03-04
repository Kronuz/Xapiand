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
#include "multivalue.h"
#include "wkt_parser.h"
#include "serialise.h"

#include <xapian/query.h>


NumericFieldProcessor::NumericFieldProcessor(const std::string& prefix_) : prefix(prefix_) { }


Xapian::Query
NumericFieldProcessor::operator()(const std::string& str)
{
	// For negative number, we receive _# and we serialise -#.
	std::string serialise(str);
	if (serialise[0] == '_') serialise[0] = '-';
	try {
		serialise = Serialise::numeric(serialise);
		return Xapian::Query(prefix + serialise);
	} catch (const SerialisationError& exc) {
		throw MSG_QueryParserError(std::string(exc.what()) + " (" + str + ")");
	}
}


BooleanFieldProcessor::BooleanFieldProcessor(const std::string& prefix_) : prefix(prefix_) { }


Xapian::Query
BooleanFieldProcessor::operator()(const std::string& str)
{
	try {
		std::string serialise = Serialise::boolean(str);
		return Xapian::Query(prefix + serialise);
	} catch (const SerialisationError& exc) {
		throw MSG_QueryParserError(std::string(exc.what()) + " (" + str + ")");
	}
}


DateFieldProcessor::DateFieldProcessor(const std::string& prefix_) : prefix(prefix_) { }


Xapian::Query
DateFieldProcessor::operator()(const std::string& str)
{
	std::string serialise(str.c_str());
	if (serialise[0] == '_') serialise[0] = '-';
	try {
		serialise = Serialise::date(serialise);
		return Xapian::Query(prefix + serialise);
	} catch (const DatetimeError& exc) {
		throw MSG_QueryParserError("Format date is not valid (" + str + "). " + std::string(exc.what()));
	}
}


GeoFieldProcessor::GeoFieldProcessor(const std::string& prefix_) : prefix(prefix_) { }


Xapian::Query
GeoFieldProcessor::operator()(const std::string& str)
{
	std::string serialise(Serialise::trixel_id(std::stoull(str)));
	return Xapian::Query(prefix + serialise);
}
