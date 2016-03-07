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


class ClientReader : public CompressorBufferReader {
protected:
	int fd;
	size_t file_size;
	BaseClient *client;
	std::string header;

public:
	ClientReader(BaseClient *client_, int fd_, size_t file_size_, const std::string &header_)
		: fd(fd_),
		  file_size(file_size_),
		  client(client_),
		  header(header_) { }
};


class ClientCompressorReader : public ClientReader {
protected:
	ssize_t begin();
	ssize_t read(char **buf, size_t size);
	ssize_t write(const char *buf, size_t size);
	ssize_t done();

public:
	ClientCompressorReader(BaseClient *client_, int fd_, size_t file_size_, const std::string &header_)
		: ClientReader(client_, fd_, file_size_, header_) { }
};


ssize_t
ClientCompressorReader::begin()
{
	if (!client->write(header)) {
		return -1;
	}
	return 1;
}


ssize_t
ClientCompressorReader::read(char **buf, size_t size)
{
	if (!*buf) {
		L_CRIT(this, "Segmentation fault in compressor reader");
		exit(EX_SOFTWARE);
	}
	return io::read(fd, *buf, size);
}


ssize_t
ClientCompressorReader::write(const char *buf, size_t size)
{
	std::string length(serialise_length(size));
	if (!client->write(length) || !client->write(buf, size)) {
		return -1;
	}
	return length.size() + size;
}


ssize_t
ClientCompressorReader::done()
{
	std::string length(serialise_length(0));
	if (!client->write(length)) {
		return -1;
	}
	return length.size();
}


class ClientDecompressorReader : public ClientReader {
protected:
	ssize_t write(const char *buf, size_t size);
public:
	ClientDecompressorReader(BaseClient *client_, int fd_, size_t file_size_, const std::string &header_)
		: ClientReader(client_, fd_, file_size_, header_) { }
};


ssize_t
ClientDecompressorReader::write(const char *buf, size_t size)
{
	client->on_read_file(buf, size);
	return size;
}


class ClientNoCompressor : public NoCompressor {
public:
	ClientNoCompressor(BaseClient *client_, int fd_=0, size_t file_size_=0) :
		NoCompressor(
			std::make_unique<ClientDecompressorReader>(client_, fd_, file_size_, NO_COMPRESSOR),
			std::make_unique<ClientCompressorReader>(client_, fd_, file_size_, NO_COMPRESSOR)
		) { }
};


class ClientLZ4Compressor : public LZ4Compressor {
public:
	ClientLZ4Compressor(BaseClient *client_, int fd_=0, size_t file_size_=0) :
		LZ4Compressor(
			std::make_unique<ClientDecompressorReader>(client_, fd_, file_size_, LZ4_COMPRESSOR),
			std::make_unique<ClientCompressorReader>(client_, fd_, file_size_, LZ4_COMPRESSOR)
		) { }
};


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
						compressor = std::make_unique<ClientNoCompressor>(this);
						break;
					case *LZ4_COMPRESSOR:
						L_CONN(this, "Receiving LZ4 compressed file (sock=%d, fd=%d)...", sock, fd);
						compressor = std::make_unique<ClientLZ4Compressor>(this);
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
						block_size = file_size;
						compressor->decompressor->clear();
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
						compressor->decompressor->append(file_buf_to_write, block_size_to_write);
						block_size -= block_size_to_write;
					}
					if (file_size == 0) {
						compressor->decompressor->clear();
						compressor->decompress();

						on_read_file_done();
						mode = MODE::READ_BUF;
						compressor.reset();
					} else if (block_size == 0) {
						compressor->decompress();

						if (buf_data) {
							length_buffer = std::string(buf_data, received);
							buf_data = nullptr;
							received = 0;
						} else {
							length_buffer.clear();
						}

						file_size = -1;
					}
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
}


bool
BaseClient::send_file(int fd)
{
	ssize_t file_size = io::lseek(fd, 0, SEEK_END);
	io::lseek(fd, 0, SEEK_SET);

	switch (*TYPE_COMPRESSOR) {
		case *NO_COMPRESSOR:
			compressor = std::make_unique<ClientNoCompressor>(this, fd, file_size);
			break;
		case *LZ4_COMPRESSOR:
			compressor = std::make_unique<ClientLZ4Compressor>(this, fd, file_size);
			break;
	}

	ssize_t compressed = compressor->compress();

	compressor.reset();

	return (compressed == file_size);
}
