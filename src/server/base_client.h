/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <atomic>                   // for std::atomic_bool, std::atomic_size_t
#include <memory>                   // for std::shared_ptr, std::unique_ptr
#include <string>                   // for std::string
#include <string_view>              // for std::string_view
#include <sys/types.h>              // for ssize_t

#include "client_compressor.h"      // for ClientLZ4Compressor, ClientLZ4Decompressor
#include "buffer.h"                 // for Buffer
#include "ev/ev++.h"                // for ev::async, ev::io, ev::loop_ref
#include "io.hh"                    // for io::*
#include "queue.h"                  // for Queue
#include "worker.h"                 // for Worker


#define BUF_SIZE 4096


enum class WR {
	OK,
	ERROR,
	RETRY,
	PENDING
};


enum class MODE {
	READ_BUF,
	READ_FILE_TYPE,
	READ_FILE
};


template <typename ClientImpl>
class BaseClient : public Worker {
	friend ClientLZ4Compressor<BaseClient<ClientImpl>>;
	friend ClientLZ4Decompressor<BaseClient<ClientImpl>>;

	std::mutex _mutex;

protected:
	ev::io io_read;
	ev::io io_write;
	ev::async write_start_async;
	ev::async read_start_async;

	std::atomic_bool waiting;
	std::atomic_bool running;
	std::atomic_bool shutting_down;

	int sock;
	std::atomic_bool closed;

	size_t writes;

	std::atomic_size_t total_received_bytes;
	std::atomic_size_t total_sent_bytes;

	MODE mode;
	ssize_t file_size;
	std::string file_size_buffer;
	size_t block_size;
	bool receive_checksum;

	queue::Queue<std::shared_ptr<Buffer>> write_queue;

	std::unique_ptr<ClientLZ4Decompressor<BaseClient<ClientImpl>>> decompressor;

	ClientImpl& client;

	BaseClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_);
	~BaseClient() noexcept;

	void write_start_async_cb(ev::async &watcher, int revents);
	void read_start_async_cb(ev::async &watcher, int revents);

	void destroy_impl() override;
	void start_impl() override;
	void stop_impl() override;

	void shutdown_impl(long long asap, long long now) override;

	void io_cb_read(ev::io &watcher, int revents) noexcept;

	void io_cb_write(ev::io &watcher, int revents) noexcept;

	WR write_from_queue();
	WR write_from_queue(int max);

	void read_file();

	void close();

	bool is_waiting() const {
		return waiting.load(std::memory_order_relaxed);
	}

	bool is_running() const {
		return running.load(std::memory_order_relaxed);
	}

	bool is_shutting_down() const {
		return shutting_down.load(std::memory_order_relaxed);
	}

	bool is_closed() const {
		return closed.load(std::memory_order_relaxed);
	}

	bool is_idle();

	ssize_t on_read(const char* buf, ssize_t received);

	void on_read_file(const char* buf, ssize_t received);

	void on_read_file_done();

	bool send_file(int fd, size_t offset = 0);

public:
	bool init(int sock_) noexcept;

	bool write(const char *buf, size_t buf_size);

	bool write(std::string_view buf) {
		return write(buf.data(), buf.size());
	}

	bool write_file(std::string_view path, bool unlink = false);

	bool write_buffer(const std::shared_ptr<Buffer>& buffer);
};
