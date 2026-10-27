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

#include <memory>           // for std::unique_ptr
#include <string>           // for string
#include <string_view>      // for string_view

#include "engine/callbacks.h"  // for engine::PostingSource
#include "engine/query.h"   // for engine::Query
#include "engine/types.h"   // for engine::valueno
#include "msgpack.h"        // for MsgPack


struct required_spc_t;


// A value source for a multiple value range: some of the document's values are inside [start, end].
class MultipleValueRange : public engine::PostingSource {
	engine::valueno slot_ = 0;

	// Range [start, end] for the search.
	std::string start, end;

	// Calculate if some their values is inside range.
	bool insideRange(std::string_view value) const noexcept;

	// Get the geospatial query
	static engine::Query query_geo(std::string_view str, const required_spc_t& field_spc);

public:
	MultipleValueRange() = default;

	/* Construct a new source which returns only documents with a
	 *  some of their values inside of [start, end].
	 *
	 *  @param slot The value slot to read values from.
	 *  @param start_  range's start.
	 *  @param end_ range's end.
	*/
	template <typename T, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<T>>::value>>
	MultipleValueRange(engine::valueno slot, T&& start_, T&& end_);

	engine::valueno slot() const override { return slot_; }
	bool accept(std::string_view value) const override { return insideRange(value); }
	double weight(std::string_view value) const override;
	std::unique_ptr<engine::PostingSource> clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view serialised) const override;
	std::string description() const override;

	// Call this function for create a new Query based in ranges.
	static engine::Query getQuery(const required_spc_t& field_spc, const MsgPack& obj);
};


// A value source for multiple value GE: some of the document's values are inside [start, ..].
class MultipleValueGE : public engine::PostingSource {
	engine::valueno slot_ = 0;

	// Range [start, ..] for the search.
	std::string start;

	// Calculate if some their values is inside range.
	bool insideRange(std::string_view value) const noexcept;

public:
	MultipleValueGE() = default;

	/* Construct a new source which returns only documents with a
	 *  some of their values inside of [start, ..].
	 *
	 *  @param slot The value slot to read values from.
	 *  @param start_  range's start.
	*/
	template <typename T, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<T>>::value>>
	MultipleValueGE(engine::valueno slot, T&& start_);

	engine::valueno slot() const override { return slot_; }
	bool accept(std::string_view value) const override { return insideRange(value); }
	double weight(std::string_view value) const override;
	std::unique_ptr<engine::PostingSource> clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view serialised) const override;
	std::string description() const override;
};


// A value source for multiple value LE: some of the document's values are inside [.., end].
class MultipleValueLE : public engine::PostingSource {
	engine::valueno slot_ = 0;

	// Range [.., end] for the search.
	std::string end;

	// Calculate if some their values is inside range.
	bool insideRange(std::string_view value) const noexcept;

public:
	MultipleValueLE() = default;

	/* Construct a new source which returns only documents with a
	 *  some of their values inside of [.., end].
	 *
	 *  @param slot The value slot to read values from.
	 *  @param end_  range's end.
	*/
	template <typename T, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<T>>::value>>
	MultipleValueLE(engine::valueno slot, T&& end_);

	engine::valueno slot() const override { return slot_; }
	bool accept(std::string_view value) const override { return insideRange(value); }
	double weight(std::string_view value) const override;
	std::unique_ptr<engine::PostingSource> clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view serialised) const override;
	std::string description() const override;
};
