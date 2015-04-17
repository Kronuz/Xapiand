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

#include "server.h"

#include "utils.h"

#include "client_http.h"
#include "client_binary.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>

#include <xapian.h>


const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


//
// Xapian Server
//

pthread_mutex_t XapiandServer::static_mutex = PTHREAD_MUTEX_INITIALIZER;
int XapiandServer::total_clients = 0;
int XapiandServer::http_clients = 0;
int XapiandServer::binary_clients = 0;


XapiandServer::XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_, int http_sock_, int binary_sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_)
	: manager(manager_),
	  iterator(manager->attach_server(this)),
	  loop(loop_ ? loop_: &dynamic_loop),
	  http_io(*loop),
	  binary_io(*loop),
	  break_loop(*loop),
	  http_sock(http_sock_),
	  binary_sock(binary_sock_),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&clients_mutex_attr);
	pthread_mutexattr_settype(&clients_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&clients_mutex, &clients_mutex_attr);

	break_loop.set<XapiandServer, &XapiandServer::break_loop_cb>(this);
	break_loop.start();

	http_io.set<XapiandServer, &XapiandServer::io_accept_http>(this);
	http_io.start(http_sock, ev::READ);

#ifdef HAVE_REMOTE_PROTOCOL
	binary_io.set<XapiandServer, &XapiandServer::io_accept_binary>(this);
	binary_io.start(binary_sock, ev::READ);
#endif  /* HAVE_REMOTE_PROTOCOL */

	LOG_OBJ(this, "CREATED SERVER!\n");
}


XapiandServer::~XapiandServer()
{
	destroy();

	break_loop.stop();

	manager->detach_server(this);

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&clients_mutex);
	pthread_mutexattr_destroy(&clients_mutex_attr);

	LOG_OBJ(this, "DELETED SERVER!\n");
}


void XapiandServer::run()
{
	LOG_OBJ(this, "Starting server loop...\n");
	loop->run(0);
	LOG_OBJ(this, "Server loop ended!\n");
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
			LOG_ERR(this, "ERROR: accept http error (sock=%d): %s\n", http_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new HttpClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}


#ifdef HAVE_REMOTE_PROTOCOL
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
			LOG_ERR(this, "ERROR: accept binary error (sock=%d): %s\n", binary_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new BinaryClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}
#endif  /* HAVE_REMOTE_PROTOCOL */


void XapiandServer::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	http_sock = -1;
	binary_sock = -1;

	http_io.stop();
	binary_io.stop();

	pthread_mutex_unlock(&qmtx);

	// http and binary sockets are closed in the manager.

	LOG_OBJ(this, "DESTROYED SERVER!\n");
}


void XapiandServer::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking server loop!\n");
	loop->break_loop();
}


std::list<BaseClient *>::const_iterator XapiandServer::attach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	std::list<BaseClient *>::const_iterator iterator = clients.insert(clients.end(), client);
	pthread_mutex_unlock(&clients_mutex);
	return iterator;
}


void XapiandServer::detach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	if (client->iterator != clients.end()) {
		clients.erase(client->iterator);
		client->iterator = clients.end();
		LOG_OBJ(this, "DETACHED CLIENT!\n");
	}
	pthread_mutex_unlock(&clients_mutex);
}


void XapiandServer::shutdown()
{
	pthread_mutex_lock(&clients_mutex);
	std::list<BaseClient *>::const_iterator it(clients.begin());
	while (it != clients.end()) {
		BaseClient *client = *(it++);
		client->shutdown();
	}
	pthread_mutex_unlock(&clients_mutex);

	if (manager->shutdown_asap) {
		if (http_clients <= 0) {
			manager->shutdown_now = manager->shutdown_asap;
		}
		destroy();
	}
	if (manager->shutdown_now) {
		break_loop.send();
	}
}
