/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "basic_string_metric.h"


/*
 * String Metric based in Soundex.
 *
 * First encodes the strings using SoundexLan and after
 * uses Metric for get a edit distance or similarity.
 *
 * Phonetic-based metric.
 */
template <typename SoundexLan, typename Metric>
class SoundexMetric : public StringMetric<SoundexMetric<SoundexLan, Metric>> {
	SoundexLan _soundex;
	Metric _metric;

	friend class StringMetric<SoundexMetric<SoundexLan, Metric>>;

	double _distance(const std::string& str1, const std::string& str2) const {
		return _metric.distance(_soundex.encode(str1), _soundex.encode(str2));
	}

	double _distance(const std::string& str2) const {
		return _metric.distance(_soundex.encode(str2));
	}

	double _similarity(const std::string& str1, const std::string& str2) const {
		return _metric.similarity(_soundex.encode(str1), _soundex.encode(str2));
	}

	double _similarity(const std::string& str2) const {
		return _metric.similarity(_soundex.encode(str2));
	}

	std::string _description() const {
		std::string desc("SoundexMetric<");
		desc.append(_soundex.description()).append(", ").append(_metric.description());
		desc.push_back('>');
		return desc;
	}

public:

	/*
	 * Soundex is icase, therefore the parameter is not necessary.
	 * Always icase=false to optimize.
	 */

	SoundexMetric(bool=false)
		: StringMetric<SoundexMetric<SoundexLan, Metric>>(false),
		  _metric(false) { }

	template <typename T>
	SoundexMetric(T&& str, bool=false)
		: StringMetric<SoundexMetric<SoundexLan, Metric>>(std::forward<T>(str), false),
		  _soundex(str),
		  _metric(_soundex.encode(), false) { }
};
