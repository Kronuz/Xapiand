/*
 * Trace/coloring hooks for the `scheduler` library, wired to Xapiand's logging.
 *
 * The standalone scheduler (github.com/Kronuz/scheduler) traces through no-op
 * hooks: L_SCHEDULER / L_DEBUG_HOOK / L_EXC / L_CALL, plus the color identifiers
 * those trace lines reference (BROWN, CLEAR_COLOR, LIGHT_SKY_BLUE, DIM_GREY,
 * PURPLE, STEEL_BLUE, DODGER_BLUE, LIGHT_GREEN, FOREST_GREEN). Pointing
 * SCHEDULER_TRACE_HEADER at this file (see CMakeLists.txt) restores exactly the
 * traced, colored output the vendored copy produced, by making Xapiand's own
 * log.h macros and colors.h palette visible to scheduler.h / debouncer.h.
 *
 * L_SCHEDULER follows log.h (scheduler.h leaves it untouched when already
 * defined); the rest come from log.h, and the colors from colors.h.
 */

#pragma once

#include "log.h"       // L_NOTHING, L_SCHEDULER, L_DEBUG_HOOK, L_EXC, L_CALL
#include "colors.h"    // BROWN, CLEAR_COLOR, LIGHT_SKY_BLUE, ... (used by trace lines)
