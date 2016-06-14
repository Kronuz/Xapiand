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

#include <string.h>
#include <vector>


struct search_t;


// New Match Decider for multiple value range.
class MultipleValueRange : public Xapian::ValuePostingSource {
	// Range [start, end] for the search.
	std::string start, end;

	// Calculate if some their values is inside range.
	bool insideRange() const noexcept;

public:
	/* Construct a new match decider which returns only documents with a
	 *  some of their values inside of [start, end].
	 *
	 *  @param slot_ The value slot to read values from.
	 *  @param start_  range's start.
	 *  @param end_ range's end.
	*/
	MultipleValueRange(Xapian::valueno slot_, const std::string& start_, const std::string& end_);

	void next(double min_wt) override;
	void skip_to(Xapian::docid min_docid, double min_wt) override;
	bool check(Xapian::docid min_docid, double min_wt) override;
	double get_weight() const override;
	MultipleValueRange* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	MultipleValueRange* unserialise_with_registry(const std::string& serialised, const Xapian::Registry&) const override;
	void init(const Xapian::Database& db_) override;
	std::string get_description() const override;

	// Call this function for create a new Query based in ranges.
	static Xapian::Query getQuery(Xapian::valueno slot_, char field_type, std::string& start_, std::string& end_, const std::string& field_name, search_t& srch);
};


// New Match Decider for multiple value GE.
class MultipleValueGE : public Xapian::ValuePostingSource {
	// Range [start, ..] for the search.
	std::string start;

	// Calculate if some their values is inside range.
	bool insideRange() const noexcept;

public:
	/* Construct a new match decider which returns only documents with a
	 *  some of their values inside of [start, ..].
	 *
	 *  @param slot_ The value slot to read values from.
	 *  @param start_  range's start.
	*/
	MultipleValueGE(Xapian::valueno slot_, const std::string& start_);

	void next(double min_wt) override;
	void skip_to(Xapian::docid min_docid, double min_wt) override;
	bool check(Xapian::docid min_docid, double min_wt) override;
	double get_weight() const override;
	MultipleValueGE* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	MultipleValueGE* unserialise_with_registry(const std::string& serialised, const Xapian::Registry&) const override;
	void init(const Xapian::Database& db_) override;
	std::string get_description() const override;
};


// New Match Decider for multiple value LE.
class MultipleValueLE : public Xapian::ValuePostingSource {
	// Range [.., end] for the search.
	std::string end;

	// Calculate if some their values is inside range.
	bool insideRange() const noexcept;

public:
	/* Construct a new match decider which returns only documents with a
	 *  some of their values inside of [.., end].
	 *
	 *  @param slot_ The value slot to read values from.
	 *  @param end_  range's end.
	*/
	MultipleValueLE(Xapian::valueno slot_, const std::string& end_);

	void next(double min_wt) override;
	void skip_to(Xapian::docid min_docid, double min_wt) override;
	bool check(Xapian::docid min_docid, double min_wt) override;
	double get_weight() const override;
	MultipleValueLE* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	MultipleValueLE* unserialise_with_registry(const std::string& serialised, const Xapian::Registry&) const override;
	void init(const Xapian::Database& db_) override;
	std::string get_description() const override;
};
