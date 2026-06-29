/*
 * Trace hooks for the `stash` library.
 *
 * Self-contained no-op stubs. The previous version included Xapiand's log.h to
 * "restore" colored stash traces, but the extracted logger's macros expand to
 * Logging::do_log (needs the full Logging class), and stash is a dependency of
 * the logger, so pulling log.h here created a circular include. stash's own trace
 * lines (L_STASH / L_DEBUG_HOOK) were L_NOTHING (off) by default anyway, so
 * stubbing them changes nothing; L_EXC (a swallowed-exception trace inside stash)
 * reduces to a no-op, matching the standalone library. Xapiand's own logging is
 * unaffected and fully real.
 */

#pragma once

// stash.h self-guards and cleans up L_STASH, so it is not defined here. It uses
// L_DEBUG_HOOK / L_EXC / L_COLLECT, stubbed to no-ops (their default).
#ifndef L_NOTHING
#define L_NOTHING(...)
#endif
#ifndef L_DEBUG_HOOK
#define L_DEBUG_HOOK L_NOTHING
#endif
#ifndef L_EXC
#define L_EXC L_NOTHING
#endif
#ifndef L_COLLECT
#define L_COLLECT L_NOTHING
#endif

// Per-operation color is unused now that L_STASH is a no-op; return no color.
#define STASH_OP_COLOR(op) ""
