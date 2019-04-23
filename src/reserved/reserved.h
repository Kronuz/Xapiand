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

#pragma once

#define RESERVED__ "_"
constexpr const char reserved__ = RESERVED__[0];


// All non-empty names starting with an underscore are reserved.
inline bool is_reserved(std::string_view field_name) {
	return !field_name.empty() && field_name[0] == reserved__;
}


// All non-empty names not starting with an underscore or a hash sign are valid.
inline bool is_valid(std::string_view field_name) {
	return !field_name.empty() && field_name[0] != reserved__ && field_name[0] != '#';
}


// All empty names or names starting with a hash sign are comments.
inline bool is_comment(std::string_view field_name) {
	return field_name.empty() || field_name[0] == '#';
}
