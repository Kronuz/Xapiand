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


#define STORAGE_MAGIC 0x12345678
#define STORAGE_BIN_HEADER_MAGIC (STORAGE_MAGIC >> 24)
#define STORAGE_BIN_FOOTER_MAGIC (STORAGE_MAGIC & 0xff)

#define STORAGE_BLOCK_SIZE (1024 * 4)
#define STORAGE_ALIGNMENT 8

#define STORAGE_BUFFER_CLEAR 1
#define STORAGE_BUFFER_CLEAR_CHAR '='

#define STORAGE_LAST_BLOCK_OFFSET (static_cast<off_t>(std::numeric_limits<uint32_t>::max()) * STORAGE_ALIGNMENT)


class StorageException {};

class StorageIOError : public StorageException {};

class StorageEOF : public StorageException {};

class StorageNoFile : public StorageException {};

class StorageCorruptVolume : public StorageException {};
class StorageUUIDMismatch : public StorageCorruptVolume {};
class StorageBadMagicNumber : public StorageCorruptVolume {};
class StorageBadHeaderMagicNumber : public StorageBadMagicNumber {};
class StorageBadBinHeaderMagicNumber : public StorageBadMagicNumber {};
class StorageBadBinFooterMagicNumber : public StorageBadMagicNumber {};
class StorageBadBinChecksum : public StorageCorruptVolume {};
class StorageIncomplete : public StorageCorruptVolume {};
class StorageIncompleteBinHeader : public StorageIncomplete {};
class StorageIncompleteBinData : public StorageIncomplete {};
class StorageIncompleteBinFooter : public StorageIncomplete {};


struct StorageHeader {
	struct StorageHeaderHead {
		uint32_t magic;
		uint16_t offset;
		char uuid[36];
		StorageHeaderHead() : magic(STORAGE_MAGIC), offset(STORAGE_BLOCK_SIZE / STORAGE_ALIGNMENT) {}
	} head;
	char padding[(STORAGE_BLOCK_SIZE - sizeof(StorageHeader::StorageHeaderHead)) / sizeof(char)];
};

#pragma pack(push, 1)
struct StorageBinHeader {
	char magic;
	uint32_t size;
	StorageBinHeader(uint32_t size_) : magic(STORAGE_BIN_HEADER_MAGIC), size(size_) { };
};

struct StorageBinFooter {
	uint32_t crc32;
	char magic;
	StorageBinFooter() : crc32(0), magic(STORAGE_BIN_FOOTER_MAGIC) { };
};
#pragma pack(pop)


template <typename StorageHeader, typename StorageBinHeader, typename StorageBinFooter>
class Storage {
	StorageHeader header;

	std::string path;
	bool writable;
	int fd;
	uint8_t volume;

	char buffer[STORAGE_BLOCK_SIZE];
	uint32_t buffer_offset;

	size_t bin_size;
	StorageBinHeader bin_header;
	StorageBinFooter bin_footer;

public:
	Storage(const std::string& path_, bool writable_)
	: path(path_),
	  writable(writable_),
	  fd(0),
	  volume(0),
	  buffer_offset(0),
	  bin_size(0),
	  bin_header(0) { }

	~Storage() {
		close();
	}

	void open(uint8_t volume_, const char* uuid) {
		close();

#if STORAGE_BUFFER_CLEAR
		if (writable) {
			memset(buffer, STORAGE_BUFFER_CLEAR_CHAR, sizeof(buffer));
		}
#endif

		volume = volume_;
		fd = ::open((path + std::to_string(volume)).c_str(), writable ? O_RDWR | O_DSYNC : O_RDONLY, 0644);
		if (fd == -1) {
			fd = ::open((path + std::to_string(volume)).c_str(), writable ? O_RDWR | O_CREAT | O_DSYNC : O_RDONLY, 0644);
			if (fd == -1) {
				throw StorageIOError();
			}
			memset(&header + sizeof(StorageHeader::StorageHeaderHead), 0, sizeof(header) - sizeof(StorageHeader::StorageHeaderHead));
			strncpy(header.head.uuid, uuid, sizeof(header.head.uuid));
			if (::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
				throw StorageIOError();
			}
		} else {
			ssize_t r = ::pread(fd, &header, sizeof(header), 0);
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(header)) {
				throw StorageIncompleteBinData();
			}
			if (header.head.magic != STORAGE_MAGIC) {
				throw StorageBadHeaderMagicNumber();
			}
			if (strncasecmp(header.head.uuid, uuid, sizeof(header.head.uuid))) {
				throw StorageUUIDMismatch();
			}
			if (writable) {
				buffer_offset = header.head.offset * STORAGE_ALIGNMENT;
				size_t offset = (buffer_offset / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
				buffer_offset -= offset;
				if (::pread(fd, buffer, sizeof(buffer), offset) == -1) {
					throw StorageIOError();
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
			throw StorageEOF();
		}
		if (::lseek(fd, offset * STORAGE_ALIGNMENT, SEEK_SET) == -1) {
			throw StorageNoFile();
		}
	}

	uint32_t write(const char *data, size_t data_size) {
		// FIXME: Compress data here!

		size_t data_size_orig = data_size;

		uint32_t current_offset = header.head.offset;

		StorageBinHeader bin_header(data_size);
		const StorageBinHeader* bin_header_data = &bin_header;
		size_t bin_header_data_size = sizeof(StorageBinHeader);

		StorageBinFooter bin_footer;
		const StorageBinFooter* bin_footer_data = &bin_footer;
		size_t bin_footer_data_size = sizeof(StorageBinFooter);
		off_t block_offset = ((current_offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;

		while (bin_header_data_size || data_size || bin_footer_data_size) {
			if (bin_header_data_size) {
				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > bin_header_data_size) {
					size = bin_header_data_size;
				}
				memcpy(buffer + buffer_offset, bin_header_data, size);
				bin_header_data_size -= size;
				bin_header_data += size;
				buffer_offset += size;
				if (buffer_offset == sizeof(buffer)) {
					buffer_offset = 0;
					goto do_write;
				}
			}

			if (data_size) {
				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
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

			if (bin_footer_data_size) {
				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > bin_footer_data_size) {
					size = bin_footer_data_size;
				}
				memcpy(buffer + buffer_offset, bin_footer_data, size);
				bin_footer_data_size -= size;
				bin_footer_data += size;
				buffer_offset += size;
				if (buffer_offset == sizeof(buffer)) {
					buffer_offset = 0;
					goto do_write;
				}
			}

		do_write:
			if (::pwrite(fd, buffer, sizeof(buffer), block_offset) != sizeof(buffer)) {
				throw StorageIOError();
			}

			if (buffer_offset == 0) {
				block_offset += STORAGE_BLOCK_SIZE;
				if (block_offset >= STORAGE_LAST_BLOCK_OFFSET) {
					throw StorageEOF();
				}
#if STORAGE_BUFFER_CLEAR
				memset(buffer, STORAGE_BUFFER_CLEAR_CHAR, sizeof(buffer));
#endif
			}
		}

		seek(STORAGE_BLOCK_SIZE / STORAGE_ALIGNMENT);

		header.head.offset += (((sizeof(StorageBinHeader) + data_size_orig + sizeof(StorageBinFooter)) + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT);
		return current_offset;
	}

	size_t read(char *buf, size_t buf_size) {
		if (!buf_size) {
			return 0;
		}

		ssize_t r;

		if (!bin_header.size) {
			if (::lseek(fd, 0, SEEK_CUR) >= header.head.offset * STORAGE_ALIGNMENT) {
				throw StorageEOF();
			}

			r = ::read(fd,  &bin_header, sizeof(StorageBinHeader));
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(StorageBinHeader)) {
				throw StorageIncompleteBinHeader();
			}

			if (bin_header.magic != STORAGE_BIN_HEADER_MAGIC) {
				throw StorageBadBinHeaderMagicNumber();
			}
		}

		if (buf_size > bin_header.size - bin_size) {
			buf_size = bin_header.size - bin_size;
		}

		if (buf_size) {
			r = ::read(fd, buf, buf_size);
			if (r == -1) {
				throw StorageIOError();
			} else if (r != buf_size) {
				throw StorageIncompleteBinData();
			}

			bin_size += r;
			// FIXME: bin_checksum update

		} else {
			r = ::read(fd, &bin_footer, sizeof(StorageBinFooter));
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(StorageBinFooter)) {
				throw StorageIncompleteBinFooter();
			}

			if (bin_footer.magic != STORAGE_BIN_FOOTER_MAGIC) {
				throw StorageBadBinFooterMagicNumber();
			}

			// FIXME: Verify whole read bin checksum here
			// throw StorageBadBinChecksum();

			bin_header.size = 0;
			bin_size = 0;
		}

		return bin_size;
	}

	void flush() {
		if (::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
			throw StorageIOError();
		}
	}

	uint32_t write(const std::string& data) {
		return write(data.data(), data.size());

	}

	std::string read() {
		std::string ret;

		ssize_t r;
		char buf[1024];
		while ((r = read(buf, sizeof(buf)))) {
			ret += std::string(buf, r);
		}

		if (r == -1) {
			throw StorageIOError();
		}
		return ret;
	}
};
