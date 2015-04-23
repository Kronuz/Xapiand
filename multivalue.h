/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#ifndef XAPIAND_INCLUDED_MULTIVALUE_H
#define XAPIAND_INCLUDED_MULTIVALUE_H

#include <xapian.h>

#include <string.h>
#include <vector>


class StringList : public std::vector<std::string> {
public:
	void unserialise(const std::string & serialised);
	void unserialise(const char ** ptr, const char * end);
	std::string serialise() const;

};


/// Class for counting the frequencies of values in the matching documents.
class MultiValueCountMatchSpy : public Xapian::ValueCountMatchSpy {
  public:
	/// Construct an empty MultiValueCountMatchSpy.
	MultiValueCountMatchSpy(): Xapian::ValueCountMatchSpy() {}

	/** Construct a MatchSpy which counts the values in a particular slot.
	 *
	 *  Further slots can be added by calling @a add_slot().
	 */
	MultiValueCountMatchSpy(Xapian::valueno slot_) : Xapian::ValueCountMatchSpy(slot_) {}

	/** Implementation of virtual operator().
	 *
	 *  This implementation tallies values for a matching document.
	 */
	void operator()(const Xapian::Document &doc, double wt);

	virtual Xapian::MatchSpy * clone() const;
	virtual std::string name() const;
	virtual std::string serialise() const;
	virtual Xapian::MatchSpy * unserialise(const std::string & serialised,
								   const Xapian::Registry & context) const;
	virtual std::string get_description() const;
};

#endif /* XAPIAND_INCLUDED_MULTIVALUE_H */
