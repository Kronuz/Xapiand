/*
 * Trace hooks for the `scheduler` library.
 *
 * Self-contained no-op stubs. The previous version included Xapiand's log.h to
 * "restore" colored scheduler traces, but the extracted logger's macros expand to
 * Logging::do_log (needs the full Logging class) and the scheduler is a dependency
 * of the logger, so pulling log.h here created a circular include.
 *
 * scheduler.h self-guards and cleans up L_SCHEDULER (and only mentions L_PRINT /
 * L_BLUE in a comment or inside no-op trace args), so those need not be defined
 * here. It does use L_DEBUG_HOOK / L_EXC / L_CALL, which we stub to no-ops (their
 * default; off in Xapiand by default anyway). L_EXC is later #undef'd and made
 * real by the logger for Xapiand's own call sites.
 */

#pragma once

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif
#ifndef L_DEBUG_HOOK
#define L_DEBUG_HOOK L_NOTHING
#endif
#ifndef L_EXC
#define L_EXC L_NOTHING
#endif
#ifndef L_CALL
#define L_CALL L_NOTHING
#endif
