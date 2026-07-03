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
