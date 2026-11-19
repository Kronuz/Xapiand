/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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
#include <string_view>                         // for std::string_view
#include <utility>                             // for std::move

#include "length.h"                            // for serialize_bool/string/length, unserialize_*
#include "engine/codec.h"                      // for engine::serialize_double, unserialize_double
#include "string_metric.h"                     // the library metrics (Levenshtein, Jaro, ..., PhoneticMetric)

// The string-similarity library is the pure algorithms; it drops the
// serialize()/unserialize() persistence (that pulled length.h + GPL
// serialize-double). Re-attach it here as thin subclasses that (de)serialize the
// metric's protected state, reproducing the original on-the-wire format exactly.
// StringKey<...> is instantiated with these wrappers in keymaker.{h,cc}.

// Base state (icase + comparison string): for metrics with no extra params
// (Jaro, Jaccard, Sorensen_Dice, LCSubstr, LCSubsequence).
template <typename Metric>
struct SerializableMetric : Metric {
	using Metric::Metric;
	std::string serialize() const {
		std::string s;
		s += serialize_bool(this->_icase);
		s += serialize_string(this->_str);
		return s;
	}
	void unserialize(const char** p, const char* p_end) {
		this->_icase = unserialize_bool(p, p_end);
		this->_str = unserialize_string(p, p_end);
	}
};

// Levenshtein adds its three costs (serialize_length), matching the original.
struct SerializableLevenshtein : Levenshtein {
	using Levenshtein::Levenshtein;
	std::string serialize() const {
		std::string s;
		s += serialize_bool(_icase);
		s += serialize_string(_str);
		s += serialize_length(_subst_cost);
		s += serialize_length(_ins_del_cost);
		s += serialize_length(_maxCost);
		return s;
	}
	void unserialize(const char** p, const char* p_end) {
		_icase = unserialize_bool(p, p_end);
		_str = unserialize_string(p, p_end);
		_subst_cost = unserialize_length(p, p_end);
		_ins_del_cost = unserialize_length(p, p_end);
		_maxCost = unserialize_length(p, p_end);
	}
};

// Jaro-Winkler adds its scaling factor and boost threshold (serialize_double).
struct SerializableJaroWinkler : Jaro_Winkler {
	using Jaro_Winkler::Jaro_Winkler;
	std::string serialize() const {
		std::string s;
		s += serialize_bool(_icase);
		s += serialize_string(_str);
		s += engine::serialize_double(_p);
		s += engine::serialize_double(_bt);
		return s;
	}
	void unserialize(const char** p, const char* p_end) {
		_icase = unserialize_bool(p, p_end);
		_str = unserialize_string(p, p_end);
		_p = engine::unserialize_double(p, p_end);
		_bt = engine::unserialize_double(p, p_end);
	}
};

// PhoneticMetric: base state + the encoder's cached code. The code equals _str
// (it is encode(value)) and is not needed to compute distance, but the original
// format wrote it, so write it (from the protected encoder's public encode()
// getter) and consume it on the way back in.
template <typename Encoder, typename Metric>
struct SerializablePhoneticMetric : PhoneticMetric<Encoder, Metric> {
	using PhoneticMetric<Encoder, Metric>::PhoneticMetric;
	std::string serialize() const {
		std::string s;
		s += serialize_bool(this->_icase);
		s += serialize_string(this->_str);
		s += serialize_string(this->_soundex.encode());
		return s;
	}
	// Restores the encoder's cached code (its _code_str) without re-encoding it.
	struct Restored : Encoder {
		explicit Restored(std::string code) { this->_code_str = std::move(code); }
	};
	void unserialize(const char** p, const char* p_end) {
		this->_icase = unserialize_bool(p, p_end);
		this->_str = unserialize_string(p, p_end);
		this->_soundex = Restored(std::string(unserialize_string(p, p_end)));
	}
};
