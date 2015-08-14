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

#include "generate_terms.h"
#include <algorithm>


::std::string
GenerateTerms::numeric(const ::std::string &start_, const ::std::string &end_, const ::std::vector<::std::string> &accuracy,
	const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes)
{
	::std::string res;
	if (!start_.empty() && !end_.empty()) {
		long long int start = strtollong(start_);
		long long int end = strtollong(end_);
		long long int size_r = end - start;

		if (size_r < 0) return res;

		::std::vector<::std::string>::const_iterator it(accuracy.begin());
		::std::vector<::std::string>::const_iterator it_p(acc_prefix.begin());
		long long int diff = size_r, diffN = LLONG_MIN, inc = 0, incUP = 0, aux, aux2, i;
		::std::string _prefixUP, _prefix;
		// Get upper and lower increase and their prefixes.
		for ( ; it != accuracy.end(); it++, it_p++) {
			aux = strtollong(*it);
			aux2 = size_r - aux;
			if (aux2 >= 0 && aux2 < diff) {
				diff = aux2;
				inc = aux;
				_prefix = *it_p + ":";
			} else if (aux2 < 0 && aux2 > diffN) {
				diffN = aux2;
				incUP = aux;
				_prefixUP = *it_p + ":";
			}
		}

		// Set upper limits. Example accuracy=1000 -> 4900..5100  ==> (U:4000 OR U:5000).
		if (incUP != 0) {
			prefixes.push_back(::std::string(_prefixUP.c_str(), 0, _prefixUP.size() - 1));
			aux = start - (start % incUP);
			aux2 = end - (end % incUP);
			res = _prefixUP + ::std::to_string(aux);
			if (aux != aux2) {
				res = "(" + res + " OR " + _prefixUP + ::std::to_string(aux2) + ")";
			}
		}

		// Set lower limits. Example accuracy=100 -> 4900..5100 ==> (L:4900 OR L:5000 OR L:5100).
		if (inc != 0) {
			start = start - (start % inc);
			end = end - (end % inc);
			aux = (end - start) / inc;
			// If terms are not too many.
			if (aux < MAX_TERMS) {
				prefixes.push_back(::std::string(_prefix.c_str(), 0, _prefix.size() - 1));
				::std::string or_terms("(" + _prefix + ::std::to_string(start));
				for (i = 1; i < aux; i++) {
					aux2 = start + inc * i;
					or_terms += " OR " + _prefix + ::std::to_string(aux2);
				}
				or_terms += (start != end) ? " OR " + _prefix + ::std::to_string(end) + ")" : ")";
				incUP != 0 ? res += " AND " + or_terms : res = or_terms;
			}
		}

		::std::transform(res.begin(), res.end(), res.begin(), TRANSFORM());
	}

	return res;
}


::std::string
GenerateTerms::date(const ::std::string &start_, const ::std::string &end_, const ::std::vector<::std::string> &accuracy, const ::std::vector<::std::string> &acc_prefix, ::std::string &prefix)
{
	::std::string res;
	if (!start_.empty() && !end_.empty()) {
		try {
			long double start = Datetime::timestamp(start_);
			long double end = Datetime::timestamp(end_);

			if (end < start) return res;

			time_t timestamp_s = (time_t) start;
			time_t timestamp_e = (time_t) end;

			struct tm *timeinfo = gmtime(&timestamp_s);
			int tm_s[6] = {timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec};
			timeinfo = gmtime(&timestamp_e);
			int tm_e[6] = {timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec};

			::std::vector<::std::string>::const_iterator it(accuracy.begin());
			bool y = false, mon = false, w = false, d = false, h = false, min = false, sec = false;
			int pos_y = 0, pos_mon = 0, pos_w = 0, pos_d = 0, pos_h = 0, pos_min = 0, pos_sec = 0;
			// Get accuracies.
			for (int pos = 0; it != accuracy.end(); it++, pos++) {
				::std::string str = stringtolower(*it);
				if (!y && str.compare("year") == 0) {
					y = true;
					pos_y = pos;
				} else if (!mon && str.compare("month") == 0) {
					mon = true;
					pos_mon = pos;
				} else if (!w && str.compare("week") == 0) {
					w = true;
					pos_w = pos;
				} else if (!d && str.compare("day") == 0) {
					d = true;
					pos_d = pos;
				} else if (!h && str.compare("hour") == 0) {
					h = true;
					pos_h = pos;
				} else if (!min && str.compare("minute") == 0) {
					min = true;
					pos_min = pos;
				} else if (!sec && str.compare("second") == 0) {
					sec = true;
					pos_sec = pos;
				}
			}

			if (tm_s[0] != tm_e[0]) {
				if (y) {
					if ((tm_e[0] - tm_s[0]) > MAX_TERMS) return res;
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			} else if (tm_s[1] != tm_e[1]) {
				if (mon) {
					prefix = acc_prefix.at(pos_mon);
					res = "(" + month(tm_s, tm_e, prefix) + ")";
				} else if (y) {
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			} else if (tm_s[2] != tm_e[2]) {
				if (d) {
					prefix = acc_prefix.at(pos_d);
					res = "(" + day(tm_s, tm_e, prefix) + ")";
				} else if (mon) {
					prefix = acc_prefix.at(pos_mon);
					res = "(" + month(tm_s, tm_e, prefix) + ")";
				} else if (y) {
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			} else if (tm_s[3] != tm_e[3]) {
				if (h) {
					prefix = acc_prefix.at(pos_h);
					res = "(" + hour(tm_s, tm_e, prefix) + ")";
				} else if (d) {
					prefix = acc_prefix.at(pos_d);
					res = "(" + day(tm_s, tm_e, prefix) + ")";
				} else if (mon) {
					prefix = acc_prefix.at(pos_mon);
					res = "(" + month(tm_s, tm_e, prefix) + ")";
				} else if (y) {
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			} else if (tm_s[4] != tm_e[4]) {
				if (min) {
					prefix = acc_prefix.at(pos_min);
					res = "(" + minute(tm_s, tm_e, prefix) + ")";
				} else if (h) {
					prefix = acc_prefix.at(pos_h);
					res = "(" + hour(tm_s, tm_e, prefix) + ")";
				} else if (d) {
					prefix = acc_prefix.at(pos_d);
					res = "(" + day(tm_s, tm_e, prefix) + ")";
				} else if (mon) {
					prefix = acc_prefix.at(pos_mon);
					res = "(" + month(tm_s, tm_e, prefix) + ")";
				} else if (y) {
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			} else {
				if (sec) {
					prefix = acc_prefix.at(pos_sec);
					res = "(" + second(tm_s, tm_e, prefix) + ")";
				} else if (min) {
					prefix = acc_prefix.at(pos_min);
					res = "(" + minute(tm_s, tm_e, prefix) + ")";
				} else if (h) {
					prefix = acc_prefix.at(pos_h);
					res = "(" + hour(tm_s, tm_e, prefix) + ")";
				} else if (d) {
					prefix = acc_prefix.at(pos_d);
					res = "(" + day(tm_s, tm_e, prefix) + ")";
				} else if (mon) {
					prefix = acc_prefix.at(pos_mon);
					res = "(" + month(tm_s, tm_e, prefix) + ")";
				} else if (y) {
					prefix = acc_prefix.at(pos_y);
					res = "(" + year(tm_s, tm_e, prefix) + ")";
				}
			}

			::std::transform(res.begin(), res.end(), res.begin(), TRANSFORM());
		} catch (const ::std::exception &ex) {
			throw Xapian::QueryParserError("Didn't understand date specification '" + prefix + "'");
		}
	}

	return res;
}


::std::string
GenerateTerms::year(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[1] = tm_s[3] = tm_s[4] = tm_s[5] = tm_e[1] = tm_e[3] = tm_e[4] = tm_e[5] = 0;
	tm_s[2] = tm_e[2] = 1;
	while (tm_s[0] != tm_e[0]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[0]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::month(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[3] = tm_s[4] = tm_s[5] = tm_e[3] = tm_e[4] = tm_e[5] = 0;
	tm_s[2] = tm_e[2] = 1;
	while (tm_s[1] != tm_e[1]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[1]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::day(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[3] = tm_s[4] = tm_s[5] = tm_e[3] = tm_e[4] = tm_e[5] = 0;
	while (tm_s[2] != tm_e[2]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[2]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::hour(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[4] = tm_s[5] = tm_e[4] = tm_e[5] = 0;
	while (tm_s[3] != tm_e[3]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[3]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::minute(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	tm_s[5] = tm_e[5] = 0;
	while (tm_s[4] != tm_e[4]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[4]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::second(int tm_s[], int tm_e[], const ::std::string &prefix)
{
	::std::string prefix_dot = prefix + ":";
	::std::string res;
	while (tm_s[5] != tm_e[5]) {
		res += prefix_dot + Serialise::date(tm_s) + " OR ";
		tm_s[5]++;
	}

	res += prefix_dot + Serialise::date(tm_e);
	return res;
}


::std::string
GenerateTerms::geo(::std::vector<range_t> &ranges, const ::std::vector<::std::string> &acc_prefix, ::std::vector<::std::string> &prefixes)
{
	::std::string result;
	::std::vector<range_t>::iterator rit(ranges.begin());
	for ( ; rit != ranges.end(); rit++) {
		::std::bitset<SIZE_BITS_ID> b1(rit->start), b2(rit->end), res;
		size_t idx = 0;
		uInt64 val;
		if (rit->start != rit->end) {
			idx = SIZE_BITS_ID - 1;
			for (; idx > 0 && b1.test(idx) == b2.test(idx); --idx) {
				res.set(idx, b1.test(idx));
			}
			size_t aux = idx % BITS_LEVEL;
			idx += aux ? BITS_LEVEL - aux : 0;
			val = res.to_ullong() >> idx;
		} else {
			val = rit->start;
		}

		size_t tmp = (acc_prefix.size() - 1) * BITS_LEVEL;
		if (idx > tmp) {
			uInt64 _start = rit->start >> tmp, _end = rit->end >> tmp;
			while (_start <= _end) {
				::std::string vterm(acc_prefix.at(acc_prefix.size() - 1) + ":" + ::std::to_string(_start));
				if (result.find(vterm) == ::std::string::npos) {
					result += (result.empty()) ? vterm : " OR " + vterm;
					prefixes.push_back(acc_prefix.at(acc_prefix.size() - 1));
				}
				_start++;
			}
		} else {
			size_t j = idx / BITS_LEVEL;
			::std::string vterm(acc_prefix.at(j) + ":" + ::std::to_string(val));
			if (result.find(vterm) == ::std::string::npos) {
				result += (result.empty()) ? vterm : " OR " + vterm;
				prefixes.push_back(acc_prefix.at(j));
			}
		}
	}

	return result;
}