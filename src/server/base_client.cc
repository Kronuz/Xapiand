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

#include "base_client.h"

#include <cassert>                  // for assert
#include <errno.h>                  // for errno
#include <memory>                   // for std::shared_ptr
#include <sys/socket.h>             // for SHUT_RDWR
#include <sysexits.h>               // for EX_SOFTWARE
#include <type_traits>              // for remove_reference<>::type
#include <utility>                  // for std::move

#include "error.hh"                 // for error::name, error::description
#include "ev/ev++.h"                // for ::EV_ERROR, ::EV_READ, ::EV_WRITE
#include "io.hh"                    // for io::read, io::close, io::lseek, io::write
#include "length.h"                 // for serialise_length, unserialise_length
#include "likely.h"                 // for likely, unlikely
#include "log.h"                    // for L_CALL, L_ERR, L_EV, L_CONN, L_OBJ
#include "manager.h"                // for sig_exit
#include "readable_revents.hh"      // for readable_revents
#include "repr.hh"                  // for repr
#include "thread.hh"                // for get_thread_name
#include "xapian.h"                 // for SerialisationError


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_TCP_ENQUEUE
// #define L_TCP_ENQUEUE L_GREEN
// #undef L_TCP_WIRE
// #define L_TCP_WIRE L_WHITE
// #undef L_EV
// #define L_EV L_MEDIUM_PURPLE
// #undef L_EV_BEGIN
// #define L_EV_BEGIN L_DELAYED_200
// #undef L_EV_END
// #define L_EV_END L_DELAYED_N_UNLOG


constexpr int WRITE_QUEUE_LIMIT = 10;
constexpr int WRITE_QUEUE_THRESHOLD = WRITE_QUEUE_LIMIT * 2 / 3;


/*
 *  ____                  ____ _ _            _
 * | __ )  __ _ ___  ___ / ___| (_) ___ _ __ | |_
 * |  _ \ / _` / __|/ _ \ |   | | |/ _ \ '_ \| __|
 * | |_) | (_| \__ \  __/ |___| | |  __/ | | | |_
 * |____/ \__,_|___/\___|\____|_|_|\___|_| |_|\__|
 *
 */

// The following are only here so BaseClient
// implementation for each clients is compiled:
#include "server/http_client.h"
#include "server/remote_protocol_client.h"
#include "server/replication_protocol_client.h"


template <typename ClientImpl>
BaseClient<ClientImpl>::BaseClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(std::move(parent_), ev_loop_, ev_flags_),
	  io_read(*ev_loop),
	  io_write(*ev_loop),
	  write_start_async(*ev_loop),
	  read_start_async(*ev_loop),
	  waiting(false),
	  running(false),
	  shutting_down(false),
	  sock(-1),
	  closed(true),
	  writes(0),
	  total_received_bytes(0),
	  total_sent_bytes(0),
	  mode(MODE::READ_BUF),
	  write_queue(WRITE_QUEUE_LIMIT, -1, WRITE_QUEUE_THRESHOLD),
	  client(static_cast<ClientImpl&>(*this))
{
	++XapiandManager::total_clients();
}


template <typename ClientImpl>
BaseClient<ClientImpl>::~BaseClient() noexcept
{
	try {
		Worker::deinit();

		if (sock != -1) {
			if (io::close(sock) == -1) {
				L_WARNING("WARNING: close {{sock:{}}} - {} ({}): {}", sock, error::name(errno), errno, error::description(errno));
			}
		}

		if (XapiandManager::total_clients().fetch_sub(1) == 0) {
			L_CRIT("Inconsistency in number of clients");
			sig_exit(-EX_SOFTWARE);
		}

		// If there are no more clients connected, try continue shutdown.
		XapiandManager::try_shutdown();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::init(int sock_) noexcept
{
	L_CALL("BaseClient::init()");

	if (sock_ == -1) {
		return false;
	}

	if (sock != -1) {
		return false;
	}

	closed = false;
	sock = sock_;

	write_start_async.set<BaseClient<ClientImpl>, &BaseClient<ClientImpl>::write_start_async_cb>(this);
	read_start_async.set<BaseClient<ClientImpl>, &BaseClient<ClientImpl>::read_start_async_cb>(this);

	io_write.set<BaseClient<ClientImpl>, &BaseClient<ClientImpl>::io_cb_write>(this);
	io_write.set(sock, ev::WRITE);

	io_read.set<BaseClient<ClientImpl>, &BaseClient<ClientImpl>::io_cb_read>(this);
	io_read.set(sock, ev::READ);

	return true;
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::close()
{
	L_CALL("BaseClient::close()");

	if (!closed.exchange(true)) {
		io::shutdown(sock, SHUT_RDWR);
	}
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::destroy_impl()
{
	L_CALL("BaseClient::destroy_impl()");

	Worker::destroy_impl();

	close();
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::start_impl()
{
	L_CALL("BaseClient::start_impl()");

	Worker::start_impl();

	write_start_async.start();
	L_EV("Start client's async update event");

	read_start_async.start();
	L_EV("Start client's async read start event");

	io_read.start();
	L_EV("Start client's read event {{sock:{}}}", sock);
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::stop_impl()
{
	L_CALL("BaseClient::stop_impl()");

	Worker::stop_impl();

	write_start_async.stop();
	L_EV("Stop client's async update event");

	read_start_async.stop();
	L_EV("Stop client's async read start event");

	io_write.stop();
	L_EV("Stop client's write event");

	io_read.stop();
	L_EV("Stop client's read event");

	write_queue.finish();
	write_queue.clear();
}


template <typename ClientImpl>
WR
BaseClient<ClientImpl>::write_from_queue()
{
	L_CALL("BaseClient::write_from_queue()");

	if (closed) {
		// Catch if connection has been flagged as closed and just return WR::ERROR
		L_CONN("WR:ERR.1: {{sock:{}}}", sock);
		return WR::ERROR;
	}

	std::lock_guard<std::mutex> lk(_mutex);

	std::shared_ptr<Buffer> buffer;
	if (write_queue.pop_front(buffer, 0)) {
		size_t buf_size = buffer->size();
		const char *buf_data = buffer->data();

#ifdef MSG_NOSIGNAL
		ssize_t sent = io::send(sock, buf_data, buf_size, MSG_NOSIGNAL);
#else
		ssize_t sent = io::write(sock, buf_data, buf_size);
#endif

		if (sent < 0) {
			write_queue.push_front(buffer, 0, true);

			if (io::ignored_errno(errno, true, true, false)) {
				L_CONN("WR:RETRY: {{sock:{}}} - {} ({}): {}", sock, error::name(errno), errno, error::description(errno));
				return WR::RETRY;
			}

			if (closed) {
				// Catch if connection has been flagged as closed and just return WR::ERROR
				L_CONN("WR:ERR.2: {{sock:{}}}", sock);
				return WR::ERROR;
			}

			L_ERR("ERROR: write error {{sock:{}}} - {} ({}): {}", sock, error::name(errno), errno, error::description(errno));
			L_CONN("WR:ERR.3: {{sock:{}}}", sock);
			close();
			return WR::ERROR;
		}

		total_sent_bytes += sent;
		L_TCP_WIRE("{{sock:{}}} <<-- {} ({} bytes)", sock, repr(buf_data, sent, true, true, 500), sent);

		buffer->remove_prefix(sent);
		if (buffer->size() != 0) {
			write_queue.push_front(buffer, 0, true);
		} else if (write_queue.empty()) {
			L_CONN("WR:OK: {{sock:{}}}", sock);
			return WR::OK;
		}

		L_CONN("WR:PENDING: {{sock:{}}}", sock);
		return WR::PENDING;
	}

	L_CONN("WR:OK.2: {{sock:{}}}", sock);
	return WR::OK;
}


template <typename ClientImpl>
WR
BaseClient<ClientImpl>::write_from_queue(int max)
{
	L_CALL("BaseClient::write_from_queue({})", max);

	WR status = WR::PENDING;

	for (int i = 0; max < 0 || i < max; ++i) {
		status = write_from_queue();
		if (status != WR::PENDING) {
			return status;
		}
	}

	return status;
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::write(const char *buf, size_t buf_size)
{
	L_CALL("BaseClient::write(<buf>, {})", buf_size);

	if (!buf_size) {
		return true;
	}

	return write_buffer(std::make_shared<Buffer>('\0', buf, buf_size));
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::write_file(std::string_view path, bool unlink)
{
	L_CALL("BaseClient::write_file(<path>, <unlink>)");

	return write_buffer(std::make_shared<Buffer>(path, unlink));
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::write_buffer(const std::shared_ptr<Buffer>& buffer)
{
	L_CALL("BaseClient::write_buffer(<buffer>)");

	do {
		if (closed) {
			return false;
		}
	} while (!write_queue.push_back(buffer, 1));

	writes += 1;
	L_TCP_ENQUEUE("{{sock:{}}} <ENQUEUE> buffer ({} bytes)", sock, buffer->full_size());

	switch (write_from_queue(-1)) {
		case WR::RETRY:
		case WR::PENDING:
			write_start_async.send();
			[[fallthrough]];
		case WR::OK:
			return true;
		default:
			return false;
	}
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::is_idle()
{
	return client.is_idle();
}


template <typename ClientImpl>
ssize_t
BaseClient<ClientImpl>::on_read(const char* buf, ssize_t received)
{
	return client.on_read(buf, received);
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::on_read_file(const char* buf, ssize_t received)
{
	client.on_read_file(buf, received);
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::on_read_file_done()
{
	client.on_read_file_done();
}


// Receive message from client socket
template <typename ClientImpl>
void
BaseClient<ClientImpl>::io_cb_read(ev::io &watcher, int revents) noexcept
{
	L_CALL("BaseClient::io_cb_read(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	try {
		L_EV_BEGIN("BaseClient::io_cb_read:BEGIN");
		L_EV_END("BaseClient::io_cb_read:END");

		assert(sock == -1 || sock == watcher.fd);

		L_DEBUG_HOOK("BaseClient::io_cb_read", "BaseClient<ClientImpl>::io_cb_read(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

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
			on_read(nullptr, received);
			detach();
			return;
		}

		if (received == 0) {
			// The peer has closed its half side of the connection.
			L_CONN("Received EOF {{sock:{}}}!", watcher.fd);
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
				decompressor = std::make_unique<ClientLZ4Decompressor<BaseClient<ClientImpl>>>(static_cast<BaseClient<ClientImpl>&>(*this));
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
	} catch (...) {
		L_EXC("ERROR: Client died with an unhandled exception");
		stop();
		destroy();
		detach();
	}
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::io_cb_write(ev::io &watcher, int revents) noexcept
{
	L_CALL("BaseClient::io_cb_write(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	try {
		L_EV_BEGIN("BaseClient::io_cb_write:BEGIN");
		L_EV_END("BaseClient::io_cb_write:END");

		assert(sock == -1 || sock == watcher.fd);

		L_DEBUG_HOOK("BaseClient::io_cb_write", "BaseClient<ClientImpl>::io_cb_write(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

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

		switch (write_from_queue(10)) {
			case WR::RETRY:
			case WR::PENDING:
				break;
			case WR::ERROR:
			case WR::OK:
				write_queue.empty([&](bool empty) {
					if (empty) {
						io_write.stop();
						L_EV("Disable write event");
						if (is_shutting_down()) {
							detach();
						}
					}
				});
				break;
		}


		if (closed) {
			detach();
		}
	} catch (...) {
		L_EXC("ERROR: Client died with an unhandled exception");
		stop();
		destroy();
		detach();
	}
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::write_start_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("BaseClient::write_start_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("BaseClient::write_start_async_cb:BEGIN");
	L_EV_END("BaseClient::write_start_async_cb:END");

	if (!closed) {
		io_write.start();
		L_EV("Enable write event [{}]", io_write.is_active());
	}
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::read_start_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("BaseClient::read_start_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("BaseClient::read_start_async_cb:BEGIN");
	L_EV_END("BaseClient::read_start_async_cb:END");

	if (!closed) {
		io_read.start();
		L_EV("Enable read event [{}]", io_read.is_active());
	}
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::read_file()
{
	L_CALL("BaseClient::read_file()");

	mode = MODE::READ_FILE_TYPE;
	file_size = -1;
	receive_checksum = false;
}


template <typename ClientImpl>
bool
BaseClient<ClientImpl>::send_file(int fd, size_t offset)
{
	L_CALL("BaseClient::send_file({}, {})", fd, offset);

	ClientLZ4Compressor<BaseClient<ClientImpl>> compressor(static_cast<BaseClient<ClientImpl>&>(*this), fd, offset);
	return (compressor.compress() != -1);
}


template <typename ClientImpl>
void
BaseClient<ClientImpl>::shutdown_impl(long long asap, long long now)
{
	L_CALL("BaseClient::shutdown_impl({}, {})", asap, now);

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


// The following are only here so BaseClient
// implementation for each clients is compiled:
template class BaseClient<HttpClient>;

#ifdef XAPIAND_CLUSTERING
template class BaseClient<RemoteProtocolClient>;
template class BaseClient<ReplicationProtocolClient>;
#endif
