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
#define STORAGE_BUFFER_CLEAR_CHAR '\0'

#define STORAGE_BLOCKS_GROW 10
#define STORAGE_BLOCKS_MIN_FREE 3

#define STORAGE_LAST_BLOCK_OFFSET (static_cast<off_t>(std::numeric_limits<uint32_t>::max()) * STORAGE_ALIGNMENT)

#define STORAGE_START_BLOCK_OFFSET (STORAGE_BLOCK_SIZE / STORAGE_ALIGNMENT)


class StorageException { };

class StorageIOError : public StorageException { };

class StorageEOF : public StorageException { };

class StorageNoFile : public StorageException { };

class StorageCorruptVolume : public StorageException { };
class StorageUUIDMismatch : public StorageCorruptVolume { };
class StorageBadMagicNumber : public StorageCorruptVolume { };
class StorageBadHeaderMagicNumber : public StorageBadMagicNumber { };
class StorageBadBinHeaderMagicNumber : public StorageBadMagicNumber { };
class StorageBadBinFooterMagicNumber : public StorageBadMagicNumber { };
class StorageBadBinChecksum : public StorageCorruptVolume { };
class StorageIncomplete : public StorageCorruptVolume { };
class StorageIncompleteBinHeader : public StorageIncomplete { };
class StorageIncompleteBinData : public StorageIncomplete { };
class StorageIncompleteBinFooter : public StorageIncomplete { };


struct StorageHeader {
	struct StorageHeaderHead {
		// uint32_t magic;
		uint16_t offset;  // required
		// char uuid[36];
	} head;
	char padding[(STORAGE_BLOCK_SIZE - sizeof(StorageHeader::StorageHeaderHead)) / sizeof(char)];

	inline void init() {
		head.offset = STORAGE_START_BLOCK_OFFSET;
		// head.magic = STORAGE_MAGIC;
		// strncpy(head.uuid, "00000000-0000-0000-0000-000000000000", sizeof(head.uuid));
	}

	inline void validate() {
		// if (head.magic != STORAGE_MAGIC) {
		// 	throw StorageBadHeaderMagicNumber();
		// }
		// if (strncasecmp(head.uuid, "00000000-0000-0000-0000-000000000000", sizeof(head.uuid))) {
		// 	throw StorageUUIDMismatch();
		// }
	}
};

#pragma pack(push, 1)
struct StorageBinHeader {
	// char magic;
	uint32_t size;  // required

	inline void init(uint32_t size_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
	}

	inline void validate() {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	throw StorageBadBinHeaderMagicNumber();
		// }
	}
};

struct StorageBinFooter {
	// uint32_t checksum;
	// char magic;

	inline void init(uint32_t checksum_) {
		// magic = STORAGE_BIN_FOOTER_MAGIC;
		// checksum = checksum_;
	}

	inline void validate(uint32_t checksum_) {
		// if (magic != STORAGE_BIN_FOOTER_MAGIC) {
		// 	throw StorageBadBinFooterMagicNumber();
		// }
		// if (checksum != checksum_) {
		// 	throw StorageBadBinChecksum();
		// }
	}
};
#pragma pack(pop)


template <typename StorageHeader, typename StorageBinHeader, typename StorageBinFooter>
class Storage {
	std::string path;
	bool writable;
	int fd;

	int free_blocks;

	char buffer[STORAGE_BLOCK_SIZE];
	uint32_t buffer_offset;

	size_t bin_size;
	off_t bin_offset;
	StorageBinHeader bin_header;
	StorageBinFooter bin_footer;

	inline void growfile() {
		if (free_blocks <= STORAGE_BLOCKS_MIN_FREE) {
			off_t file_size = ::lseek(fd, 0, SEEK_END);
			if (file_size == -1) {
				throw StorageIOError();
			}
			free_blocks = (file_size - header.head.offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE;
			if (free_blocks <= STORAGE_BLOCKS_MIN_FREE) {
				size_t new_size = file_size + STORAGE_BLOCKS_GROW * STORAGE_BLOCK_SIZE;
				if (new_size > STORAGE_LAST_BLOCK_OFFSET) {
					new_size = STORAGE_LAST_BLOCK_OFFSET;
				}
				if (new_size > file_size) {
					fallocate(fd, 0, file_size, new_size - file_size);
				}
			}
		}
	}

protected:
	StorageHeader header;

public:
	Storage()
		: writable(false),
		  fd(0),
		  free_blocks(0),
		  buffer_offset(0),
		  bin_size(0),
		  bin_offset(0) { }

	~Storage() {
		close();
	}

	void open(const std::string& path_, bool writable_) {
		close();

		path = path_;
		writable = writable_;

#if STORAGE_BUFFER_CLEAR
		if (writable) {
			memset(buffer, STORAGE_BUFFER_CLEAR_CHAR, sizeof(buffer));
		}
#endif

		fd = ::open(path.c_str(), writable ? O_RDWR | O_DSYNC : O_RDONLY, 0644);
		if (fd == -1) {
			fd = ::open(path.c_str(), writable ? O_RDWR | O_CREAT | O_DSYNC : O_RDONLY, 0644);
			if (fd == -1) {
				throw StorageIOError();
			}

			memset(&header, 0, sizeof(header));
			header.init();

			if (::write(fd, &header, sizeof(header)) != sizeof(header)) {
				throw StorageIOError();
			}
		} else {
			ssize_t r = ::read(fd, &header, sizeof(header));
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(header)) {
				throw StorageIncompleteBinData();
			}
			header.validate();

			if (writable) {
				buffer_offset = header.head.offset * STORAGE_ALIGNMENT;
				size_t offset = (buffer_offset / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
				buffer_offset -= offset;
				if (::pread(fd, buffer, sizeof(buffer), offset) == -1) {
					throw StorageIOError();
				}
			}
		}

		if (writable) {
			growfile();
		}

		seek(STORAGE_START_BLOCK_OFFSET);
	}

	void close() {
		if (fd) {
			flush();
			::close(fd);
		}

		fd = 0;
		free_blocks = 0;
		bin_size = 0;
		bin_offset = 0;
		bin_header.size = 0;
		buffer_offset = 0;
	}

	void seek(uint32_t offset) {
		if (offset > header.head.offset) {
			throw StorageEOF();
		}
		bin_offset = offset * STORAGE_ALIGNMENT;
	}

	uint32_t write(const char *data, size_t data_size) {
		// FIXME: Compress data here!

		size_t data_size_orig = data_size;

		uint32_t current_offset = header.head.offset;

		uint32_t checksum = 0;

		StorageBinHeader bin_header;
		memset(&bin_header, 0, sizeof(bin_header));
		bin_header.init(data_size);
		const StorageBinHeader* bin_header_data = &bin_header;
		size_t bin_header_data_size = sizeof(StorageBinHeader);

		StorageBinFooter bin_footer;
		memset(&bin_footer, 0, sizeof(bin_footer));
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
				bin_footer.init(checksum);

				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > bin_footer_data_size) {
					size = bin_footer_data_size;
				}
				memcpy(buffer + buffer_offset, bin_footer_data, size);
				bin_footer_data_size -= size;
				bin_footer_data += size;
				buffer_offset += size;

				// Align the buffer_offset to the next storage alignment
				buffer_offset = ((block_offset + buffer_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT - block_offset;

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
				--free_blocks;
#if STORAGE_BUFFER_CLEAR
				memset(buffer, STORAGE_BUFFER_CLEAR_CHAR, sizeof(buffer));
#endif
			}
		}

		header.head.offset += (((sizeof(StorageBinHeader) + data_size_orig + sizeof(StorageBinFooter)) + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT);

		return current_offset;
	}

	size_t read(char *buf, size_t buf_size) {
		if (!buf_size) {
			return 0;
		}

		ssize_t r;
		uint32_t checksum = 0;

		if (!bin_header.size) {
			if (::lseek(fd, bin_offset, SEEK_SET) >= header.head.offset * STORAGE_ALIGNMENT) {
				throw StorageEOF();
			}

			r = ::read(fd,  &bin_header, sizeof(StorageBinHeader));
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(StorageBinHeader)) {
				throw StorageIncompleteBinHeader();
			}
			bin_offset += r;
			bin_header.validate();
		}

		if (buf_size > bin_header.size - bin_size) {
			buf_size = bin_header.size - bin_size;
		}

		if (buf_size) {
			r = ::read(fd, buf, buf_size);
			if (r == -1) {
				throw StorageIOError();
			} else if (static_cast<size_t>(r) != buf_size) {
				throw StorageIncompleteBinData();
			}
			bin_offset += r;

			bin_size += r;

			checksum = 0;  // FIXME: bin_checksum update

		} else {
			r = ::read(fd, &bin_footer, sizeof(StorageBinFooter));
			if (r == -1) {
				throw StorageIOError();
			} else if (r != sizeof(StorageBinFooter)) {
				throw StorageIncompleteBinFooter();
			}
			bin_offset += r;
			bin_footer.validate(checksum);

			// Align the bin_offset to the next storage alignment
			bin_offset = ((bin_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT;

			bin_header.size = 0;
			bin_size = 0;
		}

		return bin_size;
	}

	void flush() {
		if (::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
			throw StorageIOError();
		}
		growfile();
	}

	inline uint32_t write(const std::string& data) {
		return write(data.data(), data.size());

	}

	inline std::string read() {
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
