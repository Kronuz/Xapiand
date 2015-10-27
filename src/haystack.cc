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

#include "haystack.h"

#include <fcntl.h>
#include <unistd.h>

#define ALIGNMENT 8
#define HEADER_SIZE 33

#define BUFFER_SIZE (ALIGNMENT * 1024)
#define INDEX_CACHE (1024 * 1024)
#define MAGIC_HEADER 0x0010091a15f74bff
#define MAGIC_FOOTER 0x0048391831d0dfff

#define FLAG_DELETED (1 << 0)


#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset)
{
	off_t cur = lseek(fd, 0, SEEK_CUR);
	lseek(fd, offset, SEEK_SET);
	ssize_t ret = read(fd, buf, nbyte);
	lseek(fd, cur, SEEK_SET);
	return ret;
}
#endif

#ifndef HAVE_PWRITE
ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
{
	off_t cur = lseek(fd, 0, SEEK_CUR);
	lseek(fd, offset, SEEK_SET);
	ssize_t ret = write(fd, buf, nbyte);
	lseek(fd, cur, SEEK_SET);
	return ret;
}
#endif


struct VolumeError : std::exception
{
};


Haystack::Haystack(const std::string &path_) :
	index_path(path_ + "/haystack.index"),
	data_path(path_ + "/haystack.data"),
	index_file(open(index_path.c_str(), O_RDONLY | O_CLOEXEC)),
	data_file(open(data_path.c_str(), O_RDONLY | O_CLOEXEC)),
	index_base(0)
{
	index.reserve(INDEX_CACHE);
}


Haystack::~Haystack()
{
	if (index_file) close(index_file);
	if (data_file) close(data_file);
}


uint32_t Haystack::get_offset(uint32_t docid)
{
	size_t docpos = docid % INDEX_CACHE;
	uint32_t new_index_base = docid / INDEX_CACHE;

	if (index_base != new_index_base || docpos > index.size()) {
		if (!index_file) {
			return -1;
		}
		index_base = new_index_base;
		size_t index_offset = index_base * sizeof(uint32_t) + HEADER_SIZE;
		index.resize(INDEX_CACHE);
		ssize_t size = pread(index_file, index.data(), INDEX_CACHE * sizeof(uint32_t), index_offset) / sizeof(uint32_t);
		index.resize(size);
	}

	return index[docpos];
}


ssize_t Haystack::read(uint32_t docid, uint64_t cookie, char *data, size_t size)
{
	char buffer[BUFFER_SIZE];
	char* buffer_data = buffer;

	uint32_t offset = get_offset(docid);
	if (offset == static_cast<uint32_t>(-1)) {
		return 0;
	}

	if (!data_file) {
		return 0;
	}

	off_t real_offset = offset * ALIGNMENT + HEADER_SIZE;

	size_t buffer_size = pread(data_file, buffer_data, BUFFER_SIZE, real_offset);
	if (buffer_size < sizeof(NeedleHeader)) {
		return -1;
	}
	real_offset += buffer_size;

	NeedleHeader* header = (NeedleHeader *)(buffer_data);
	buffer_data += sizeof(NeedleHeader);
	buffer_size -= sizeof(NeedleHeader);

	if (header->magic != MAGIC_HEADER) {
		return -1;
	}
	if (header->cookie != cookie) {
		return -1;
	}

	size_t total_size = 0;
	size_t this_size = size;

	uint32_t chunk_size;
	do {
		chunk_size = *(uint32_t *)buffer_data;
		buffer_data += sizeof(uint32_t);
		buffer_size -= sizeof(uint32_t);

		if (!chunk_size) {
			break;
		}

		if (this_size > chunk_size) {
			this_size = chunk_size;
		}

		while (this_size) {
			size_t current_size = buffer_size;
			if (current_size > this_size) {
				current_size = this_size;
			}
			if (current_size) {
				if (total_size + current_size > size) {
					current_size = size - total_size;
					memcpy(data, buffer_data, current_size);
					return total_size; // Cannot read whole file into the buffer!
				}
				memcpy(data, buffer_data, current_size);
				data += current_size;
				this_size -= current_size;
				total_size += current_size;
				if (!this_size) {
					buffer_data += current_size;
					break;
				}
			} else {
				return -1;
			}
			buffer_data = buffer;
			buffer_size = pread(data_file, buffer_data, BUFFER_SIZE, real_offset);
			real_offset += buffer_size;
		}
		if (buffer_size < sizeof(uint32_t)) {
			memcpy(buffer, buffer_data, buffer_size);
			buffer_data = buffer;
			buffer_size = pread(data_file, buffer_data + buffer_size, BUFFER_SIZE - buffer_size, real_offset);
			real_offset += buffer_size;
		}

	} while (chunk_size);

	NeedleFooter* footer = (NeedleFooter *)(buffer_data);

	if (footer->magic != MAGIC_FOOTER) {
		return -1;
	}

	return total_size;
}


WritableHaystack::WritableHaystack(const std::string &path_) :
	Haystack(path_)
{
	// FIXME: Validate last needle, if it's wrong, step back
	if (index_file) close(index_file);
	if (data_file) close(data_file);

	index_file = open(index_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);  // FIXME: This should be exclusive append
	if (index_file == -1) {
		throw VolumeError();
	}

	data_file = open(data_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);  // FIXME: This should be exclusive append
	if (data_file == -1) {
		close(index_file);
		throw VolumeError();
	}

	off_t real_offset = lseek(data_file, 0, SEEK_END);
	if (real_offset == -1) {
		close(index_file);
		close(data_file);
		throw VolumeError();
	}
	if (real_offset == 0) {
		if (ftruncate(index_file, HEADER_SIZE) == -1) {
			close(index_file);
			close(data_file);
			throw VolumeError();
		}
		if (ftruncate(data_file, HEADER_SIZE) == -1) {
			close(index_file);
			close(data_file);
			throw VolumeError();
		}
		real_offset = lseek(data_file, 0, SEEK_END);
		if (real_offset <= 0) {
			close(index_file);
			close(data_file);
			throw VolumeError();
		}
	}
	offset = (real_offset - HEADER_SIZE) / ALIGNMENT;
}


void WritableHaystack::set_offset(uint32_t docid, uint32_t offset)
{
	get_offset(docid);  // Open/udpate index if needed

	size_t docpos = docid % INDEX_CACHE;
	if (index.size() < docpos) {
		index.resize(docpos + 1);
	}
	index[docpos] = offset;

	size_t index_offset = index_base * sizeof(uint32_t) + HEADER_SIZE;

	// The following should be asynchronous:
	pwrite(index_file, index.data(), index.size() * sizeof(uint32_t), index_offset);
	fsync(index_file);
}


void WritableHaystack::write_header(uint64_t cookie, size_t size)
{
	NeedleHeader header = {
		.magic = MAGIC_HEADER,
		.cookie = cookie,
		.size = size,
	};

	if (::write(data_file, &header, sizeof(header)) != sizeof(header)) {
		throw VolumeError();
	}
}


ssize_t WritableHaystack::write_chunk(const char *data, size_t size)
{
	uint32_t chunk_size = size;

	if (::write(data_file, &chunk_size, sizeof(uint32_t)) != sizeof(uint32_t)) {
		throw VolumeError();
	}

	ssize_t total_size = ::write(data_file, data, size);
	if (total_size != size) {
		throw VolumeError();
	}

	return total_size;
}


uint32_t WritableHaystack::write_footer(size_t total_size, uint64_t checksum)
{
	off_t real_offset = offset * ALIGNMENT + HEADER_SIZE;

	uint32_t zero = 0;
	if (::write(data_file, &zero, sizeof(uint32_t)) != sizeof(uint32_t)) {
		throw VolumeError();
	}

	NeedleFooter footer = {
		.magic = MAGIC_FOOTER,
		.checksum = checksum,
	};
	if (::write(data_file, &footer, sizeof(footer)) != sizeof(footer)) {
		throw VolumeError();
	}

	// Add padding
	uint32_t new_offset = (real_offset - HEADER_SIZE + sizeof(NeedleHeader) + total_size + sizeof(NeedleFooter) + ALIGNMENT - 1) / ALIGNMENT;
	if (ftruncate(data_file, HEADER_SIZE + new_offset * ALIGNMENT) == -1) {
		throw VolumeError();
	}

	fsync(data_file);

	return new_offset;
}


ssize_t WritableHaystack::write(uint32_t docid, uint64_t cookie, const char *data, size_t size)
{
	off_t real_offset = offset * ALIGNMENT + HEADER_SIZE;

	ssize_t total_size = 0;

	try {
		write_header(cookie, size);
		total_size += write_chunk(data, size);
		uint32_t new_offset = write_footer(total_size, 0);

		set_offset(docid, offset);
		offset = new_offset;
	} catch (VolumeError) {
		if (ftruncate(data_file, real_offset) == -1) {
			throw VolumeError();
		}
		if ((real_offset = lseek(data_file, 0, SEEK_END)) == -1) {
			throw VolumeError();
		}
		offset = (real_offset - HEADER_SIZE) / ALIGNMENT;

		throw;
	}

	return total_size;
}


#ifdef TESTING_HAYSTACK
// Use test as: c++ -DTESTING_HAYSTACK -std=c++14 -g -x c++ haystack.cc -o test_haystack && ./test_haystack

#include <cassert>
#include <iostream>


void test_haystack(){
	ssize_t length;

	WritableHaystack wh(".");
	const char data[] = "Hello World";
	length = wh.write(1, 123, data, sizeof(data));
	assert(length == sizeof(data));

	Haystack rh(".");
	char buffer[100];
	length = rh.read(1, 123, buffer, sizeof(buffer));
	assert(length == sizeof(data));

	assert(strncmp(buffer, data, sizeof(buffer)) == 0);
}


int main()
{
	test_haystack();
	return 0;
}

#endif
