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

// The Asio transport for Xapiand's Xapian remote-backend protocol -- the counterpart to
// http_asio.h, on the same Kronuz/reactor runtime. Each accepted connection is one coroutine
// (a reactor::Session): the server greets, then for each framed request it runs the blocking
// Xapian dispatch on the reactor's offload pool (the coroutine suspends, the reactor stays
// free) and writes the framed replies. This replaces the libev BaseClient FSM + the
// ThreadPool<RemoteProtocolClient> runner: the protocol logic (RemoteProtocolClient's msg_*
// handlers) is unchanged; only the I/O core moved from ev::io + a write_queue to a coroutine
// + a reply buffer.

#pragma once

#include "config.h"   // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

// remote_protocol_client.h pulls NO libev (verified), so there is no EV_ERROR clash with the
// Asio headers below; include it first anyway to keep the app types ahead of the runtime.
#include "remote_protocol_client.h"           // RemoteProtocolClient, RemoteMessageType, FILE_FOLLOWS
#include "xapian/common/pack.h"               // for unpack_uint (message length framing)

#include <asio.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <unistd.h>                           // ::write, ::close

#include "flume.h"                            // flume::Receiver (FILE_FOLLOWS file receive)
#include "reactor.h"                          // reactor::TcpServer / Reactor / BindOptions

namespace remote {

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

// Write every byte of `data` to the socket.
inline asio::awaitable<void> write_all(asio::ip::tcp::socket& socket, std::string data) {
	if (!data.empty()) {
		co_await asio::async_write(socket, asio::buffer(data), asio::use_awaitable);
	}
	co_return;
}

// Read exactly one framed message off the socket into (type_out, body_out). Returns false on
// a clean EOF / read error. A normal message is `[type][pack_uint len][body]`. FILE_FOLLOWS
// (type 0xfd) is `[0xfd][real_type]` followed by a flume-framed file stream, which is streamed
// into a temp file; type_out is the real type and body_out is the temp-file path (matching the
// classic on_read behavior). `buffered` carries bytes read past one message to the next call.
inline asio::awaitable<bool> read_message(asio::ip::tcp::socket& socket, std::string& buffered,
                                          RemoteProtocolClient& client, char& type_out, std::string& body_out) {
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

			// Stream the compressed file into a temp file via flume.
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
			type_out = real_type;
			body_out = path;
			co_return true;
		}

		// Normal message: [type][pack_uint len][body]. Parse the varint length; if the header
		// or the body is not fully present yet, read more and retry.
		const char* p = buffered.data() + 1;
		const char* p_end = buffered.data() + buffered.size();
		std::size_t len = 0;
		if (!unpack_uint(&p, p_end, &len)) {
			std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
			if (n == 0) { co_return false; }
			buffered.append(tmp, n);
			continue;
		}
		std::size_t header = static_cast<std::size_t>(p - buffered.data());
		if (buffered.size() - header < len) {
			std::size_t n = co_await socket.async_read_some(asio::buffer(tmp), use_awaitable);
			if (n == 0) { co_return false; }
			buffered.append(tmp, n);
			continue;
		}
		type_out = type;
		body_out = buffered.substr(header, len);
		buffered.erase(0, header + len);
		co_return true;
	}
}

// The per-connection coroutine. Greet, then loop: read a request, dispatch it (offloaded to
// the reactor pool when there is one), write the replies, until EOF or a handler asked to
// close (detach()/destroy() -> client.closing()).
inline asio::awaitable<void> serve_remote_connection(asio::ip::tcp::socket socket, reactor::Reactor* reactor) {
	using asio::use_awaitable;
	try {
		asio::error_code opt_ec;
		socket.set_option(asio::ip::tcp::no_delay(true), opt_ec);

		RemoteProtocolClient client;

		// The server greets first (REPLY_UPDATE), like the classic INIT_REMOTE -> msg_update("").
		client.greeting();
		co_await write_all(socket, client.take_reply());

		std::string buffered;
		for (;;) {
			char type = 0;
			std::string body;
			if (!co_await read_message(socket, buffered, client, type, body)) { break; }

			auto mtype = static_cast<RemoteMessageType>(type);
			if (reactor->pool() != nullptr) {
				// The un-stallable path: run the blocking Xapian dispatch on the pool while the
				// reactor stays free; resume here. remote_server catches its own exceptions.
				co_await asio::co_spawn(reactor->pool()->get_executor(),
					[&client, mtype, &body]() -> asio::awaitable<void> {
						client.remote_server(mtype, body);
						co_return;
					}, use_awaitable);
			} else {
				client.remote_server(mtype, body);
			}

			if (client.has_reply()) {
				co_await write_all(socket, client.take_reply());
			}
			if (client.closing()) { break; }
		}
	} catch (const std::exception&) {
		// client hangup / read-write error -- drop the connection.
	}
	asio::error_code ec;
	socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

}  // namespace detail


// The Xapian remote-backend service, served over a reactor::TcpServer: N shared-nothing
// reactors on N threads (one port via SO_REUSEPORT or a portable shared acceptor), each with
// a bounded offload pool for the blocking Xapian-bound dispatch. A thin adapter, like
// http::HttpAsioService.
class RemoteProtocolAsioService {
public:
	RemoteProtocolAsioService(std::size_t reactors, std::size_t workers, std::size_t queue_limit)
		: server_(reactors, workers, static_cast<int>(queue_limit),
		          [](asio::ip::tcp::socket socket, reactor::Reactor& r) -> asio::awaitable<void> {
			          return detail::serve_remote_connection(std::move(socket), &r);
		          }) {}

	RemoteProtocolAsioService(const RemoteProtocolAsioService&) = delete;
	RemoteProtocolAsioService& operator=(const RemoteProtocolAsioService&) = delete;

	void set_bind_options(const reactor::BindOptions& options) { server_.set_bind_options(options); }

	// Bind + run all reactors (each on its own thread). Returns once the threads are launched.
	void start(unsigned short port) { server_.start(port); }

	// Abort in-flight work, stop the loops, join. Also runs from the destructor.
	void stop() { server_.stop(); }

	std::size_t reactors() const { return server_.reactors(); }

private:
	reactor::TcpServer server_;
};

}  // namespace remote

#endif  // XAPIAND_CLUSTERING
