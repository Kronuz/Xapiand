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

#include "base_client.h"

#include <errno.h>               // for errno, ECONNRESET
#include <memory>                // for std::shared_ptr
#include <sys/socket.h>          // for SHUT_RDWR
#include <sysexits.h>            // for EX_SOFTWARE
#include <type_traits>           // for remove_reference<>::type
#include <utility>               // for std::move
#include <xapian.h>              // for SerialisationError

#include "cassert.hh"            // for assert

#include "ev/ev++.h"             // for ::EV_ERROR, ::EV_READ, ::EV_WRITE
#include "ignore_unused.h"       // for ignore_unused
#include "io.hh"                 // for io::read, io::close, io::lseek, io::write
#include "length.h"              // for serialise_length, unserialise_length
#include "likely.h"              // for likely, unlikely
#include "log.h"                 // for L_CALL, L_ERR, L_EV, L_CONN, L_OBJ
#include "manager.h"             // for sig_exit
#include "readable_revents.hh"   // for readable_revents
#include "repr.hh"               // for repr
#include "server.h"              // for XapiandServer


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


BaseClient::BaseClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_)
	: Worker(std::move(parent_), ev_loop_, ev_flags_),
	  io_read(*ev_loop),
	  io_write(*ev_loop),
	  write_start_async(*ev_loop),
	  read_start_async(*ev_loop),
	  waiting(false),
	  running(false),
	  shutting_down(false),
	  sock(sock_),
	  closed(false),
	  writes(0),
	  total_received_bytes(0),
	  total_sent_bytes(0),
	  mode(MODE::READ_BUF),
	  write_queue(WRITE_QUEUE_LIMIT, -1, WRITE_QUEUE_THRESHOLD)
{
	if (sock == -1) {
		throw std::invalid_argument("Invalid socket");
	}

	write_start_async.set<BaseClient, &BaseClient::write_start_async_cb>(this);
	read_start_async.set<BaseClient, &BaseClient::read_start_async_cb>(this);

	io_write.set<BaseClient, &BaseClient::io_cb_write>(this);
	io_write.set(sock, ev::WRITE);

	++XapiandServer::total_clients;

	start();
}


BaseClient::~BaseClient()
{
	if (XapiandServer::total_clients.fetch_sub(1) == 0) {
		L_CRIT("Inconsistency in number of binary clients");
		sig_exit(-EX_SOFTWARE);
	}

	// If shutting down and there are no more clients connected,
	// continue shutdown.
	if (XapiandManager::manager->shutdown_asap.load() != 0) {
		if (XapiandServer::total_clients == 0) {
			XapiandManager::manager->shutdown_sig(0);
		}
	}

	io::close(sock);

	Worker::deinit();
}


void
BaseClient::close()
{
	L_CALL("BaseClient::close()");

	if (!closed.exchange(true)) {
		io::shutdown(sock, SHUT_RDWR);
	}
}


void
BaseClient::destroy_impl()
{
	L_CALL("BaseClient::destroy_impl()");

	Worker::destroy_impl();

	close();
}


void
BaseClient::start_impl()
{
	L_CALL("BaseClient::start_impl()");

	Worker::start_impl();

	write_start_async.start();
	L_EV("Start client's async update event");

	read_start_async.start();
	L_EV("Start client's async read start event");

	io_read.start();
	L_EV("Start client's read event (sock=%d)", sock);
}


void
BaseClient::stop_impl()
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


WR
BaseClient::write_from_queue()
{
	L_CALL("BaseClient::write_from_queue()");

	if (closed) {
		L_ERR("ERROR: write error {sock:%d}: Socket already closed!", sock);
		L_CONN("WR:ERR.1: {sock:%d}", sock);
		return WR::ERROR;
	}

	std::lock_guard<std::mutex> lk(_mutex);

	std::shared_ptr<Buffer> buffer;
	if (write_queue.front(buffer)) {
		size_t buf_size = buffer->size();
		const char *buf_data = buffer->data();

#ifdef MSG_NOSIGNAL
		ssize_t sent = io::send(sock, buf_data, buf_size, MSG_NOSIGNAL);
#else
		ssize_t sent = io::write(sock, buf_data, buf_size);
#endif

		if (sent < 0) {
			if (io::ignored_errno(errno, true, true, false)) {
				L_CONN("WR:RETRY: {sock:%d} - %d: %s", sock, errno, strerror(errno));
				return WR::RETRY;
			}

			L_ERR("ERROR: write error {sock:%d} - %d: %s", sock, errno, strerror(errno));
			L_CONN("WR:ERR.2: {sock:%d}", sock);
			close();
			return WR::ERROR;
		}

		total_sent_bytes += sent;
		L_TCP_WIRE("{sock:%d} <<-- %s (%zu bytes)", sock, repr(buf_data, sent, true, true, 500), sent);

		buffer->remove_prefix(sent);
		if (buffer->size() == 0) {
			if (write_queue.pop(buffer)) {
				if (write_queue.empty()) {
					L_CONN("WR:OK: {sock:%d}", sock);
					return WR::OK;
				}
			}
		}

		L_CONN("WR:PENDING: {sock:%d}", sock);
		return WR::PENDING;
	}

	L_CONN("WR:OK.2: {sock:%d}", sock);
	return WR::OK;
}


WR
BaseClient::write_from_queue(int max)
{
	L_CALL("BaseClient::write_from_queue(%d)", max);

	WR status = WR::PENDING;

	for (int i = 0; max < 0 || i < max; ++i) {
		status = write_from_queue();
		if (status != WR::PENDING) {
			return status;
		}
	}

	return status;
}


bool
BaseClient::write(const char *buf, size_t buf_size)
{
	L_CALL("BaseClient::write(<buf>, %zu)", buf_size);

	return write_buffer(std::make_shared<Buffer>('\0', buf, buf_size));
}


bool
BaseClient::write_file(std::string_view path, bool unlink)
{
	L_CALL("BaseClient::write_file(<path>, <unlink>)");

	return write_buffer(std::make_shared<Buffer>(path, unlink));
}


bool
BaseClient::write_buffer(const std::shared_ptr<Buffer>& buffer)
{
	L_CALL("BaseClient::write_buffer(<buffer>)");

	if (!write_queue.push(buffer)) {
		return false;
	}

	if (closed) {
		return false;
	}

	writes += 1;
	L_TCP_ENQUEUE("{sock:%d} <ENQUEUE> buffer (%zu bytes)", sock, buffer->full_size());

	switch (write_from_queue(-1)) {
		case WR::RETRY:
		case WR::PENDING:
			write_start_async.send();
			/* FALLTHROUGH */
		case WR::OK:
			return true;
		default:
			return false;
	}
}


void
BaseClient::io_cb_write(ev::io &watcher, int revents)
{
	L_CALL("BaseClient::io_cb_write(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), sock);

	L_EV_BEGIN("BaseClient::io_cb_write:BEGIN");
	L_EV_END("BaseClient::io_cb_write:END");

	assert(sock == watcher.fd);
	ignore_unused(watcher);

	L_DEBUG_HOOK("BaseClient::io_cb_write", "BaseClient::io_cb_write(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), sock);

	if (closed) {
		detach();
		return;
	}

	if ((revents & EV_ERROR) != 0) {
		L_ERR("ERROR: got invalid event {sock:%d} - %d: %s", sock, errno, strerror(errno));
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
					if (shutting_down) {
						detach();
					}
				}
			});
			break;
	}


	if (closed) {
		detach();
	}
}


void
BaseClient::write_start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("BaseClient::write_start_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	L_EV_BEGIN("BaseClient::write_start_async_cb:BEGIN");
	L_EV_END("BaseClient::write_start_async_cb:END");

	ignore_unused(revents);

	if (!closed) {
		io_write.start();
		L_EV("Enable write event [%d]", io_write.is_active());
	}
}


void
BaseClient::read_start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("BaseClient::read_start_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	L_EV_BEGIN("BaseClient::read_start_async_cb:BEGIN");
	L_EV_END("BaseClient::read_start_async_cb:END");

	ignore_unused(revents);

	if (!closed) {
		io_read.start();
		L_EV("Enable read event [%d]", io_read.is_active());
	}
}


void
BaseClient::read_file()
{
	L_CALL("BaseClient::read_file()");

	mode = MODE::READ_FILE_TYPE;
	file_size = -1;
	receive_checksum = false;
}
