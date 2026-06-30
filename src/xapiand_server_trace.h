/*
 * Logging hooks for the `server` engine, wired to Xapiand.
 *
 * The standalone engine logs through a no-op L_* family. This is the engine's
 * SERVER_TRACE_HEADER: it routes those macros to Xapiand's real logger (log.h)
 * and pulls in the helpers the engine's L_* arguments use (repr for byte
 * buffers, error::name/description for errno). The engine's __repr__() debug
 * builders pick up Xapiand's real colors.h / strings.hh directly off the include
 * path (the library's compat/ shims are not on Xapiand's path), so this only has
 * to supply the L_* family and its argument helpers.
 */

#pragma once

#include "log.h"           // the real L_CALL / L_ERR / L_EV / L_CONN / ... family
#include "repr.hh"         // repr (byte-buffer arg in L_* calls)
#include "error.hh"        // error::name, error::description (errno args in L_* calls)
