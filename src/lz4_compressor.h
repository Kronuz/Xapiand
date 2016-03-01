/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "lz4/lz4.h"
#include "xapiand.h"

#include <fcntl.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#define LZ4_BLOCK_SIZE (1024 * 2)
#define LZ4_FILE_READ_SIZE (LZ4_BLOCK_SIZE * 2)	// it must be greater than or equal to LZ4_COMPRESSBOUND(LZ4_BLOCK_SIZE).
#define LZ4_RING_BUFFER_BYTES (1024 * 256 + LZ4_BLOCK_SIZE)


template<typename Impl>
class LZ4BlockStreaming {
protected:
	// These variables must be defined in init function.
	size_t _size;
	bool _finish;
	size_t _offset;

	const size_t block_size;
	const size_t cmpBuf_size;

	char* const cmpBuf;
	char* const buffer;

	inline std::string _init() {
		return static_cast<Impl*>(this)->init();
	}

	inline std::string _next() {
		return static_cast<Impl*>(this)->next();
	}

public:
	LZ4BlockStreaming(size_t block_size_)
		: block_size(block_size_),
		  cmpBuf_size(LZ4_COMPRESSBOUND(block_size)),
		  cmpBuf((char*)malloc(cmpBuf_size)),
		  buffer((char*)malloc(LZ4_RING_BUFFER_BYTES)) { }

	// This class is not CopyConstructible or CopyAssignable.
	LZ4BlockStreaming(const LZ4BlockStreaming&) = delete;
	LZ4BlockStreaming operator=(const LZ4BlockStreaming&) = delete;

	~LZ4BlockStreaming() {
		free(cmpBuf);
		free(buffer);
	}

	class iterator : public std::iterator<std::input_iterator_tag, LZ4BlockStreaming> {
		LZ4BlockStreaming* obj;
		std::string current_str;

		friend class LZ4BlockStreaming;

	public:
		iterator(LZ4BlockStreaming* o, std::string&& str)
			: obj(o),
			  current_str(std::move(str)) { }

		iterator(iterator&& it)
			: obj(std::move(it.obj)),
			  current_str(std::move(it.current_str)) { }

		// iterator is not CopyConstructible or CopyAssignable.
		iterator(const iterator&) = delete;
		iterator operator=(const iterator&) = delete;

		iterator& operator++() {
			current_str = obj->_next();
			return *this;
		}

		std::string operator*() const {
			return current_str;
		}

		const std::string* operator->() const {
			return &current_str;
		}

		size_t size() const noexcept {
			return current_str.size();
		}

		bool operator==(const iterator& other) const {
			return current_str == other.current_str;
		}

		bool operator!=(const iterator& other) const {
			return !operator==(other);
		}

		explicit operator bool() const {
			return !current_str.empty();
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
};


class LZ4CompressData : public LZ4BlockStreaming<LZ4CompressData> {
	LZ4_stream_t* const lz4Stream;

	const char* data;
	const size_t data_size;
	size_t data_offset;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4CompressData>;

public:
	LZ4CompressData(const char* data_, size_t data_size_);

	~LZ4CompressData();
};


class LZ4CompressFile : public LZ4BlockStreaming<LZ4CompressFile> {
	LZ4_stream_t* const lz4Stream;

	int fd;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4CompressFile>;

public:
	LZ4CompressFile(const std::string& filename);

	~LZ4CompressFile();
};


class LZ4DecompressData : public LZ4BlockStreaming<LZ4DecompressData> {
	LZ4_streamDecode_t* const lz4StreamDecode;

	const char* data;
	const size_t data_size;
	size_t data_offset;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4DecompressData>;

public:
	LZ4DecompressData(const char* data_, size_t data_size_);

	~LZ4DecompressData();
};


class LZ4DecompressFile : public LZ4BlockStreaming<LZ4DecompressFile> {
	LZ4_streamDecode_t* const lz4StreamDecode;

	int fd;

	char* const data;
	ssize_t data_size;
	size_t data_offset;

	std::string init();
	std::string next();

	friend class LZ4BlockStreaming<LZ4DecompressFile>;

public:
	LZ4DecompressFile(const std::string& filename);

	~LZ4DecompressFile();
};
