/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
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

// The Asio transport for Xapiand's replication protocol -- the counterpart to
// remote_protocol_service.h, on the same Kronuz/reactor runtime. Two roles, one coroutine each:
//   * serve_replication_connection -- an ACCEPTED inbound connection (server): greet, then
//     answer MSG_GET_CHANGESETS/MSG_SET_REVISION (streaming changesets + whole DB files).
//   * connect_and_replicate -- an OUTBOUND connection (client): connect to a primary, then
//     fetch changesets (receiving whole DB files). Spawned by trigger_replication.
// The blocking Xapian dispatch runs on the reactor's offload pool (the coroutine suspends).
// The handlers write replies + STREAM whole DB files directly to the socket fd (blocking, in
// bounded memory via flume) while the coroutine is suspended -- nothing races the socket.
// FILE_FOLLOWS files are received into a temp file (flume) on the read path, then dispatched.

#pragma once

#include "config.h"   // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

// replication_protocol_client.h pulls NO libev, so no EV_ERROR clash with the Asio headers.
#include "replication_protocol_client.h"      // ReplicationProtocolClient, types, FILE_FOLLOWS
#include "endpoint.h"                          // for Endpoint (trigger args)
#include "length.h"                            // for unserialise_length_and_check (framing)

#include <asio.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <sys/socket.h>                        // ::shutdown
#include <unistd.h>                            // ::write, ::close

#include "flume.h"                             // flume::Receiver (FILE_FOLLOWS file receive)
#include "reactor.h"                           // reactor::TcpServer / Reactor / BindOptions / Abortable

namespace replication {

// The outbound-replication request (moved here from the retired replication_protocol.h): a
// source (primary) endpoint to pull from, the destination (local) endpoint to write, and
// whether this is the cluster's own bootstrap database (a fatal-if-it-fails transfer).
struct TriggerReplicationArgs {
	Endpoint src_endpoint;
	Endpoint dst_endpoint;
	bool cluster_database;
};

namespace detail {

// A flume Sink that writes the decompressed incoming-file bytes to a fd.
struct FdSink {
	int fd;
	bool write(const char* d, std::size_t n) {
		std::size_t off = 0;
		while (off < n) {
			ssize_t w = ::write(fd, d + off, n - off);
			if (w <= 0) { return false; }
			off += static_cast<std::size_t>(w);
		}
		return true;
	}
};

// Registered with the reactor so a handler blocked in a bounded write on the offload pool
// unwedges at server stop: abort() shuts the fd down, making the pending poll return.
struct SocketAbortable : reactor::Abortable {
	int fd;
	explicit SocketAbortable(int fd_) : fd(fd_) {}
	void abort() override { ::shutdown(fd, SHUT_RDWR); }
};

// Read exactly one framed message off the socket into (type_out, body_out). Returns false on
// a clean EOF / read error. A normal message is `[type][serialise_length len][body]`.
// FILE_FOLLOWS (0xfd) is `[0xfd][real_type]` + a flume-framed file, streamed into a temp file;
// type_out is the real type and body_out is the temp-file path (matching the classic on_read).
inline asio::awaitable<bool> read_message(asio::ip::tcp::socket& socket, std::string& buffered,
                                          ReplicationProtocolClient& client, char& type_out, std::string& body_out) {
	using asio::use_awaitable;
	char tmp[64 * 1024];

	for (;;) {
		if (buffered.empty()) {
			std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
			if (n == 0) { co_return false; }
			buffered.append(tmp, n);
		}

		char type = buffered[0];

		if (type == FILE_FOLLOWS) {
			if (buffered.size() < 2) {
				std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
				if (n == 0) { co_return false; }
				buffered.append(tmp, n);
				continue;
			}
			char real_type = buffered[1];
			buffered.erase(0, 2);

			auto [fd, path] = client.new_temp_file();
			if (fd == -1) { co_return false; }
			FdSink sink{fd};
			flume::Receiver<FdSink> receiver(sink);
			using St = flume::Receiver<FdSink>::Status;
			St st = St::NeedMore;
			if (!buffered.empty()) {
				st = receiver.feed(buffered.data(), buffered.size());
				buffered.clear();
			}
			while (st == St::NeedMore) {
				std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
				if (n == 0) { ::close(fd); co_return false; }
				st = receiver.feed(tmp, n);
			}
			::close(fd);
			if (st != St::Done) { co_return false; }
			buffered = receiver.leftover();   // bytes read past the file stream = the next message
			type_out = real_type;
			body_out = path;
			co_return true;
		}

		// Normal message: [type][serialise_length len][body]. unserialise_length_and_check
		// throws SerialisationError if the varint or the len bytes are not all present yet.
		const char* p = buffered.data() + 1;
		const char* p_end = buffered.data() + buffered.size();
		std::size_t len = 0;
		bool need_more = false;
		try {
			len = static_cast<std::size_t>(unserialise_length_and_check(&p, p_end));
		} catch (...) {
			need_more = true;
		}
		if (need_more) {
			std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
			if (n == 0) { co_return false; }
			buffered.append(tmp, n);
			continue;
		}
		std::size_t header = static_cast<std::size_t>(p - buffered.data());
		type_out = type;
		body_out = buffered.substr(header, len);
		buffered.erase(0, header + len);
		co_return true;
	}
}

// Drive one connection's message loop: read a request, dispatch it (offloaded to the pool),
// until EOF or a handler asked to close. `is_server` picks replication_server vs client.
inline asio::awaitable<void> run_loop(asio::ip::tcp::socket& socket, reactor::Reactor* reactor,
                                      ReplicationProtocolClient& client, bool is_server) {
	using asio::use_awaitable;
	std::string buffered;
	for (;;) {
		char type = 0;
		std::string body;
		if (!co_await read_message(socket, buffered, client, type, body)) { break; }

		if (reactor->pool() != nullptr) {
			co_await asio::co_spawn(reactor->pool()->get_executor(),
				[&client, is_server, type, &body]() -> asio::awaitable<void> {
					if (is_server) {
						client.replication_server(static_cast<ReplicationMessageType>(type), body);
					} else {
						client.replication_client(static_cast<ReplicationReplyType>(type), body);
					}
					co_return;
				}, use_awaitable);
		} else {
			if (is_server) {
				client.replication_server(static_cast<ReplicationMessageType>(type), body);
			} else {
				client.replication_client(static_cast<ReplicationReplyType>(type), body);
			}
		}
		if (client.closing()) { break; }
	}
}

// Inbound (server): an accepted connection. Greet REPLY_WELCOME, then answer requests.
inline asio::awaitable<void> serve_replication_connection(asio::ip::tcp::socket socket, reactor::Reactor* reactor) {
	try {
		asio::error_code opt_ec;
		socket.set_option(asio::ip::tcp::no_delay(true), opt_ec);

		ReplicationProtocolClient client;
		client.set_socket_fd(socket.native_handle());
		auto ab = std::make_shared<SocketAbortable>(socket.native_handle());
		reactor->track(ab);

		client.greeting();   // REPLY_WELCOME (a small blocking write on a fresh connection)
		if (!client.closing()) {
			co_await run_loop(socket, reactor, client, /*is_server=*/true);
		}
	} catch (const std::exception&) {
		// hangup / read-write error -- drop the connection.
	}
	asio::error_code ec;
	socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

}  // namespace detail

// Defined in the .cc (needs manager.h/libev): the fatal handler for when the cluster's own
// bootstrap database cannot be replicated -- preserves the classic sig_exit behavior without
// pulling libev into this Asio header.
void cluster_database_replication_failed();

namespace detail {
inline asio::awaitable<void> connect_and_replicate(reactor::Reactor* reactor, std::string host, int port,
                                                   Endpoint src_endpoint, Endpoint dst_endpoint, bool cluster_database) {
	using asio::use_awaitable;
	try {
		auto client = std::make_unique<ReplicationProtocolClient>(cluster_database);

		bool ok = false;
		co_await asio::co_spawn(reactor->pool() != nullptr ? reactor->pool()->get_executor() : co_await asio::this_coro::executor,
			[&]() -> asio::awaitable<void> {
				ok = client->init_replication_protocol(host, port, src_endpoint, dst_endpoint);
				co_return;
			}, use_awaitable);
		if (!ok) {
			if (cluster_database) { cluster_database_replication_failed(); }
			co_return;   // deferred / failed; a retry was scheduled inside.
		}

		auto ex = co_await asio::this_coro::executor;
		asio::ip::tcp::socket socket(ex, asio::ip::tcp::v4(), client->socket_fd());
		auto ab = std::make_shared<SocketAbortable>(socket.native_handle());
		reactor->track(ab);

		// No greeting: the client receives REPLY_WELCOME first (reply_welcome sends the request).
		co_await run_loop(socket, reactor, *client, /*is_server=*/false);

		asio::error_code ec;
		socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
	} catch (const std::exception&) {
		// connect / read-write error -- give up this transfer (a retry may be scheduled).
	}
}

}  // namespace detail


// The replication service over a reactor::TcpServer (the inbound/server side) plus an outbound
// spawner (trigger_replication). A thin adapter, like http::HttpAsioService, except it also
// initiates connections.
class ReplicationProtocolService {
public:
	ReplicationProtocolService(std::size_t reactors, std::size_t workers, std::size_t queue_limit)
		: server_(reactors, workers, static_cast<int>(queue_limit),
		          [](asio::ip::tcp::socket socket, reactor::Reactor& r) -> asio::awaitable<void> {
			          return detail::serve_replication_connection(std::move(socket), &r);
		          }) {}

	ReplicationProtocolService(const ReplicationProtocolService&) = delete;
	ReplicationProtocolService& operator=(const ReplicationProtocolService&) = delete;

	void set_bind_options(const reactor::BindOptions& options) { server_.set_bind_options(options); }
	void start(unsigned short port) { server_.start(port); }
	void stop() { server_.stop(); }
	std::size_t reactors() const { return server_.reactors(); }

	// The outbound trigger: validate the request + (if this node should replicate) spawn an
	// outbound connect_and_replicate coroutine on one of the reactors. Defined in the .cc
	// (heavy Xapiand deps: node resolution, shard locks, stalled-DB removal).
	void trigger_replication(const TriggerReplicationArgs& args);

private:
	reactor::TcpServer server_;
	std::size_t next_reactor_ = 0;
};

}  // namespace replication

#endif  // XAPIAND_CLUSTERING
