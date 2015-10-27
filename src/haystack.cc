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
ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset)
{
	lseek(fd, offset, SEEK_SET);
	ssize_t ret = read(fd, buf, nbyte);
	return ret;
}
#endif

#ifndef HAVE_PWRITE
ssize_t pwrite(int fd, const void* buf, size_t nbyte, off_t offset)
{
	lseek(fd, offset, SEEK_SET);
	ssize_t ret = write(fd, buf, nbyte);
	return ret;
}
#endif


HaystackVolume::HaystackVolume(const std::string& path_, bool writable) :
	data_path(path_ + "/haystack.data"),
	data_file(::open(data_path.c_str(), writable ? O_RDWR | O_CREAT | O_CLOEXEC : O_RDONLY | O_CLOEXEC, 0644))
{
	if (data_file == -1) {
		throw VolumeError();
	}

	off_t real_offset = lseek(data_file, 0, SEEK_END);
	if (real_offset == -1) {
		::close(data_file);
		throw VolumeError();
	}
	if (real_offset == 0) {
		if (ftruncate(data_file, HEADER_SIZE) == -1) {
			::close(data_file);
			throw VolumeError();
		}
		real_offset = lseek(data_file, 0, SEEK_END);
		if (real_offset <= 0) {
			::close(data_file);
			throw VolumeError();
		}
	}
	offset = (real_offset - HEADER_SIZE) / ALIGNMENT;
}


HaystackVolume::~HaystackVolume()
{
	::close(data_file);
}


uint32_t HaystackVolume::get_offset()
{
	return offset;
}


HaystackFile::HaystackFile(const std::shared_ptr<HaystackVolume> &volume_, uint64_t cookie_) :
	volume(volume_),
	offset(volume->get_offset()),
	real_offset(offset * ALIGNMENT + HEADER_SIZE),
	cookie(cookie_),
	total_size(0),
	checksum(0),
	state(open)
{
}


HaystackFile::~HaystackFile()
{
	close();
}


uint32_t HaystackFile::seek(uint32_t offset_)
{
	if (state != open && state != reading) {
		return -1;
	}
	state = reading;
	offset = offset_;
	real_offset = offset * ALIGNMENT + HEADER_SIZE;
	return offset;
}


ssize_t HaystackFile::read(char* data, size_t size)
{
	char buffer[BUFFER_SIZE];
	char* buffer_data = buffer;

	if (offset == volume->get_offset()) {
		return 0;
	}

	state = reading;

	size_t buffer_size = pread(volume->data_file, buffer_data, BUFFER_SIZE, real_offset);
	if (buffer_size < sizeof(NeedleHeader)) {
		return -1;
	}
	real_offset += buffer_size;

	NeedleHeader* header = (NeedleHeader *)(buffer_data);
	buffer_data += sizeof(NeedleHeader);
	buffer_size -= sizeof(NeedleHeader);

	if (header->magic != MAGIC_HEADER) {
		return -2;
	}
	if (header->cookie != cookie) {
		return -3;
	}

	total_size = 0;
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
				return -4;
			}
			buffer_data = buffer;
			buffer_size = pread(volume->data_file, buffer_data, BUFFER_SIZE, real_offset);
			real_offset += buffer_size;
		}
		if (buffer_size < sizeof(uint32_t)) {
			memcpy(buffer, buffer_data, buffer_size);
			buffer_data = buffer;
			buffer_size = pread(volume->data_file, buffer_data + buffer_size, BUFFER_SIZE - buffer_size, real_offset);
			real_offset += buffer_size;
		}
	} while (chunk_size);

	NeedleFooter* footer = (NeedleFooter *)(buffer_data);

	if (footer->magic != MAGIC_FOOTER) {
		return -5;
	}

	return total_size;
}


void HaystackFile::write_header(size_t size)
{
	NeedleHeader header = {
		.magic = MAGIC_HEADER,
		.cookie = cookie,
		.size = size,
	};
	if (pwrite(volume->data_file, &header, sizeof(header), real_offset) != sizeof(header)) {
		throw VolumeError();
	}
	real_offset += sizeof(header);
}


size_t HaystackFile::write_chunk(const char* data, size_t size)
{
	uint32_t chunk_size = size;
	if (pwrite(volume->data_file, &chunk_size, sizeof(uint32_t), real_offset) != sizeof(uint32_t)) {
		throw VolumeError();
	}
	real_offset += sizeof(uint32_t);

	ssize_t written = pwrite(volume->data_file, data, size, real_offset);
	if (written != size) {
		throw VolumeError();
	}
	real_offset += size;

	return size;
}


uint32_t HaystackFile::write_footer()
{
	uint32_t zero = 0;
	if (pwrite(volume->data_file, &zero, sizeof(uint32_t), real_offset) != sizeof(uint32_t)) {
		throw VolumeError();
	}
	real_offset += sizeof(uint32_t);

	NeedleFooter footer = {
		.magic = MAGIC_FOOTER,
		.checksum = checksum,
	};
	if (pwrite(volume->data_file, &footer, sizeof(footer), real_offset) != sizeof(footer)) {
		throw VolumeError();
	}
	real_offset += sizeof(footer);

	// Add padding
	uint32_t new_offset = (real_offset - HEADER_SIZE + ALIGNMENT - 1) / ALIGNMENT;
	if (ftruncate(volume->data_file, HEADER_SIZE + new_offset * ALIGNMENT) == -1) {
		throw VolumeError();
	}

	write_header(total_size);

	fsync(volume->data_file);

	return new_offset;
}


ssize_t HaystackFile::write(const char* data, size_t size)
{
	if (state != open && state != writing) {
		return -1;
	}

	if (offset != volume->get_offset()) {
		state = error;
		return -1;
	}

	off_t rewind_offset = real_offset;

	try {
		if (state == open) {
			state = writing;
			write_header(0);
		}
		size = write_chunk(data, size);
		total_size += size;
	} catch (VolumeError) {
		state = error;
		if (ftruncate(volume->data_file, rewind_offset) == -1) {
			throw VolumeError();
		}
		if ((rewind_offset = lseek(volume->data_file, 0, SEEK_END)) == -1) {
			throw VolumeError();
		}
		offset = (rewind_offset - HEADER_SIZE) / ALIGNMENT;
		throw;
	}

	return size;
}


void HaystackFile::close()
{
	if (state == writing) {
		offset = write_footer();
	}
	state = closed;
}


HaystackIndex::HaystackIndex(const std::string& path_, bool writable) :
	index_path(path_ + "/haystack.index"),
	index_file(::open(index_path.c_str(), writable ? O_RDWR | O_CREAT | O_CLOEXEC : O_RDONLY | O_CLOEXEC, 0644)),
	index_base(0)
{
	if (index_file == -1) {
		throw VolumeError();
	}

	index.reserve(INDEX_CACHE);
}


HaystackIndex::~HaystackIndex()
{
	::close(index_file);
}


uint32_t HaystackIndex::get_offset(uint32_t docid)
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


void HaystackIndex::set_offset(uint32_t docid, uint32_t offset)
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


Haystack::Haystack(const std::string& path_, bool writable) :
	index(std::make_shared<HaystackIndex>(path_, writable)),
	volume(std::make_shared<HaystackVolume>(path_, writable))
{
}


HaystackIndexedFile Haystack::open(uint32_t docid, uint64_t cookie, int mode)
{
	HaystackIndexedFile f(this, docid, cookie);
	if (!(mode & O_APPEND)) {
		uint32_t offset = index->get_offset(docid);
		f.seek(offset);
	}
	return f;
}


HaystackIndexedFile::HaystackIndexedFile(Haystack* haystack, uint32_t docid_, uint64_t cookie_) :
	HaystackFile(haystack->volume, cookie_),
	index(haystack->index),
	docid(docid_)
{
}


HaystackIndexedFile::~HaystackIndexedFile()
{
	close();
}


void HaystackIndexedFile::close()
{
	uint32_t cur_offset = offset;
	HaystackFile::close();
	index->set_offset(docid, cur_offset);
}


#ifdef TESTING_HAYSTACK
// Use test as: c++ -DTESTING_HAYSTACK -std=c++14 -g -x c++ haystack.cc -o test_haystack && ./test_haystack

#include <cassert>
#include <iostream>


void test_haystack()
{
	ssize_t length;

	Haystack writable_haystack(".", true);
	const char data[] = "Hello World";
	HaystackIndexedFile wf = writable_haystack.open(1, 123, O_APPEND);
	length = wf.write(data, sizeof(data));
	wf.close();
	assert(length == sizeof(data));

	Haystack haystack(".");
	char buffer[100];
	HaystackIndexedFile rf = haystack.open(1, 123);
	length = rf.read(buffer, sizeof(buffer));
	assert(length == sizeof(data));

	assert(strncmp(buffer, data, sizeof(buffer)) == 0);
}


int main()
{
	test_haystack();
	return 0;
}

#endif
