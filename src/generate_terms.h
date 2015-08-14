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

#ifndef XAPIAND_INCLUDED_GENERATE_TERMS_H
#define XAPIAND_INCLUDED_GENERATE_TERMS_H

#include "config.h"
#include "htm.h"

#define MAX_TERMS 100

#define INC_LEVEL 5
#define BITS_LEVEL 10


namespace GenerateTerms {
	typedef struct TRANSFORM_S {
		char operator() (char c) { return  (c == '-') ? '_' : c;}
	} TRANSFORM;

	::std::string numeric(const ::std::string &start_, const ::std::string &end_, const ::std::vector<::std::string> &accuracy, const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes);
	::std::string date(const ::std::string &start_, const ::std::string &end_, const ::std::vector<::std::string> &accuracy, const ::std::vector<::std::string> &acc_prefix, ::std::string &prefix);
	::std::string year(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string month(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string day(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string hour(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string minute(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string second(int tm_s[], int tm_e[], const ::std::string &prefix);
	::std::string geo(::std::vector<range_t> &ranges, const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes);
};


#endif /* XAPIAND_INCLUDED_GENERATE_TERMS_H */