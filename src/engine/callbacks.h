/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "engine/doc_view.h"       // for DocView
#include "engine/types.h"          // for valueno


// Host callbacks: code Xapiand hands to an engine, which calls it while matching. Each has a
// name, a serialised form and a clone, so an engine can ship it to another process and rebuild
// it there. An engine that cannot rebuild one never calls unserialise.
namespace engine {

// The documents whose value in one slot passes a test. The engine reads the slot's values
// in docid order and asks accept() of each; weight() scores a document that passed.
//
// accept, weight and clone are const and thread-safe: one source is shared by every
// translation of its query, and later by every shard. accept returns false on a value that
// does not decode. A selectivity estimate, for an engine's planner, is planned, not built.
class PostingSource {
public:
	virtual ~PostingSource() = default;

	virtual valueno slot() const = 0;
	virtual bool accept(std::string_view value) const = 0;
	virtual double weight(std::string_view value) const = 0;

	// The most weight() can return; 0 when the source does not say.
	virtual double max_weight() const {
		return 0.0;
	}

	virtual std::unique_ptr<PostingSource> clone() const = 0;
	virtual std::string name() const = 0;
	virtual std::string serialise() const = 0;
	// Throws Error(errc::invalid) on bytes that serialise() could not have produced.
	virtual std::unique_ptr<PostingSource> unserialise(std::string_view serialised) const = 0;
	virtual std::string description() const = 0;
};


// A sort key built from a matching document: documents order by memcmp of their keys.
class KeyMaker {
public:
	virtual ~KeyMaker() = default;

	virtual std::string key(const DocView& doc) const = 0;

	virtual std::unique_ptr<KeyMaker> clone() const = 0;
	virtual std::string name() const = 0;
	virtual std::string serialise() const = 0;
	// Throws Error(errc::invalid) on bytes that serialise() could not have produced.
	virtual std::unique_ptr<KeyMaker> unserialise(std::string_view serialised) const = 0;
};


// Sees every matching document, with its weight, and accumulates a result. Results from
// several shards merge through their serialised form.
class MatchSpy {
public:
	virtual ~MatchSpy() = default;

	virtual void see(const DocView& doc, double weight) = 0;

	virtual std::unique_ptr<MatchSpy> clone() const = 0;
	virtual std::string name() const = 0;
	virtual std::string serialise() const = 0;
	// Throws Error(errc::invalid) on bytes that serialise() could not have produced.
	virtual std::unique_ptr<MatchSpy> unserialise(std::string_view serialised) const = 0;
	virtual std::string serialise_results() const = 0;
	virtual void merge_results(std::string_view serialised) = 0;
	virtual std::string description() const = 0;
};

}  // namespace engine
