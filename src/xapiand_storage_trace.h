/*
 * Logging hooks for the `storage` library, wired to Xapiand.
 *
 * The storage engine logs through an L_* family that is no-op by default. This
 * header is the engine's STORAGE_TRACE_HEADER: it routes those macros to
 * Xapiand's real logger (logger.h provides L_CALL / L_DEBUG / L_ERR / L_WARNING /
 * L_WARNING_ONCE / L_EXC) and pulls in repr.hh, which the engine's log-message
 * arguments use (repr(path), repr(buf, size)). This restores the exact in-tree
 * storage logging with no behavior change.
 */

#pragma once

#include "logger.h"       // L_CALL, L_DEBUG, L_ERR, L_WARNING, L_WARNING_ONCE, L_EXC
#include "repr.hh"        // repr (used in the engine's L_* message arguments)
