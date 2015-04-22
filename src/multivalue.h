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

#include <string>

#define MULTIVALUE_MAGIC "\xef\xcd"

/// Class to serialise a list of strings in a form suitable for
/// ValueCountMatchSpy.
class StringListSerialiser {
  private:
	int items;
	std::string values;

  public:
	/// Default constructor.
	StringListSerialiser() : items(0) {}

	/// Initialise with a string.
	/// (The string represents a serialised form, rather than a single value to
	/// be serialised.)
	StringListSerialiser(const std::string & initial) : items(0) {
		append(initial);
	}

	/// Initialise from a pair of iterators.
	template <class Iterator>
	StringListSerialiser(Iterator begin, Iterator end) : items(0) {
		while (begin != end) append(*begin++);
	}

	/// Add a string to the end of the list.
	void append(const std::string & value);

	/// Get the serialised result.
	const std::string get() const;
};


/// Class to unserialise a list of strings serialised by a StringListSerialiser.
/// The class can be used as an iterator: use the default constructor to get
/// an end iterator.
class StringListUnserialiser {
  private:
	bool is_list;
	std::string serialised;
	std::string curritem;
	const char * pos;

	/// Read the next item from the serialised form.
	void read_next();

	/// Compare this iterator with another
	friend bool operator==(const StringListUnserialiser & a,
						   const StringListUnserialiser & b);
	friend bool operator!=(const StringListUnserialiser & a,
						   const StringListUnserialiser & b);

  public:
	/// Default constructor - use this to define an end iterator.
	StringListUnserialiser() : pos(NULL) {}

	/// Constructor which takes a serialised list of strings, and creates an
	/// iterator pointing to the first of them.
	StringListUnserialiser(const std::string & in);

	/// Copy constructor
	StringListUnserialiser(const StringListUnserialiser & other)
			: serialised(other.serialised),
			  curritem(other.curritem),
			  pos((other.pos == NULL) ? NULL : serialised.data() + (other.pos - other.serialised.data()))
	{}

	/// Assignment operator
	void operator=(const StringListUnserialiser & other) {
		serialised = other.serialised;
		curritem = other.curritem;
		pos = (other.pos == NULL) ? NULL : serialised.data() + (other.pos - other.serialised.data());
	}

	/// Get the current item
	std::string operator *() const {
		return curritem;
	}

	/// Move to the next item.
	StringListUnserialiser & operator++() {
		read_next();
		return *this;
	}

	/// Move to the next item (postfix).
	StringListUnserialiser operator++(int) {
		StringListUnserialiser tmp = *this;
		read_next();
		return tmp;
	}

	// Allow use as an STL iterator
	typedef std::input_iterator_tag iterator_category;
	typedef std::string value_type;
	typedef size_t difference_type;
	typedef std::string * pointer;
	typedef std::string & reference;
};

inline bool operator==(const StringListUnserialiser & a,
					   const StringListUnserialiser & b) {
	return (a.pos == b.pos);
}

inline bool operator!=(const StringListUnserialiser & a,
					   const StringListUnserialiser & b) {
	return (a.pos != b.pos);
}


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
