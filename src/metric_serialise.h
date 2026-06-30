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

#include <string>                              // for std::string

#include "length.h"                            // for serialise_bool/string/length, unserialise_*
#include "xapian/common/serialise-double.h"    // for serialise_double, unserialise_double
#include "string_metric.h"                     // the library metrics (Levenshtein, Jaro, ..., PhoneticMetric)

// The string-similarity library is the pure algorithms; it drops the
// serialise()/unserialise() persistence (that pulled length.h + GPL
// serialise-double). Re-attach it here as thin subclasses that (de)serialise the
// metric's protected state, reproducing the original on-the-wire format exactly.
// StringKey<...> is instantiated with these wrappers in keymaker.{h,cc}.

// Base state (icase + comparison string): for metrics with no extra params
// (Jaro, Jaccard, Sorensen_Dice, LCSubstr, LCSubsequence).
template <typename Metric>
struct SerialisableMetric : Metric {
	using Metric::Metric;
	std::string serialise() const {
		std::string s;
		s += serialise_bool(this->_icase);
		s += serialise_string(this->_str);
		return s;
	}
	void unserialise(const char** p, const char* p_end) {
		this->_icase = unserialise_bool(p, p_end);
		this->_str = unserialise_string(p, p_end);
	}
};

// Levenshtein adds its three costs (serialise_length), matching the original.
struct SerialisableLevenshtein : Levenshtein {
	using Levenshtein::Levenshtein;
	std::string serialise() const {
		std::string s;
		s += serialise_bool(_icase);
		s += serialise_string(_str);
		s += serialise_length(_subst_cost);
		s += serialise_length(_ins_del_cost);
		s += serialise_length(_maxCost);
		return s;
	}
	void unserialise(const char** p, const char* p_end) {
		_icase = unserialise_bool(p, p_end);
		_str = unserialise_string(p, p_end);
		_subst_cost = unserialise_length(p, p_end);
		_ins_del_cost = unserialise_length(p, p_end);
		_maxCost = unserialise_length(p, p_end);
	}
};

// Jaro-Winkler adds its scaling factor and boost threshold (serialise_double).
struct SerialisableJaroWinkler : Jaro_Winkler {
	using Jaro_Winkler::Jaro_Winkler;
	std::string serialise() const {
		std::string s;
		s += serialise_bool(_icase);
		s += serialise_string(_str);
		s += serialise_double(_p);
		s += serialise_double(_bt);
		return s;
	}
	void unserialise(const char** p, const char* p_end) {
		_icase = unserialise_bool(p, p_end);
		_str = unserialise_string(p, p_end);
		_p = unserialise_double(p, p_end);
		_bt = unserialise_double(p, p_end);
	}
};

// PhoneticMetric: base state + the encoder's cached code. The code equals _str
// (it is encode(value)) and is not needed to compute distance, but the original
// format wrote it, so write it (from the protected encoder's public encode()
// getter) and consume it on the way back in.
template <typename Encoder, typename Metric>
struct SerialisablePhoneticMetric : PhoneticMetric<Encoder, Metric> {
	using PhoneticMetric<Encoder, Metric>::PhoneticMetric;
	std::string serialise() const {
		std::string s;
		s += serialise_bool(this->_icase);
		s += serialise_string(this->_str);
		s += serialise_string(this->_soundex.encode());
		return s;
	}
	void unserialise(const char** p, const char* p_end) {
		this->_icase = unserialise_bool(p, p_end);
		this->_str = unserialise_string(p, p_end);
		unserialise_string(p, p_end);  // consume the redundant encoder code
	}
};
