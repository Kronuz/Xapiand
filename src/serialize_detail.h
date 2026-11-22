/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

// What the serialize_*.cc pieces share: the UUID separator list and encoder macro, and
// serialize_detail::positive_of (defined in serialize_exact.cc).

#pragma once

#include "config.h"                 // for XAPIAND_UUID_ENCODED

#include <cstdint>                  // for uint64_t
#include <string_view>              // for std::string_view


constexpr char UUID_SEPARATOR_LIST = ';';

#ifdef XAPIAND_UUID_ENCODED
#define UUID_ENCODER (Base59::dubaluchk())
#endif


namespace serialize_detail {

uint64_t positive_of(int* errno_save, std::string_view text) noexcept;

}  // namespace serialize_detail
