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

#include "field_parser.h"


int test_field_parser() {

	INIT_LOG
	std::vector<Fieldparser_t> fields {
		{ "Color:Blue", "Color:", "Color", "Blue", "", "", "", "" },
		{ "Color:\"dark blue\"", "Color:", "Color", "dark blue", "\"dark blue\"", "", "", "" },
		{ "Color:'light blue'", "Color:", "Color", "light blue", "", "'light blue'", "", "" },
		{ "color_range:[a70d0d,ec500d]",  "color_range:", "color_range", "", "", "", "a70d0d", "ec500d" },
		{ "green",  "", "", "green", "", "", "", "" },
		{ "\"dark green\"",  "", "", "dark green", "\"dark green\"", "", "", "" },
		{ "'light green'",  "", "", "light green", "", "'light green'", "", "" },
		{ "[100,200]",  "", "", "", "", "", "100", "200" },
		{ "[\"initial range\",\"end of range\"]",  "", "", "", "", "", "initial range", "end of range" },
		{ "['initial range','end of range']",  "", "", "", "", "", "initial range", "end of range" },
	};

	int count = 0;
	for (auto& field : fields) {
		FieldParser fp(field.field);
		fp.parse();

		if (fp.get_field_dot() != field.field_name_dot) {
			L_ERR(nullptr, "\nError: The field with dots should be:\n  %s\nbut it is:\n  %s", field.field_name_dot.c_str(), fp.get_field_dot().c_str());
			++count;
		}

		if (fp.get_field() != field.field_name) {
			L_ERR(nullptr, "\nError: The field should be:\n  %s\nbut it is:\n  %s", field.field_name.c_str(), fp.get_field().c_str());
			++count;
		}

		if (fp.get_value() != field.value) {
			L_ERR(nullptr, "\nError: The value should be:\n  %s\nbut it is:\n  %s", field.value.c_str(), fp.get_value().c_str());
			++count;
		}

		if (fp.get_doubleq_value() != field.double_quote) {
			L_ERR(nullptr, "\nError: The double quote value should be:\n  %s\nbut it is:\n  %s", field.double_quote.c_str(), fp.get_doubleq_value().c_str());
			++count;
		}

		if (fp.get_singleq_value() != field.single_quote) {
			L_ERR(nullptr, "\nError: The single quote value should be:\n  %s\nbut it is:\n  %s", field.single_quote.c_str(), fp.get_singleq_value().c_str());
			++count;
		}

		if (fp.start != field.start) {
			L_ERR(nullptr, "\nError: The start value range should be:\n  %s\nbut it is:\n  %s", field.start.c_str(), fp.start.c_str());
			++count;
		}

		if (fp.end != field.end) {
			L_ERR(nullptr, "\nError: The end value range should be:\n  %s\nbut it is:\n  %s", field.end.c_str(), fp.end.c_str());
			++count;
		}
	}

	RETURN(count);
}