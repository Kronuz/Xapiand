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

#include <array>                 // for std::array
#include <cstring>               // for string and memcpy
#include <stdlib.h>              // for malloc, free
#include "string_view.hh"        // for std::string_view
#include <zlib.h>                // for z_stream

#include "exception.h"           // for Error
#include "io.hh"                 // for close, open
#include "stringified.hh"        // for stringified
#include "likely.h"              // for likely, unlikely


#define DEFLATE_BLOCK_SIZE 16384


class DeflateException : public Error {
public:
	template<typename... Args>
	DeflateException(Args&&... args) : Error(std::forward<Args>(args)...) { }
};


class DeflateIOError : public DeflateException {
public:
	template<typename... Args>
	DeflateIOError(Args&&... args) : DeflateException(std::forward<Args>(args)...) { }
};


template <typename Impl>
class DeflateBlockStreaming {
protected:
	enum class DeflateState : uint8_t {
		NONE,
		INIT,
		END,
	};

	bool gzip;
	z_stream strm;
	int stream;

	static constexpr size_t cmpBuf_size = DEFLATE_BLOCK_SIZE;
	static constexpr size_t buffer_size = DEFLATE_BLOCK_SIZE;

	std::unique_ptr<char[]> cmpBuf;
	std::unique_ptr<char[]> buffer;

	DeflateState state;

	std::string _init() {
		return static_cast<Impl*>(this)->init();
	}

	std::string _next() {
		return static_cast<Impl*>(this)->next();
	}

public:
	explicit DeflateBlockStreaming(bool gzip_)
		: gzip(gzip_),
		  stream(0),
		  state(DeflateState::NONE) { }

	class iterator : public std::iterator<std::input_iterator_tag, DeflateBlockStreaming> {
		DeflateBlockStreaming* obj;
		std::string current_str;
		size_t offset;

		friend class DeflateBlockStreaming;

	public:
		iterator()
			: obj(nullptr),
			  offset(0) { }

		iterator(DeflateBlockStreaming* o, std::string&& str)
			: obj(o),
			  current_str(std::move(str)),
			  offset(0) { }

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
			return obj->state != DeflateState::END;
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
};


class DeflateData {
protected:
	const char* data;
	size_t data_size;
	size_t data_offset;

	DeflateData(const char* data_, size_t data_size_)
		: data{data_},
		  data_size{data_size_},
		  data_offset{0} { }

	~DeflateData() = default;

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
class DeflateCompressData : public DeflateData, public DeflateBlockStreaming<DeflateCompressData> {
	std::string next();

	friend class DeflateBlockStreaming<DeflateCompressData>;

public:
	DeflateCompressData(const char* data_=nullptr, size_t data_size_=0, bool gzip_=false);

	~DeflateCompressData();

	std::string init();
	std::string next(const char* input, size_t input_size, int flush=Z_PARTIAL_FLUSH);

	static int FINISH_COMPRESS;

	void reset(const char* data_, size_t data_size_, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		data_offset = 0;
		add_data(data_, data_size_);
	}
};


/*
 * Decompress Data.
 */
class DeflateDecompressData : public DeflateData, public DeflateBlockStreaming<DeflateDecompressData> {
	std::string init();
	std::string next();

	friend class DeflateBlockStreaming<DeflateDecompressData>;

public:
	DeflateDecompressData(const char* data_=nullptr, size_t data_size_=0, bool gzip_=false);

	~DeflateDecompressData();

	void reset(const char* data_, size_t data_size_, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		data_offset = 0;
		add_data(data_, data_size_);
	}
};


class DeflateFile {
protected:
	int fd;
	off_t fd_offset;
	off_t fd_nbytes;
	bool fd_internal;

	size_t bytes_readed;
	size_t size_file;

	explicit DeflateFile(std::string_view filename)
		: fd{-1},
		  fd_offset{0},
		  fd_nbytes{-1},
		  fd_internal{false},
		  bytes_readed{0},
		  size_file{0}
	{
		open(filename);
	}

	DeflateFile(int fd_, off_t fd_offset_, off_t fd_nbytes_)
		: fd{-1},
		  fd_offset{0},
		  fd_nbytes{-1},
		  fd_internal{false},
		  bytes_readed{0},
		  size_file{0}
	{
		add_fildes(fd_, fd_offset_, fd_nbytes_);
	}

	virtual ~DeflateFile() {
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
		bytes_readed = 0;
		size_file = 0;
		return ret;
	}

	void open(std::string_view filename) {
		stringified filename_string(filename);
		fd = io::open(filename_string.c_str(), O_RDONLY);
		if unlikely(fd == -1) {
			THROW(DeflateIOError, "Cannot open file: %s", filename_string);
		}
		fd_offset = 0;
		fd_nbytes = -1;
		fd_internal = true;
	}

	void add_fildes(int fd_, size_t fd_offset_, size_t fd_nbytes_) {
		fd = fd_;
		fd_offset = fd_offset_;
		fd_nbytes = fd_nbytes_;
		fd_internal = false;
	}

	void add_file(std::string_view filename) {
		open(filename);
	}
};


/*
 * Compress a file.
 */
class DeflateCompressFile : public DeflateFile, public DeflateBlockStreaming<DeflateCompressFile> {
	std::string init();
	std::string next();

	friend class DeflateBlockStreaming<DeflateCompressFile>;

public:
	DeflateCompressFile(std::string_view filename, bool gzip_=false);

	DeflateCompressFile(int fd_=0, off_t fd_offset_=-1, off_t fd_nbytes_=-1, bool gzip_=false);

	~DeflateCompressFile();

	void reset(int fd_, size_t fd_offset_, size_t fd_nbytes_, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		add_fildes(fd_, fd_offset_, fd_nbytes_);
	}

	void reset(std::string_view filename, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		open(filename);
	}
};


/*
 * Decompress a file.
 */
class DeflateDecompressFile : public DeflateFile, public DeflateBlockStreaming<DeflateDecompressFile> {
	std::string init();
	std::string next();

	friend class DeflateBlockStreaming<DeflateDecompressFile>;

public:
	DeflateDecompressFile(std::string_view filename, bool gzip_=false);

	DeflateDecompressFile(int fd_=0, off_t fd_offset_=-1, off_t fd_nbytes_=-1, bool gzip_=false);

	~DeflateDecompressFile();

	void reset(int fd_, size_t fd_offset_, size_t fd_nbytes_, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		add_fildes(fd_, fd_offset_, fd_nbytes_);
	 }

	void reset(std::string_view filename, bool gzip_=false) {
		gzip = gzip_;
		stream = 0;
		open(filename);
	}
};


static inline std::string
compress_deflate(std::string_view uncompressed)
{
	std::string compressed;
	DeflateCompressData compressor(uncompressed.data(), uncompressed.size());
	for (auto it = compressor.begin(); it; ++it) {
		compressed.append(*it);
	}
	return compressed;
}


static inline std::string
decompress_deflate(std::string_view compressed)
{
	std::string decompressed;
	DeflateDecompressData decompressor(compressed.data(), compressed.size());
	for (auto it = decompressor.begin(); it; ++it) {
		decompressed.append(*it);
	}
	return decompressed;
}
