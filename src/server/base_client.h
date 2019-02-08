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

#include <atomic>                   // for std::atomic_bool
#include <errno.h>                  // for errno, ECONNRESET
#include <memory>                   // for std::shared_ptr, std::unique_ptr
#include <string>                   // for std::string
#include <sys/types.h>              // for ssize_t

#include "buffer.h"                 // for Buffer
#include "cassert.h"                // for ASSERT
#include "client_compressor.h"      // for ClientLZ4Compressor, ClientLZ4Decompressor
#include "ev/ev++.h"                // for ev::async, ev::io, ev::loop_ref
#include "error.hh"                 // for error:name, error::description
#include "io.hh"                    // for io::*
#include "lz4/xxhash.h"             // for XXH32_state_t
#include "ignore_unused.h"          // for ignore_unused
#include "log.h"                    // for L_CALL, L_ERR, L_EV_BEGIN, L_CONN
#include "queue.h"                  // for Queue
#include "readable_revents.hh"      // for readable_revents
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


class BaseClient : public Worker {
	void destroy_impl() override;
	void start_impl() override;
	void stop_impl() override;

	std::mutex _mutex;

public:
	bool write(const char *buf, size_t buf_size);

	bool write(std::string_view buf) {
		return write(buf.data(), buf.size());
	}

	bool write_file(std::string_view path, bool unlink = false);

	bool write_buffer(const std::shared_ptr<Buffer>& buffer);

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

	BaseClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_);
	~BaseClient() noexcept;

	void write_start_async_cb(ev::async &watcher, int revents);
	void read_start_async_cb(ev::async &watcher, int revents);

	// Socket is writable
	void _io_cb_write(ev::io &watcher, int revents);

	void io_cb_write(ev::io &watcher, int revents) noexcept {
		try {
			_io_cb_write(watcher, revents);
		} catch (...) {
			L_EXC("ERROR: Client died with an unhandled exception");
			stop();
			destroy();
			detach();
		}
	}

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
};


// The following is the CRTP for BaseClient

template <typename ClientImpl>
class MetaBaseClient : public BaseClient {
	friend ClientLZ4Compressor<MetaBaseClient<ClientImpl>>;
	friend ClientLZ4Decompressor<MetaBaseClient<ClientImpl>>;

protected:
	std::unique_ptr<ClientLZ4Decompressor<MetaBaseClient<ClientImpl>>> decompressor;

	ClientImpl& client;

	bool is_idle() {
		return client.is_idle();
	}

	ssize_t on_read(const char* buf, ssize_t received) {
		return client.on_read(buf, received);
	}

	void on_read_file(const char* buf, ssize_t received) {
		client.on_read_file(buf, received);
	}

	void on_read_file_done() {
		client.on_read_file_done();
	}

	MetaBaseClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_) :
		BaseClient(parent_, ev_loop_, ev_flags_, sock_),
		client(static_cast<ClientImpl&>(*this)) {
		io_read.set<MetaBaseClient<ClientImpl>, &MetaBaseClient<ClientImpl>::io_cb_read>(this);
		io_read.set(sock, ev::READ);
	}

	// Receive message from client socket
	void _io_cb_read(ev::io &watcher, int revents) {
		L_CALL("BaseClient::io_cb_read(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

		L_EV_BEGIN("BaseClient::io_cb_read:BEGIN");
		L_EV_END("BaseClient::io_cb_read:END");

		ASSERT(sock == -1 || sock == watcher.fd);
		ignore_unused(watcher);

		L_DEBUG_HOOK("BaseClient::io_cb_read", "BaseClient::io_cb_read(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

		if (closed) {
			stop();
			destroy();
			detach();
			return;
		}

		if ((revents & EV_ERROR) != 0) {
			L_ERR("ERROR: got invalid event {{sock:{}}} - {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
			stop();
			destroy();
			detach();
			return;
		}

		char read_buffer[BUF_SIZE];
		ssize_t received = io::read(watcher.fd, read_buffer, BUF_SIZE);

		if (received < 0) {
			if (io::ignored_errno(errno, true, true, false)) {
				L_CONN("Ignored error: {{sock:{}}} - {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
				return;
			}

			if (errno == ECONNRESET) {
				L_CONN("Received ECONNRESET {{sock:{}}}!", watcher.fd);
			} else {
				L_ERR("ERROR: read error {{sock:{}}} - {}: {} ({})", watcher.fd, error::name(errno), errno, error::description(errno));
			}
			close();
			on_read(nullptr, received);
			detach();
			return;
		}

		if (received == 0) {
			// The peer has closed its half side of the connection.
			L_CONN("Received EOF {{sock:{}}}!", watcher.fd);
			close();
			on_read(nullptr, received);
			detach();
			return;
		}

		const char* buf_data = read_buffer;
		const char* buf_end = read_buffer + received;

		total_received_bytes += received;
		L_TCP_WIRE("{{sock:{}}} -->> {} ({} bytes)", watcher.fd, repr(buf_data, received, true, true, 500), received);

		do {
			if ((received > 0) && mode == MODE::READ_BUF) {
				buf_data += on_read(buf_data, received);
				received = buf_end - buf_data;
			}

			if ((received > 0) && mode == MODE::READ_FILE_TYPE) {
				L_CONN("Receiving file {{sock:{}}}...", watcher.fd);
				decompressor = std::make_unique<ClientLZ4Decompressor<MetaBaseClient<ClientImpl>>>(static_cast<MetaBaseClient<ClientImpl>&>(*this));
				file_size_buffer.clear();
				receive_checksum = false;
				mode = MODE::READ_FILE;
			}

			if ((received > 0) && mode == MODE::READ_FILE) {
				if (file_size == -1) {
					try {
						auto processed = -file_size_buffer.size();
						file_size_buffer.append(buf_data, std::min(buf_data + 10, buf_end));  // serialized size is at most 10 bytes
						const char* o = file_size_buffer.data();
						const char* p = o;
						const char* p_end = p + file_size_buffer.size();
						file_size = unserialise_length(&p, p_end);
						processed += p - o;
						file_size_buffer.clear();
						buf_data += processed;
						received -= processed;
					} catch (const Xapian::SerialisationError) {
						break;
					}

					if (receive_checksum) {
						receive_checksum = false;
						if (!decompressor->verify(static_cast<uint32_t>(file_size))) {
							L_ERR("Data is corrupt!");
							return;
						}
						on_read_file_done();
						mode = MODE::READ_BUF;
						decompressor.reset();
						continue;
					}

					block_size = file_size;
					decompressor->clear();
				}

				const char *file_buf_to_write;
				size_t block_size_to_write;
				size_t buf_left_size = buf_end - buf_data;
				if (block_size < buf_left_size) {
					file_buf_to_write = buf_data;
					block_size_to_write = block_size;
					buf_data += block_size;
					received = buf_left_size - block_size;
				} else {
					file_buf_to_write = buf_data;
					block_size_to_write = buf_left_size;
					buf_data = nullptr;
					received = 0;
				}

				if (block_size_to_write) {
					decompressor->append(file_buf_to_write, block_size_to_write);
					block_size -= block_size_to_write;
				}

				if (file_size == 0) {
					decompressor->clear();
					decompressor->decompress();
					receive_checksum = true;
					file_size = -1;
				} else if (block_size == 0) {
					decompressor->decompress();
					file_size = -1;
				}
			}

			if (closed) {
				detach();
				return;
			}
		} while (received > 0);
	}

	void io_cb_read(ev::io &watcher, int revents) noexcept {
		try {
			_io_cb_read(watcher, revents);
		} catch (...) {
			L_EXC("ERROR: Client died with an unhandled exception");
			stop();
			destroy();
			detach();
		}
	}

	bool send_file(int fd, size_t offset = 0) {
		L_CALL("BaseClient::send_file({}, {})", fd, offset);

		ClientLZ4Compressor<MetaBaseClient<ClientImpl>> compressor(static_cast<MetaBaseClient<ClientImpl>&>(*this), fd, offset);
		return (compressor.compress() != -1);
	}

	void shutdown_impl(long long asap, long long now) override {
		L_CALL("HttpClient::shutdown_impl({}, {})", asap, now);

		Worker::shutdown_impl(asap, now);

		if (asap) {
			shutting_down = true;
			if (now != 0 || is_idle()) {
				stop(false);
				destroy(false);
				detach();
			}
		} else {
			if (is_idle()) {
				stop(false);
				destroy(false);
				detach();
			}
		}
	}
};
