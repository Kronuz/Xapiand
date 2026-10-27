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

#include <cstddef>                // for std::size_t
#include <string>                  // for std::string
#include <memory>                  // for std::unique_ptr
#include <utility>                 // for std::move

#include "engine/doc_view.h"       // for DocView
#include "engine/document.h"       // for Document
#include "engine/error.h"          // for Error, errc
#include "engine/types.h"          // for WriteResult
#include "xapian.h"                // for Xapian::Document, Xapian::DocumentInfo, Xapian::Error


// The Xapian implementation of the engine seam. Everything that names Xapian lives here.
namespace engine::xapian {

// The document of the Xapian implementation: a Xapian document, built in place.
class XapianDocument final : public Document::Native {
public:
	explicit XapianDocument(Xapian::Document doc = {}) : doc_(std::move(doc)) { }

	void add_term(std::string_view term, termcount wdf_inc) override;
	void add_boolean_term(std::string_view term) override;
	void add_posting(std::string_view term, termpos position, termcount wdf_inc) override;
	void add_value(valueno slot, std::string value) override;
	std::string get_value(valueno slot) const override;
	void set_data(std::string data) override;
	std::string get_data() const override;
	std::string serialise() const override;

	const Xapian::Document& xapian() const noexcept { return doc_; }

private:
	Xapian::Document doc_;
};

// The Xapian document inside an engine document of this implementation.
const Xapian::Document& xapian_document(const Document& doc);

// An engine document with a copy of the terms, positions, values and data.
Document from_xapian(const Xapian::Document& doc);

WriteResult from_xapian(const Xapian::DocumentInfo& info);
Xapian::DocumentInfo to_xapian(const WriteResult& result);

// The kind of failure the exception being handled reports. Call only inside a catch block.
errc classify_current() noexcept;

// The Xapian library version, "major.minor.revision".
std::string version();

// A lazy view over a Xapian document.
std::unique_ptr<DocView> make_view(Xapian::Document doc);

// The views made and not yet destroyed, for the tests that check ownership.
std::size_t live_views() noexcept;

}  // namespace engine::xapian
