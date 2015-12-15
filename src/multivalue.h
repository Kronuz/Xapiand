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

#pragma once

#include <xapian.h>

#include <string>
#include <vector>


/*
 * This class serializes a string vector.
 * i.e
 * StringList = {a, ..., b}
 * values = encode_length(a.size()) + a + ... + encode_length(b.size()) + b
 * symbol '+' means concatenate
 * serialise = encode_length(values.size())values
 */
class StringList : public std::vector<std::string> {
public:
	void unserialise(const std::string& serialised);
	void unserialise(const char** ptr, const char* end);
	std::string serialise() const;

};


/// Class for counting the frequencies of values in the matching documents.
class MultiValueCountMatchSpy : public Xapian::ValueCountMatchSpy {
	bool is_geo;

public:
	/// Construct an empty MultiValueCountMatchSpy.
	MultiValueCountMatchSpy() = default;

	/** Construct a MatchSpy which counts the values in a particular slot.
	 *
	 *  Further slots can be added by calling @a add_slot().
	 */
	MultiValueCountMatchSpy(Xapian::valueno slot_, bool _is_geo = false) : Xapian::ValueCountMatchSpy(slot_), is_geo(_is_geo) { }

	/** Implementation of virtual operator().
	 *
	 *  This implementation tallies values for a matching document.
	 */
	void operator()(const Xapian::Document &doc, double wt) override;

	Xapian::MatchSpy* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	Xapian::MatchSpy* unserialise(const std::string& serialised, const Xapian::Registry& context) const override;
	std::string get_description() const override;
};
