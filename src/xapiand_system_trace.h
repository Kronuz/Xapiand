/*
 * Logging hooks for the `system` library, wired to Xapiand.
 *
 * system.cc / memory_stats.cc emit an L_* family that is no-op by default
 * (system_trace.h). This header is the library's SYSTEM_TRACE_HEADER: it routes those
 * macros to Xapiand's real logger (log.h provides L_CALL / L_DEBUG / L_ERR / L_INFO /
 * L_WARNING / L_WARNING_ONCE) and pulls in error.hh, whose error::name /
 * error::description appear inside the L_ERR arguments. This restores the exact in-tree
 * system logging with no behavior change.
 */

#pragma once

#include "error.hh"                 // for error::name, error::description (inside L_ERR args)
#include "log.h"                    // for L_CALL, L_DEBUG, L_ERR, L_INFO, L_WARNING, L_WARNING_ONCE
