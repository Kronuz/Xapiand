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

#include "mime_types.hh"

#include <cstdlib>                                // for getenv
#include <fstream>                                // for std::ifstream
#include <sstream>                                // for std::istringstream
#include <unordered_map>                          // for std::unordered_map

#include "config.h"                               // for MIME_TYPES_PATH
#include "database/data.h"                        // for ct_type_t
#include "hashes.hh"                              // for fnv1ah32
#include "log.h"                                  // for L_WARNING_ONCE
#include "string.hh"                              // for string::lower


std::unordered_map<std::string, ct_type_t>
load_mime_types()
{
	std::unordered_map<std::string, ct_type_t> mime_types;

	enum {
		EXPECT_TYPES,
		EXPECT_START,
		EXPECT_KEY,
		EXPECT_VALUE,
		ENDED,
	} state = EXPECT_TYPES;

	std::string mime_types_path(getenv("XAPIAN_MIME_TYPES_PATH") != nullptr ? getenv("XAPIAN_MIME_TYPES_PATH") : MIME_TYPES_PATH);
	std::ifstream lines(mime_types_path);
	if (lines.is_open()) {
		std::string line;
		size_t line_num = 0;
		ct_type_t ct_type;
		while (std::getline(lines, line)) {
			++line_num;
			std::istringstream words(line);
			for (auto it = std::istream_iterator<std::string>(words); it != std::istream_iterator<std::string>(); ++it) {
				auto word = std::string_view(*it);
				if (word.front() == '#') {
					break;  // skip whole line
				}
				switch (state) {
					case EXPECT_TYPES:
						if (word == "types") {
							state = EXPECT_START;
						} else {
							L_WARNING("Unexpected {} in mime types file: {}:{}", repr(word), mime_types_path, line_num);
						}
						break;
					case EXPECT_START:
						if (word == "{") {
							state = EXPECT_KEY;
						} else {
							L_WARNING("Unexpected {} in mime types file: {}:{}", repr(word), mime_types_path, line_num);
						}
						break;
					case EXPECT_KEY:
						if (word == "}") {
							state = ENDED;
						} else {
							ct_type = ct_type_t(word);
							state = EXPECT_VALUE;
						}
						break;
					case EXPECT_VALUE:
						if (word.back() == ';') {
							word.remove_suffix(1);
							state = EXPECT_KEY;
						}
						if (word == "}") {
							L_WARNING("Unexpected {} in mime types file: {}:{}", repr(word), mime_types_path, line_num);
						} else {
							mime_types[string::lower(word)] = ct_type;
						}
						break;
					case ENDED:
						break;
				}
			}
		}
		if (state != ENDED) {
			L_WARNING("Unexpected EOF in mime types file: {}:{}", mime_types_path, line_num);
		}
	} else {
		L_WARNING_ONCE("Cannot open mime types file: {}", mime_types_path);
	}

	return mime_types;
}


const ct_type_t&
mime_type(std::string_view extension)
{
	static const auto mime_types = load_mime_types();

	auto found = extension.find_last_of('.');
	if (found != std::string_view::npos) {
		extension.remove_prefix(found + 1);
	}

	if (!extension.empty()) {
		auto it = mime_types.find(string::lower(extension));
		if (it != mime_types.end()) {
			return it->second;
		}
	}

	static const ct_type_t no_type;
	return no_type;
}
