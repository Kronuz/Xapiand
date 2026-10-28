/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// libFuzzer target for the strata document's decoder (the bytes the write-ahead log keeps): any input
// decodes to a document that serialises back to the same bytes, or raises a syntax error.

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint8_t
#include <cstdlib>                 // for std::abort
#include <string_view>             // for std::string_view

#include "engine/error.h"          // for Error, errc
#include "engine/strata/document.h"  // for unserialise_document


extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	const std::string_view bytes(reinterpret_cast<const char*>(data), size);
	try {
		const auto doc = engine::strata::unserialise_document(bytes);
		const auto again = engine::strata::unserialise_document(doc.serialise());
		if (again.serialise() != doc.serialise()) {
			std::abort();  // the form is not a fixed point
		}
		const auto& before = engine::strata::strata_document(doc);
		const auto& after = engine::strata::strata_document(again);
		if (before.version() != after.version() || before.replayed() != after.replayed() || before.replayed() != (before.version() != 0)) {
			std::abort();  // the version the log kept is the one replay reads
		}
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::syntax) {
			std::abort();
		}
	}
	return 0;
}
