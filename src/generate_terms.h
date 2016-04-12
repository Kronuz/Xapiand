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

#include "config.h"
#include "fields.h"
#include "htm.h"

#include <unordered_set>


#define MAX_TERMS 100


namespace GenerateTerms {
	std::string numeric(const std::string& start_, const std::string& end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
		std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<NumericFieldProcessor>>& nfps, Xapian::QueryParser& queryparser);
	std::string date(const std::string& start_, const std::string& end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
		std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<DateFieldProcessor>>& dfps, Xapian::QueryParser& queryparser);
	std::string year(int tm_s[], int tm_e[], const std::string& prefix);
	std::string month(int tm_s[], int tm_e[], const std::string& prefix);
	std::string day(int tm_s[], int tm_e[], const std::string& prefix);
	std::string hour(int tm_s[], int tm_e[], const std::string& prefix);
	std::string minute(int tm_s[], int tm_e[], const std::string& prefix);
	std::string second(int tm_s[], int tm_e[], const std::string& prefix);
	std::string geo(const std::vector<range_t>& ranges,  const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
		std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<GeoFieldProcessor>>& gfps, Xapian::QueryParser& queryparser);
};
