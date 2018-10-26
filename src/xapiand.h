/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#define XAPIAND_PID_FILE             "xapiand.pid"
#define XAPIAND_LOG_FILE             "xapiand.log"

#define DBPOOL_SIZE              300     /* Maximum number of database endpoints in database pool. */
#define MAX_CLIENTS              1000    /* Maximum number of open client connections */
#define MAX_DATABASES            400     /* Maximum number of open databases */
#define NUM_SERVERS              10      /* Number of servers. */
#define NUM_REPLICATORS          3       /* Number of replicators. */
#define NUM_COMMITTERS           10      /* Number of threads handling the commits. */
#define NUM_FSYNCHERS            10      /* Number of threads handling the fsyncs. */
#define FLUSH_THRESHOLD          100000  /* Database flush threshold (default for xapian is 10000) */
#define TASKS_SIZE               100     /* Client tasks threadpool's size. */
#define CONCURRENCY_MULTIPLIER   4       /* Server workers multiplier (by number of CPUs) */
#define ENDPOINT_LIST_SIZE       10      /* Endpoints List's size. */
