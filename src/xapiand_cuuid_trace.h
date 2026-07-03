/*
 * Logging hooks for the `cuuid` library, wired to Xapiand.
 *
 * uuid.cc emits L_CALL traces that are no-op by default (cuuid_trace.h). This
 * header is the library's CUUID_TRACE_HEADER: it routes those macros to Xapiand's
 * real logger and pulls in repr.hh, whose repr(...) calls appear inside L_CALL
 * arguments when that category is enabled.
 */

#pragma once

#include "repr.hh"                  // for repr (inside L_CALL args)
#include "log.h"                    // for L_CALL, L_NOTHING
