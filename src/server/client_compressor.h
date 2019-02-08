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

#pragma once

#include "compressor_lz4.h"      // for LZ4CompressFile, LZ4DecompressData
#include "length.h"              // for serialise_length
#include "log.h"                 // for L_CALL, L_ERR, L_EV_BEGIN, L_CONN
#include "lz4/xxhash.h"          // for XXH32_createState, XXH32_digest, XXH32_state_t, XXH32_reset


#define COMPRESSION_SEED 0xCEED


//   ____
//  / ___|___  _ __ ___  _ __  _ __ ___  ___ ___  ___  _ __
// | |   / _ \| '_ ` _ \| '_ \| '__/ _ \/ __/ __|/ _ \| '__|
// | |__| (_) | | | | | | |_) | | |  __/\__ \__ \ (_) | |
//  \____\___/|_| |_| |_| .__/|_|  \___||___/___/\___/|_|
//                      |_|

template <typename Writer>
class ClientNoCompressor {
	Writer& writer;

	XXH32_state_t* xxh_state;
	size_t offset;
	int fd;

public:
	ClientNoCompressor(Writer& writer, int fd, size_t offset = 0) :
		writer(writer),
		xxh_state(XXH32_createState()),
		offset(offset),
		fd(fd) {}

	~ClientNoCompressor() noexcept {
		XXH32_freeState(xxh_state);
	}

	ssize_t compress() {
		L_CALL("compress()");

		if unlikely(io::lseek(fd, offset, SEEK_SET) != static_cast<off_t>(offset)) {
			L_ERR("IO error: lseek");
			return -1;
		}

		char buffer[LZ4_BLOCK_SIZE];
		XXH32_reset(xxh_state, COMPRESSION_SEED);

		size_t size = 0;
		ssize_t r;
		while ((r = io::read(fd, buffer, sizeof(buffer))) > 0) {
			std::string length(serialise_length(r));
			if (!writer.write(length) || !writer.write(buffer, r)) {
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

		if (!writer.write(serialise_length(0)) || !writer.write(serialise_length(XXH32_digest(xxh_state)))) {
			L_ERR("Write Footer failed!");
			return -1;
		}

		return size;
	}
};


template <typename Writer>
class ClientLZ4Compressor : public LZ4CompressFile {
	Writer& writer;

public:
	ClientLZ4Compressor(Writer& writer, int fd, size_t offset = 0) :
		LZ4CompressFile(fd, offset, -1, COMPRESSION_SEED),
		writer(writer) {}

	ssize_t compress() {
		L_CALL("compress()");

		try {
			auto it = begin();
			while (it) {
				std::string length(serialise_length(it.size()));
				if (!writer.write(length) || !writer.write(it->data(), it.size())) {
					L_ERR("Write failed!");
					return -1;
				}
				++it;
			}
		} catch (const std::exception& e) {
			L_ERR("{}", e.what());
			return -1;
		}

		if (!writer.write(serialise_length(0)) || !writer.write(serialise_length(get_digest()))) {
			L_ERR("Write Footer failed!");
			return -1;
		}

		return size();
	}
};


//  ____
// |  _ \  ___  ___ ___  _ __ ___  _ __  _ __ ___  ___ ___  ___  _ __
// | | | |/ _ \/ __/ _ \| '_ ` _ \| '_ \| '__/ _ \/ __/ __|/ _ \| '__|
// | |_| |  __/ (_| (_) | | | | | | |_) | | |  __/\__ \__ \ (_) | |
// |____/ \___|\___\___/|_| |_| |_| .__/|_|  \___||___/___/\___/|_|
//                                |_|

template <typename Reader>
class ClientNoDecompressor {
	Reader& reader;

	XXH32_state_t* xxh_state;
	std::string input;

public:
	ClientNoDecompressor(Reader& reader) :
		reader(reader),
		xxh_state(XXH32_createState()) {
		XXH32_reset(xxh_state, COMPRESSION_SEED);
	}

	~ClientNoDecompressor() noexcept {
		XXH32_freeState(xxh_state);
	}

	void clear() noexcept {
		L_CALL("clear()");

		input.clear();
	}

	void append(const char *buf, size_t size) {
		L_CALL("append({})", repr(buf, size));

		input.append(buf, size);
	}

	ssize_t decompress() {
		L_CALL("decompress()");

		size_t size = input.size();
		const char* data = input.data();
		reader.on_read_file(data, size);
		XXH32_update(xxh_state, data, size);

		return size;
	}

	bool verify(uint32_t checksum_) noexcept {
		L_CALL("verify({:#06x})", checksum_);

		return XXH32_digest(xxh_state) == checksum_;
	}
};


template <typename Reader>
class ClientLZ4Decompressor : public LZ4DecompressData {
	Reader& reader;

	std::string input;

public:
	ClientLZ4Decompressor(Reader& reader) :
		LZ4DecompressData(nullptr, 0, COMPRESSION_SEED),
		reader(reader) {}


	void clear() noexcept {
		L_CALL("clear()");

		input.clear();
	}

	void append(const char *buf, size_t size) {
		L_CALL("append({})", repr(buf, size));

		input.append(buf, size);
	}

	ssize_t decompress() {
		L_CALL("decompress()");

		add_data(input.data(), input.size());
		auto it = begin();
		while (it) {
			reader.on_read_file(it->data(), it.size());
			++it;
		}
		return size();
	}

	bool verify(uint32_t checksum_) noexcept {
		L_CALL("verify({:#06x})", checksum_);

		return get_digest() == checksum_;
	}
};
