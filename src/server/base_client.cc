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

#include <algorithm>             // for move
#include <chrono>                // for operator""ms
#include <exception>             // for exception
#include <memory>                // for shared_ptr, unique_ptr, default_delete
#include <cstdio>                // for SEEK_SET
#include <errno.h>               // for __error, errno, ECONNRESET
#include <sys/socket.h>          // for shutdown, SHUT_RDWR
#include <sysexits.h>            // for EX_SOFTWARE
#include <type_traits>           // for remove_reference<>::type
#include <xapian.h>              // for SerialisationError

#include "cassert.hh"            // for assert

#include "ev/ev++.h"             // for ::EV_ERROR, ::EV_READ, ::EV_WRITE
#include "ignore_unused.h"       // for ignore_unused
#include "io.hh"                 // for read, close, lseek, write, ignored_errno
#include "length.h"              // for serialise_length, unserialise_length
#include "likely.h"              // for likely, unlikely
#include "log.h"                 // for L_CALL, L_ERR, L_EV, L_CONN, L_OBJ
#include "lz4/xxhash.h"          // for XXH32_createState, XXH32_digest, XXH...
#include "lz4_compressor.h"      // for LZ4BlockStreaming<>::iterator, LZ4Co...
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
// #undef L_EV_BEGIN
// #define L_EV_BEGIN L_DELAYED_200
// #undef L_EV_END
// #define L_EV_END L_DELAYED_N_UNLOG


#define BUF_SIZE 4096

#define NO_COMPRESSOR "\01"
#define LZ4_COMPRESSOR "\02"
#define TYPE_COMPRESSOR LZ4_COMPRESSOR

#define CMP_SEED 0xCEED


constexpr int WRITE_QUEUE_LIMIT = 10;
constexpr int WRITE_QUEUE_THRESHOLD = WRITE_QUEUE_LIMIT * 2 / 3;


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


class ClientLZ4Compressor : public LZ4CompressFile {
	BaseClient& client;

public:
	ClientLZ4Compressor(BaseClient& client_, int fd, size_t offset=0)
		: LZ4CompressFile(fd, offset, -1, CMP_SEED),
		  client(client_) { }

	ssize_t compress();
};


ssize_t
ClientLZ4Compressor::compress()
{
	L_CALL("ClientLZ4Compressor::compress()");

	if (!client.write(LZ4_COMPRESSOR)) {
		L_ERR("Write Header failed!");
		return -1;
	}

	try {
		auto it = begin();
		while (it) {
			std::string length(serialise_length(it.size()));
			if (!client.write(length) || !client.write(it->data(), it.size())) {
				L_ERR("Write failed!");
				return -1;
			}
			++it;
		}
	} catch (const std::exception& e) {
		L_ERR("%s", e.what());
		return -1;
	}

	if (!client.write(serialise_length(0)) || !client.write(serialise_length(get_digest()))) {
		L_ERR("Write Footer failed!");
		return -1;
	}

	return size();
}


class ClientNoCompressor {
	BaseClient& client;
	int fd;
	size_t offset;
	XXH32_state_t* xxh_state;

public:
	ClientNoCompressor(BaseClient& client_, int fd_, size_t offset_=0)
		: client(client_),
		  fd(fd_),
		  offset(offset_),
		  xxh_state(XXH32_createState()) { }

	~ClientNoCompressor() {
		XXH32_freeState(xxh_state);
	}

	ssize_t compress();
};


ssize_t
ClientNoCompressor::compress()
{
	L_CALL("ClientNoCompressor::compress()");

	if (!client.write(NO_COMPRESSOR)) {
		L_ERR("Write Header failed!");
		return -1;
	}

	if unlikely(io::lseek(fd, offset, SEEK_SET) != static_cast<off_t>(offset)) {
		L_ERR("IO error: lseek");
		return -1;
	}

	char buffer[LZ4_BLOCK_SIZE];
	XXH32_reset(xxh_state, CMP_SEED);

	size_t size = 0;
	ssize_t r;
	while ((r = io::read(fd, buffer, sizeof(buffer))) > 0) {
		std::string length(serialise_length(r));
		if (!client.write(length) || !client.write(buffer, r)) {
			L_ERR("Write failed!");
			return -1;
		}
		size += r;
		XXH32_update(xxh_state, buffer, r);
	}

	if (r < 0) {
		L_ERR("IO error: read");
		return -1;
	}

	if (!client.write(serialise_length(0)) || !client.write(serialise_length(XXH32_digest(xxh_state)))) {
		L_ERR("Write Footer failed!");
		return -1;
	}

	return size;
}


class ClientDecompressor {
protected:
	BaseClient& client;
	std::string input;

public:
	ClientDecompressor(BaseClient& client_)
		: client(client_) { }

	virtual ~ClientDecompressor() = default;

	inline void clear() noexcept {
		L_CALL("ClientDecompressor::clear()");

		input.clear();
	}

	inline void append(const char *buf, size_t size) {
		L_CALL("ClientDecompressor::append(%s)", repr(buf, size));

		input.append(buf, size);
	}

	virtual ssize_t decompress() = 0;
	virtual bool verify(uint32_t checksum_) noexcept = 0;
};


class ClientLZ4Decompressor : public ClientDecompressor, public LZ4DecompressData {
public:
	ClientLZ4Decompressor(BaseClient& client_)
		: ClientDecompressor(client_),
		  LZ4DecompressData(nullptr, 0, CMP_SEED) { }

	ssize_t decompress() override;

	bool verify(uint32_t checksum_) noexcept override {
		L_CALL("ClientLZ4Decompressor::verify(0x%04x)", checksum_);

		return get_digest() == checksum_;
	}
};


ssize_t
ClientLZ4Decompressor::decompress()
{
	L_CALL("ClientLZ4Decompressor::decompress()");

	add_data(input.data(), input.size());
	auto it = begin();
	while (it) {
		client.on_read_file(it->data(), it.size());
		++it;
	}
	return size();
}


class ClientNoDecompressor : public ClientDecompressor {
	XXH32_state_t* xxh_state;

public:
	ClientNoDecompressor(BaseClient& client_)
		: ClientDecompressor(client_),
		  xxh_state(XXH32_createState()) {
		XXH32_reset(xxh_state, CMP_SEED);
	}

	~ClientNoDecompressor() override {
		XXH32_freeState(xxh_state);
	}

	ssize_t decompress() override;

	bool verify(uint32_t checksum_) noexcept override {
		L_CALL("ClientNoDecompressor::verify(0x%04x)", checksum_);

		return XXH32_digest(xxh_state) == checksum_;
	}
};


ssize_t
ClientNoDecompressor::decompress()
{
	L_CALL("ClientNoDecompressor::decompress()");

	size_t size = input.size();
	const char* data = input.data();
	client.on_read_file(data, size);
	XXH32_update(xxh_state, data, size);

	return size;
}


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
	io_read.set<BaseClient, &BaseClient::io_cb_read>(this);
	io_read.set(sock, ev::READ);
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
}


void
BaseClient::close()
{
	L_CALL("BaseClient::close()");

	if (!closed.exchange(true)) {
		io::shutdown(sock, SHUT_RDWR);
	}
}


bool
BaseClient::is_idle()
{
	return !waiting && !running && write_queue.empty();
}


void
BaseClient::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("BaseClient::shutdown_impl(%d, %d)", (int)asap, (int)now);

	shutting_down = true;

	Worker::shutdown_impl(asap, now);

	if (now != 0 || is_idle()) {
		stop(false);
		destroy(false);
		detach();
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

	// Stop and free watcher if client socket is closing
	io_read.stop();
	L_EV("Stop client's read event");

	io_write.stop();
	L_EV("Stop client's write event");

	read_start_async.stop();
	L_EV("Stop client's async read start event");

	write_start_async.stop();
	L_EV("Stop client's async update event");

	write_queue.finish();
	write_queue.clear();
}


inline WR
BaseClient::write_from_queue()
{
	L_CALL("BaseClient::write_from_queue()");

	if (closed) {
		L_ERR("ERROR: write error {sock:%d}: Socket already closed!", sock);
		L_CONN("WR:ERR.1: {sock:%d}", sock);
		close();
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

	assert(sock == watcher.fd);
	ignore_unused(watcher);

	if (closed) {
		return;
	}

	L_DEBUG_HOOK("BaseClient::io_cb_write", "BaseClient::io_cb_write(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), sock);

	L_EV_BEGIN("BaseClient::io_cb_write:BEGIN");

	if ((revents & EV_ERROR) != 0) {
		L_ERR("ERROR: got invalid event {sock:%d} - %d: %s", sock, errno, strerror(errno));
		stop();
		destroy();
		detach();
		L_EV_END("BaseClient::io_cb_write:END");
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
				}
			});
			break;
	}


	if (closed) {
		stop();
		destroy();
		detach();
	}

	L_EV_END("BaseClient::io_cb_write:END");
}


void
BaseClient::io_cb_read(ev::io &watcher, int revents)
{
	L_CALL("BaseClient::io_cb_read(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), sock);

	assert(sock == watcher.fd);
	ignore_unused(watcher);

	if (closed) {
		return;
	}

	L_DEBUG_HOOK("BaseClient::io_cb_read", "BaseClient::io_cb_read(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), sock);

	L_EV_BEGIN("BaseClient::io_cb_read:BEGIN");

	if ((revents & EV_ERROR) != 0) {
		L_ERR("ERROR: got invalid event {sock:%d} - %d: %s", sock, errno, strerror(errno));
		stop();
		destroy();
		detach();
		L_EV_END("BaseClient::io_cb_read:END");
		return;
	}

	char read_buffer[BUF_SIZE];
	ssize_t received = io::read(sock, read_buffer, BUF_SIZE);

	if (received < 0) {
		if (io::ignored_errno(errno, true, true, false)) {
			L_CONN("Ignored error: {sock:%d} - %d: %s", sock, errno, strerror(errno));
			L_EV_END("BaseClient::io_cb_read:END");
			return;
		}

		if (errno == ECONNRESET) {
			L_CONN("Received ECONNRESET {sock:%d}!", sock);
			on_read(nullptr, received);
			stop();
			destroy();
			detach();
			L_EV_END("BaseClient::io_cb_read:END");
			return;
		}

		L_ERR("ERROR: read error {sock:%d} - %d: %s", sock, errno, strerror(errno));
		on_read(nullptr, received);
		stop();
		destroy();
		detach();
		L_EV_END("BaseClient::io_cb_read:END");
		return;
	}

	if (received == 0) {
		// The peer has closed its half side of the connection.
		L_CONN("Received EOF {sock:%d}!", sock);
		on_read(nullptr, received);
		stop();
		destroy();
		detach();
		L_EV_END("BaseClient::io_cb_read:END");
		return;
	}

	const char* buf_data = read_buffer;
	const char* buf_end = read_buffer + received;

	total_received_bytes += received;
	L_TCP_WIRE("{sock:%d} -->> %s (%zu bytes)", sock, repr(buf_data, received, true, true, 500), received);

	do {
		if ((received > 0) && mode == MODE::READ_BUF) {
			buf_data += on_read(buf_data, received);
			received = buf_end - buf_data;
		}

		if ((received > 0) && mode == MODE::READ_FILE_TYPE) {
			auto compressor = *buf_data++;
			switch (compressor) {
				case *NO_COMPRESSOR:
					L_CONN("Receiving uncompressed file {sock:%d}...", sock);
					decompressor = std::make_unique<ClientNoDecompressor>(*this);
					break;
				case *LZ4_COMPRESSOR:
					L_CONN("Receiving LZ4 compressed file {sock:%d}...", sock);
					decompressor = std::make_unique<ClientLZ4Decompressor>(*this);
					break;
				default:
					L_CONN("Received wrong file mode: %s {sock:%d}!", repr(std::string(1, compressor)), sock);
					stop();
					destroy();
					detach();
					L_EV_END("BaseClient::io_cb_read:END");
					return;
			}
			--received;
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
						L_EV_END("BaseClient::io_cb_read:END");
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
			stop();
			destroy();
			detach();
			L_EV_END("BaseClient::io_cb_read:END");
			return;
		}
	} while (received > 0);

	L_EV_END("BaseClient::io_cb_read:END");
}


void
BaseClient::write_start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("BaseClient::write_start_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	L_EV_BEGIN("BaseClient::write_start_async_cb:BEGIN");

	if (!closed) {
		io_write.start();
		L_EV("Enable write event [%d]", io_write.is_active());
	}

	L_EV_END("BaseClient::write_start_async_cb:END");
}


void
BaseClient::read_start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("BaseClient::read_start_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	L_EV_BEGIN("BaseClient::read_start_async_cb:BEGIN");

	if (!closed) {
		io_read.start();
		L_EV("Enable read event [%d]", io_read.is_active());
	}

	L_EV_END("BaseClient::read_start_async_cb:END");
}


void
BaseClient::read_file()
{
	L_CALL("BaseClient::read_file()");

	mode = MODE::READ_FILE_TYPE;
	file_size = -1;
	receive_checksum = false;
}


bool
BaseClient::send_file(int fd, size_t offset)
{
	L_CALL("BaseClient::send_file(%d, %zu)", fd, offset);

	ssize_t compressed = -1;
	switch (*TYPE_COMPRESSOR) {
		case *NO_COMPRESSOR: {
			ClientNoCompressor compressor(*this, fd, offset);
			compressed = compressor.compress();
			break;
		}
		case *LZ4_COMPRESSOR: {
			ClientLZ4Compressor compressor(*this, fd, offset);
			compressed = compressor.compress();
			break;
		}
	}

	return compressed != -1;
}
