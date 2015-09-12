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

#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_base.h"
#include "utils.h"

#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 12
#define LZ4F_BLOCK_SIZE_ID LZ4F_max256KB
#define LZ4F_BLOCK_SIZE (256*1024)

#define BUF_SIZE 4096

const int WRITE_QUEUE_SIZE = 10;

#define WR_OK 0
#define WR_ERR 1
#define WR_RETRY 2
#define WR_PENDING 3

#define MODE_READ_BUF 0
#define MODE_READ_FILE_TYPE 1
#define MODE_READ_FILE_RAW 2
#define MODE_READ_FILE_LZ4 3

BaseClient::BaseClient(XapiandServer *server_, ev::loop_ref *loop_, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_)
	: Worker(server_, loop_),
	  io_read(*loop),
	  io_write(*loop),
	  async_write(*loop),
	  closed(false),
	  sock(sock_),
	  written(0),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_),
	  write_queue(WRITE_QUEUE_SIZE),
	  mode(MODE_READ_BUF),
	  lz4_dCtx(NULL),
	  lz4_buffer(NULL),
	  read_buffer(new char[BUF_SIZE])

{
	inc_ref();

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = ++XapiandServer::total_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	async_write.set<BaseClient, &BaseClient::async_write_cb>(this);
	async_write.start();

	io_read.set<BaseClient, &BaseClient::io_cb>(this);
	io_read.start(sock, ev::READ);

	io_write.set<BaseClient, &BaseClient::io_cb>(this);
	io_write.set(sock, ev::WRITE);

	LOG_OBJ(this, "CREATED CLIENT! (%d clients)\n", total_clients);
}


BaseClient::~BaseClient()
{
	destroy();

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = --XapiandServer::total_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	LOG_OBJ(this, "DELETED CLIENT! (%d clients left)\n", total_clients);
	assert(total_clients >= 0);

	delete []read_buffer;
	delete []lz4_buffer;
	if (lz4_dCtx) {
		LZ4F_errorCode_t errorCode = LZ4F_freeDecompressionContext(lz4_dCtx);
		if (LZ4F_isError(errorCode)) {
			LOG_ERR(this, "Failed to free decompression context: error %zd\n", errorCode);
		}
	}
}


void BaseClient::destroy()
{
	close();

	pthread_mutex_lock(&qmtx);
	if (sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	// Stop and free watcher if client socket is closing
	io_read.stop();
	io_write.stop();

	::close(sock);
	sock = -1;
	pthread_mutex_unlock(&qmtx);

	write_queue.finish();
	while (!write_queue.empty()) {
		Buffer *buffer;
		if (write_queue.pop(buffer, 0)) {
			delete buffer;
		}
	}

	LOG_OBJ(this, "DESTROYED CLIENT!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	LOG_OBJ(this, "CLOSED CLIENT!\n");
}


void BaseClient::io_update() {
	if (sock != -1) {
		if (write_queue.empty()) {
			if (closed) {
				destroy();
			} else {
				io_write.stop();
			}
		} else {
			io_write.start();
		}
	}

	if (sock == -1) {
		rel_ref();
	}

}


void BaseClient::io_cb(ev::io &watcher, int revents)
{
	if (revents & EV_ERROR) {
		LOG_ERR(this, "ERROR: got invalid event (sock=%d): %s\n", sock, strerror(errno));
		destroy();
		return;
	}

	LOG_EV(this, "%s (sock=%d) %x\n", (revents & EV_WRITE & EV_READ) ? "IO_CB" : (revents & EV_WRITE) ? "WRITE_CB" : (revents & EV_READ) ? "READ_CB" : "IO_CB", sock, revents);

	if (sock == -1) {
		return;
	}

	assert(sock == watcher.fd);

	if (sock != -1 && revents & EV_WRITE) {
		write_cb();
	}

	if (sock != -1 && revents & EV_READ) {
		read_cb();
	}

	io_update();
}


int BaseClient::write_directly()
{
	if (sock == -1) {
		LOG_ERR(this, "ERROR: write error (sock=%d): Socket already closed!\n", sock);
		return WR_ERR;
	} else if (!write_queue.empty()) {
		Buffer* buffer = write_queue.front();

		size_t buf_size = buffer->nbytes();
		const char *buf_data = buffer->dpos();

#ifdef MSG_NOSIGNAL
		ssize_t written = ::send(sock, buf_data, buf_size, MSG_NOSIGNAL);
#else
		ssize_t written = ::write(sock, buf_data, buf_size);
#endif

		if (written < 0) {
			if (ignored_errorno(errno, false)) {
				return WR_RETRY;
			} else {
				LOG_ERR(this, "ERROR: write error (sock=%d): %s\n", sock, strerror(errno));
				return WR_ERR;
			}
		} else if (written == 0) {
			return WR_PENDING;
		} else {
			LOG_CONN_WIRE(this, "(sock=%d) <<-- '%s'\n", sock, repr(buf_data, written).c_str());
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				if (write_queue.pop(buffer)) {
					delete buffer;
					if (write_queue.empty()) {
						return WR_OK;
					} else {
						return WR_PENDING;
					}
				}
			} else {
				return WR_PENDING;
			}
		}
	}
	return WR_OK;
}


void BaseClient::write_cb()
{
	int status;
	do {
		pthread_mutex_lock(&qmtx);
		status = write_directly();
		pthread_mutex_unlock(&qmtx);
		if (status == WR_ERR) {
			destroy();
			return;
		} else if (status == WR_RETRY) {
			io_write.start();
			return;
		}
	} while (status != WR_OK);
	io_write.stop();
}


void BaseClient::read_cb()
{
	LZ4F_errorCode_t errorCode;

	if (sock != -1) {
		ssize_t received = ::read(sock, read_buffer, BUF_SIZE);
		const char *buf_end = read_buffer + received;
		const char *buf_data = read_buffer;

		if (received < 0) {
			if (sock != -1 && !ignored_errorno(errno, false)) {
				LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", sock, strerror(errno));
				destroy();
				return;
			}
		} else if (received == 0) {
			// The peer has closed its half side of the connection.
			LOG_CONN(this, "Received EOF (sock=%d)!\n", sock);
			destroy();
			return;
		} else {
			LOG_CONN_WIRE(this, "(sock=%d) -->> '%s'\n", sock, repr(buf_data, received).c_str());

			if (mode == MODE_READ_FILE_TYPE) {
				switch (*buf_data++) {
					case '\00':
						mode = MODE_READ_FILE_RAW;
						break;
					case '\01':
						mode = MODE_READ_FILE_LZ4;

						if (!lz4_buffer) {
							lz4_buffer = new char[LZ4F_BLOCK_SIZE];
							if (!lz4_buffer) {
								LOG_ERR(this, "Not enough memory!!\n");
								destroy();
								return;
							}
						}

						if (lz4_dCtx) {
							errorCode = LZ4F_freeDecompressionContext(lz4_dCtx);
							if (LZ4F_isError(errorCode)) {
								LOG_ERR(this, "Failed to free decompression context: error %zd\n", errorCode);
								destroy();
								return;
							}
							lz4_dCtx = NULL;
						}
						errorCode = LZ4F_createDecompressionContext(&lz4_dCtx, LZ4F_VERSION);
						if (LZ4F_isError(errorCode)) {
							LOG_ERR(this, "Failed to create decompression context: error %zd\n", errorCode);
							destroy();
							return;
						}

						break;
					default:
						LOG_CONN(this, "Received wrong file mode!\n");
						destroy();
						return;
				}
				received--;
				length_buffer.clear();
			}

			if (received && (mode == MODE_READ_FILE_RAW || mode == MODE_READ_FILE_LZ4)) {
				do {

					if (file_size == -1) {
						if (buf_data) {
							length_buffer.append(buf_data, received);
						}
						buf_data = length_buffer.data();

						buf_end = buf_data + length_buffer.size();
						file_size = decode_length(&buf_data, buf_end, false);

						if (file_size == -1) {
							return;
						}
						block_size = file_size;
						file_buffer.clear();
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
						buf_data = NULL;
						received = 0;
					}

					if (mode == MODE_READ_FILE_RAW) {
						if (block_size_to_write) {
							on_read_file(file_buf_to_write, block_size_to_write);
							block_size -= block_size_to_write;
						}
						if (file_size == 0 || block_size == 0) {
							on_read_file_done();
							mode = MODE_READ_BUF;
						}

					} else if (mode == MODE_READ_FILE_LZ4) {
						if (block_size_to_write) {
							file_buffer.append(file_buf_to_write, block_size_to_write);
							block_size -= block_size_to_write;
						}
						if (file_size == 0) {
							on_read_file_done();
							mode = MODE_READ_BUF;
							if (lz4_dCtx) {
								errorCode = LZ4F_freeDecompressionContext(lz4_dCtx);
								if (LZ4F_isError(errorCode)) {
									LOG_ERR(this, "Failed to free decompression context: error %zd\n", errorCode);
									destroy();
									return;
								}
								lz4_dCtx = NULL;
							}
						} else if (block_size == 0) {
							const char *srcBuf = file_buffer.data();
							size_t readSize = file_buffer.size();
							size_t nextToLoad = readSize;

							size_t readPos = 0;
							size_t srcSize = 0;

							for(; readPos < readSize && nextToLoad; readPos += srcSize) {
								size_t decSize = LZ4F_BLOCK_SIZE;
								srcSize = readSize - readPos;

								nextToLoad = LZ4F_decompress(lz4_dCtx, lz4_buffer, &decSize, srcBuf + readPos, &srcSize, NULL);
								if(LZ4F_isError(nextToLoad)) {
									LOG_ERR(this, "Failed decompression: error %zd\n", nextToLoad);
									destroy();
									return;
								}

								if(decSize) {
									on_read_file(lz4_buffer, decSize);
								}
							}

							if (buf_data) {
								length_buffer = std::string(buf_data, received);
								buf_data = NULL;
								received = 0;
							} else {
								length_buffer.clear();
							}

							file_size = -1;
						}
					}
				} while (file_size == -1);
			}

			if (received && (mode == MODE_READ_BUF)) {
				on_read(buf_data, received);
			}
		}
	}
}


void BaseClient::async_write_cb(ev::async &watcher, int revents)
{
	io_update();
}


bool BaseClient::write(const char *buf, size_t buf_size)
{
	int status;

	Buffer *buffer = new Buffer('\0', buf, buf_size);
	if (!buffer || !write_queue.push(buffer)) {
		return false;
	}

	LOG_CONN_WIRE(this, "(sock=%d) <ENQUEUE> '%s'\n", sock, repr(buf, buf_size).c_str());

	written += 1;

	do {
		pthread_mutex_lock(&qmtx);
		status = write_directly();
		pthread_mutex_unlock(&qmtx);
		if (status == WR_ERR) {
			destroy();
			return false;
		} else if (status == WR_RETRY) {
			async_write.send();
			return true;
		}
	} while (status != WR_OK);
	return true;
}


void BaseClient::shutdown()
{
	::shutdown(sock, SHUT_RDWR);

	Worker::shutdown();

	if (manager()->shutdown_now) {
		LOG_EV(this, "Signaled destroy!!\n");
		destroy();
		rel_ref();
	}
}


void BaseClient::read_file()
{
	mode = MODE_READ_FILE_TYPE;
	file_size = -1;
}


bool BaseClient::send_file(int fd)
{
	static const LZ4F_preferences_t lz4_preferences = {
		{ LZ4F_max256KB, LZ4F_blockLinked, LZ4F_contentChecksumEnabled, LZ4F_frame, 0, { 0, 0 } },
		0,   /* compression level */
		0,   /* autoflush */
		{ 0, 0, 0, 0 },  /* reserved, must be set to 0 */
	};

	LZ4F_compressionContext_t ctx;
	LZ4F_errorCode_t errorCode = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
	if (LZ4F_isError(errorCode)) {
		LOG_ERR(this, "Failed to create compression context: error %zd\n", errorCode);
		return false;
	}

	size_t file_size = ::lseek(fd, 0, SEEK_END);
	::lseek(fd, 0, SEEK_SET);

	size_t count = 0;

	if (!lz4_buffer) {
		lz4_buffer = new char[LZ4F_BLOCK_SIZE];
		if (!lz4_buffer) {
			LOG_ERR(this, "Not enough memory!!\n");
			return false;
		}
	}

	size_t frame_size = LZ4F_compressBound(LZ4F_BLOCK_SIZE, &lz4_preferences);
	size_t dst_size =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	char *dst_buf = new char[dst_size];
	if (!dst_buf) {
		LOG_ERR(this, "Not enough memory!!\n");
		return false;
	}

	// Signal LZ4 compressed content
	if (!write("\01")) {
		LOG_ERR(this, "Write failed!\n");
		LZ4F_freeCompressionContext(ctx);
		delete []dst_buf;
		return false;
	}

	size_t bytes;
	size_t offset = LZ4F_compressBegin(ctx, dst_buf, frame_size, &lz4_preferences);

	while (count < file_size) {
		size_t src_size = ::read(fd, lz4_buffer, LZ4F_BLOCK_SIZE);
		if (src_size == -1) {
			if (errno == EAGAIN) {
				continue;
			}
			LOG_ERR(this, "Read error!!\n");
			LZ4F_freeCompressionContext(ctx);
			delete []dst_buf;
			return false;
		} else if (src_size == 0) {
			break;
		}

		bytes = LZ4F_compressUpdate(ctx, dst_buf + offset, dst_size - offset, lz4_buffer, src_size, NULL);
		if (LZ4F_isError(bytes)) {
			LOG_ERR(this, "Compression failed: error %zd\n", bytes);
			LZ4F_freeCompressionContext(ctx);
			delete []dst_buf;
			return false;
		}

		offset += bytes;
		count += bytes;

		if (dst_size - offset < frame_size + LZ4_FOOTER_SIZE) {
			if (!write(encode_length(offset)) ||
				!write(dst_buf, offset)) {
				LOG_ERR(this, "Write failed!\n");
				LZ4F_freeCompressionContext(ctx);
				delete []dst_buf;
				return false;
			}
			offset = 0;
		}
	}

	bytes = LZ4F_compressEnd(ctx, dst_buf + offset, dst_size -  offset, NULL);
	if (LZ4F_isError(bytes)) {
		LOG_ERR(this, "Compression failed: error %zd\n", bytes);
		LZ4F_freeCompressionContext(ctx);
		delete []dst_buf;
		return false;
	}

	offset += bytes;

	if (!write(encode_length(offset)) ||
		!write(dst_buf, offset) ||
		!write(encode_length(0))) {
		LOG_ERR(this, "Write failed!\n");
		LZ4F_freeCompressionContext(ctx);
		delete []dst_buf;
		return false;
	}

	LZ4F_freeCompressionContext(ctx);
	delete []dst_buf;

	return count != file_size;
}