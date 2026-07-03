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
