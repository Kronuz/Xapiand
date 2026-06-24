/*
 * Trace/coloring hooks for the `stash` library, wired to Xapiand's logging.
 *
 * The standalone stash (github.com/Kronuz/stash) traces through four no-op
 * hooks: L_STASH / L_DEBUG_HOOK / L_EXC and STASH_OP_COLOR(op). Pointing
 * STASH_TRACE_HEADER at this file (see CMakeLists.txt) restores the exact
 * colored, logged output the vendored copy produced, by mapping those hooks onto
 * Xapiand's own log.h macros and colors.h palette.
 *
 * L_STASH stays L_NOTHING (off) by default, like before; L_DEBUG_HOOK / L_EXC
 * come from log.h, and the per-operation color matches the old Stash::_col().
 */

#pragma once

#include "log.h"       // L_NOTHING, L_DEBUG_HOOK, L_EXC
#include "colors.h"    // CLEAR_COLOR, DIM_GREY, PURPLE, CYAN, ... (used by trace lines)

// Per-operation color, matching the original Stash::_col(): walk -> clear,
// peep -> dim grey, clean -> purple. Returns a pointer valid for the trace call
// (function-local statics, as in the original).
inline const char* xapiand_stash_op_color(int op) {
	switch (op) {
		case 0: { static constexpr auto c = CLEAR_COLOR; return c.c_str(); }  // walk
		case 1: { static constexpr auto c = DIM_GREY;    return c.c_str(); }  // peep
		case 2: { static constexpr auto c = PURPLE;      return c.c_str(); }  // clean
		default: { static constexpr auto c = CLEAR_COLOR; return c.c_str(); }
	}
}

#define STASH_OP_COLOR(op) xapiand_stash_op_color(static_cast<int>(op))
