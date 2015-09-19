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

#ifndef XAPIAND_INCLUDED_CLIENT_BASE_H
#define XAPIAND_INCLUDED_CLIENT_BASE_H

#include "xapiand.h"

#include "worker.h"
#include "server.h"
#include "threadpool.h"
#include "database.h"

#include "compressor.h"

#include "ev/ev++.h"

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


class BaseClient : public Task, public Worker {
	friend Compressor;
public:
	BaseClient(XapiandServer *server_, ev::loop_ref *loop, int s, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	virtual ~BaseClient();

	inline XapiandServer * server() const {
		return static_cast<XapiandServer *>(_parent);
	}

	inline XapiandManager *manager() const {
		return static_cast<XapiandServer *>(_parent)->manager();
	}

protected:
	friend XapiandServer;

	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	ev::io io_read;
	ev::io io_write;
	ev::async async_write;

	bool closed;
	int sock;
	int written;
	std::string length_buffer;

	Compressor *compressor;
	char *read_buffer;

	int mode;
	size_t file_size;
	size_t block_size;

	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	Endpoints endpoints;

	Queue<Buffer *> write_queue;

	void async_write_cb(ev::async &watcher, int revents);

	void io_update();

	// Generic callback
	void io_cb(ev::io &watcher, int revents);

	// Receive message from client socket
	void read_cb(ev::io &watcher, int revents);

	// Socket is writable
	void write_cb(ev::io &watcher, int revents);

	int write_directly(int fd);

	void read_file();
	bool send_file(int fd);
	void destroy();
	void shutdown();

public:
	virtual void on_read_file(const char *buf, size_t received) = 0;

	virtual void on_read_file_done() = 0;

	virtual void on_read(const char *buf, size_t received) = 0;

	void close();

	bool write(const char *buf, size_t buf_size);

	inline bool write(const char *buf)
	{
		return write(buf, strlen(buf));
	}

	inline bool write(const std::string &buf)
	{
		return write(buf.c_str(), buf.size());
	}
};

#endif  /* XAPIAND_INCLUDED_CLIENT_BASE_H */