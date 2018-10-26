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

#include "replicator.h"

#ifdef XAPIAND_CLUSTERING

#include "database.h"
#include "manager.h"
#include "server/discovery.h"


XapiandReplicator::XapiandReplicator(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(parent_, ev_loop_, ev_flags_) {

	L_OBJ("CREATED XAPIAN REPLICATOR!");
}

XapiandReplicator::~XapiandReplicator()
{
	destroyer();
}


void
XapiandReplicator::destroy_impl()
{
	destroyer();
}


void
XapiandReplicator::destroyer()
{
	L_CALL("XapiandReplicator::destroyer()");

	XapiandManager::manager->database_pool.updated_databases.finish();
}


void
XapiandReplicator::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("XapiandReplicator::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now) {
		detach();
	}
}


void
XapiandReplicator::run()
{
	L_CALL("XapiandReplicator::run()");

	if (auto discovery = XapiandManager::manager->weak_discovery.lock()) {
		DatabaseUpdate update;
		while (XapiandManager::manager->database_pool.updated_databases.pop(update)) {
			L_DEBUG("Replicator was informed database was updated: %s", repr(update.endpoint.to_string()));
			discovery->signal_db_update(update);
		}
	}

	detach();
}

#endif
