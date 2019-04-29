/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#pragma once

#include "config.h"           // for HAVE_EXECINFO_H, XAPIAND_TRACEBACKS

#include <exception>          // for std::exception_ptr
#include <string>             // for std::string
#include <signal.h>           // for siginfo_t
#include <vector>             // for std::vector
#ifdef HAVE_PTHREADS
#include <pthread.h>          // for pthread_t
#endif
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>         // for backtrace
#else
static inline int backtrace(void**, int) { return 0; }
#endif

void** backtrace();

void init_thread_info(pthread_t pthread, const char* name);

void collect_callstack_sig_handler(int signum, siginfo_t* info, void* ptr);
std::string dump_callstacks();
void callstacks_snapshot();

void** exception_callstack(std::exception_ptr& eptr);

std::string traceback(const char* function, const char* filename, int line, int skip = 1);
std::string traceback(const char* function, const char* filename, int line, void** callstack, int skip = 1);

#define TRACEBACK() ::traceback(__func__, __FILE__, __LINE__)

#ifdef XAPIAND_TRACEBACKS
#define BACKTRACE() backtrace()
#else
#define BACKTRACE() nullptr
#endif
