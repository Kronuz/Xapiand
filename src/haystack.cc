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
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define ALIGNMENT 8
#define HEADER_SIZE 12

#define BUFFER_SIZE (ALIGNMENT * 1024)
#define INDEX_CACHE (1024 * 1024)
#define MAGIC_HEADER 0x4b535948
#define MAGIC_FOOTER 0x4859534b

#define FLAG_DELETED (1 << 0)


HaystackVolume::HaystackVolume(const std::string& path_, bool writable)
	: data_path(path_ + "/haystack.data"),
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
	eof_offset = static_cast<offset_t>((real_offset - HEADER_SIZE) / ALIGNMENT);
}


HaystackVolume::~HaystackVolume()
{
	::close(data_file);
}


offset_t
HaystackVolume::offset()
{
	return eof_offset;
}


HaystackFile::HaystackFile(const std::shared_ptr<HaystackVolume> &volume_, did_t id_, cookie_t cookie_) :
	header({
		.head = {
			.magic = MAGIC_HEADER,
			.id = id_,
			.cookie = cookie_,
			.size = 0,
		},
		.chunk_size = 0,
	}),
	footer({
		.foot = {
			.magic = MAGIC_FOOTER,
			.checksum = 0,
		}
	}),
	buffer(nullptr),
	buffer_size(0),
	available_buffer(0),
	next_chunk_size(0),
	wanted_id(id_),
	wanted_cookie(cookie_),
	volume(volume_),
	current_offset(volume->offset()),
	real_offset(current_offset * ALIGNMENT + HEADER_SIZE),
	state(State::OPENED) { }


HaystackFile::~HaystackFile()
{
	delete []buffer;
}


did_t
HaystackFile::id()
{
	return header.head.id;
}


size_t
HaystackFile::size()
{
	return header.head.size;
}


offset_t
HaystackFile::offset()
{
	return current_offset;
}


cookie_t
HaystackFile::cookie()
{
	return header.head.cookie;
}


checksum_t
HaystackFile::checksum()
{
	return footer.foot.checksum;
}


offset_t
HaystackFile::_seek(offset_t offset)
{
	state = State::OPENED;
	current_offset = offset;
	real_offset = current_offset * ALIGNMENT + HEADER_SIZE;
	return current_offset;
}


offset_t
HaystackFile::seek(offset_t offset)
{
	if (state != State::H_EOF && state != State::OPENED && state != State::READING) {
		errno = EBADSTATE;
		return -1;
	}
	return _seek(offset);
}


offset_t
HaystackFile::next()
{
	// End of file, go to next file
	return seek(static_cast<offset_t>((real_offset - HEADER_SIZE + ALIGNMENT - 1) / ALIGNMENT));
}


offset_t
HaystackFile::rewind()
{
	offset_t offset = _seek(current_offset);

	if (state == State::WRITING) {
		if (ftruncate(volume->data_file, real_offset) == -1) {
			throw VolumeError();
		}
		if ((real_offset = lseek(volume->data_file, 0, SEEK_END)) == -1) {
			throw VolumeError();
		}
	}
	return offset;
}


ssize_t
HaystackFile::read(char* data, size_t size)
{
	if (state != State::H_EOF && state != State::OPENED && state != State::READING) {
		errno = EBADSTATE;
		return -1;
	}

	if (current_offset == volume->offset()) {
		errno = EOF;
		return -1;
	}

	if (state == State::H_EOF) {
		return 0;
	}

	if (state == State::OPENED) {
		state = State::READING;

		ssize_t header_size = sizeof(NeedleHeader::Head) + sizeof(chunk_size_t);
		if (pread(volume->data_file, &header, header_size, real_offset) != header_size) {
			state = State::ERROR;
			errno = EOFH;
			return -1;
		}
		real_offset += header_size;

		if (header.head.magic != MAGIC_HEADER) {
			state = State::ERROR;
			errno = ECORRUPTH;
			return -1;
		}
		if (wanted_id != 0 && header.head.id != wanted_id) {
			state = State::ERROR;
			errno = EBADID;
			return -1;
		}
		if (wanted_cookie != 0 && header.head.cookie != wanted_cookie) {
			state = State::ERROR;
			errno = EBADCOOKIE;
			return -1;
		}

		next_chunk_size = header.chunk_size;
	}

	ssize_t ret_size = 0;

	while (size) {
		if (!buffer || !available_buffer) {
			if (!next_chunk_size) {
				break;
			}
			available_buffer = next_chunk_size + sizeof(chunk_size_t);
			if (available_buffer > buffer_size) {
				delete []buffer;
				buffer_size = available_buffer;
				buffer = new char[buffer_size];
			}
			if (pread(volume->data_file, buffer, available_buffer, real_offset) != available_buffer) {
				state = State::ERROR;
				errno = EOFB;
				return -1;
			}
			real_offset += available_buffer;
			available_buffer -= sizeof(chunk_size_t);
			next_chunk_size = *(chunk_size_t*)(buffer + available_buffer);
		}

		ssize_t current_size = size;
		if (current_size > available_buffer) {
			current_size = available_buffer;
		}
		memcpy(data, buffer, current_size);
		available_buffer -= current_size;
		size -= current_size;
		ret_size += current_size;
	}

	if (!ret_size && !available_buffer && !next_chunk_size) {
		if (pread(volume->data_file, &footer.foot, sizeof(NeedleFooter::Foot), real_offset) != sizeof(NeedleFooter::Foot)) {
			state = State::ERROR;
			errno = EOFF;
			return -1;
		}
		real_offset += sizeof(NeedleFooter::Foot);

		if (footer.foot.magic != MAGIC_FOOTER) {
			state = State::ERROR;
			errno = ECORRUPTF;
			return -1;
		}
		// if (footer.checksum != checksum) {
		// 	state = State::ERROR;
		// 	errno = EBADCHECKSUM;
		// 	return -1;
		// }

		state = State::H_EOF;
	}

	return ret_size;
}


void
HaystackFile::write_header(size_t size)
{
	real_offset = current_offset * ALIGNMENT + HEADER_SIZE; // Header *always* starts at offset!
	header.head.size = size;
	if (pwrite(volume->data_file, &header.head, sizeof(NeedleHeader::Head), real_offset) != sizeof(NeedleHeader::Head)) {
		throw VolumeError();
	}
	real_offset += sizeof(NeedleHeader::Head);
}


size_t
HaystackFile::write_chunk(const char* data, size_t size)
{
	chunk_size_t chunk_size = static_cast<chunk_size_t>(size);
	if (pwrite(volume->data_file, &chunk_size, sizeof(chunk_size_t), real_offset) != sizeof(chunk_size_t)) {
		throw VolumeError();
	}
	real_offset += sizeof(chunk_size_t);

	ssize_t written = pwrite(volume->data_file, data, size, real_offset);
	if (written != static_cast<ssize_t>(size)) {
		throw VolumeError();
	}
	real_offset += size;

	return size;
}


offset_t
HaystackFile::write_footer()
{
	chunk_size_t zero = 0;
	if (pwrite(volume->data_file, &zero, sizeof(chunk_size_t), real_offset) != sizeof(chunk_size_t)) {
		throw VolumeError();
	}
	real_offset += sizeof(chunk_size_t);

	if (pwrite(volume->data_file, &footer, sizeof(NeedleFooter::Foot), real_offset) != sizeof(NeedleFooter::Foot)) {
		throw VolumeError();
	}
	real_offset += sizeof(NeedleFooter::Foot);

	// Add padding
	offset_t new_offset = static_cast<offset_t>((real_offset - HEADER_SIZE + ALIGNMENT - 1) / ALIGNMENT);
	if (ftruncate(volume->data_file, HEADER_SIZE + new_offset * ALIGNMENT) == -1) {
		throw VolumeError();
	}

	write_header(header.head.size);

	fsync(volume->data_file);

	return new_offset;
}


ssize_t
HaystackFile::write(const char* data, size_t size)
{
	if (state != State::OPENED && state != State::WRITING) {
		errno = EBADSTATE;
		return -1;
	}

	if (!header.head.id) {
		errno = ENOID;
		return -1;
	}

	if (current_offset != volume->offset()) {
		state = State::ERROR;
		errno = EPOS;
		return -1;
	}

	try {
		if (state == State::OPENED) {
			state = State::WRITING;
			write_header(0);
		}
		size = write_chunk(data, size);
		header.head.size += size;
	} catch (VolumeError) {
		rewind();
		state = State::ERROR;
		throw;
	}

	return size;
}


offset_t
HaystackFile::commit()
{
	offset_t offset = current_offset;
	if (state == State::WRITING) {
		current_offset = write_footer();
	}
	state = State::OPENED;
	return offset;
}


HaystackIndex::HaystackIndex(const std::string& path_, bool writable)
	: index_path(path_ + "/haystack.index"),
	  index_file(::open(index_path.c_str(), writable ? O_RDWR | O_CREAT | O_CLOEXEC : O_RDONLY | O_CLOEXEC, 0644)),
	  index_base(0),
	  index_touched(-1)
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


offset_t
HaystackIndex::get_offset(did_t id)
{
	size_t docpos = id % INDEX_CACHE;
	did_t new_index_base = id / INDEX_CACHE;

	if (index_base != new_index_base || docpos > index.size()) {
		flush();
		index_base = new_index_base;
		size_t index_offset = index_base * sizeof(did_t) + HEADER_SIZE;
		index.resize(INDEX_CACHE);
		ssize_t size = pread(index_file, index.data(), INDEX_CACHE * sizeof(did_t), index_offset) / sizeof(did_t);
		index.resize(size);
	}

	return index[docpos];
}


void
HaystackIndex::set_offset(did_t id, offset_t offset)
{
	offset_t old_offset = get_offset(id);  // Open/udpate index if needed
	if (old_offset != offset) {
		size_t docpos = id % INDEX_CACHE;
		if (index.size() < docpos) {
			index.resize(docpos + 1);
		}
		index[docpos] = offset;
		index_touched = index_base;
	}
}


void
HaystackIndex::flush()
{
	if (index_touched == index_base) {
		index_touched = -1;
		size_t index_offset = index_base * sizeof(offset_t) + HEADER_SIZE;
		pwrite(index_file, index.data(), index.size() * sizeof(offset_t), index_offset);
		fsync(index_file);
	}
}


Haystack::Haystack(const std::string& path_, bool writable)
	: index(std::make_shared<HaystackIndex>(path_, writable)),
	  volume(std::make_shared<HaystackVolume>(path_, writable)) { }


HaystackIndexedFile
Haystack::open(did_t id, cookie_t cookie, int mode)
{
	HaystackIndexedFile f(this, id, cookie);
	if (!(mode & O_APPEND)) {
		offset_t offset = index->get_offset(id);
		f.seek(offset);
	}
	return f;
}


void
Haystack::flush()
{
	index->flush();
}


HaystackIndexedFile::HaystackIndexedFile(Haystack* haystack, did_t id_, cookie_t cookie_)
	: HaystackFile(haystack->volume, id_, cookie_),
	  index(haystack->index) { }


offset_t
HaystackIndexedFile::commit()
{
	offset_t offset = HaystackFile::commit();
	index->set_offset(id(), offset);
	index->flush();
	return offset;
}
