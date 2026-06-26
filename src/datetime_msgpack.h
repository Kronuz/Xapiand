/*
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

#pragma once

// Xapiand-side glue for the standalone `datetime` library.
//
// The pure string/number parsers (ISO 8601, date math, time, timedelta) live in
// the extracted `datetime` library (datetime.h / datetime.cc). This file keeps
// the half that depends on Xapiand internals: parsing a date/time given as a
// MsgPack structured object ({year: 2021, month: 3, ...}) or a MsgPack scalar,
// which needs MsgPack, the RESERVED_* key tokens, and the perfect-hash dispatch.
// Each overload extracts the value out of the MsgPack and delegates to the
// library's string/double parsers.

#include <string_view>     // for std::string_view

#include "datetime.h"      // the standalone library: Datetime::*, tm_t, clk_t, DatetimeError

class MsgPack;


namespace Datetime {
	// MsgPack-aware overloads. These extend the Datetime namespace alongside the
	// library's string/double parsers and ultimately delegate to them.
	tm_t DatetimeParser(const MsgPack& value);
	double time_to_double(const MsgPack& _time);
	double timedelta_to_double(const MsgPack& timedelta);
}
