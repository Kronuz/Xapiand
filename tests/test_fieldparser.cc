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

#include "test_fieldparser.h"

// #define DEBUG_FIELD_PARSER 1

#ifdef TEST_SINGLE
#  define TESTING_DATABASE 0
#  define TESTING_ENDPOINTS 0
#  define TESTING_LOGS 0
#endif

#include "utils.h"
#include "field_parser.h"

#include <string>
#include <vector>


struct Fieldparser_t {
	std::string field;
	std::string field_name_colon;
	std::string field_name;
	std::string value;
	std::string double_quote_value;
	std::string single_quote_value;
	std::string start;
	std::string end;
	std::string values;
};


int test_field_parser() {
	INIT_LOG
	std::vector<Fieldparser_t> fields {
		{ "Color:Blue", "Color:", "Color", "Blue", "", "", "", "", "Blue" },
		{ "Color:\"dark blue\"", "Color:", "Color", "dark blue", "\"dark blue\"", "", "", "", "\"dark blue\"" },
		{ "Color:'light blue'", "Color:", "Color", "light blue", "", "'light blue'", "", "", "'light blue'" },
		{ "color_range:[a70d0d,ec500d]", "color_range:", "color_range", "a70d0d", "", "", "a70d0d", "ec500d", "[a70d0d,ec500d]" },
		{ "green", "", "", "green", "", "", "", "", "green" },
		{ "\"dark green\"", "", "", "dark green", "\"dark green\"", "", "", "", "\"dark green\"" },
		{ "'light green'", "", "", "light green", "", "'light green'", "", "", "'light green'" },
		{ "[100,200]", "", "", "100", "", "", "100", "200", "[100,200]" },
		{ "Field:[100,200]", "Field:", "Field", "100", "", "", "100", "200", "[100,200]" },
		{ "['initial range','end of range']", "", "", "initial range", "", "'initial range'", "initial range", "end of range", "['initial range','end of range']" },
		{ "Field:['initial range','end of range']", "Field:", "Field", "initial range", "", "'initial range'", "initial range", "end of range", "['initial range','end of range']" },
		{ "[\"initial range\",\"end of range\"]", "", "", "initial range", "\"initial range\"", "", "initial range", "end of range", "[\"initial range\",\"end of range\"]" },
		{ "Field:[\"initial range\",\"end of range\"]", "Field:", "Field", "initial range", "\"initial range\"", "", "initial range", "end of range", "[\"initial range\",\"end of range\"]" },
		{ "100..200", "", "", "100", "", "", "100", "200", "100..200" },
		{ "Field:100..200", "Field:", "Field", "100", "", "", "100", "200", "100..200" },
		{ "'initial range'..'end of range'", "", "", "initial range", "", "'initial range'", "initial range", "end of range", "'initial range'..'end of range'" },
		{ "Field:'initial range'..'end of range'", "Field:", "Field", "initial range", "", "'initial range'", "initial range", "end of range", "'initial range'..'end of range'" },
		{ "\"initial range\"..\"end of range\"", "", "", "initial range", "\"initial range\"", "", "initial range", "end of range", "\"initial range\"..\"end of range\"" },
		{ "Field:\"initial range\"..\"end of range\"", "Field:", "Field", "initial range", "\"initial range\"", "", "initial range", "end of range", "\"initial range\"..\"end of range\"" },

		{ "[100]", "", "", "100", "", "", "100", "", "[100]" },
		{ "[100,]", "", "", "100", "", "", "100", "", "[100,]" },
		{ "[,200]", "", "", "", "", "", "", "200", "[,200]" },
		{ "[,,300]", "", "", "", "", "", "", "", "[,,300]" },
		{ "[100,200,300,400]", "", "", "100", "", "", "100", "200", "[100,200,300,400]" },
		{ "100..200..300..400", "", "", "100", "", "", "100", "200", "100..200..300..400" },

		{ "100", "", "", "100", "", "", "", "", "100" },
		{ "100..", "", "", "100", "", "", "100", "", "100.." },
		{ "..200", "", "", "", "", "", "", "200", "..200" },
		{ "....300", "", "", "", "", "", "", "", "....300" },
		{ "Field:100..", "Field:", "Field", "100", "", "", "100", "", "100.." },
		{ "Field:..200", "Field:", "Field", "", "", "", "", "200", "..200" },
	};

	int count = 0;
	for (auto& field : fields) {
		FieldParser fp(field.field);
		fp.parse(4);

		if (fp.get_field_name_colon() != field.field_name_colon) {
			L_ERR(nullptr, "\nError: The field with colon should be:\n  %s\nbut it is:\n  %s", field.field_name_colon.c_str(), fp.get_field_name_colon().c_str());
			++count;
		}

		if (fp.get_field_name() != field.field_name) {
			L_ERR(nullptr, "\nError: The field name should be:\n  %s\nbut it is:\n  %s", field.field_name.c_str(), fp.get_field_name().c_str());
			++count;
		}

		if (fp.get_value() != field.value) {
			L_ERR(nullptr, "\nError: The value should be:\n  %s\nbut it is:\n  %s", field.value.c_str(), fp.get_value().c_str());
			++count;
		}

		if (fp.get_double_quoted_value() != field.double_quote_value) {
			L_ERR(nullptr, "\nError: The double quote value should be:\n  %s\nbut it is:\n  %s", field.double_quote_value.c_str(), fp.get_double_quoted_value().c_str());
			++count;
		}

		if (fp.get_single_quoted_value() != field.single_quote_value) {
			L_ERR(nullptr, "\nError: The single quote value should be:\n  %s\nbut it is:\n  %s", field.single_quote_value.c_str(), fp.get_single_quoted_value().c_str());
			++count;
		}

		if (fp.get_start() != field.start) {
			L_ERR(nullptr, "\nError: The start value range should be:\n  %s\nbut it is:\n  %s", field.start.c_str(), fp.get_start().c_str());
			++count;
		}

		if (fp.get_end() != field.end) {
			L_ERR(nullptr, "\nError: The end value range should be:\n  %s\nbut it is:\n  %s", field.end.c_str(), fp.get_end().c_str());
			++count;
		}

		if (fp.get_values() != field.values) {
			L_ERR(nullptr, "\nError: The values should be:\n  %s\nbut are:\n  %s", field.values.c_str(), fp.get_values().c_str());
			++count;
		}
	}

	RETURN(count);
}


#ifdef TEST_SINGLE
// c++ -std=c++14 -fsanitize=address -Wall -Wextra -g -O2 -o tst -DTEST_SINGLE -Ibuild/src -Isrc tests/test_fieldparser.cc && ./tst
#include "../src/exception.cc"
#include "../src/field_parser.cc"
int main(int, char const *[]) {
	return test_field_parser();
}
#endif
