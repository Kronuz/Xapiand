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

#include "engine/doc_view.h"       // for DocView
#include "engine/document.h"       // for Document
#include "engine/error.h"          // for Error, errc
#include "engine/types.h"          // for WriteResult
#include "xapian.h"                // for Xapian::Document, Xapian::DocumentInfo, Xapian::Error


// The Xapian implementation of the engine seam. Everything that names Xapian lives here.
namespace engine::xapian {

// A Xapian document with the same terms, positions, values and data.
Xapian::Document to_xapian(const Document& doc);

// The engine document with the same terms, positions, values and data.
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
