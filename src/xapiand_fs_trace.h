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

/*
 * Logging hooks for the `fs` library, wired to Xapiand.
 *
 * fs.cc emits an L_* family that is no-op by default (fs_trace.h). This header is the
 * library's FS_TRACE_HEADER: it routes those macros to Xapiand's real logger (log.h
 * provides L_CALL / L_DEBUG / L_ERR / L_INFO / L_WARNING) and pulls in error.hh, whose
 * error::name / error::description appear inside the L_ERR arguments, and repr.hh, whose
 * repr(path) appears inside the L_CALL arguments. This restores the exact in-tree fs
 * logging with no behavior change. L_FS (fs-specific verbose trace) stays no-op: fs.cc
 * defines it to L_NOTHING when a host leaves it undefined, matching the in-tree default.
 */

#pragma once

#include "error.hh"                 // for error::name, error::description (inside L_ERR args)
#include "repr.hh"                  // for repr (inside L_CALL args)
#include "log.h"                    // for L_CALL, L_DEBUG, L_ERR, L_INFO, L_WARNING
