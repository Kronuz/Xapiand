/*
 * Trace / registration hooks for the `threadpool` library, wired to Xapiand.
 *
 * THREADPOOL_THREAD_REGISTER stays real: it registers freshly named threads with
 * Xapiand's crash-callstack registry (traceback.h), which has no logger
 * dependency, so it is kept (no feature loss for crash dumps). L_EXC (a
 * swallowed-exception trace inside a worker) becomes a self-contained no-op:
 * pulling Xapiand's log.h here would create a circular include, since the
 * extracted logger's L_EXC needs the full Logging class and the logger depends on
 * the threadpool. Xapiand's own logging is unaffected and fully real.
 */

#pragma once

#include "traceback.h"    // init_thread_info (Xapiand's crash-callstack registry)

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif
#ifndef L_EXC
#define L_EXC L_NOTHING
#endif

#define THREADPOOL_THREAD_REGISTER(pthread, name) init_thread_info(pthread, name)
