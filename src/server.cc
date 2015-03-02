/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>

#include <xapian.h>

#include "utils.h"

#include "xapiand.h"
#include "server.h"
#include "client_http.h"
#include "client_binary.h"


const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


//
// Xapian Server
//

int XapiandServer::total_clients = 0;


XapiandServer::XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_, int http_sock_, int binary_sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_)
	: manager(manager_),
	  iterator(manager_->servers.end()),
	  loop(loop_ ? loop_: &dynamic_loop),
	  http_io(*loop),
	  binary_io(*loop),
	  break_loop(*loop),
	  http_sock(http_sock_),
	  binary_sock(binary_sock_),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_)
{
	pthread_mutex_init(&clients_mutex, 0);

	break_loop.set<XapiandServer, &XapiandServer::break_loop_cb>(this);
	break_loop.start();

	http_io.set<XapiandServer, &XapiandServer::io_accept_http>(this);
	http_io.start(http_sock, ev::READ);

	binary_io.set<XapiandServer, &XapiandServer::io_accept_binary>(this);
	binary_io.start(binary_sock, ev::READ);

	manager->attach_server(this);
	LOG_OBJ(this, "CREATED SERVER!\n");
}


XapiandServer::~XapiandServer()
{
	http_io.stop();
	binary_io.stop();
	break_loop.stop();

	pthread_mutex_destroy(&clients_mutex);

	manager->detach_server(this);
	LOG_OBJ(this, "DELETED SERVER!\n");
}


void XapiandServer::run()
{
	LOG_OBJ(this, "Starting loop...\n");
	loop->run(0);
}

void XapiandServer::io_accept_http(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid http event (sock=%d): %s\n", http_sock, strerror(errno));
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sock = ::accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sock < 0) {
		if (errno != EAGAIN) {
			LOG_CONN(this, "ERROR: accept http error (sock=%d): %s\n", http_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new HttpClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}


void XapiandServer::io_accept_binary(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid binary event (sock=%d): %s\n", binary_sock, strerror(errno));
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sock = ::accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sock < 0) {
		if (errno != EAGAIN) {
			LOG_CONN(this, "ERROR: accept binary error (sock=%d): %s\n", binary_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new BinaryClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}

void XapiandServer::destroy()
{
	http_io.stop();
	binary_io.stop();

	LOG_OBJ(this, "DESTROYED SERVER!\n");
}


void XapiandServer::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking server loop!\n");
	loop->break_loop();
}


void XapiandServer::attach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	assert(client->iterator == clients.end());
	client->iterator = clients.insert(clients.end(), client);
	pthread_mutex_unlock(&clients_mutex);
}


void XapiandServer::detach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	if (client->iterator != clients.end()) {
		clients.erase(client->iterator);
		client->iterator = clients.end();
	}
	pthread_mutex_unlock(&clients_mutex);
}


void XapiandServer::shutdown()
{
	std::list<BaseClient *>::const_iterator it(clients.begin());
	while (it != clients.end()) {
		(*it)->shutdown();
		it = clients.begin();
	}

	if (manager->shutdown_asap) {
		destroy();
		if (total_clients == 0) {
			manager->shutdown_now = manager->shutdown_asap;
		}
	}
	if (manager->shutdown_now) {
		break_loop.send();
	}
}
