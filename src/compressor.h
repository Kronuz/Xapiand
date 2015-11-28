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

#pragma once

#include <assert.h>
#include <memory>

#include "utils.h"
#include "lz4/lz4frame.h"

#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 12
#define LZ4F_BLOCK_SIZE_ID LZ4F_max256KB
#define LZ4F_BLOCK_SIZE (256 * 1024)
#define NOCOMPRESS_BUFFER_SIZE (16 * 1024)

typedef char *char_ptr;


class CompressorReader
{
public:
	virtual ~CompressorReader() {};

	virtual ssize_t begin() = 0;
	virtual ssize_t read(char **buf, size_t size) = 0;
	virtual ssize_t write(const char *buf, size_t size) = 0;
	virtual ssize_t done() = 0;

	virtual void clear() = 0;
	virtual void append(const char *buf, size_t size) = 0;
};


class CompressorBufferReader : public CompressorReader
{
public:
	ssize_t begin() {
		output.clear();
		offset = 0;
		return 0;
	};
	ssize_t read(char **buf, size_t size) {
		//LOG_DEBUG(this, "input size %lu \n",input.size());
		if (input.size() == 0) {
			//LOG_DEBUG(this, "base_read (0)\n");
			return 0;
		}
		if (offset >= input.size()) {
			errno = EWOULDBLOCK;
			//LOG_DEBUG(this, "base_read (-1)\n");
			return -1;
		}
		if (*buf) {
			if (size > input.size() - offset) {
				size = input.size() - offset;
			}
			memcpy(buf, input.data() + offset, size);
			offset += size;
		} else {
			*buf = (char *)input.data();
			offset = input.size();
			size = offset;
		}
		//LOG_DEBUG(this, "base_read: %s (%zu)\n", repr(*buf, size).c_str(), size);
		return size;
	};
	ssize_t write(const char *buf, size_t size) {
		//LOG_DEBUG(this, "base_write: %s (%zu)\n", repr(buf, size).c_str(), size);
		output.append(buf, size);
		return size;
	};
	ssize_t done() { return 0; };

	size_t offset;
	std::string input;
	std::string output;

public:
	CompressorBufferReader() : offset(0) {}

	void clear() {
		input.clear();
		offset = 0;
	}
	void append(const char *buf, size_t size) {
		input.append(buf, size);
	}
};


class Compressor {
public:
	std::unique_ptr<CompressorReader> decompressor;
	std::unique_ptr<CompressorReader> compressor;

	virtual ssize_t decompress() = 0;
	virtual ssize_t compress() = 0;

	Compressor(std::unique_ptr<CompressorReader> &&decompressor_, std::unique_ptr<CompressorReader> &&compressor_) :
		decompressor(std::move(decompressor_)),
		compressor(std::move(compressor_))
	{}
};


class NoCompressor : public Compressor
{
	ssize_t count;
	char *buffer;

public:
	NoCompressor(std::unique_ptr<CompressorReader> &&decompressor_, std::unique_ptr<CompressorReader> &&compressor_) :
		Compressor(std::move(decompressor_), std::move(compressor_)), count(-1), buffer(nullptr) {}

	~NoCompressor() {
		delete []buffer;
	}

	ssize_t decompress() {
		// LOG_DEBUG(this, "decompress!\n");
		if (count == -1) {
			count = 0;
			// LOG_DEBUG(this, "decompress (decompressor->begin)\n");
			if (decompressor->begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		// LOG_DEBUG(this, "decompress (while)\n");
		while (true) {
			char *src_buffer = nullptr;
			// LOG_DEBUG(this, "decompress (decompressor->read)\n");
			ssize_t read_size = decompressor->read(&src_buffer, -1);
			if (read_size == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (read_size == 0) {
				// LOG_DEBUG(this, "decompress (decompressor->read=0)\n");
				break;
			}
			count += read_size;
			if (decompressor->write(src_buffer, read_size) < 0) {
				// LOG_DEBUG(this, "decompress (decompressor->write)\n");
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}
		}

		// LOG_DEBUG(this, "decompress (decompressor->done)\n");
		if (decompressor->done() < 0) {
			LOG_ERR(this, "Done failed!\n");
			return -1;
		}

		return count;
	}

	ssize_t compress() {
		// LOG_DEBUG(this, "compress!\n");
		if (count == -1) {
			count = 0;

			if (!buffer) {
				buffer = new char[NOCOMPRESS_BUFFER_SIZE];
			}

			// LOG_DEBUG(this, "compress (compressor->begin)\n");
			if (compressor->begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		// LOG_DEBUG(this, "compress (while)\n");
		while (true) {
			// LOG_DEBUG(this, "compress (compressor->read)\n");
			ssize_t src_size = compressor->read(&buffer, NOCOMPRESS_BUFFER_SIZE);
			if (src_size == -1) {
				// LOG_DEBUG(this, "compress (compressor->read=-1)\n");
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (src_size == 0) {
				// LOG_DEBUG(this, "compress (compressor->read=0)\n");
				break;
			}
			count += src_size;
			// LOG_DEBUG(this, "compress (compressor->write)\n");
			if (compressor->write(buffer, src_size) < 0) {
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}
		}

		// LOG_DEBUG(this, "compress (compressor->done)\n");
		if (compressor->done() < 0) {
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
	LZ4Compressor(std::unique_ptr<CompressorReader> &&decompressor_, std::unique_ptr<CompressorReader> &&compressor_) :
		Compressor(std::move(decompressor_), std::move(compressor_)),
		c_ctx(nullptr),
		d_ctx(nullptr),
		buffer(nullptr),
		work_buffer(nullptr),
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
		// LOG_DEBUG(this, "decompress!\n");

		if (!d_ctx) {
			if (!buffer) {
				buffer = new char[LZ4F_BLOCK_SIZE];
			}
			LZ4F_errorCode_t errorCode = LZ4F_createDecompressionContext(&d_ctx, LZ4F_VERSION);
			if (LZ4F_isError(errorCode)) {
				LOG_ERR(this, "Failed to create decompression context: error %zd\n", errorCode);
				throw MSG_Error("Failed to create decompression context");
			}

			// LOG_DEBUG(this, "decompress (decompressor->begin)\n");
			if (decompressor->begin() < 0) {
				LOG_ERR(this, "Begin failed!\n");
				return -1;
			}
		}

		// LOG_DEBUG(this, "decompress (while)\n");
		while (true) {
			char *src_buffer = nullptr;
			// LOG_DEBUG(this, "decompress (decompressor->read)\n");
			ssize_t read_size = decompressor->read(&src_buffer, -1);
			if (read_size == -1) {
				// LOG_DEBUG(this, "decompress (decompressor->read=-1)\n");
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (read_size == 0) {
				// LOG_DEBUG(this, "decompress (decompressor->read=0)\n");
				break;
			}

			size_t src_size;
			size_t nextToLoad = read_size;

			// LOG_DEBUG(this, "\t read_size: %zu\n", read_size);

			for (ssize_t read_pos = 0; read_pos < read_size && nextToLoad; read_pos += src_size) {
				src_size = read_size - read_pos;
				size_t dst_size = LZ4F_BLOCK_SIZE;

				// LOG_DEBUG(this, "\t src_size (1): %zu\n", src_size);
				// LOG_DEBUG(this, "\t dst_size (1): %zu\n", dst_size);

				nextToLoad = LZ4F_decompress(d_ctx, buffer, &dst_size, src_buffer + read_pos, &src_size, nullptr);
				if (LZ4F_isError(nextToLoad)) {
					LOG_ERR(this, "Failed decompression: error %zd\n", nextToLoad);
					return -1;
				}

				// LOG_DEBUG(this, "\t nextToLoad: %zu\n", nextToLoad);
				// LOG_DEBUG(this, "\t src_size (2): %zu\n", src_size);
				// LOG_DEBUG(this, "\t dst_size (2): %zu\n", dst_size);

				if (dst_size) {
					count += dst_size;
					// LOG_DEBUG(this, "decompress (decompressor->write)\n");
					if (decompressor->write(buffer, dst_size) < 0) {
						LOG_ERR(this, "Write failed!\n");
						return -1;
					}
				}
			}
		}

		// LOG_DEBUG(this, "decompress (decompressor->done)\n");
		if (decompressor->done() < 0) {
			LOG_ERR(this, "Done failed!\n");
			return -1;
		}

		return count;
	}

	ssize_t compress() {
		// LOG_DEBUG(this, "compress!\n");
		size_t bytes;
		if (!c_ctx) {
			assert(work_buffer == nullptr);

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
			// LOG_DEBUG(this, "compress (compressor->begin)\n");
			if (compressor->begin() < 0) {
				LOG_ERR(this, "Write failed!\n");
				return -1;
			}

			offset = LZ4F_compressBegin(c_ctx, work_buffer, frame_size, &lz4_preferences);
		}

		// LOG_DEBUG(this, "compress (while)\n");
		while (true) {
			// LOG_DEBUG(this, "compress (compressor->read)\n");
			ssize_t src_size = compressor->read(&buffer, LZ4F_BLOCK_SIZE);
			if (src_size == -1) {
				// LOG_DEBUG(this, "compress (compressor->read=-1)\n");
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return count;
				}
				LOG_ERR(this, "Read error!!\n");
				return -1;
			} else if (src_size == 0) {
				// LOG_DEBUG(this, "compress (compressor->read=0)\n");
				break;
			}

			bytes = LZ4F_compressUpdate(c_ctx, work_buffer + offset, work_size - offset, buffer, src_size, nullptr);
			if (LZ4F_isError(bytes)) {
				LOG_ERR(this, "Compression failed: error %zd\n", bytes);
				return -1;
			}

			offset += bytes;
			count += bytes;

			if (work_size - offset < frame_size + LZ4_FOOTER_SIZE) {
				// LOG_DEBUG(this, "compress (compressor->write)\n");
				if (compressor->write(work_buffer, offset) < 0) {
					LOG_ERR(this, "Write failed!\n");
					return -1;
				}
				offset = 0;
			}
		}

		bytes = LZ4F_compressEnd(c_ctx, work_buffer + offset, work_size -  offset, nullptr);
		if (LZ4F_isError(bytes)) {
			LOG_ERR(this, "Compression failed: error %zd\n", bytes);
			return -1;
		}

		offset += bytes;

		// LOG_DEBUG(this, "compress (compressor->done)\n");
		if (compressor->write(work_buffer, offset) < 0 || compressor->done() < 0) {
			LOG_ERR(this, "Write failed!\n");
			return -1;
		}

		return count;
	}
};
