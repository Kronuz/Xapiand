/*
 * Trace / registration hooks for the `threadpool` library, wired to Xapiand.
 *
 * THREADPOOL_THREAD_REGISTER stays real: it registers freshly named threads with
 * the traceback library's crash-callstack registry (traceback.h), which has no
 * logger dependency, so it is kept (no feature loss for crash dumps). The
 * threadpool library has no thread-exit hook, so threads are never deregistered
 * (registry slots are not reclaimed; this matches the old in-tree behavior). L_EXC (a
 * swallowed-exception trace inside a worker) becomes a self-contained no-op:
 * pulling Xapiand's log.h here would create a circular include, since the
 * extracted logger's L_EXC needs the full Logging class and the logger depends on
 * the threadpool. Xapiand's own logging is unaffected and fully real.
 */

#pragma once

#include "traceback.h"    // traceback::register_thread (crash-callstack registry)

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif
#ifndef L_EXC
#define L_EXC L_NOTHING
#endif

#define THREADPOOL_THREAD_REGISTER(pthread, name) traceback::register_thread(pthread, name)
