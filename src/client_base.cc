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

#include "client_base.h"

#include "utils.h"
#include "length.h"

#include <sys/socket.h>
#include <sysexits.h>
#include <unistd.h>
#include <memory>

#define BUF_SIZE 4096

#define NO_COMPRESSOR "\01"
#define LZ4_COMPRESSOR "\02"
#define TYPE_COMPRESSOR LZ4_COMPRESSOR

#define CMP_SEED 0xCEED


constexpr int WRITE_QUEUE_SIZE = 10;

enum class WR {
	OK,
	ERR,
	RETRY,
	PENDING,
	CLOSED
};

enum class MODE {
	READ_BUF,
	READ_FILE_TYPE,
	READ_FILE
};


class ClientLZ4Compressor : public LZ4CompressFile {
	BaseClient *client;

public:
	ClientLZ4Compressor(BaseClient *client_, int fd, size_t offset=0)
		: LZ4CompressFile(fd, offset, -1, CMP_SEED),
		  client(client_) { }

	ssize_t compress();
};


ssize_t
ClientLZ4Compressor::compress()
{
	if (!client->write(LZ4_COMPRESSOR)) {
		L_ERR(this, "Write Header failed!");
		return -1;
	}

	try {
		auto it = begin();
		while (it) {
			std::string length(serialise_length(it.size()));
			if (!client->write(length) || !client->write(it->data(), it.size())) {
				L_ERR(this, "Write failed!");
				return -1;
			}
			++it;
		}
	} catch (const std::exception& e) {
		L_ERR(this, "%s", e.what());
		return -1;
	}

	if (!client->write(serialise_length(0)) || !client->write(serialise_length(get_digest()))) {
		L_ERR(this, "Write Footer failed!");
		return -1;
	}

	return size();
}


class ClientNoCompressor {
	BaseClient *client;
	int fd;
	size_t offset;
	XXH32_state_t* xxh_state;

public:
	ClientNoCompressor(BaseClient *client_, int fd_, size_t offset_=0)
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
	if (!client->write(NO_COMPRESSOR)) {
		L_ERR(this, "Write Header failed!");
		return -1;
	}

	if unlikely(io::lseek(fd, offset, SEEK_SET) != static_cast<off_t>(offset)) {
		L_ERR(this, "IO error: lseek");
		return -1;
	}

	char buffer[LZ4_BLOCK_SIZE];
	XXH32_reset(xxh_state, CMP_SEED);

	size_t size = 0;
	ssize_t r;
	while ((r = io::read(fd, buffer, sizeof(buffer))) > 0) {
		std::string length(serialise_length(r));
		if (!client->write(length) || !client->write(buffer, r)) {
			L_ERR(this, "Write failed!");
			return -1;
		}
		size += r;
		XXH32_update(xxh_state, buffer, r);
	}

	if (r < 0) {
		L_ERR(this, "IO error: read");
		return -1;
	}

	if (!client->write(serialise_length(0)) || !client->write(serialise_length(XXH32_digest(xxh_state)))) {
		L_ERR(this, "Write Footer failed!");
		return -1;
	}

	return size;
}


class ClientDecompressor {
protected:
	BaseClient *client;
	std::string input;

public:
	ClientDecompressor(BaseClient *client_)
		: client(client_) { }

	inline void clear() noexcept {
		input.clear();
	}

	inline void append(const char *buf, size_t size) {
		input.append(buf, size);
	}

	virtual ssize_t decompress() = 0;
	virtual bool verify(uint32_t checksum_) noexcept = 0;
};


class ClientLZ4Decompressor : public ClientDecompressor, public LZ4DecompressData {
public:
	ClientLZ4Decompressor(BaseClient *client_)
		: ClientDecompressor(client_),
		  LZ4DecompressData(nullptr, 0, CMP_SEED) { }

	ssize_t decompress() override;

	bool verify(uint32_t checksum_) noexcept override {
		return get_digest() == checksum_;
	}
};


ssize_t
ClientLZ4Decompressor::decompress()
{
	add_data(input.data(), input.size());
	auto it = begin();
	while (it) {
		client->on_read_file(it->data(), it.size());
		++it;
	}
	return size();
}


class ClientNoDecompressor : public ClientDecompressor {
	XXH32_state_t* xxh_state;

public:
	ClientNoDecompressor(BaseClient *client_)
		: ClientDecompressor(client_),
		  xxh_state(XXH32_createState()) {
		XXH32_reset(xxh_state, CMP_SEED);
	}

	~ClientNoDecompressor() {
		XXH32_freeState(xxh_state);
	}

	ssize_t decompress() override;

	bool verify(uint32_t checksum_) noexcept override {
		return XXH32_digest(xxh_state) == checksum_;
	}
};


ssize_t
ClientNoDecompressor::decompress()
{
	size_t size = input.size();
	const char* data = input.data();
	client->on_read_file(data, size);
	XXH32_update(xxh_state, data, size);

	return size;
}


BaseClient::BaseClient(const std::shared_ptr<BaseServer>& server_, ev::loop_ref *loop_, int sock_)
	: Worker(std::move(server_), loop_),
	  io_read(*loop),
	  io_write(*loop),
	  async_write(*loop),
	  async_read(*loop),
	  closed(false),
	  sock(sock_),
	  written(0),
	  read_buffer(new char[BUF_SIZE]),
	  mode(MODE::READ_BUF),
	  write_queue(WRITE_QUEUE_SIZE)
{
	async_write.set<BaseClient, &BaseClient::async_write_cb>(this);
	async_write.start();
	L_EV(this, "Start async write event");

	async_read.set<BaseClient, &BaseClient::async_read_cb>(this);
	async_read.start();
	L_EV(this, "Start async read event");

	io_read.set<BaseClient, &BaseClient::io_cb>(this);
	io_read.start(sock, ev::READ);
	L_EV(this, "Start read event (sock=%d)", sock);

	io_write.set<BaseClient, &BaseClient::io_cb>(this);
	io_write.set(sock, ev::WRITE);
	L_EV(this, "Setup write event (sock=%d)", sock);

	int total_clients = ++XapiandServer::total_clients;
	L_OBJ(this, "CREATED BASE CLIENT! (%d clients)", total_clients);
}


BaseClient::~BaseClient()
{
	destroy_impl();

	delete []read_buffer;

	int total_clients = --XapiandServer::total_clients;
	if (total_clients < 0) {
		L_CRIT(this, "Inconsistency in number of clients, end up with negative number");
		exit(EX_SOFTWARE);
	}

	L_OBJ(this, "DELETED BASE CLIENT! (%d clients left)", total_clients);
}


void
BaseClient::destroy_impl()
{
	L_OBJ(this, "DESTROYING BASE CLIENT!");
	close();

	std::unique_lock<std::mutex> lk(qmtx);
	if (sock == -1) {
		return;
	}

	// Stop and free watcher if client socket is closing
	io_read.stop();
	L_EV(this, "Stop read event (sock=%d)", sock);

	io_write.stop();
	L_EV(this, "Stop write event (sock=%d)", sock);

	io::close(sock);
	sock = -1;
	lk.unlock();

	write_queue.finish();
	write_queue.clear();

	L_OBJ(this, "DESTROYED BASE CLIENT!");
}


void
BaseClient::close()
{
	if (closed) {
		return;
	}

	::shutdown(sock, SHUT_RDWR);
	closed = true;

	L_OBJ(this, "CLOSED BASE CLIENT!");
}


void
BaseClient::io_cb_update()
{
	if (sock != -1) {
		if (write_queue.empty()) {
			if (closed) {
				destroy();
			} else {
				io_write.stop();
				L_EV(this, "Disable write event (sock=%d)", sock);
			}
		} else {
			io_write.start();
			L_EV(this, "Enable write event (sock=%d)", sock);
		}
	}
}


void
BaseClient::io_cb(ev::io &watcher, int revents)
{
	int fd = watcher.fd;

	L_EV_BEGIN(this, "BaseClient::io_cb:BEGIN");
	L_EV(this, "%s (sock=%d, fd=%d) %x", (revents & EV_ERROR) ? "EV_ERROR" : (revents & EV_WRITE & EV_READ) ? "IO_CB" : (revents & EV_WRITE) ? "WRITE_CB" : (revents & EV_READ) ? "READ_CB" : "IO_CB", sock, fd, revents);

	if (revents & EV_ERROR) {
		L_ERR(this, "ERROR: got invalid event (sock=%d, fd=%d): %s", sock, fd, strerror(errno));
		destroy();
	}

	assert(sock == fd || sock == -1);

	if (revents & EV_WRITE) {
		io_cb_write(fd);
	}

	if (revents & EV_READ) {
		io_cb_read(fd);
	}

	io_cb_update();

	L_EV_END(this, "BaseClient::io_cb:END");
}


WR
BaseClient::write_directly(int fd)
{
	if (fd == -1) {
		L_ERR(this, "ERROR: write error (sock=%d, fd=%d): Socket already closed!", sock, fd);
		L_CONN(this, "WR:ERR.1: (sock=%d, fd=%d)", sock, fd);
		return WR::ERR;
	} else if (!write_queue.empty()) {
		std::shared_ptr<Buffer> buffer = write_queue.front();

		size_t buf_size = buffer->nbytes();
		const char *buf_data = buffer->dpos();

#ifdef MSG_NOSIGNAL
		ssize_t written = ::send(fd, buf_data, buf_size, MSG_NOSIGNAL);
#else
		ssize_t written = io::write(fd, buf_data, buf_size);
#endif

		if (written < 0) {
			if (ignored_errorno(errno, false)) {
				L_CONN(this, "WR:RETRY: (sock=%d, fd=%d)", sock, fd);
				return WR::RETRY;
			} else {
				L_ERR(this, "ERROR: write error (sock=%d, fd=%d): %s", sock, fd, strerror(errno));
				L_CONN(this, "WR:ERR.2: (sock=%d, fd=%d)", sock, fd);
				return WR::ERR;
			}
		} else if (written == 0) {
			L_CONN(this, "WR:CLOSED: (sock=%d, fd=%d)", sock, fd);
			return WR::CLOSED;
		} else {
			auto str(repr(buf_data, written, true, 500));
			L_CONN_WIRE(this, "(sock=%d, fd=%d) <<-- '%s' [%zu] (%zu bytes)", sock, fd, str.c_str(), str.size(), written);
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				if (write_queue.pop(buffer)) {
					if (write_queue.empty()) {
						L_CONN(this, "WR:OK.1: (sock=%d, fd=%d)", sock, fd);
						return WR::OK;
					} else {
						L_CONN(this, "WR:PENDING.1: (sock=%d, fd=%d)", sock, fd);
						return WR::PENDING;
					}
				}
			} else {
				L_CONN(this, "WR:PENDING.2: (sock=%d, fd=%d)", sock, fd);
				return WR::PENDING;
			}
		}
	}
	L_CONN(this, "WR:OK.2: (sock=%d, fd=%d)", sock, fd);
	return WR::OK;
}


bool
BaseClient::_write(int fd, bool async)
{
	WR status;

	do {
		std::unique_lock<std::mutex> lk(qmtx);
		status = write_directly(fd);
		lk.unlock();

		switch (status) {
			case WR::ERR:
			case WR::CLOSED:
				if (!async) {
					io_write.stop();
					L_EV(this, "Disable write event (sock=%d, fd=%d)", sock, fd);
				}
				destroy();
				return false;
			case WR::RETRY:
				if (!async) {
					io_write.start();
					L_EV(this, "Enable write event (sock=%d, fd=%d)", sock, fd);
				} else {
					async_write.send();
				}
				return true;
			default:
				break;
		}
	} while (status != WR::OK);

	if (!async) {
		io_write.stop();
		L_EV(this, "Disable write event (sock=%d, fd=%d)", sock, fd);
	}

	return true;
}


bool
BaseClient::write(const char *buf, size_t buf_size)
{
	if (!write_queue.push(std::make_shared<Buffer>('\0', buf, buf_size))) {
		return false;
	}

	//L_CONN_WIRE(this, "(sock=%d) <ENQUEUE> '%s'", sock, repr(buf, buf_size).c_str());

	written += 1;

	return _write(sock, true);
}


void
BaseClient::io_cb_write(int fd)
{
	_write(fd, false);
}


void
BaseClient::io_cb_read(int fd)
{
	if (!closed) {
		ssize_t received = io::read(fd, read_buffer, BUF_SIZE);
		const char *buf_end = read_buffer + received;
		const char *buf_data = read_buffer;

		if (received < 0) {
			if (!ignored_errorno(errno, false)) {
				if (errno == ECONNRESET) {
					L_WARNING(this, "WARNING: read error (sock=%d, fd=%d): %s", sock, fd, strerror(errno));
				} else {
					L_ERR(this, "ERROR: read error (sock=%d, fd=%d): %s", sock, fd, strerror(errno));
				}
				destroy();
				return;
			}
		} else if (received == 0) {
			// The peer has closed its half side of the connection.
			L_CONN(this, "Received EOF (sock=%d, fd=%d)!", sock, fd);
			destroy();
			return;
		} else {
			auto str(repr(buf_data, received, true, 500));
			L_CONN_WIRE(this, "(sock=%d, fd=%d) -->> '%s' [%zu] (%zu bytes)", sock, fd, str.c_str(), str.size(), received);

			if (mode == MODE::READ_FILE_TYPE) {
				switch (*buf_data++) {
					case *NO_COMPRESSOR:
						L_CONN(this, "Receiving uncompressed file (sock=%d, fd=%d)...", sock, fd);
						decompressor = std::make_unique<ClientNoDecompressor>(this);
						break;
					case *LZ4_COMPRESSOR:
						L_CONN(this, "Receiving LZ4 compressed file (sock=%d, fd=%d)...", sock, fd);
						decompressor = std::make_unique<ClientLZ4Decompressor>(this);
						break;
					default:
						L_CONN(this, "Received wrong file mode (sock=%d, fd=%d)!", sock, fd);
						destroy();
						return;
				}
				--received;
				length_buffer.clear();
				mode = MODE::READ_FILE;
			}

			if (received && mode == MODE::READ_FILE) {
				do {
					if (file_size == -1) {
						if (buf_data) {
							length_buffer.append(buf_data, received);
						}
						buf_data = length_buffer.data();

						buf_end = buf_data + length_buffer.size();
						try {
							file_size = unserialise_length(&buf_data, buf_end, false);
						} catch (Xapian::SerialisationError) {
							return;
						}

						if (receive_checksum) {
							receive_checksum = false;
							if (decompressor->verify(static_cast<uint32_t>(file_size))) {
								on_read_file_done();
								mode = MODE::READ_BUF;
								decompressor.reset();
								break;
							} else {
								L_ERR(this, "Data is corrupt!");
								return;
							}
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
					} else if (block_size == 0) {
						decompressor->decompress();
						if (buf_data) {
							length_buffer = std::string(buf_data, received);
							buf_data = nullptr;
							received = 0;
						} else {
							length_buffer.clear();
						}
					}
					file_size = -1;
				} while (file_size == -1);
			}

			if (received && mode == MODE::READ_BUF) {
				on_read(buf_data, received);
			}
		}
	}
}


void
BaseClient::async_write_cb(ev::async &, int)
{
	L_CALL(this, "BaseClient::async_write_cb");
	L_EV_BEGIN(this, "BaseClient::async_write_cb:BEGIN");

	io_cb_update();

	L_EV_END(this, "BaseClient::async_write_cb:END");
}


void
BaseClient::async_read_cb(ev::async &, int)
{
	L_CALL(this, "BaseClient::async_read_cb");
	L_EV_BEGIN(this, "BaseClient::async_read_cb:BEGIN");

	if (!closed) {
		io_read.start();
		L_EV(this, "Enable read event (sock=%d) [%d]", sock, io_read.is_active());
	}

	L_EV_END(this, "BaseClient::async_read_cb:END");
}


void
BaseClient::shutdown_impl(time_t asap, time_t now)
{
	L_OBJ(this , "SHUTDOWN BASE CLIENT! (%d %d)", asap, now);

	Worker::shutdown_impl(asap, now);

	if (now) {
		destroy();
	}
}


void
BaseClient::read_file()
{
	mode = MODE::READ_FILE_TYPE;
	file_size = -1;
	receive_checksum = false;
}


bool
BaseClient::send_file(int fd, size_t offset)
{
	ssize_t compressed = -1;
	switch (*TYPE_COMPRESSOR) {
		case *NO_COMPRESSOR: {
			ClientNoCompressor compressor(this, fd, offset);
			compressed = compressor.compress();
			break;
		}
		case *LZ4_COMPRESSOR: {
			ClientLZ4Compressor compressor(this, fd, offset);
			compressed = compressor.compress();
			break;
		}
	}

	return compressed != -1;
}
