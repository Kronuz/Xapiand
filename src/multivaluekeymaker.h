/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "datetime.h"
#include "stl_serialise.h"
#include "utils.h"
#include "wkt_parser.h"

#include <float.h>


const std::string MAX_CMPVALUE(Xapian::sortable_serialise(DBL_MAX));
const std::string STR_FOR_EMPTY("\xff");

// Vector of slots
struct key_values_t {
	Xapian::valueno slot;
	char type;
	double valuenumeric;
	std::string valuestring;
	CartesianUSet valuegeo;
	bool reverse;
	bool hasValue;
};


/*
 * KeyMaker subclass which combines several Multivalues.
 * This class only is used for sorting, there are two cases:
 * Case 1: Ascending (include in the query -> sort:+field_name or sort:field_name), of all values is selected the smallest.
 * Case 2: Descending (include in the query -> sort:-field_name), of all values is selected the largest.

 * For collapsing, it is used Xapian::MultiValueKeyMaker.
 */
class Multi_MultiValueKeyMaker : public Xapian::KeyMaker {
	// Vector of slots
	std::vector<key_values_t> slots;

public:
	Multi_MultiValueKeyMaker() = default;

	template <class Iterator>
	Multi_MultiValueKeyMaker(Iterator begin, Iterator end) {
		while (begin != end) add_value(*begin++);
	}

	virtual std::string operator()(const Xapian::Document& doc) const override;

	void add_value(Xapian::valueno slot, char type, const std::string& value, bool reverse = false) {
		if (!value.empty()) {
			key_values_t ins_key = { slot, type, 0, "", CartesianUSet(), reverse, true };
			switch (type) {
				case FLOAT_TYPE:
						ins_key.valuenumeric = strict(std::stod, value);
					break;
				case INTEGER_TYPE:
					ins_key.valuenumeric = strict(std::stoll, value);
					break;
				case POSITIVE_TYPE:
					ins_key.valuenumeric = strict(std::stoull, value);
					break;
				case DATE_TYPE:
					ins_key.valuenumeric = Datetime::timestamp(value);
					break;
				case BOOLEAN_TYPE:
					ins_key.valuestring = Serialise::boolean(value);
					break;
				case STRING_TYPE:
					ins_key.valuestring = value;
					break;
				case GEO_TYPE:
					RangeList ranges;
					EWKT_Parser::getRanges(value, true, HTM_MIN_ERROR, ranges, ins_key.valuegeo);
					break;
			}
			slots.push_back(std::move(ins_key));
		} else if (type != GEO_TYPE) {
			slots.push_back({ slot, type, 0, value, CartesianUSet(), reverse, false });
		}
	}
};
