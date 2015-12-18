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

#include "binary.h"
#include "client_binary.h"


BinaryServer::BinaryServer(const std::shared_ptr<XapiandServer>& server_, ev::loop_ref *loop_, const std::shared_ptr<Binary> &binary_)
	: BaseServer(server_, loop_, binary_->sock),
	  binary(binary_),
	  async_signal(*loop_)
{
	async_signal.set<BinaryServer, &BinaryServer::async_signal_cb>(this);
	async_signal.start();
	L_EV(this, "Start binary async signal event");

	L_EV(this, "Start binary accept event (sock=%d)", binary->sock);
	L_OBJ(this, "CREATED BINARY SERVER! [%llx]", this);
}


BinaryServer::~BinaryServer()
{
	L_OBJ(this, "DELETED BINARY SERVER! [%llx]", this);
}


void
BinaryServer::async_signal_cb(ev::async &, int)
{
	L_EV_BEGIN(this, "BinaryServer::async_signal_cb:BEGIN");

	while (binary->tasks.call(share_this<BinaryServer>())) {}

	L_EV_END(this, "BinaryServer::async_signal_cb:END");
}


void
BinaryServer::io_accept_cb(ev::io &watcher, int revents)
{
	L_EV_BEGIN(this, "BinaryServer::io_accept_cb:BEGIN");

	if (EV_ERROR & revents) {
		L_EV(this, "ERROR: got invalid binary event (sock=%d): %s", binary->sock, strerror(errno));
		L_EV_END(this, "BinaryServer::io_accept_cb:END");
		return;
	}

	assert(binary->sock == watcher.fd || binary->sock == -1);

	int client_sock = binary->accept();
	if (client_sock < 0) {
		if (!ignored_errorno(errno, false)) {
			L_ERR(this, "ERROR: accept binary error (sock=%d): %s", binary->sock, strerror(errno));
		}
		L_EV_END(this, "BinaryServer::io_accept_cb:END");
		return;
	}

	auto client = Worker::create<BinaryClient>(share_this<BinaryServer>(), loop, client_sock, active_timeout, idle_timeout);

	if (!client->init_remote()) {
		client->destroy();
		L_EV_END(this, "BinaryServer::io_accept_cb:END");
		return;
	}

	L_INFO(this, "Accepted new client! (sock=%d)", client_sock);

	L_EV_END(this, "BinaryServer::io_accept_cb:END");
}


bool
BinaryServer::trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	int client_sock = binary->connection_socket();
	if (client_sock < 0) {
		return false;
	}

	auto client = Worker::create<BinaryClient>(share_this<BinaryServer>(), loop, client_sock, active_timeout, idle_timeout);

	if (!client->init_replication(src_endpoint, dst_endpoint)) {
		client->destroy();
		return false;
	}

	L_INFO(this, "Database being synchronized from %s...", src_endpoint.as_string().c_str());

	return true;
}


bool
BinaryServer::store(const Endpoints &endpoints, const Xapian::docid &did, const std::string &filename)
{
	int client_sock = binary->connection_socket();
	if (client_sock < 0) {
		return false;
	}

	auto client = Worker::create<BinaryClient>(share_this<BinaryServer>(), loop, client_sock, active_timeout, idle_timeout);

	if (!client->init_storing(endpoints, did, filename)) {
		client->destroy();
		return false;
	}

	L_INFO(this, "Storing %s in %s...", filename.c_str(), endpoints.as_string().c_str());

	return true;
}

#endif /* HAVE_REMOTE_PROTOCOL */
