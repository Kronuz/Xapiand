/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "logger.h"

#define _ L_NOTHING

#ifdef NDEBUG
#define L_OBJ_BEGIN _
#define L_OBJ_END _
#define L_DATABASE_BEGIN _
#define L_DATABASE_END _
#define L_EV_BEGIN _
#define L_EV_END _
#else
#define L_OBJ_BEGIN L_DELAYED_1000
#define L_OBJ_END L_DELAYED_N_UNLOG
#define L_DATABASE_BEGIN L_DELAYED_200
#define L_DATABASE_END L_DELAYED_N_UNLOG
#define L_EV_BEGIN L_DELAYED_600
#define L_EV_END L_DELAYED_N_UNLOG
#endif

#define L_MARK _LOG(false, LOG_DEBUG, "🔥  " DEBUG_COL, args)

#define L_INIT auto start = std::chrono::system_clock::now()


////////////////////////////////////////////////////////////////////////////////
// Enable the following when needed. Use L_* or L_STACKED_* or L_UNINDENTED_*
// ex. L_STACKED_DARK_GREY, L_CYAN, L_STACKED_LOG or L_MAGENTA

#define L_ERRNO _
#define L_TRACEBACK _
#define L_CALL _
#define L_TIME _
#define L_CONN _
#define L_RAFT _
#define L_RAFT_PROTO _
#define L_DISCOVERY _
#define L_DISCOVERY_PROTO _
#define L_REPLICATION _
#define L_OBJ _
#define L_THREADPOOL _
#define L_DATABASE _
#define L_DATABASE_WAL _
#define L_HTTP _
#define L_BINARY _
#define L_HTTP_PROTO_PARSER _
#define L_EV _//_
#define L_HTTP_WIRE _
#define L_BINARY_WIRE _
#define L_TCP_WIRE _
#define L_UDP_WIRE _
#define L_HTTP_PROTO _
#define L_BINARY_PROTO _
#define L_DATABASE_WRAP_INIT _
#define L_DATABASE_WRAP _
#define L_INDEX _
#define L_SEARCH _
