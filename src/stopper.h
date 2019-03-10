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

#include <algorithm>               // for std::transform, std::min
#include <iterator>                // for std::back_inserter
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint32_t
#include <memory>                  // for std::unique_ptr, std::make_unique

#include "hashes.hh"               // for fnv1ah32
#include "phf.hh"                  // for phf
#include "xapian.h"                // for Xapian::Stopper


// Same implementation as Xapian::SimpleStopper, only this uses perfect hashes
// which is much faster ~ 5.24591s -> 0.861319s
template <std::size_t max_size = 1000>
class SimpleStopper : public Xapian::Stopper {
	phf::phf<phf::fast_phf, std::uint32_t, max_size> stop_words;

public:
	SimpleStopper() { }

	template <class Iterator>
	SimpleStopper(Iterator begin, Iterator end) {
		std::vector<std::uint32_t> result;
		result.reserve(max_size);
		std::transform(begin, end, std::back_inserter(result), fnv1ah32{});
		stop_words.assign(result.data(), std::min(max_size, result.size()));
	}

	virtual bool operator()(const std::string& term) const {
		return stop_words.count(hh(term));
	}
};

const std::unique_ptr<Xapian::Stopper>& getStopper(std::string_view language);
