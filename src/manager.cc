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

#include "xapiand.h"
#include "utils.h"
#include "manager.h"
#include "server.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <sys/socket.h>


XapiandManager::XapiandManager(ev::loop_ref *loop_, int http_port_, int binary_port_)
	: loop(loop_ ? loop_: &dynamic_loop),
	  break_loop(*loop),
	  async_shutdown(*loop),
	  thread_pool(10),
	  http_port(http_port_),
	  binary_port(binary_port_)
{	
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);
	
	pthread_mutexattr_init(&servers_mutex_attr);
	pthread_mutexattr_settype(&servers_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&servers_mutex, &servers_mutex_attr);
	
	break_loop.set<XapiandManager, &XapiandManager::break_loop_cb>(this);
	break_loop.start();

	async_shutdown.set<XapiandManager, &XapiandManager::shutdown_cb>(this);
	async_shutdown.start();
	
	bind_http();
	bind_binary();

	assert(http_sock != -1 && binary_sock != -1);
	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&servers_mutex);
	pthread_mutexattr_destroy(&servers_mutex_attr);

	LOG_OBJ(this, "DELETED MANAGER!\n");
}


void XapiandManager::check_tcp_backlog(int tcp_backlog)
{
#if defined(NET_CORE_SOMAXCONN)
	int name[3] = {CTL_NET, NET_CORE, NET_CORE_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"net.core.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#elif defined(KIPC_SOMAXCONN)
	int name[3] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"kern.ipc.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#endif
}


void XapiandManager::bind_http()
{
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};
	
	http_sock = socket(PF_INET, SOCK_STREAM, 0);
	
	setsockopt(http_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	error = setsockopt(http_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));
	
	error = setsockopt(http_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));
	
	error = setsockopt(http_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(http_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(http_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_ERR(this, "ERROR: http bind error (sock=%d): %s\n", http_sock, strerror(errno));
		::close(http_sock);
		http_sock = -1;
	} else {
		fcntl(http_sock, F_SETFL, fcntl(http_sock, F_GETFL, 0) | O_NONBLOCK);
		
		check_tcp_backlog(tcp_backlog);
		listen(http_sock, tcp_backlog);
	}
}


void XapiandManager::bind_binary()
{
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};
	
	binary_sock = socket(PF_INET, SOCK_STREAM, 0);
	
	setsockopt(binary_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	error = setsockopt(binary_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	error = setsockopt(binary_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	error = setsockopt(binary_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(binary_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(binary_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_ERR(this, "ERROR: binary bind error (sock=%d): %s\n", binary_sock, strerror(errno));
		::close(binary_sock);
		binary_sock = -1;
	} else {
		fcntl(binary_sock, F_SETFL, fcntl(binary_sock, F_GETFL, 0) | O_NONBLOCK);

		check_tcp_backlog(tcp_backlog);
		listen(binary_sock, tcp_backlog);
	}
}


void XapiandManager::sig_shutdown_handler(int sig)
{
	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	time_t now = time(NULL);
	if (shutdown_now && sig != SIGTERM) {
		if (sig && shutdown_now + 1 < now) {
			LOG(this, "You insist... exiting now.\n");
			// remove pid file here, use: getpid();
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (shutdown_asap && sig != SIGTERM) {
		if (shutdown_asap + 1 < now) {
			shutdown_now = now;
			LOG(this, "Trying immediate shutdown.\n");
		}
	} else {
		shutdown_asap = now;
		switch (sig) {
			case SIGINT:
				LOG(this, "Received SIGINT scheduling shutdown...\n");
				break;
			case SIGTERM:
				LOG(this, "Received SIGTERM scheduling shutdown...\n");
				break;
			default:
				LOG(this, "Received shutdown signal, scheduling shutdown...\n");
		};
	}
	shutdown();
}


void XapiandManager::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}
	
	if (http_sock != -1) {
		::close(http_sock);
		http_sock = -1;
	}
	
	if (binary_sock != -1) {
		::close(binary_sock);
		binary_sock = -1;
	}

	pthread_mutex_unlock(&qmtx);

	LOG_OBJ(this, "DESTROYED MANAGER!\n");
}


void XapiandManager::shutdown_cb(ev::async &watcher, int revents)
{
	sig_shutdown_handler(0);
}


std::list<XapiandServer *>::const_iterator XapiandManager::attach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator iterator = servers.insert(servers.end(), server);
	pthread_mutex_unlock(&servers_mutex);
	return iterator;
}


void XapiandManager::detach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	if (server->iterator != servers.end()) {
		servers.erase(server->iterator);
		server->iterator = servers.end();
		LOG_OBJ(this, "DETACHED SERVER!\n");
	}
	pthread_mutex_unlock(&servers_mutex);
}


void XapiandManager::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking manager loop!\n");
	loop->break_loop();
}


void XapiandManager::shutdown()
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator it(servers.begin());
	while (it != servers.end()) {
		XapiandServer *server = *(it++);
		server->shutdown();
	}
	pthread_mutex_unlock(&servers_mutex);

	if (shutdown_asap) {
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop.send();
	}
}


//
//void XapiandManager::run()
//{
//	XapiandServer * server = new XapiandServer(this, loop, http_sock, binary_sock, &database_pool, &thread_pool);
//	server->run(NULL);
//	delete server;
//}


void XapiandManager::run(int num_servers)
{
	LOG(this, "Listening on %d (http), %d (xapian)...\n", http_port, binary_port);

	ThreadPool server_pool(num_servers);
	for (int i = 0; i < num_servers; i++) {
		XapiandServer *server = new XapiandServer(this, NULL, http_sock, binary_sock, &database_pool, &thread_pool);
		server_pool.addTask(server);
	}
	
	LOG_OBJ(this, "Starting manager loop...\n");
	loop->run();
	LOG_OBJ(this, "Manager loop ended!\n");
	
	LOG_OBJ(this, "Waiting for threads...\n");
	
	server_pool.finish();
	server_pool.join();

	LOG_OBJ(this, "Server ended!\n");
}
