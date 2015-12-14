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

#pragma once

#include "serialise.h"
#include "geospatialrange.h"
#include "utils.h"

#include <float.h>


const std::string MAX_CMPVALUE(Xapian::sortable_serialise(DBL_MAX));
const std::string STR_FOR_EMPTY("\xff");

// Vector of slots
typedef struct keys_values_s {
	Xapian::valueno slot;
	char type;
	double valuenumeric;
	std::string valuestring;
	CartesianList valuegeo;
	bool reverse;
	bool hasValue;
} keys_values_t;


/*
 * KeyMaker subclass which combines several Multivalues.
 * This class only is used for sorting, there are two cases:
 * Case 1: Ascending (include in the query -> sort:+field_name or sort:field_name), of all values is selected the smallest.
 * Case 2: Descending (include in the query -> sort:-field_name), of all values is selected the largest.

 * For collapsing, it is used Xapian::MultiValueKeyMaker.
 */
class Multi_MultiValueKeyMaker : public Xapian::KeyMaker {
	// Vector of slots
	std::vector<keys_values_t> slots;

	public:
		Multi_MultiValueKeyMaker() { }

		template <class Iterator>
		Multi_MultiValueKeyMaker(Iterator begin, Iterator end) {
			while (begin != end) add_value(*begin++);
		}

		virtual std::string operator()(const Xapian::Document & doc) const;

		void add_value(Xapian::valueno slot, char type, const std::string &value, bool reverse = false) {
			if (!value.empty()) {
				keys_values_t ins_key = { slot, type, 0, "", CartesianList(), reverse, true };
				switch (type) {
					case NUMERIC_TYPE:
						ins_key.valuenumeric = std::stod(value);
						break;
					case DATE_TYPE:
						ins_key.valuenumeric = Datetime::timestamp(value);
						break;
					case BOOLEAN_TYPE:
						ins_key.valuestring = strcasecmp(value.c_str(), "true") == 0 ? "t" : "f";
						break;
					case STRING_TYPE:
						ins_key.valuestring = value;
						break;
					case GEO_TYPE:
						std::vector<range_t> ranges;
						EWKT_Parser::getRanges(value, true, HTM_MIN_ERROR, ranges, ins_key.valuegeo);
						break;
				}
				slots.push_back(ins_key);
			} else if (type != GEO_TYPE) {
				keys_values_t ins_key = { slot, type, 0, value, CartesianList(), reverse, false };
				slots.push_back(ins_key);
			}
		}
};
