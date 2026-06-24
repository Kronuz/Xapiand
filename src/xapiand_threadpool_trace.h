/*
 * Trace/registration hooks for the `threadpool` library, wired to Xapiand.
 *
 * The standalone threadpool (github.com/Kronuz/threadpool) traces through two
 * no-op hooks: L_EXC(...) (an exception swallowed in a worker) and
 * THREADPOOL_THREAD_REGISTER(pthread, name) (register a freshly named thread).
 * Pointing THREADPOOL_TRACE_HEADER at this file (see CMakeLists.txt) restores
 * the behavior the vendored copy had, by mapping those hooks onto Xapiand's
 * log.h macro and traceback.h's crash-callstack registry.
 *
 * The "Xapiand:" thread-name prefix is restored separately via
 * THREADPOOL_THREAD_NAME_PREFIX (also set in CMakeLists.txt).
 */

#pragma once

#include "log.h"          // L_EXC
#include "traceback.h"    // init_thread_info

#define THREADPOOL_THREAD_REGISTER(pthread, name) init_thread_info(pthread, name)
