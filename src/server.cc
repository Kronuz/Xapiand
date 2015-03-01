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

#include <xapian.h>

#include "utils.h"

#include "server.h"
#include "client_http.h"
#include "client_binary.h"


const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


//
// Xapian Server
//

time_t XapiandServer::shutdown = (time_t)0;
time_t XapiandServer::shutdown_asap = (time_t)0;
int XapiandServer::total_clients = 0;


void XapiandServer::sig_shutdown_handler(int sig) {
	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	time_t now = time(NULL);
	if (shutdown_asap && sig == SIGINT) {
		if (shutdown_asap + 1 < now) {
			LOG((void *)NULL, "You insist... exiting now.\n");
			// remove pid file here, use: getpid();
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (shutdown && sig == SIGINT) {
		if (shutdown + 1 < now) {
			shutdown_asap = now;
			LOG((void *)NULL, "Trying immediate shutdown.\n");
		}
	} else {
		shutdown = now;
		switch (sig) {
			case SIGINT:
				LOG((void *)NULL, "Received SIGINT scheduling shutdown...\n");
				break;
			case SIGTERM:
				LOG((void *)NULL, "Received SIGTERM scheduling shutdown...\n");
				break;
			default:
				LOG((void *)NULL, "Received shutdown signal, scheduling shutdown...\n");
		};
	}
}

XapiandServer::XapiandServer(ev::loop_ref *loop_, int http_sock_, int binary_sock_)
	: loop(loop_ ? loop_: &dynamic_loop),
	  http_io(*loop),
	  binary_io(*loop),
	  quit(*loop),
	  http_sock(http_sock_),
	  binary_sock(binary_sock_)
{
	sigint.set<XapiandServer, &XapiandServer::signal_cb>(this);
	sigint.start(SIGINT);
	sigterm.set<XapiandServer, &XapiandServer::signal_cb>(this);
	sigterm.start(SIGTERM);
	
	quit.set<XapiandServer, &XapiandServer::quit_cb>(this);
	quit.start();

	http_io.set<XapiandServer, &XapiandServer::io_accept_http>(this);
	http_io.start(http_sock, ev::READ);

	binary_io.set<XapiandServer, &XapiandServer::io_accept_binary>(this);
	binary_io.start(binary_sock, ev::READ);
}


XapiandServer::~XapiandServer()
{
	http_io.stop();
	binary_io.stop();
	quit.stop();
	sigint.stop();
	sigterm.stop();
}


void XapiandServer::run()
{
	LOG_OBJ(this, "Starting loop...\n");
	loop->run(0);
}

void XapiandServer::quit_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking loop!\n");
	loop->break_loop();
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
		new HttpClient(loop, client_sock, &database_pool, active_timeout, idle_timeout);
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
		new BinaryClient(loop, client_sock, &database_pool, active_timeout, idle_timeout);
	}
}

void XapiandServer::destroy()
{
	if (http_sock == -1 && binary_sock == -1) {
		return;
	}

	if (http_sock != -1) {
		http_io.stop();
		::close(http_sock);
		http_sock = -1;
	}

	if (binary_sock != -1) {
		binary_io.stop();
		::close(binary_sock);
		binary_sock = -1;
	}

	LOG_OBJ(this, "DESTROYED!\n");
}


void XapiandServer::signal_cb(ev::sig &signal, int revents)
{
	sig_shutdown_handler(signal.signum);
	if (shutdown) {
		destroy();
		if (total_clients == 0) {
			shutdown_asap = shutdown;
		}
	}
	if (shutdown_asap) {
		LOG_OBJ(this, "Breaking default loop!\n");
		signal.loop.break_loop();
		quit.send();
	}
}
