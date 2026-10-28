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

#include "prospect/index.hh"       // for prospect::index

#include "engine/strata/document.h"  // for StrataDocument


// The value columns of the strata engine: the seam's opaque byte values, one single-valued bytes
// column per slot (xapiand-on-strata D3). prospect's values plugin (phase4/values-bytes, step P1)
// provides the columns; until it lands the engine stores none, so a document read back has no values
// and the shard's DocView returns empty ones. The thin adapter below is the one place to wire it:
// put() copies a document's values into the index document, and DocView::value(slot) reads
// `value(ord, slot)` from the segment.
namespace engine::strata::values {

inline constexpr bool available = false;

inline void
put(prospect::index::document&, const StrataDocument&)
{
	// TODO(P1): copy the slots' bytes into the document's value columns.
}

}  // namespace engine::strata::values
