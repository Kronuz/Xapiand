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

#pragma once

#include <algorithm>        // for move
#include <array>            // for std::array
#include <cstring>          // for size_t, memcpy
#include <fcntl.h>
#include <functional>       // for function, __base
#include <iostream>
#include <iterator>         // for input_iterator_tag, iterator
#include <stdlib.h>         // for malloc, free
#include <string.h>
#include <string>           // for string
#include "string_view.hh"   // for std::string_view
#include <fcntl.h>          // for O_RDONLY
#include <sys/stat.h>
#include <sys/types.h>      // for off_t, uint16_t, ssize_t, uint32_t
#include <type_traits>      // for forward

#include "exception.h"      // for Error
#include "io.hh"            // for close, open
#include "likely.h"         // for likely, unlikely
#include "lz4/lz4.h"        // for LZ4_COMPRESSBOUND, LZ4_resetStream, LZ4_stre...
#include "lz4/xxhash.h"     // for XXH32_createState, XXH32_reset, XXH32_digest
#include "stringified.hh"   // for stringified


constexpr size_t LZ4_BLOCK_SIZE        = 1024 * 2;
constexpr size_t LZ4_MAX_CMP_SIZE      = sizeof(uint16_t) + LZ4_COMPRESSBOUND(LZ4_BLOCK_SIZE);
constexpr size_t LZ4_RING_BUFFER_BYTES = 1024 * 256 + LZ4_BLOCK_SIZE;


class LZ4Exception : public Error {
public:
	template<typename... Args>
	LZ4Exception(Args&&... args) : Error(std::forward<Args>(args)...) { }
};


class LZ4IOError : public LZ4Exception {
public:
	template<typename... Args>
	LZ4IOError(Args&&... args) : LZ4Exception(std::forward<Args>(args)...) { }
};


class LZ4CorruptVolume : public LZ4Exception {
public:
	template<typename... Args>
	LZ4CorruptVolume(Args&&... args) : LZ4Exception(std::forward<Args>(args)...) { }
};


template<typename Impl>
class LZ4BlockStreaming {
protected:
	// These variables must be defined in init function.
	size_t _size;
	size_t _offset;

	std::array<char, LZ4_COMPRESSBOUND(LZ4_BLOCK_SIZE)> cmpBuf;
	std::array<char, LZ4_RING_BUFFER_BYTES> buffer;

	XXH32_state_t xxh_state;

	std::string _init() {
		return static_cast<Impl*>(this)->init();
	}

	std::string _next() {
		return static_cast<Impl*>(this)->next();
	}

	void _reset(int seed) {
		_size = 0;
		_offset = 0;
		XXH32_reset(&xxh_state, seed);
	}

public:
	explicit LZ4BlockStreaming(int seed)
		: _size(0),
		  _offset(0)
	{
		XXH32_reset(&xxh_state, seed);
	}

	// This class is not CopyConstructible or CopyAssignable.
	LZ4BlockStreaming(const LZ4BlockStreaming&) = delete;
	LZ4BlockStreaming& operator=(const LZ4BlockStreaming&) = delete;

	class iterator : public std::iterator<std::input_iterator_tag, LZ4BlockStreaming> {
		LZ4BlockStreaming* obj;
		std::string current_str;
		size_t offset;

		friend class LZ4BlockStreaming;

	public:
		iterator()
			: obj(nullptr),
			  offset(0) { }

		iterator(LZ4BlockStreaming* o, std::string&& str)
			: obj(o),
			  current_str(std::move(str)),
			  offset(0) { }

		iterator(iterator&& it)
			: obj(std::move(it.obj)),
			  current_str(std::move(it.current_str)),
			  offset(std::move(it.offset)) { }

		iterator& operator=(iterator&& it) {
			obj = std::move(it.obj);
			current_str = std::move(it.current_str);
			offset = std::move(it.offset);
			return *this;
		}

		// iterator is not CopyConstructible or CopyAssignable.
		iterator(const iterator&) = delete;
		iterator& operator=(const iterator&) = delete;

		iterator& operator++() {
			current_str = obj->_next();
			offset = 0;
			return *this;
		}

		const std::string& operator*() const noexcept {
			return current_str;
		}

		const std::string* operator->() const noexcept {
			return &current_str;
		}

		size_t size() const noexcept {
			return current_str.size();
		}

		bool operator==(const iterator& other) const noexcept {
			return current_str == other.current_str;
		}

		bool operator!=(const iterator& other) const noexcept {
			return !operator==(other);
		}

		explicit operator bool() const noexcept {
			return !current_str.empty();
		}

		size_t read(char* buf, size_t buf_size) {
			size_t res_size = current_str.size() - offset;
			if (!res_size) {
				current_str = obj->_next();
				offset = 0;
				res_size = current_str.size();
			}

			if (res_size < buf_size) {
				buf_size = res_size;
			}
			memcpy(buf, current_str.data() + offset, buf_size);
			offset += buf_size;
			return buf_size;
		}
	};

	iterator begin() {
		return iterator(this, _init());
	}

	iterator end() {
		return iterator(this, std::string());
	}

	size_t size() const noexcept {
		return _size;
	}

	uint32_t get_digest() {
		return XXH32_digest(&xxh_state);
	}
};


class LZ4Data {
protected:
	const char* data;
	size_t data_size;
	size_t data_offset;

	LZ4Data(const char* data_, size_t data_size_)
		: data(data_),
		  data_size(data_size_),
		  data_offset(0) { }

	~LZ4Data() = default;

public:
	int close() {
		data_offset = 0;
		return 0;
	}

	void add_data(const char* data_, size_t data_size_) {
		data = data_;
		data_size = data_size_;
	}
};


/*
 * Compress Data.
 */
class LZ4CompressData : public LZ4Data, public LZ4BlockStreaming<LZ4CompressData> {
	LZ4_stream_t* const lz4Stream;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4CompressData>;

public:
	LZ4CompressData(const char* data_=nullptr, size_t data_size_=0, int seed_=0);

	~LZ4CompressData();

	void reset(const char* data_, size_t data_size_, int seed=0) {
		_reset(seed);
		add_data(data_, data_size_);
		LZ4_resetStream(lz4Stream);
	}
};


/*
 * Decompress Data.
 */
class LZ4DecompressData : public LZ4Data, public LZ4BlockStreaming<LZ4DecompressData> {
	LZ4_streamDecode_t* const lz4StreamDecode;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4DecompressData>;

public:
	LZ4DecompressData(const char* data_=nullptr, size_t data_size_=0, int seed=0);

	~LZ4DecompressData();

	void reset(const char* data_, size_t data_size_, int seed=0) {
		_reset(seed);
		add_data(data_, data_size_);
	}
};


class LZ4File {
protected:
	int fd;
	off_t fd_offset;
	off_t fd_nbytes;
	bool fd_internal;

	const size_t block_size;

	std::function<size_t()> get_read_size;

	LZ4File(size_t block_size_, std::string_view filename)
		: fd{-1},
		  fd_offset{0},
		  fd_nbytes{-1},
		  fd_internal{false},
		  block_size{block_size_}
	{
		open(filename);
	}

	LZ4File(size_t block_size_, int fd_, off_t fd_offset_, off_t fd_nbytes_)
		: fd{-1},
		  fd_offset{0},
		  fd_nbytes{-1},
		  fd_internal{false},
		  block_size{block_size_}
	{
		add_fildes(fd_, fd_offset_, fd_nbytes_);
	}

	virtual ~LZ4File() {
		close();
	}

public:
	int close() {
		int ret = 0;
		if (fd_internal && fd != -1) {
			ret = io::close(fd);
		}
		fd = -1;
		fd_offset = 0;
		fd_nbytes = -1;
		fd_internal = false;
		return ret;
	}

	void open(std::string_view filename) {
		stringified filename_string(filename);
		fd = io::open(filename_string.c_str(), O_RDONLY);
		if unlikely(fd == -1) {
			THROW(LZ4IOError, "Cannot open file: %s", filename_string);
		}
		fd_offset = 0;
		fd_internal = true;
		fd_nbytes = -1;
		get_read_size = [this]() { return block_size; };
	}

	void add_fildes(int fd_, size_t fd_offset_, size_t fd_nbytes_) {
		fd = fd_;
		fd_offset = fd_offset_;
		fd_nbytes = fd_nbytes_;
		fd_internal = false;
		if (fd_nbytes == -1) {
			get_read_size = [this]() { return block_size; };
		} else {
			get_read_size = [this]() {
				size_t size = fd_nbytes > static_cast<off_t>(block_size) ? block_size : fd_nbytes;
				fd_nbytes -= size;
				return size;
			};
		}
	}

	void add_file(std::string_view filename) {
		open(filename);
	}
};


/*
 * Compress a file.
 */
class LZ4CompressFile : public LZ4File, public LZ4BlockStreaming<LZ4CompressFile> {
	LZ4_stream_t* const lz4Stream;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4CompressFile>;

public:
	LZ4CompressFile(std::string_view filename, int seed=0);

	LZ4CompressFile(int fd_=0, off_t fd_offset_=-1, off_t fd_nbytes_=-1, int seed=0);

	~LZ4CompressFile();

	void reset(int fd_, size_t fd_offset_, size_t fd_nbytes_, int seed=0) {
		_reset(seed);
		add_fildes(fd_, fd_offset_, fd_nbytes_);
		LZ4_resetStream(lz4Stream);
	}

	void reset(std::string_view filename, int seed=0) {
		_reset(seed);
		open(filename);
		LZ4_resetStream(lz4Stream);
	}
};


/*
 * Decompress a file.
 */
class LZ4DecompressFile : public LZ4File, public LZ4BlockStreaming<LZ4DecompressFile> {
	LZ4_streamDecode_t* const lz4StreamDecode;

	char* const data;
	ssize_t data_size;
	size_t data_offset;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4DecompressFile>;

public:
	LZ4DecompressFile(std::string_view filename, int seed=0);

	LZ4DecompressFile(int fd_=0, off_t fd_offset_=-1, off_t fd_nbytes_=-1, int seed=0);

	~LZ4DecompressFile();

	void reset(int fd_, size_t fd_offset_, size_t fd_nbytes_, int seed=0) {
		_reset(seed);
		add_fildes(fd_, fd_offset_, fd_nbytes_);
	}

	void reset(std::string_view filename, int seed=0) {
		_reset(seed);
		open(filename);
	}
};


static inline std::string
compress_lz4(std::string_view uncompressed)
{
	std::string compressed;
	LZ4CompressData compressor(uncompressed.data(), uncompressed.size());
	for (auto it = compressor.begin(); it; ++it) {
		compressed.append(*it);
	}
	return compressed;
}


static inline std::string
decompress_lz4(std::string_view compressed)
{
	std::string decompressed;
	LZ4DecompressData decompressor(compressed.data(), compressed.size());
	for (auto it = decompressor.begin(); it; ++it) {
		decompressed.append(*it);
	}
	return decompressed;
}
