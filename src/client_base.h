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

#pragma once

#include "xapiand.h"

#include "servers/server_base.h"

#include "compressor.h"

#include "ev/ev++.h"

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

class Buffer {
	size_t len;
	char *data;

public:
	size_t pos;
	char type;

	Buffer(char type_, const char *bytes, size_t nbytes)
		: len(nbytes),
		  data(new char [len]),
		  pos(0),
		  type(type_)
	{
		memcpy(data, bytes, len);
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


enum class MODE;
enum class WR;


class BaseClient : public Task<>, public Worker {
	friend Compressor;

	bool _write(int fd, bool async);

protected:
	BaseClient(std::shared_ptr<BaseServer> server_, ev::loop_ref *loop_, int sock_);

public:
	virtual ~BaseClient();

	virtual void on_read_file(const char *buf, size_t received) = 0;

	virtual void on_read_file_done() = 0;

	virtual void on_read(const char *buf, size_t received) = 0;

	void close();

	void destructor_body();

	bool write(const char *buf, size_t buf_size);

	inline bool write(const char *buf) {
		return write(buf, strlen(buf));
	}

	inline bool write(const std::string &buf) {
		return write(buf.c_str(), buf.size());
	}

	inline decltype(auto) manager() noexcept {
		return std::static_pointer_cast<BaseServer>(_parent)->manager();
	}

protected:
	std::mutex qmtx;

	ev::io io_read;
	ev::io io_write;
	ev::async async_write;
	ev::async async_read;

	std::atomic_bool closed;
	int sock;
	int written;
	std::string length_buffer;

	std::unique_ptr<Compressor> compressor;
	char *read_buffer;

	MODE mode;
	ssize_t file_size;
	size_t block_size;

	Endpoints endpoints;

	queue::Queue<std::shared_ptr<Buffer>> write_queue;

	void async_write_cb(ev::async &watcher, int revents);
	void async_read_cb(ev::async &watcher, int revents);

	void io_cb_update();

	// Generic callback
	void io_cb(ev::io &watcher, int revents);

	// Receive message from client socket
	void io_cb_read(int fd);

	// Socket is writable
	void io_cb_write(int fd);

	WR write_directly(int fd);

	void read_file();
	bool send_file(int fd);


public:
	void shutdown();
	void destroy();
};
