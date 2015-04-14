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

#ifndef XAPIAND_INCLUDED_CLIENT_BASE_H
#define XAPIAND_INCLUDED_CLIENT_BASE_H

#include <ev++.h>

#include "server.h"
#include "threadpool.h"
#include "database.h"

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

class Buffer {
	char *data;
	size_t len;

public:
	size_t pos;
	char type;

	Buffer(char type_, const char *bytes, size_t nbytes)
		: pos(0),
		  len(nbytes),
		  type(type_),
		  data(new char [nbytes])
	{
		memcpy(data, bytes, nbytes);
	}

	virtual ~Buffer() {
		delete [] data;
	}

	const char *dpos() {
		return data + pos;
	}

	size_t nbytes() {
		return len - pos;
	}
};


class BaseClient : public Task {
public:
	XapiandServer *server;

	BaseClient(XapiandServer *server_, ev::loop_ref *loop, int s, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	virtual ~BaseClient();

protected:
	friend XapiandServer;
	std::list<BaseClient *>::const_iterator iterator;


	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	ev::io io_read;
	ev::io io_write;
	ev::async async_write;

	bool closed;
	int sock;

	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	Endpoints endpoints;

	Queue<Buffer *> write_queue;

	void async_write_cb(ev::async &watcher, int revents);

	void io_update();

	// Generic callback
	void io_cb(ev::io &watcher, int revents);

	// Receive message from client socket
	void read_cb();
	
	// Socket is writable
	void write_cb();

	virtual void on_read(const char *buf, ssize_t received) = 0;

	inline void write(const char *buf)
	{
		write(buf, strlen(buf));
	}
	
	inline void write(const std::string &buf)
	{
		write(buf.c_str(), buf.size());
	}

	void write(const char *buf, size_t buf_size);
	
	void close();
	void destroy();
	void shutdown();
};


#endif  /* XAPIAND_INCLUDED_CLIENT_BASE_H */