/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Trace / registration hooks for the `threadpool` library, wired to Xapiand.
 *
 * THREADPOOL_THREAD_REGISTER stays real: it registers freshly named threads with
 * the traceback library's crash-callstack registry (traceback.h), which has no
 * logger dependency, so it is kept (no feature loss for crash dumps).
 * THREADPOOL_THREAD_UNREGISTER is its symmetric teardown: the threadpool library
 * now fires it as each thread exits, so we map it to traceback::deregister_thread
 * and the fixed-size registry reclaims the slot instead of leaking it on thread
 * churn (workers/clients come and go). L_EXC (a
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
#define THREADPOOL_THREAD_UNREGISTER() traceback::deregister_thread()
