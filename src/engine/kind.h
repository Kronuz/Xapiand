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

#include <cstdint>                 // for std::uint8_t
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view


namespace engine {

// The storage engines an index can have. An index has one, fixed when it is created.
enum class Kind : std::uint8_t {
	xapian,
	strata,
};

constexpr const char* to_string(Kind kind) noexcept {
	return kind == Kind::strata ? "strata" : "xapian";
}

// "xapian" or "strata"; empty for anything else.
std::optional<Kind> parse_kind(std::string_view name) noexcept;

// The engine new indexes get: the XAPIAND_ENGINE environment variable, read once; Xapian when unset
// or unknown.
Kind default_kind();

// The engine of the shard at `path`: the one whose files are there, else the default for a new
// shard. The registry (".xapiand/...") is always Xapian.
Kind kind_of(const std::string& path);

}  // namespace engine
