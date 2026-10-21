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

#include <stdexcept>               // for std::invalid_argument
#include <string>                  // for std::string

#include "snowball.hh"             // for snowball::stemmer
#include "xapian.h"                // for Xapian::Stem, Xapian::StemImplementation


// A Xapian::Stem backed by the snowball library, so the query parser stems exactly
// as the term generator does at index time. Each Stem owns its own stemmer.
class SnowballStem : public Xapian::StemImplementation {
	snowball::stemmer stemmer;

public:
	explicit SnowballStem(const std::string& language) : stemmer(language) { }

	std::string operator()(const std::string& word) override {
		return std::string(stemmer(word));
	}

	std::string get_description() const override {
		return "Xapian::Stem(snowball)";
	}
};


inline Xapian::Stem
makeStem(const std::string& language)
{
	try {
		return Xapian::Stem(new SnowballStem(language));
	} catch (const std::invalid_argument&) {
		throw Xapian::InvalidArgumentError("Language code " + language + " unknown");
	}
}
