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

#ifndef XAPIAND_INCLUDED_LZ4_WRAPPER_H
#define XAPIAND_INCLUDED_LZ4_WRAPPER_H

#include <assert.h>

#include "utils.h"
#include "lz4/lz4frame.h"

#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 12
#define LZ4F_BLOCK_SIZE_ID LZ4F_max256KB
#define LZ4F_BLOCK_SIZE (256 * 1024)

typedef char *char_ptr;

class Compressor
{
	ssize_t base_read(std::string &buffer, size_t &offset, char **buf, size_t size) {
		if (buffer.size() == 0) {
			// LOG(this, "base_read (0)\n");
			return 0;
		}
		if (offset >= buffer.size()) {
			errno = EWOULDBLOCK;
			// LOG(this, "base_read (-1)\n");
			return -1;
		}
		if (*buf) {
			if (size > buffer.size() - offset) {
				size = buffer.size() - offset;
			}
			memcpy(buf, buffer.data() + offset, size);
			offset += size;
		} else {
			*buf = (char *)buffer.data();
			offset = buffer.size();
			size = offset;
		}
		// LOG(this, "base_read: %s (%zu)\n", repr(*buf, size).c_str(), size);
		return size;
	}

	ssize_t base_write(std::string &buffer, size_t &offset, const char *buf, size_t size) {
		// LOG(this, "base_write: %s (%zu)\n", repr(buf, size).c_str(), size);
		buffer.append(buf, size);
		return size;
	}

protected:
	virtual ssize_t compressor_begin() {
		compressed.clear();
		compressed_offset = 0;
		decompressed_offset = 0;
		return 0;
	};
	virtual ssize_t compressor_read(char **buf, size_t size) {
		return base_read(decompressed, decompressed_offset, buf, size);
	};
	virtual ssize_t compressor_write(const char *buf, size_t size) {
		return base_write(compressed, compressed_offset, buf, size);
	};
	virtual ssize_t compressor_done() { return 0; };

	virtual ssize_t decompressor_begin() {
		decompressed.clear();
		decompressed_offset = 0;
		compressed_offset = 0;
		return 0;
	};
	virtual ssize_t decompressor_read(char **buf, size_t size) {
		return base_read(compressed, compressed_offset, buf, size);
	};
	virtual ssize_t decompressor_write(const char *buf, size_t size) {
		return base_write(decompressed, decompressed_offset, buf, size);
	};
	virtual ssize_t decompressor_done() { return 0; };

public:
	size_t decompressed_offset;
	std::string decompressed;

	size_t compressed_offset;
	std::string compressed;

	Compressor() : decompressed_offset(0), compressed_offset(0) {}
	virtual ~Compressor() {}

	virtual ssize_t compress() = 0;
	virtual ssize_t decompress() = 0;
};


class NoCompressor : public Compressor
{
	size_t count;

public:
	NoCompressor() : count(-1) {}
	~NoCompressor() {}

	ssize_t decompress() {
		if (count == -1) {
			count = 0;
			if (decompressor_begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		while (true) {
			char *src_buffer = NULL;
			size_t read_size = decompressor_read(&src_buffer, -1);
			if (read_size == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (read_size == 0) {
				break;
			}
			count += read_size;
			if (decompressor_write(src_buffer, read_size) < 0) {
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}
		}

		if (decompressor_done() < 0) {
			LOG_ERR(this, "Done failed!\n");
			return -1;
		}

		return count;
	}

	ssize_t compress() {
		if (count == -1) {
			count = 0;
			if (compressor_begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		while (true) {
			char *src_buffer = NULL;
			size_t read_size = compressor_read(&src_buffer, -1);
			if (read_size == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (read_size == 0) {
				break;
			}
			count += read_size;
			if (compressor_write(src_buffer, read_size) < 0) {
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}
		}

		if (compressor_done() < 0) {
			LOG_ERR(this, "Done failed!\n");
			return -1;
		}

		return count;
	}
};

class LZ4Compressor : public Compressor
{
	LZ4F_compressionContext_t c_ctx;
	LZ4F_decompressionContext_t d_ctx;
	char *buffer;
	char *work_buffer;
	size_t work_size;
	size_t frame_size;
	size_t count;
	size_t offset;

public:
	LZ4Compressor()
		: c_ctx(NULL),
		  d_ctx(NULL),
		  buffer(NULL),
		  work_buffer(NULL),
		  count(0)
	{};

	~LZ4Compressor() {
		delete []buffer;
		delete []work_buffer;

		LZ4F_errorCode_t errorCode;

		if (d_ctx) {
			errorCode = LZ4F_freeDecompressionContext(d_ctx);
			if (LZ4F_isError(errorCode)) {
				LOG_ERR(this, "Failed to free decompression context: error %zd\n", errorCode);
			}
		}

		if (c_ctx) {
			errorCode = LZ4F_freeCompressionContext(c_ctx);
			if (LZ4F_isError(errorCode)) {
				LOG_ERR(this, "Failed to free compression context: error %zd\n", errorCode);
			}
		}
	};

	ssize_t decompress() {
		// LOG(this, "decompress!\n");

		if (!d_ctx) {
			if (!buffer) {
				buffer = new char [LZ4F_BLOCK_SIZE];
			}
			LZ4F_errorCode_t errorCode = LZ4F_createDecompressionContext(&d_ctx, LZ4F_VERSION);
			if (LZ4F_isError(errorCode)) {
				LOG_ERR(this, "Failed to create decompression context: error %zd\n", errorCode);
				throw MSG_Error("Failed to create decompression context");
			}

			// LOG(this, "decompress (decompressor_begin)\n");
			if (decompressor_begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		// LOG(this, "decompress (while)\n");
		while (true) {
			char *src_buffer = NULL;
			// LOG(this, "decompress (decompressor_read)\n");
			size_t read_size = decompressor_read(&src_buffer, -1);
			if (read_size == -1) {
				// LOG(this, "decompress (decompressor_read=-1)\n");
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (read_size == 0) {
				// LOG(this, "decompress (decompressor_read=0)\n");
				break;
			}

			size_t src_size;
			size_t nextToLoad = read_size;

			// LOG(this, "\t read_size: %zu\n", read_size);

			for (size_t read_pos = 0; read_pos < read_size && nextToLoad; read_pos += src_size) {
				src_size = read_size - read_pos;
				size_t dst_size = LZ4F_BLOCK_SIZE;

				// LOG(this, "\t src_size (1): %zu\n", src_size);
				// LOG(this, "\t dst_size (1): %zu\n", dst_size);

				nextToLoad = LZ4F_decompress(d_ctx, buffer, &dst_size, src_buffer + read_pos, &src_size, NULL);
				if (LZ4F_isError(nextToLoad)) {
					LOG_ERR(this, "Failed decompression: error %zd\n", nextToLoad);
					return -1;
				}

				// LOG(this, "\t nextToLoad: %zu\n", nextToLoad);
				// LOG(this, "\t src_size (2): %zu\n", src_size);
				// LOG(this, "\t dst_size (2): %zu\n", dst_size);

				if (dst_size) {
					count += dst_size;
					// LOG(this, "decompress (decompressor_write)\n");
					if (decompressor_write(buffer, dst_size) < 0) {
						LOG_ERR(this, "Write failed!\n");
						return -1;
					}
				}
			}
		}

		// LOG(this, "decompress (decompressor_done)\n");
		if (decompressor_done() < 0) {
			LOG_ERR(this, "Done failed!\n");
			return -1;
		}

		return count;
	}

	ssize_t compress() {
		size_t bytes;

		if (!c_ctx) {
			assert(work_buffer == NULL);

			if (!buffer) {
				buffer = new char[LZ4F_BLOCK_SIZE];
			}

			static const LZ4F_preferences_t lz4_preferences = {
				{ LZ4F_max256KB, LZ4F_blockLinked, LZ4F_contentChecksumEnabled, LZ4F_frame, 0, { 0, 0 } },
				0,   /* compression level */
				0,   /* autoflush */
				{ 0, 0, 0, 0 },  /* reserved, must be set to 0 */
			};
			frame_size = LZ4F_compressBound(LZ4F_BLOCK_SIZE, &lz4_preferences);

			LZ4F_errorCode_t errorCode = LZ4F_createCompressionContext(&c_ctx, LZ4F_VERSION);
			if (LZ4F_isError(errorCode)) {
				LOG_ERR(this, "Failed to create compression context: error %zd\n", errorCode);
				throw MSG_Error("Failed to create compression context");
			}

			work_size =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
			work_buffer = new char[work_size];
			if (!work_buffer) {
				LOG_ERR(this, "Not enough memory!!\n");
				return -1;
			}

			// Signal LZ4 compressed content
			if (compressor_begin() < 0) {
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}

			offset = LZ4F_compressBegin(c_ctx, work_buffer, frame_size, &lz4_preferences);
		}

		while (true) {
			size_t src_size = compressor_read(&buffer, LZ4F_BLOCK_SIZE);
			if (src_size == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (src_size == 0) {
				break;
			}

			bytes = LZ4F_compressUpdate(c_ctx, work_buffer + offset, work_size - offset, buffer, src_size, NULL);
			if (LZ4F_isError(bytes)) {
				LOG_ERR(this, "Compression failed: error %zd\n", bytes);
				return -1;
			}

			offset += bytes;
			count += bytes;

			if (work_size - offset < frame_size + LZ4_FOOTER_SIZE) {
				if (compressor_write(work_buffer, offset) < 0) {
					LOG_ERR(this, "Write failed!\n");
					return -1;
				}
				offset = 0;
			}
		}

		bytes = LZ4F_compressEnd(c_ctx, work_buffer + offset, work_size -  offset, NULL);
		if (LZ4F_isError(bytes)) {
			LOG_ERR(this, "Compression failed: error %zd\n", bytes);
			return -1;
		}

		offset += bytes;

		if (compressor_write(work_buffer, offset) < 0 || compressor_done() < 0) {
			LOG_ERR(this, "Write failed!\n");
			return -1;
		}

		return count;
	}
};

#endif  /* XAPIAND_INCLUDED_LZ4_WRAPPER_H */
