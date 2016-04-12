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

#include "generate_terms.h"

#include "datetime.h"
#include "utils.h"
#include "database.h"

#include <algorithm>
#include <bitset>
#include <map>
#include <limits.h>


inline static bool isnotSubtrixel(std::string& last_valid, uint64_t id_trixel) {
	std::string res(std::bitset<SIZE_BITS_ID>(id_trixel).to_string());
	res.assign(res.substr(res.find('1')));
	if (res.find(last_valid) == 0) {
		return false;
	} else {
		last_valid.assign(res);
		return true;
	}
}


inline static std::string transform_to_string(int _timeinfo[]) {
	time_t tt = 0;
	struct tm *timeinfo = gmtime(&tt);
	timeinfo->tm_year   = _timeinfo[5];
	timeinfo->tm_mon    = _timeinfo[4];
	timeinfo->tm_mday   = _timeinfo[3];
	timeinfo->tm_hour   = _timeinfo[2];
	timeinfo->tm_min    = _timeinfo[1];
	timeinfo->tm_sec    = _timeinfo[0];
	return std::to_string(Datetime::timegm(timeinfo));
}


inline static void add_date_prefix(std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<DateFieldProcessor>>& dfps,
	Xapian::QueryParser& queryparser, const std::string& prefix)
{
	// Xapian does not allow repeat prefixes.
	if (added_prefixes.insert(prefix).second) {
		auto dfp = std::make_unique<DateFieldProcessor>(prefix);
		queryparser.add_prefix(prefix, dfp.get());
		dfps.push_back(std::move(dfp));
	}
}


inline static void add_geo_prefix(std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<GeoFieldProcessor>>& gfps,
	Xapian::QueryParser& queryparser, const std::string& prefix)
{
	// Xapian does not allow repeat prefixes.
	if (added_prefixes.insert(prefix).second) {
		auto gfp = std::make_unique<GeoFieldProcessor>(prefix);
		queryparser.add_prefix(prefix, gfp.get());
		gfps.push_back(std::move(gfp));
	}
}


inline static void add_numeric_prefix(std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<NumericFieldProcessor>>& nfps,
	Xapian::QueryParser& queryparser, const std::string& prefix)
{
	// Xapian does not allow repeat prefixes.
	if (added_prefixes.insert(prefix).second) {
		auto nfp = std::make_unique<NumericFieldProcessor>(prefix);
		queryparser.add_prefix(prefix, nfp.get());
		nfps.push_back(std::move(nfp));
	}
}


std::string
GenerateTerms::numeric(const std::string& start_, const std::string& end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
	std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<NumericFieldProcessor>>& nfps, Xapian::QueryParser& queryparser)
{
	std::string result_terms;

	if (accuracy.empty() || start_.empty() || end_.empty()) {
		return result_terms;
	}

	try {
		auto d_start = std::stod(start_);
		auto d_end = std::stod(end_);

		auto size_r = d_end - d_start;

		// If the range is negative or its values are outside of range of a long long,
		// return empty terms because these won't be generated correctly.
		if (size_r < 0.0 || d_start <= LLONG_MIN || d_end >= LLONG_MAX) {
			return result_terms;
		}

		auto start = static_cast<long long>(d_start);
		auto end = static_cast<long long>(d_end);

		// Find the upper or equal accuracy.
		int pos = 0, len = static_cast<int>(accuracy.size());
		while (pos < len && accuracy[pos] < size_r) {
			++pos;
		}

		// If there is a upper accuracy.
		if (pos < len) {
			auto _acc = static_cast<long long>(accuracy[pos]);
			add_numeric_prefix(added_prefixes, nfps, queryparser, acc_prefix[pos]);
			auto aux = start - (start % _acc);
			auto aux2 = end - (end % _acc);
			std::string prefix_dot(acc_prefix[pos] + ":");
			result_terms.assign(prefix_dot).append(to_query_string(std::to_string(aux)));
			if (aux != aux2) {
				result_terms.append(" OR ").append(prefix_dot).append(to_query_string(std::to_string(aux2)));
			}
		}

		// If there is a lower accuracy.
		if (--pos >= 0) {
			auto _acc = static_cast<long long>(accuracy[pos]);
			start = start - (start % _acc);
			end = end - (end % _acc);
			long long aux = (end - start) / _acc;
			// If terms are not too many.
			if (aux < MAX_TERMS) {
				std::string prefix_dot(acc_prefix[pos] + ":");
				add_numeric_prefix(added_prefixes, nfps, queryparser, acc_prefix[pos]);
				std::string or_terms(prefix_dot + to_query_string(std::to_string(start)));
				for (int i = 1; i < aux; ++i) {
					long long aux2 = start + accuracy[pos] * i;
					or_terms.append(" OR ").append(prefix_dot).append(to_query_string(std::to_string(aux2)));
				}
				if (start != end) {
					or_terms.append(" OR ").append(prefix_dot).append(to_query_string(std::to_string(end)));
				}
				if (result_terms.empty()) {
					result_terms.assign(or_terms);
				} else {
					result_terms.append("(").append(result_terms).append(") AND (").append(or_terms).append(")");
				}
			}
		}
	} catch (const std::exception&) {
		throw MSG_ClientError("Didn't understand numeric format: %s..%s", start_.c_str(), end_.c_str());
	}

	return result_terms;
}


std::string
GenerateTerms::date(const std::string& start_, const std::string& end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
	std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<DateFieldProcessor>>& dfps, Xapian::QueryParser& queryparser)
{
	std::string result_terms;

	if (accuracy.empty() || start_.empty() || end_.empty()) {
		return result_terms;
	}

	try {
		auto start = Datetime::timestamp(start_);
		auto end = Datetime::timestamp(end_);

		if (end < start) {
			return result_terms;
		}

		auto timestamp_s = static_cast<time_t>(start);
		auto timestamp_e = static_cast<time_t>(end);

		auto timeinfo = gmtime(&timestamp_s);
		int tm_s[6] = { timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year };
		timeinfo = gmtime(&timestamp_e);
		int tm_e[6] = { timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year };

		// Find the accuracy needed.
		int acc = toUType(unitTime::YEAR);
		while (acc >= toUType(unitTime::SECOND) && (tm_e[acc] - tm_s[acc]) == 0) {
			--acc;
		}

		// Find the upper or equal accuracy.
		auto pos = 0u;
		while (accuracy[pos] < acc) {
			++pos;
		}

		// If the accuracy needed is in accuracy.
		if (acc == accuracy[pos]) {
			switch ((unitTime)accuracy[pos]) {
				case unitTime::YEAR:
					if ((tm_e[5] - tm_s[5]) > MAX_TERMS) {
						return result_terms;
					}
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(year(tm_s, tm_e, acc_prefix[pos]));
					break;
				case unitTime::MONTH:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(month(tm_s, tm_e, acc_prefix[pos]));
					break;
				case unitTime::DAY:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(day(tm_s, tm_e, acc_prefix[pos]));
					break;
				case unitTime::HOUR:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(hour(tm_s, tm_e, acc_prefix[pos]));
					break;
				case unitTime::MINUTE:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(minute(tm_s, tm_e, acc_prefix[pos]));
					break;
				case unitTime::SECOND:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					result_terms.assign(second(tm_s, tm_e, acc_prefix[pos]));
					break;
			}
		}

		// If there is an upper accuracy
		if (accuracy[pos] != acc || ++pos < accuracy.size()) {
			switch ((unitTime)accuracy[pos]) {
				case unitTime::YEAR:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(year(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(year(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
				case unitTime::MONTH:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(month(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(month(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
				case unitTime::DAY:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(day(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(day(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
				case unitTime::HOUR:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(hour(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(hour(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
				case unitTime::MINUTE:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(minute(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(minute(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
				case unitTime::SECOND:
					add_date_prefix(added_prefixes, dfps, queryparser, acc_prefix[pos]);
					if (result_terms.empty()) {
						result_terms.assign(second(tm_s, tm_e, acc_prefix[pos]));
					} else {
						result_terms.assign(second(tm_s, tm_e, acc_prefix[pos])).append(" AND (").append(result_terms).append(")");
					}
					break;
			}
		}
	} catch (const DatetimeError&) {
		throw MSG_ClientError("Didn't understand date format: %s..%s", start_.c_str(), end_.c_str());
	}

	return result_terms;
}


std::string
GenerateTerms::year(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_s[4] = tm_e[0] = tm_e[1] = tm_e[2] = tm_e[4] = 0;
	tm_s[3] = tm_e[3] = 1;
	while (tm_s[5] != tm_e[5]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[5];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::month(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_e[1] = tm_e[2] = tm_e[3] = 0;
	tm_s[3] = tm_e[3] = 1;
	while (tm_s[4] != tm_e[4]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[4];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::day(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	tm_s[0] = tm_s[1] = tm_s[2] = tm_e[0] = tm_e[1] = tm_e[2] = 0;
	while (tm_s[3] != tm_e[3]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[3];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::hour(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	tm_s[0] = tm_s[1] = tm_e[0] = tm_e[1] = 0;
	while (tm_s[2] != tm_e[2]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[2];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::minute(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	tm_s[0] = tm_e[0] = 0;
	while (tm_s[1] != tm_e[1]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[1];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::second(int tm_s[], int tm_e[], const std::string& prefix)
{
	std::string prefix_dot(prefix + ":"), res;
	while (tm_s[0] != tm_e[0]) {
		res.append(prefix_dot).append(to_query_string(transform_to_string(tm_s))).append(" OR ");
		++tm_s[0];
	}
	res.append(prefix_dot).append(to_query_string(transform_to_string(tm_e)));
	return res;
}


std::string
GenerateTerms::geo(const std::vector<range_t>& ranges,  const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix,
	std::unordered_set<std::string>& added_prefixes, std::vector<std::unique_ptr<GeoFieldProcessor>>& gfps, Xapian::QueryParser& queryparser)
{
	std::string last_valid, result_terms;

	// The user does not specify the accuracy.
	if (accuracy.empty()) {
		return result_terms;
	}

	for (const auto& range : ranges) {
		std::bitset<SIZE_BITS_ID> b1(range.start), b2(range.end), res;
		auto idx = -1;
		uint64_t val;
		if (range.start != range.end) {
			for (idx = SIZE_BITS_ID - 1; idx > 0 && b1.test(idx) == b2.test(idx); --idx) {
				res.set(idx, b1.test(idx));
			}
			val = res.to_ullong();
		} else {
			val = range.start;
		}

		for (auto i = static_cast<int>(accuracy.size() - 1); i > 1; --i) {
			if ((START_POS - 2 * accuracy[i]) > idx) {
				uint64_t id_trixel = val >> static_cast<int>(START_POS - accuracy[i] * 2);
				auto pos = i - 2;
				add_geo_prefix(added_prefixes, gfps, queryparser, acc_prefix[pos]);
				if (last_valid.empty()) {
					last_valid.assign(std::bitset<SIZE_BITS_ID>(id_trixel).to_string());
					last_valid.assign(last_valid.substr(last_valid.find('1')));
					result_terms.assign(acc_prefix[pos]).append(":").append(std::to_string(id_trixel));
				} else if (isnotSubtrixel(last_valid, id_trixel)) {
					result_terms.append(" OR ").append(acc_prefix[pos]).append(1, ':').append(std::to_string(id_trixel));
				}
				break;
			}
		}
	}

	return result_terms;
}
