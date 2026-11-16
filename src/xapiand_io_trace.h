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
 * Logging hooks for the `io` library, wired to Xapiand.
 *
 * io.cc emits a small L_* family that is no-op by default (io_trace.h). This
 * header is the library's IO_TRACE_HEADER: it routes those macros to Xapiand's
 * real logger (log.h provides L_CALL / L_ERRNO / L_ERR) and pulls in error.hh,
 * whose error::name / error::description appear inside the L_ERRNO arguments, and
 * traceback.h, used by the optional IO_CHECK_FDES fd tracker. This restores the
 * exact in-tree io logging with no behavior change.
 */

#pragma once

#include "error.hh"                 // for error::name, error::description (inside L_ERRNO args)
#include "traceback.h"              // for traceback::traceback (IO_CHECK_FDES only)
#include "log.h"                    // for L_CALL, L_ERRNO, L_ERR
