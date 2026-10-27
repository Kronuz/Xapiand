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

#ifdef ENGINE_CENSUS
#include <cstddef>                 // for std::size_t
#include <functional>              // for std::function
#endif
#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move

#include "engine/types.h"          // for docid, termcount, termpos, valueno


namespace engine {

// A document under construction: its terms, values and stored data, owned by the caller. Each
// add_* call has the semantics of the Xapian call it replaces, so building one is the same as
// building that engine's document.
//
// The document is built straight into the engine's own representation (a Xapian document for the
// Xapian engine), so a shard writes it without converting it and nothing is built twice. The
// representation is opaque here; the engine that made it is the only one that reads it back.
class Document {
public:
	// The engine's side of a document: the calls below, over its own storage. The engine defines it.
	class Native {
	public:
		virtual ~Native() = default;

		virtual void add_term(std::string_view term, termcount wdf_inc) = 0;
		virtual void add_boolean_term(std::string_view term) = 0;
		virtual void add_posting(std::string_view term, termpos position, termcount wdf_inc) = 0;
		virtual void add_value(valueno slot, std::string value) = 0;
		virtual std::string get_value(valueno slot) const = 0;
		virtual void set_data(std::string data) = 0;
		virtual std::string get_data() const = 0;
		virtual std::string serialise() const = 0;

#ifdef ENGINE_CENSUS
		// Calls visit(term, wdf, positions) for each term in term order; the number of values set.
		virtual void census_terms(const std::function<void(std::string_view, termcount, std::size_t)>& visit) const = 0;
		virtual std::size_t census_values() const = 0;
#endif
	};

	// An empty document of the engine. Defined by the engine.
	Document();
	explicit Document(std::unique_ptr<Native> native) : native_(std::move(native)) { }

	// Adds wdf_inc to the term's within-document frequency, creating the term if needed.
	void add_term(std::string_view term, termcount wdf_inc = 1) {
		native_->add_term(term, wdf_inc);
	}

	// Makes sure the term exists, without changing its frequency.
	void add_boolean_term(std::string_view term) {
		native_->add_boolean_term(term);
	}

	// Adds a position to the term, and wdf_inc to its frequency.
	void add_posting(std::string_view term, termpos position, termcount wdf_inc = 1) {
		native_->add_posting(term, position, wdf_inc);
	}

	// Sets the value of a slot; an empty value removes it.
	void add_value(valueno slot, std::string value) {
		native_->add_value(slot, std::move(value));
	}

	// The value of a slot, empty when unset.
	std::string get_value(valueno slot) const {
		return native_->get_value(slot);
	}

	void set_data(std::string data) {
		native_->set_data(std::move(data));
	}

	std::string get_data() const {
		return native_->get_data();
	}

	// The engine's own form of the document, for the engine that made it.
	Native& native() noexcept { return *native_; }
	const Native& native() const noexcept { return *native_; }

	// The stored form of a document, as the write-ahead log keeps it. Defined by the engine.
	std::string serialise() const {
		return native_->serialise();
	}
	static Document unserialise(std::string_view serialised);

private:
	std::unique_ptr<Native> native_;
};

}  // namespace engine
