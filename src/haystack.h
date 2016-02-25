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

#include "xapiand.h"

#include <fcntl.h>
#include <string>
#include <unistd.h>

#include <limits>


#define HAYSTACK_MAGIC 0x123456

#define HAYSTACK_BLOCK_SIZE (1024 * 4)
#define HAYSTACK_ALIGNMENT 8

#define HAYSTACK_BUFFER_CLEAR 1
#define HAYSTACK_BUFFER_CLEAR_CHAR '='

#define HAYSTACK_LAST_BLOCK_OFFSET (static_cast<off_t>(std::numeric_limits<uint32_t>::max()) * HAYSTACK_ALIGNMENT)


struct HaystackHeader {
	struct head_t {
		uint32_t magic;
		uint16_t offset;
		char uuid[36];
	} head;
	char padding[(HAYSTACK_BLOCK_SIZE - sizeof(head_t)) / sizeof(char)];
};
struct HaystackNeedleHeader {
	uint32_t size;
	HaystackNeedleHeader(uint32_t size_) : size(size_) { };
};
struct HaystackNeedleFooter {
	uint32_t crc32;
	HaystackNeedleFooter() : crc32(0) { };
};


template <typename HaystackHeader, typename HaystackNeedleHeader, typename HaystackNeedleFooter>
class Haystack {
	HaystackHeader header;

	std::string path;
	bool writable;
	int fd;
	uint8_t volume;

	char buffer[HAYSTACK_BLOCK_SIZE];
	uint32_t buffer_offset;

	size_t needle_total;
	HaystackNeedleHeader needle_header;
	HaystackNeedleFooter needle_footer;

public:
	Haystack(const std::string& path_, bool writable_)
	: path(path_),
	  writable(writable_),
	  fd(0),
	  volume(0),
	  buffer_offset(0),
	  needle_total(0),
	  needle_header(0) { }

	~Haystack() {
		close();
	}

	void open(uint8_t volume_, const char* uuid) {
		close();

#if HAYSTACK_BUFFER_CLEAR
		if (writable) {
			memset(buffer, HAYSTACK_BUFFER_CLEAR_CHAR, sizeof(buffer));
		}
#endif

		volume = volume_;
		fd = ::open((path + std::to_string(volume)).c_str(), writable ? O_RDWR | O_DSYNC : O_RDONLY, 0644);
		if (fd == -1) {
			fd = ::open((path + std::to_string(volume)).c_str(), writable ? O_RDWR | O_CREAT | O_DSYNC : O_RDONLY, 0644);
			if (fd == -1) {
				throw std::exception();
			}
			memset(&header, 0, sizeof(header));
			header.head.magic = HAYSTACK_MAGIC;
			header.head.offset = HAYSTACK_BLOCK_SIZE / HAYSTACK_ALIGNMENT;
			strcpy(header.head.uuid, uuid);  // FIXME
			if (::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
				throw std::exception(/* Cannot write to volume */);
			}
		} else {
			ssize_t r = ::pread(fd, &header, sizeof(header), 0);
			if (r == -1) {
				throw std::exception(/* Cannot read from volume */);
			} else if (r != sizeof(header)) {
				throw std::exception(/* Corrupt File: Cannot read data */);
			}
			if (header.head.magic != HAYSTACK_MAGIC) {
				throw std::exception(/* Corrupt file: Invalid magic number */);
			}
			if (strncasecmp(header.head.uuid, uuid, sizeof(header.head.uuid))) {
				throw std::exception(/* Corrupt file: UUID mismatch */);
			}
			if (writable) {
				buffer_offset = header.head.offset * HAYSTACK_ALIGNMENT;
				size_t offset = (buffer_offset / HAYSTACK_BLOCK_SIZE) * HAYSTACK_BLOCK_SIZE;
				buffer_offset -= offset;
				if (::pread(fd, buffer, sizeof(buffer), offset) == -1) {
					throw std::exception(/* Cannot read from volume */);
				}
			}
		}
	}

	void close() {
		if (fd) {
			flush();
			::close(fd);
		}
	}

	void seek(uint32_t offset) {
		if (offset > header.head.offset) {
			throw std::exception(/* Beyond EOF */);
		}
		if (::lseek(fd, offset * HAYSTACK_ALIGNMENT, SEEK_SET) == -1) {
			throw std::exception(/* No open file */);
		}
	}

	void write(const char *data, size_t data_size) {
		// FIXME: Compress data here!

		size_t data_size_orig = data_size;

		HaystackNeedleHeader needle_header(data_size);
		const HaystackNeedleHeader* needle_header_data = &needle_header;
		size_t needle_header_data_size = sizeof(HaystackNeedleHeader);

		HaystackNeedleFooter needle_footer;
		const HaystackNeedleFooter* needle_footer_data = &needle_footer;
		size_t needle_footer_data_size = sizeof(HaystackNeedleFooter);
		off_t block_offset = ((header.head.offset * HAYSTACK_ALIGNMENT) / HAYSTACK_BLOCK_SIZE) * HAYSTACK_BLOCK_SIZE;

		while (needle_header_data_size || data_size || needle_footer_data_size) {
			if (needle_header_data_size) {
				size_t size = HAYSTACK_BLOCK_SIZE - buffer_offset;
				if (size > needle_header_data_size) {
					size = needle_header_data_size;
				}
				memcpy(buffer + buffer_offset, needle_header_data, size);
				needle_header_data_size -= size;
				needle_header_data += size;
				buffer_offset += size;
				if (buffer_offset == sizeof(buffer)) {
					buffer_offset = 0;
					goto do_write;
				}
			}

			if (data_size) {
				size_t size = HAYSTACK_BLOCK_SIZE - buffer_offset;
				if (size > data_size) {
					size = data_size;
				}
				memcpy(buffer + buffer_offset, data, size);
				data_size -= size;
				data += size;
				buffer_offset += size;
				if (buffer_offset == sizeof(buffer)) {
					buffer_offset = 0;
					goto do_write;
				}
			}

			if (needle_footer_data_size) {
				size_t size = HAYSTACK_BLOCK_SIZE - buffer_offset;
				if (size > needle_footer_data_size) {
					size = needle_footer_data_size;
				}
				memcpy(buffer + buffer_offset, needle_footer_data, size);
				needle_footer_data_size -= size;
				needle_footer_data += size;
				buffer_offset += size;
				if (buffer_offset == sizeof(buffer)) {
					buffer_offset = 0;
					goto do_write;
				}
			}

		do_write:
			if (::pwrite(fd, buffer, sizeof(buffer), block_offset) != sizeof(buffer)) {
				throw std::exception(/* Cannot write to volume */);
			}

			if (buffer_offset == 0) {
				block_offset += HAYSTACK_BLOCK_SIZE;
				if (block_offset >= HAYSTACK_LAST_BLOCK_OFFSET) {
					throw std::exception(/* EOF */);
				}
#if HAYSTACK_BUFFER_CLEAR
				memset(buffer, HAYSTACK_BUFFER_CLEAR_CHAR, sizeof(buffer));
#endif
			}
		}

		seek(HAYSTACK_BLOCK_SIZE / HAYSTACK_ALIGNMENT);

		header.head.offset += (((sizeof(HaystackNeedleHeader) + data_size_orig + sizeof(HaystackNeedleFooter)) + HAYSTACK_ALIGNMENT - 1) / HAYSTACK_ALIGNMENT);
	}

	size_t read(char *buf, size_t buf_size) {
		if (!buf_size) {
			return 0;
		}

		ssize_t r;

		if (!needle_header.size) {
			if (::lseek(fd, 0, SEEK_CUR) >= header.head.offset * HAYSTACK_ALIGNMENT) {
				throw std::exception(/* EOF */);
			}

			r = ::read(fd,  &needle_header, sizeof(HaystackNeedleHeader));
			if (r == -1) {
				throw std::exception(/* Cannot read from volume */);
			} else if (r != sizeof(HaystackNeedleHeader)) {
				throw std::exception(/* Corrupt File: Cannot read needle header */);
			}
		}

		if (buf_size > needle_header.size - needle_total) {
			buf_size = needle_header.size - needle_total;
		}

		if (buf_size) {
			r = ::read(fd, buf, buf_size);
			if (r == -1) {
				throw std::exception(/* Cannot read from volume */);
			} else if (r != buf_size) {
				throw std::exception(/* Corrupt File: Cannot read data */);
			}

			needle_total += r;
			// FIXME: needle_checksum update

		} else {
			r = ::read(fd, &needle_footer, sizeof(HaystackNeedleFooter));
			if (r == -1) {
				throw std::exception(/* Cannot read from volume */);
			} else if (r != sizeof(HaystackNeedleFooter)) {
				throw std::exception(/* Corrupt File: Cannot read needle footer */);
			}

			// FIXME: Verify whole read needle checksum here

			needle_header.size = 0;
			needle_total = 0;
		}

		return needle_total;
	}

	void flush() {
		if (::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
			throw std::exception(/* Cannot write to volume */);
		}
	}

	void write(const std::string& data) {
		write(data.data(), data.size());

	}

	std::string read() {
		std::string ret;

		ssize_t r;
		char buf[1024];
		while ((r = read(buf, sizeof(buf)))) {
			ret += std::string(buf, r);
		}

		if (r == -1) {
			throw std::exception(/* Cannot read from volume */);
		}
		return ret;
	}
};
