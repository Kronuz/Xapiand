/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "xapiand.h"

#include <atomic>        // for atomic_bool, atomic_int
#include <memory>        // for shared_ptr, unique_ptr
#include <string.h>      // for size_t, memcpy, strlen
#include <string>        // for string
#include <sys/types.h>   // for ssize_t
#include <time.h>        // for time_t

#include "ev/ev++.h"     // for async, io, loop_ref (ptr only)
#include "io.h"          // for io::*
#include "queue.h"       // for Queue
#include "worker.h"      // for Worker

// #define L_CONN L_DEBUG

class BaseServer;
class LZ4CompressFile;

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

class Buffer {
	std::string _data;
	std::string_view _data_view;

	std::string _path;
	int _fd;
	bool _unlink;
	size_t _max_pos;

	void feed() {
		if (_fd != -1 && _data_view.empty() && pos < _max_pos) {
			_data.resize(4096);
			io::lseek(_fd, pos, SEEK_SET);
			auto _read = io::read(_fd, &_data[0], 4096UL);
			if (_read > 0) {
				_data.resize(_read);
			}
			_data_view = _data;
		}
	}

public:
	size_t pos;
	char type;

	Buffer()
		: _data_view(_data),
		  _fd(-1),
		  _unlink(false),
		  _max_pos(0),
		  pos(0),
		  type('\xff')
	{ }

	Buffer(int fd)
		: _fd(fd),
		  _unlink(false),
		  _max_pos(io::lseek(_fd, 0, SEEK_END)),
		  pos(0),
		  type('\0')
	{ }

	Buffer(std::string_view path, bool unlink = false)
		: _path(path),
		  _fd(io::open(_path.c_str())),
		  _unlink(unlink),
		  _max_pos(io::lseek(_fd, 0, SEEK_END)),
		  pos(0),
		  type('\0')
	{ }

	Buffer(char type, const char *bytes, size_t nbytes)
		: _data(bytes, nbytes),
		  _data_view(_data),
		  _fd(-1),
		  _unlink(false),
		  _max_pos(nbytes),
		  pos(0),
		  type(type)
	{ }

	~Buffer() {
		if (_fd != -1) {
			io::close(_fd);
			if (!_path.empty() && _unlink) {
				io::unlink(_path.c_str());
			}
		}
	}

	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&&) = default;
	Buffer& operator=(Buffer&&) = default;

	const char *data() {
		feed();
		return _data_view.data();
	}

	size_t size() {
		feed();
		return std::min(_max_pos, _data_view.size());
	}

	size_t full_size() {
		feed();
		return std::max(_max_pos, _data_view.size());
	}

	void remove_prefix(size_t n) {
		assert(n <= _data_view.size());
		_data_view.remove_prefix(n);
		pos += n;
	}

	// Legacy:

	const char *dpos() {
		return _data.data() + pos;
	}

	size_t nbytes() {
		return _data.size() - pos;
	}
};


enum class MODE;
enum class WR;


class ClientDecompressor;


class BaseClient : public Worker {
	friend LZ4CompressFile;

	void destroyer();
	void stop();

	std::mutex _mutex;

protected:
	BaseClient(const std::shared_ptr<BaseServer>& server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_);

public:
	virtual ~BaseClient();

	virtual ssize_t on_read(const char *buf, ssize_t received) = 0;
	virtual void on_read_file(const char *buf, ssize_t received) = 0;
	virtual void on_read_file_done() = 0;

	void close();

	bool write(const char *buf, size_t buf_size);
	inline bool write(std::string_view buf) {
		return write(buf.data(), buf.size());
	}

	bool write_file(std::string_view path, bool unlink = false);

	bool write_buffer(const std::shared_ptr<Buffer>& buffer);

protected:
	ev::io io_read;
	ev::io io_write;
	ev::async write_start_async;
	ev::async read_start_async;

	std::atomic_bool idle;
	std::atomic_bool shutting_down;
	std::atomic_bool closed;
	std::atomic_int sock;
	int written;

	std::unique_ptr<ClientDecompressor> decompressor;

	MODE mode;
	ssize_t file_size;
	std::string file_size_buffer;
	size_t block_size;
	bool receive_checksum;

	queue::Queue<std::shared_ptr<Buffer>> write_queue;

	void write_start_async_cb(ev::async &watcher, int revents);
	void read_start_async_cb(ev::async &watcher, int revents);

	// Receive message from client socket
	void io_cb_read(ev::io &watcher, int revents);

	// Socket is writable
	void io_cb_write(ev::io &watcher, int revents);

	WR write_from_queue(int fd);
	WR write_from_queue(int fd, int max);

	void read_file();
	bool send_file(int fd, size_t offset=0);

	void destroy_impl() override;
	void shutdown_impl(time_t asap, time_t now) override;
};
