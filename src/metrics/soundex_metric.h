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
class SoundexMetric : public Metric {
	SoundexLan _soundex;

	double _distance(std::string_view str1, std::string_view str2) const {
		return Metric::distance(_soundex.encode(str1), _soundex.encode(str2));
	}

	double _distance(std::string_view str2) const {
		return Metric::distance(_soundex.encode(str2));
	}

	double _similarity(std::string_view str1, std::string_view str2) const {
		return Metric::similarity(_soundex.encode(str1), _soundex.encode(str2));
	}

	double _similarity(std::string_view str2) const {
		return Metric::similarity(_soundex.encode(str2));
	}

	std::string _description() const {
		std::string desc("SoundexMetric<");
		desc.append(_soundex.description()).append(", ").append(Metric::description());
		desc.push_back('>');
		return desc;
	}

public:

	/*
	 * Soundex is icase, therefore the parameter is not necessary.
	 * Always icase=false to optimize.
	 */
	SoundexMetric(bool = false)
		: Metric(false) { }

	template <typename T>
	SoundexMetric(T&& str, bool = false) :
		Metric(false),
		_soundex(str) {
		this->_str = _soundex.encode();
	}

	std::string serialise() const override {
		std::string serialised;
		serialised += Metric::serialise();
		serialised += _soundex.serialise();
		return serialised;
	}

	void unserialise(const char** p, const char* p_end) override {
		Metric::unserialise(p, p_end);
		_soundex.unserialise(p, p_end);
	}

	std::string_view name() const noexcept {
		return _soundex.name();
	}
};
