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


// The key adapter of the strata engine, until the strata index keeps keys without a prefix (fields-and-keys
// F3): the index's key is a lead byte and the host's key, which the key field's terms also name.
namespace engine::strata::keys {

// A key leads with a byte no host key can start with (the key is the id's type character, ASCII).
inline constexpr char lead = '\xff';

// The index's key for the host's key.
inline std::string
stored(std::string_view key)
{
	std::string result;
	result.reserve(key.size() + 1);
	result.push_back(lead);
	result.append(key);
	return result;
}

// The host's key of an index's key.
inline std::string_view
host(std::string_view stored_key)
{
	return stored_key.substr(1);
}

// The key prefix the index is opened with.
inline std::string
key_prefix()
{
	return std::string(1, lead);
}

}  // namespace engine::strata::keys
