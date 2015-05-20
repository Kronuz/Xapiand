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

#include "replicator.h"


XapiandReplicator::XapiandReplicator(XapiandManager *manager_, ev::loop_ref *loop_, DatabasePool *database_pool_, ThreadPool *thread_pool_)
	: Worker(manager_, loop_),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_)
{
}

XapiandReplicator::~XapiandReplicator()
{
}

void
XapiandReplicator::on_commit(const Endpoint &endpoint)
{
	manager()->discovery(
		DISCOVERY_DB_UPDATED,
		serialise_length(endpoint.mastery_level) +  // The mastery level of the database
		serialise_string(endpoint.path) +  // The path of the index
		local_node.serialise()   // The node where the index is at
	);
}

void
XapiandReplicator::run()
{
	// Function that retrieves a task from a queue, runs it and deletes it
	LOG_OBJ(this, "Replicator started...\n");
	Endpoint endpoint;
	while (database_pool->updated_databases.pop(endpoint)) {
		LOG(this, "REPLICATOR GOT DATABASE!\n");
		on_commit(endpoint);
	}
	LOG_OBJ(this, "Replicator ended!\n");
}

void
XapiandReplicator::shutdown()
{
	database_pool->updated_databases.finish();
}
