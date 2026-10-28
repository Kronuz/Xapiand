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

#include <string>                  // for std::string
#include <string_view>             // for std::string_view


// The key adapter of the strata engine: how a document's id term becomes the index's key. This is the
// one place that names the key lead byte; the fields-and-keys design (no key prefix) replaces the
// three functions below and nothing else.
namespace engine::strata::keys {

// A key leads with a byte no term can start with (terms are UTF-8 text), so a field named "Quote"
// never collides with the key prefix; the term follows it unchanged.
inline constexpr char lead = '\xff';

// The index's key for a document's id term.
inline std::string
engine_key(std::string_view term)
{
	std::string key;
	key.reserve(term.size() + 1);
	key.push_back(lead);
	key.append(term);
	return key;
}

// The key prefix the index is opened with.
inline std::string
key_prefix()
{
	return std::string(1, lead);
}

}  // namespace engine::strata::keys
