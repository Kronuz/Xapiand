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

#include <cstddef>             // for std::size_t
#include <string>              // for std::string
#include "string_view.hh"      // for std::string_view

#include "cassert.h"           // for ASSERT
#include "io.hh"               // for io::*

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

class Buffer {
	std::string _data;
	std::string_view _data_view;

	std::string _path;
	int _fd;
	bool _unlink;
	std::size_t _max_pos;

	void feed() {
		if (_fd != -1 && _data_view.empty() && pos < _max_pos) {
			_data.resize(4096);
			io::lseek(_fd, pos, SEEK_SET);
			auto _read = io::read(_fd, &_data[0], 4096UL);
			if (_read > 0) {
				_data.resize(_read);
			}
			_data_view = _data;
		}
	}

public:
	std::size_t pos;
	char type;

	Buffer()
		: _data_view(_data),
		  _fd(-1),
		  _unlink(false),
		  _max_pos(0),
		  pos(0),
		  type('\xff')
	{ }

	Buffer(int fd)
		: _fd(fd),
		  _unlink(false),
		  _max_pos(io::lseek(_fd, 0, SEEK_END)),
		  pos(0),
		  type('\0')
	{ }

	Buffer(std::string_view path, bool unlink = false)
		: _path(path),
		  _fd(io::open(_path.c_str())),
		  _unlink(unlink),
		  _max_pos(io::lseek(_fd, 0, SEEK_END)),
		  pos(0),
		  type('\0')
	{ }

	Buffer(char type, const char *bytes, std::size_t nbytes)
		: _data(bytes, nbytes),
		  _data_view(_data),
		  _fd(-1),
		  _unlink(false),
		  _max_pos(nbytes),
		  pos(0),
		  type(type)
	{ }

	~Buffer() {
		if (_fd != -1) {
			io::close(_fd);
			if (!_path.empty() && _unlink) {
				io::unlink(_path.c_str());
			}
		}
	}

	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&&) = default;
	Buffer& operator=(Buffer&&) = default;

	const char *data() {
		feed();
		return _data_view.data();
	}

	std::size_t size() {
		feed();
		return std::min(_max_pos, _data_view.size());
	}

	std::size_t full_size() {
		feed();
		return std::max(_max_pos, _data_view.size());
	}

	void remove_prefix(std::size_t n) {
		ASSERT(n <= _data_view.size());
		_data_view.remove_prefix(n);
		pos += n;
	}

	// Legacy:

	const char *dpos() {
		return _data.data() + pos;
	}

	std::size_t nbytes() {
		return _data.size() - pos;
	}
};
