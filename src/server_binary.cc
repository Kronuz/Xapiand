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

#include "server_binary.h"

#ifdef HAVE_REMOTE_PROTOCOL

#include "client_binary.h"


BinaryServer::BinaryServer(XapiandServer *server_, ev::loop_ref *loop_, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_)
	: BaseServer(server_, loop_, sock_, database_pool_, thread_pool_)
{
	LOG_EV(this, "Start binary accept event (sock=%d)\n", sock);
	LOG_OBJ(this, "CREATED BINARY SERVER!\n");
}


BinaryServer::~BinaryServer()
{
	LOG_OBJ(this, "DELETED BINARY SERVER!\n");
}


void BinaryServer::io_accept(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid binary event (sock=%d): %s\n", binary_sock, strerror(errno));
		return;
	}

	assert(sock == watcher.fd || sock == -1);

	int client_sock;
	if ((client_sock = accept_tcp(watcher.fd)) < 0) {
		if (!ignored_errorno(errno, false)) {
			LOG_ERR(this, "ERROR: accept binary error (sock=%d): %s\n", sock, strerror(errno));
		}
	} else {
		BinaryClient *client = new BinaryClient(server, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);

		if (!client->init_remote()) {
			delete client;
		}
	}
}


#endif /* HAVE_REMOTE_PROTOCOL */
