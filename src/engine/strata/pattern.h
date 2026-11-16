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

#include "engine/query.h"          // for PATTERN_SINGLE, PATTERN_MULTI


// The string tests behind wildcard and edit-distance nodes (docs/mechanisms/strata-callbacks.md).
namespace engine::strata {

// The bytes a wildcard pattern starts with, up to its first pattern character: every term that
// matches starts with them.
std::string wildcard_prefix(std::string_view pattern, unsigned pattern_flags);

// Whether the term matches the pattern. Without pattern flags the pattern is a prefix; with them
// it matches the whole term, '?' one character and '*' any run of characters, each only when its
// flag is set.
bool wildcard_matches(std::string_view pattern, std::string_view term, unsigned pattern_flags);

// Whether two UTF-8 strings are at most `distance` edits apart: insertions, deletions,
// substitutions and swaps of two neighbouring characters, each one edit.
bool within_edit_distance(std::string_view a, std::string_view b, unsigned distance);

// The bytes of the first `chars` characters of the text (all of it when it has fewer).
std::string_view head(std::string_view text, std::size_t chars);

}  // namespace engine::strata
