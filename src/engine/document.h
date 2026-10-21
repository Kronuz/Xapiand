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

#include <map>                     // for std::map
#include <set>                     // for std::set
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move

#include "engine/types.h"          // for docid, termcount, termpos, valueno


namespace engine {

// A document under construction: its terms, values and stored data, owned by the caller and
// independent of any engine. Each add_* call has the semantics of the Xapian call it replaces,
// so building one is the same as building that engine's document.
class Document {
public:
	struct Term {
		termcount wdf = 0;
		std::set<termpos> positions;
	};

	// Adds wdf_inc to the term's within-document frequency, creating the term if needed.
	void add_term(std::string term, termcount wdf_inc = 1) {
		terms_[std::move(term)].wdf += wdf_inc;
	}

	// Makes sure the term exists, without changing its frequency.
	void add_boolean_term(std::string term) {
		terms_[std::move(term)];
	}

	// Adds a position to the term, and wdf_inc to its frequency.
	void add_posting(std::string term, termpos position, termcount wdf_inc = 1) {
		auto& entry = terms_[std::move(term)];
		entry.positions.insert(position);
		entry.wdf += wdf_inc;
	}

	// Sets the value of a slot; an empty value removes it.
	void add_value(valueno slot, std::string value) {
		if (value.empty()) {
			values_.erase(slot);
		} else {
			values_[slot] = std::move(value);
		}
	}

	// The value of a slot, empty when unset.
	std::string get_value(valueno slot) const {
		auto it = values_.find(slot);
		return it == values_.end() ? std::string() : it->second;
	}

	void set_data(std::string data) {
		data_ = std::move(data);
	}

	std::string get_data() const {
		return data_;
	}

	const std::map<std::string, Term>& terms() const noexcept {
		return terms_;
	}

	const std::map<valueno, std::string>& values() const noexcept {
		return values_;
	}

	// The stored form of a document, as the write-ahead log keeps it. Defined by the engine.
	std::string serialise() const;
	static Document unserialise(std::string_view serialised);

private:
	std::map<std::string, Term> terms_;
	std::map<valueno, std::string> values_;
	std::string data_;
};

}  // namespace engine
