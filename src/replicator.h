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

#include <ev++.h>

		// void on_commit() {
		// 	Database *database = NULL;
		// 	if (database_pool->checkout(&database, endpoints_, 0)) {
		// 		int mastery_level = database->mastery_level;
		// 		database_pool->checkin(&database);

		// 		if (mastery_level != -1) {
		// 			const Endpoint &endpoint = *endpoints.begin();
		// 			server->manager->discovery(
		// 				DISCOVERY_DB_UPDATED,
		// 				serialise_length(mastery_level) +  // The mastery level of the database
		// 				serialise_string(endpoint.path) +  // The path of the index
		// 				manager->this_node.serialise()  // The node where the index is at
		// 			);
		// 		}
		// 	}
		// }

class XapiandReplicator : public Task, public Worker
{
	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	XapiandReplicator(XapiandManager *manager_, ev::loop_ref *loop_, DatabasePool *database_pool_, ThreadPool *thread_pool_);
	~XapiandReplicator();

	void run();
};

#endif /* XAPIAND_INCLUDED_REPLICATOR_H */
