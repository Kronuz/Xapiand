/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#ifndef XAPIAND_INCLUDED_REPLICATOR_H
#define XAPIAND_INCLUDED_REPLICATOR_H

#include "database.h"
#include "threadpool.h"
#include "worker.h"
#include "manager.h"
#include "length.h"

#include <ev++.h>


class XapiandReplicator : public Task, public Worker
{
	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	void run();
	void shutdown();

	void on_commit(const Endpoint &endpoint, int mastery_level);

public:
	XapiandReplicator(XapiandManager *manager_, ev::loop_ref *loop_, DatabasePool *database_pool_, ThreadPool *thread_pool_);
	~XapiandReplicator();


	inline XapiandManager * manager() const {
		return static_cast<XapiandManager *>(_parent);
	}
};

#endif /* XAPIAND_INCLUDED_REPLICATOR_H */
