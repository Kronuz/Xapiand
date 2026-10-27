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

#include <functional>              // for std::function
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector

#include "engine/types.h"          // for docid, termcount, valueno


namespace engine {

// Called once per term of a document.
using TermSink = std::function<void(std::string_view term, termcount wdf)>;

// Called once per value slot that holds a value, in slot order.
using ValueSink = std::function<void(valueno slot, std::string_view value)>;

// One term of a document with what the info endpoints report about it.
struct TermDetail {
	std::string_view term;                 // valid only during the sink call; copy it to keep it
	termcount wdf = 0;
	std::optional<doccount> termfreq;  // empty when the document is not tied to a database
	std::vector<termpos> positions;    // empty when the term has none
};
using TermDetailSink = std::function<void(const TermDetail&)>;

// A stored document read lazily from a shard: the engine fetches only what is asked for.
class DocView {
public:
	virtual ~DocView() = default;

	virtual docid did() const = 0;
	virtual std::string data() const = 0;
	virtual std::string value(valueno slot) const = 0;
	virtual rev version() const = 0;
	virtual void terms(std::string_view prefix, const TermSink& sink) const = 0;

	// Every value, in slot order.
	virtual void values(const ValueSink& sink) const = 0;

	// Every term in term order, with its frequency and positions.
	virtual void term_details(const TermDetailSink& sink) const = 0;

	// The document in the form `engine::Document::unserialise` and the log read back.
	virtual std::string serialise() const = 0;
};

}  // namespace engine
