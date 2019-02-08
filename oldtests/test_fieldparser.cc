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

#include "test_fieldparser.h"

#include <string>
#include <vector>

#include "field_parser.h"
#include "utils.h"


struct Fieldparser_t {
	std::string field;
	std::string field_name;
	std::string value;
	std::string double_quote_value;
	std::string single_quote_value;
	std::string start;
	std::string end;
	std::string values;
	FieldParser::Range range;
};


std::string readable_range(FieldParser::Range range) {
	switch (range) {
		case FieldParser::Range::none:
			return "none";
		case FieldParser::Range::open:
			return "open";
		case FieldParser::Range::closed_right:
			return "closed_right";
		case FieldParser::Range::closed_left:
			return "closed_left";
		case FieldParser::Range::closed:
			return "closed";
	}
}


int test_field_parser() {
	INIT_LOG
	std::vector<Fieldparser_t> fields {
		{ "Color:Blue", "Color", "Blue", "", "", "", "", "Blue", FieldParser::Range::none },
		{ "Color:\"dark blue\"", "Color", "dark blue", "\"dark blue\"", "", "", "", "\"dark blue\"", FieldParser::Range::none },
		{ "Color:'light blue'", "Color", "light blue", "", "'light blue'", "", "", "'light blue'", FieldParser::Range::none },
		{ "color_range:[a70d0d,ec500d]", "color_range", "a70d0d", "", "", "a70d0d", "ec500d", "[a70d0d,ec500d]", FieldParser::Range::closed },
		{ "green", "", "green", "", "", "", "", "green", FieldParser::Range::none },
		{ "\"dark green\"", "", "dark green", "\"dark green\"", "", "", "", "\"dark green\"", FieldParser::Range::none },
		{ "'light green'", "", "light green", "", "'light green'", "", "", "'light green'", FieldParser::Range::none },
		{ "[100,200]", "", "100", "", "", "100", "200", "[100,200]", FieldParser::Range::closed },
		{ "Field:[100,200]", "Field", "100", "", "", "100", "200", "[100,200]", FieldParser::Range::closed },
		{ "['initial range','end of range']", "", "initial range", "", "'initial range'", "initial range", "end of range", "['initial range','end of range']", FieldParser::Range::closed },
		{ "Field:['initial range','end of range']", "Field", "initial range", "", "'initial range'", "initial range", "end of range", "['initial range','end of range']", FieldParser::Range::closed },
		{ "[\"initial range\",\"end of range\"]", "", "initial range", "\"initial range\"", "", "initial range", "end of range", "[\"initial range\",\"end of range\"]", FieldParser::Range::closed },
		{ "Field:[\"initial range\",\"end of range\"]", "Field", "initial range", "\"initial range\"", "", "initial range", "end of range", "[\"initial range\",\"end of range\"]", FieldParser::Range::closed },
		{ "100..200", "", "100", "", "", "100", "200", "100..200", FieldParser::Range::closed },
		{ "Field:100..200", "Field", "100", "", "", "100", "200", "100..200", FieldParser::Range::closed },
		{ "'initial range'..'end of range'", "", "initial range", "", "'initial range'", "initial range", "end of range", "'initial range'..'end of range'", FieldParser::Range::closed },
		{ "Field:'initial range'..'end of range'", "Field", "initial range", "", "'initial range'", "initial range", "end of range", "'initial range'..'end of range'", FieldParser::Range::closed },
		{ "\"initial range\"..\"end of range\"", "", "initial range", "\"initial range\"", "", "initial range", "end of range", "\"initial range\"..\"end of range\"", FieldParser::Range::closed },
		{ "Field:\"initial range\"..\"end of range\"", "Field", "initial range", "\"initial range\"", "", "initial range", "end of range", "\"initial range\"..\"end of range\"", FieldParser::Range::closed },

		{ "[100]", "", "100", "", "", "100", "", "[100]", FieldParser::Range::closed },
		{ "[100,]", "", "100", "", "", "100", "", "[100,]", FieldParser::Range::closed },
		{ "[,200]", "", "", "", "", "", "200", "[,200]", FieldParser::Range::closed },
		{ "[,,300]", "", "", "", "", "", "", "[,,300]", FieldParser::Range::closed },
		{ "[100,200,300,400]", "", "100", "", "", "100", "200", "[100,200,300,400]", FieldParser::Range::closed },
		{ "100..200..300..400", "", "100", "", "", "100", "200", "100..200..300..400", FieldParser::Range::closed },

		{ "100", "", "100", "", "", "", "", "100", FieldParser::Range::none },
		{ "100..", "", "100", "", "", "100", "", "100..", FieldParser::Range::closed },
		{ "..200", "", "", "", "", "", "200", "..200", FieldParser::Range::closed },
		{ "....300", "", "", "", "", "", "", "....300", FieldParser::Range::closed },
		{ "Field:100..", "Field", "100", "", "", "100", "", "100..", FieldParser::Range::closed },
		{ "Field:..200", "Field", "", "", "", "", "200", "..200", FieldParser::Range::closed },

		{ "(100,200]", "", "100", "", "", "100", "200", "(100,200]", FieldParser::Range::closed_right },
		{ "[100,200)", "", "100", "", "", "100", "200", "[100,200)", FieldParser::Range::closed_left },
		{ "(100,200)", "", "100", "", "", "100", "200", "(100,200)", FieldParser::Range::open },

		{ "nested.field.name:value", "nested.field.name", "value", "", "", "", "", "value", FieldParser::Range::none },
	};

	int count = 0;
	for (auto& field : fields) {
		FieldParser fp(field.field);
		fp.parse(4);

		if (fp.get_field_name() != field.field_name) {
			L_ERR("\nError: The field name should be:\n  {}\nbut it is:\n  {}", field.field_name, fp.get_field_name());
			++count;
		}

		if (fp.get_value() != field.value) {
			L_ERR("\nError: The value should be:\n  {}\nbut it is:\n  {}", field.value, fp.get_value());
			++count;
		}

		if (fp.get_double_quoted_value() != field.double_quote_value) {
			L_ERR("\nError: The double quote value should be:\n  {}\nbut it is:\n  {}", field.double_quote_value, fp.get_double_quoted_value());
			++count;
		}

		if (fp.get_single_quoted_value() != field.single_quote_value) {
			L_ERR("\nError: The single quote value should be:\n  {}\nbut it is:\n  {}", field.single_quote_value, fp.get_single_quoted_value());
			++count;
		}

		if (fp.get_start() != field.start) {
			L_ERR("\nError: The start value range should be:\n  {}\nbut it is:\n  {}", field.start, fp.get_start());
			++count;
		}

		if (fp.get_end() != field.end) {
			L_ERR("\nError: The end value range should be:\n  {}\nbut it is:\n  {}", field.end, fp.get_end());
			++count;
		}

		if (fp.get_values() != field.values) {
			L_ERR("\nError: The values should be:\n  {}\nbut are:\n  {}", field.values, fp.get_values());
			++count;
		}

		if (fp.range != field.range) {
			L_ERR("\nError: The range type should be:\n  {}\nbut it is:\n  {}", readable_range(field.range), readable_range(fp.range));
			++count;
		}
	}

	RETURN(count);
}
