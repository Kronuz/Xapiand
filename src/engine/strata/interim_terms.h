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

#include <string>                  // for std::string
#include <string_view>             // for std::string_view


// The interim mapping from the seam's (field, term) to the flat terms the strata index holds today
// (fields-and-keys.md C4). It is the one place the strata engine joins a field name and a term, and
// the only one: the strata adapter on fields (SF) deletes this file and resolves each field to the
// index's field id instead. Nothing else in the engine may build a term from a field name.
namespace engine::strata::interim {

// The flat term for a term under a field: the name and the bytes, exactly Xapian's term.
inline std::string
flat_term(std::string_view field, std::string_view term)
{
	std::string flat;
	flat.reserve(field.size() + term.size());
	flat.append(field).append(term);
	return flat;
}

}  // namespace engine::strata::interim
