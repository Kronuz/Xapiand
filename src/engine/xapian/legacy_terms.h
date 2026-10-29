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

#pragma once

#include <cstddef>                 // for std::size_t
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "engine/document.h"       // for KEY_FIELD


// How the Xapian engine spells the seam's (field, term) and key: the one place that knows Xapian's
// flat terms. A field's name leads its terms; the key field's terms (the id's type character and its
// bytes) lead with 'Q', so a Xapian index holds the same terms as before the seam named fields.
namespace engine::xapian {

inline constexpr std::string_view id_prefix = "Q";

// The field's prefix in a Xapian term.
inline std::string_view
term_prefix(std::string_view field)
{
	return field == KEY_FIELD ? id_prefix : field;
}

// Appends the Xapian term of `term` under `field` to `out`.
inline std::string&
join_into(std::string& out, std::string_view field, std::string_view term)
{
	out.append(term_prefix(field)).append(term);
	return out;
}

inline std::string
join(std::string_view field, std::string_view term)
{
	std::string out;
	out.reserve(field.size() + term.size());
	return join_into(out, field, term);
}

// The Xapian term that names a document: the id term of its key.
inline std::string
key_term(std::string_view key)
{
	return join(KEY_FIELD, key);
}

}  // namespace engine::xapian
