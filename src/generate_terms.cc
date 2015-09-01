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

#include "generate_terms.h"
#include "serialise.h"
#include "datetime.h"
#include "utils.h"
#include "database.h"

#include "generate_terms.h"
#include <algorithm>
#include <bitset>
#include <map>
#include <limits.h>


static bool isnotSubtrixel(std::string &last_valid, const uInt64 &id_trixel) {
	std::string res(std::bitset<SIZE_BITS_ID>(id_trixel).to_string());
	res = res.substr(res.find("1"));
	if (res.find(last_valid) == 0) {
		return false;
	} else {
		last_valid = res;
		return true;
	}
}


void
GenerateTerms::numeric(::std::string &result_terms, const ::std::string &start_, const ::std::string &end_, const ::std::vector<double> &accuracy,
	const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes)
{
	if (!start_.empty() && !end_.empty() && !accuracy.empty()) {
		double d_start = strtodouble(start_);
		double d_end = strtodouble(end_);
		double size_r = d_end - d_start;

		// If the range is negative or its values are outside of range of a long long,
		// return empty terms because these won't be generated correctly.
		if (size_r < 0.0 || d_start <= LLONG_MIN || d_end >= LLONG_MAX) return;

		long long start = static_cast<long long>(d_start);
		long long end = static_cast<long long>(d_end);

		// Find the upper or equal accuracy.
		int pos = 0, len = (int)accuracy.size();
		while (pos < len && accuracy[pos] < size_r) pos++;
		// If there is a upper accuracy.
		bool up_acc = false;
		if (pos < len) {
			long long _acc = static_cast<long long>(accuracy[pos]);
			std::string prefix(acc_prefix[pos] + ":");
			prefixes.push_back(acc_prefix[pos]);
			long long aux = start - (start % _acc);
			long long aux2 = end - (end % _acc);
			result_terms = prefix + ::std::to_string(aux);
			if (aux != aux2) result_terms += " OR " + prefix + ::std::to_string(aux2);
			up_acc = true;
		}
		// If there is a lower accuracy.
		if (--pos >= 0) {
			long long _acc = static_cast<long long>(accuracy[pos]);
			start = start - (start % _acc);
			end = end - (end % _acc);
			long long aux = (end - start) / _acc;
			// If terms are not too many.
			if (aux < MAX_TERMS) {
				std::string prefix(acc_prefix[pos] + ":");
				prefixes.push_back(acc_prefix[pos]);
				::std::string or_terms(prefix + ::std::to_string(start));
				for (int i = 1; i < aux; i++) {
					long long aux2 = start + accuracy[pos] * i;
					or_terms += " OR " + prefix + ::std::to_string(aux2);
				}
				if (start != end) or_terms += " OR " + prefix + ::std::to_string(end);
				result_terms = up_acc ? "(" + result_terms + ") AND (" + or_terms + ")" : or_terms;
			}
		}

		::std::transform(result_terms.begin(), result_terms.end(), result_terms.begin(), TRANSFORM());
	}
}


void
GenerateTerms::date(::std::string &result_terms, const ::std::string &start_, const ::std::string &end_, const ::std::vector<double> &accuracy, const ::std::vector<::std::string> &acc_prefix, ::std::vector<std::string> &prefixes)
{
	if (!start_.empty() && !end_.empty() && !accuracy.empty()) {
		try {
			double start = Datetime::timestamp(start_);
			double end = Datetime::timestamp(end_);

			if (end < start) return;

			time_t timestamp_s = (time_t) start;
			time_t timestamp_e = (time_t) end;

			struct tm *timeinfo = gmtime(&timestamp_s);
			int tm_s[6] = { timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year };
			timeinfo = gmtime(&timestamp_e);
			int tm_e[6] = { timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year };

			// Find the accuracy needed.
			char acc = DB_YEAR2INT;
			while (acc >= DB_SECOND2INT && (tm_e[acc] - tm_s[acc]) == 0) acc--;

			// Find the upper or equal accuracy.
			char pos = 0;
			while (accuracy[pos] < acc) pos++;
			// If the accuracy needed is in accuracy.
			if (acc == accuracy[pos]) {
				switch ((char)accuracy[pos]) {
					case DB_YEAR2INT:
						if ((tm_e[5] - tm_s[5]) > MAX_TERMS) return;
						prefixes.push_back(acc_prefix[pos]);
						result_terms = year(tm_s, tm_e, prefixes.back());
						break;
					case DB_MONTH2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = month(tm_s, tm_e, prefixes.back());
						break;
					case DB_DAY2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = day(tm_s, tm_e, prefixes.back());
						break;
					case DB_HOUR2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = hour(tm_s, tm_e, prefixes.back());
						break;
					case DB_MINUTE2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = minute(tm_s, tm_e, prefixes.back());
						break;
					case DB_SECOND2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = second(tm_s, tm_e, prefixes.back());
						break;
				}
			}
			// If there is an upper accuracy
			if (accuracy[pos] != acc || ++pos < accuracy.size()) {
				switch ((char)accuracy[pos]) {
					case DB_YEAR2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? year(tm_s, tm_e, prefixes.back()) :
									   year(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
					case DB_MONTH2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? month(tm_s, tm_e, prefixes.back()) :
									   month(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
					case DB_DAY2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? day(tm_s, tm_e, prefixes.back()) :
									   day(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
					case DB_HOUR2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? hour(tm_s, tm_e, prefixes.back()) :
									   hour(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
					case DB_MINUTE2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? minute(tm_s, tm_e, prefixes.back()) :
									   minute(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
					case DB_SECOND2INT:
						prefixes.push_back(acc_prefix[pos]);
						result_terms = result_terms.empty() ? second(tm_s, tm_e, prefixes.back()) :
									   second(tm_s, tm_e, prefixes.back()) + " AND (" + result_terms + ")";
						break;
				}
			}

			::std::transform(result_terms.begin(), result_terms.end(), result_terms.begin(), TRANSFORM());
		} catch (const ::std::exception &ex) {
			throw Xapian::QueryParserError("Didn't understand date specification");
		}
	}
}


::std::string
GenerateTerms::year(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_s[4] = tm_e[0] = tm_e[1] = tm_e[2] = tm_e[4] = 0;
	tm_s[3] = tm_e[3] = 1;
	while (tm_s[5] != tm_e[5]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[5]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::month(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_e[1] = tm_e[2] = tm_e[3] = 0;
	tm_s[3] = tm_e[3] = 1;
	while (tm_s[4] != tm_e[4]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[4]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::day(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_e[0] = tm_e[1] = tm_e[2] = 0;
	while (tm_s[3] != tm_e[3]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[3]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::hour(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[0] = tm_s[1] = tm_e[0] = tm_e[1] = 0;
	while (tm_s[2] != tm_e[2]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[2]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::minute(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[0] = tm_e[0] = 0;
	while (tm_s[1] != tm_e[1]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[1]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::second(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	while (tm_s[0] != tm_e[0]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[0]++;
	}
	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


void
GenerateTerms::geo(::std::string &result_terms, const ::std::vector<range_t> &ranges,  const ::std::vector<double> &accuracy, const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes)
{
	// The user does not specify the accuracy.
	if (acc_prefix.empty()) return;

	::std::map<uInt64, ::std::string> result;
	::std::set<::std::string> aux_p;
	::std::vector<range_t>::const_iterator rit(ranges.begin());
	for ( ; rit != ranges.end(); rit++) {
		::std::bitset<SIZE_BITS_ID> b1(rit->start), b2(rit->end), res;
		int idx = -1;
		uInt64 val;
		if (rit->start != rit->end) {
			idx = SIZE_BITS_ID - 1;
			for ( ; idx > 0 && b1.test(idx) == b2.test(idx); --idx) res.set(idx, b1.test(idx));
			val = res.to_ullong();
		} else val = rit->start;

		int posF = -1, j = 0;
		for (int i = (int)(accuracy.size() - 1); i > 1; --i) {
			if ((START_POS - 2 * accuracy[i]) > idx) {
				posF = START_POS - accuracy[i] * 2;
				j = i - 2;
				break;
			}
		}
        if (posF != -1) result.insert(::std::pair<uInt64, ::std::string>(val >> posF, acc_prefix[j]));
	}

	// The search have trixels more big that the biggest trixel in accuracy.
	if (result.empty()) return;

	// Delete duplicates terms.
	::std::map<uInt64, ::std::string>::iterator it(result.begin());
	std::string last_valid(std::bitset<SIZE_BITS_ID>(it->first).to_string());
	last_valid.assign(last_valid.substr(last_valid.find("1")));
	result_terms = it->second + ":" + ::std::to_string(it->first);
	aux_p.insert(it->second);
	for (++it; it != result.end(); ++it) {
		if (isnotSubtrixel(last_valid, it->first)) {
			result_terms += " OR " + it->second + ":" + ::std::to_string(it->first);
			aux_p.insert(it->second);
		}
	}
	prefixes.assign(aux_p.begin(), aux_p.end());
}