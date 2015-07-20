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

#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_base.h"
#include "utils.h"


const int WRITE_QUEUE_SIZE = 10;

#define WR_OK 0
#define WR_ERR 1
#define WR_RETRY 2
#define WR_PENDING 3

BaseClient::BaseClient(XapiandServer *server_, ev::loop_ref *loop_, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_)
	: Worker(server_, loop_),
	  io_read(*loop),
	  io_write(*loop),
	  async_write(*loop),
	  closed(false),
	  sock(sock_),
	  written(0),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_),
	  write_queue(WRITE_QUEUE_SIZE)
{
	inc_ref();

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = ++XapiandServer::total_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	async_write.set<BaseClient, &BaseClient::async_write_cb>(this);
	async_write.start();

	io_read.set<BaseClient, &BaseClient::io_cb>(this);
	io_read.start(sock, ev::READ);

	io_write.set<BaseClient, &BaseClient::io_cb>(this);
	io_write.set(sock, ev::WRITE);

	LOG_OBJ(this, "CREATED CLIENT! (%d clients)\n", total_clients);
}


BaseClient::~BaseClient()
{
	destroy();

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = --XapiandServer::total_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	LOG_OBJ(this, "DELETED CLIENT! (%d clients left)\n", total_clients);
	assert(total_clients >= 0);
}


void BaseClient::destroy()
{
	close();

	pthread_mutex_lock(&qmtx);
	if (sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	// Stop and free watcher if client socket is closing
	io_read.stop();
	io_write.stop();

	::close(sock);
	sock = -1;
	pthread_mutex_unlock(&qmtx);

	write_queue.finish();
	while (!write_queue.empty()) {
		Buffer *buffer;
		if (write_queue.pop(buffer, 0)) {
			delete buffer;
		}
	}

	LOG_OBJ(this, "DESTROYED CLIENT!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	LOG_OBJ(this, "CLOSED CLIENT!\n");
}


void BaseClient::io_update() {
	if (sock != -1) {
		if (write_queue.empty()) {
			if (closed) {
				destroy();
			} else {
				io_write.stop();
			}
		} else {
			io_write.start();
		}
	}

	if (sock == -1) {
		rel_ref();
	}

}


void BaseClient::io_cb(ev::io &watcher, int revents)
{
	if (revents & EV_ERROR) {
		LOG_ERR(this, "ERROR: got invalid event (sock=%d): %s\n", sock, strerror(errno));
		destroy();
		return;
	}

	LOG_EV(this, "%s (sock=%d) %x\n", (revents & EV_WRITE & EV_READ) ? "IO_CB" : (revents & EV_WRITE) ? "WRITE_CB" : (revents & EV_READ) ? "READ_CB" : "IO_CB", sock, revents);

	if (sock == -1) {
		return;
	}

	assert(sock == watcher.fd);

	if (sock != -1 && revents & EV_WRITE) {
		write_cb();
	}

	if (sock != -1 && revents & EV_READ) {
		read_cb();
	}

	io_update();
}


int BaseClient::write_directly()
{
	if (sock == -1 && !write_queue.empty()) {
		return WR_ERR;
	} else if (!write_queue.empty()) {
		Buffer* buffer = write_queue.front();
		
		size_t buf_size = buffer->nbytes();
		const char * buf = buffer->dpos();
		
#ifdef MSG_NOSIGNAL
		ssize_t written = ::send(sock, buf, buf_size, MSG_NOSIGNAL);
#else
		ssize_t written = ::write(sock, buf, buf_size);
#endif

		if (written < 0) {
			if (ignored_errorno(errno, false)) {
				return WR_RETRY;
			} else {
				LOG_ERR(this, "ERROR: write error (sock=%d): %s\n", sock, strerror(errno));
				return WR_ERR;
			}
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				if(write_queue.pop(buffer)) {
					delete buffer;
					if (write_queue.empty()) {
						return WR_OK;
					} else {
						return WR_PENDING;
					}
				}
			} else {
				return WR_PENDING;
			}
		}
	   }
	return WR_OK;
}


void BaseClient::write_cb()
{
	int status;
	do {
		pthread_mutex_lock(&qmtx);
		status = write_directly();
		pthread_mutex_unlock(&qmtx);
		if (status == WR_ERR) {
			destroy();
			return;
		} else if (status == WR_RETRY) {
			io_write.start();
			return;
		}
	} while (status != WR_OK);
	io_write.stop();
}

	
void BaseClient::read_cb()
{
	if (sock != -1) {
		char buf[1024];

		ssize_t received = ::read(sock, buf, sizeof(buf));

		if (received < 0) {
			if (sock != -1 && !ignored_errorno(errno, false)) {
				LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", sock, strerror(errno));
				destroy();
			}
		} else if (received == 0) {
			// The peer has closed its half side of the connection.
			LOG_CONN(this, "Received EOF (sock=%d)!\n", sock);
			destroy();
		} else {
			LOG_CONN_WIRE(this, "(sock=%d) -->> '%s'\n", sock, repr(buf, received).c_str());
			on_read(buf, received);
		}
	}
}


void BaseClient::async_write_cb(ev::async &watcher, int revents)
{
	io_update();
}


bool BaseClient::write(const char *buf, size_t buf_size)
{
	int status;
	LOG_CONN_WIRE(this, "(sock=%d) <ENQUEUE> '%s'\n", sock, repr(buf, buf_size).c_str());

	Buffer *buffer = new Buffer('\0', buf, buf_size);
	if (!write_queue.push(buffer)) {
		return false;
	}

	written += 1;

	do {
		pthread_mutex_lock(&qmtx);
		status = write_directly();
		pthread_mutex_unlock(&qmtx);
		if (status == WR_ERR) {
			destroy();
			return false;
		} else if (status == WR_RETRY) {
			async_write.send();
			return true;
		}
	} while (status != WR_OK);
	return true;
}

void BaseClient::shutdown()
{
	::shutdown(sock, SHUT_RDWR);

	Worker::shutdown();

	if (manager()->shutdown_now) {
		LOG_EV(this, "Signaled destroy!!\n");
		destroy();
		rel_ref();
	}
}
