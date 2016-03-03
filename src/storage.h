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

#include "io_utils.h"
#include "lz4_compressor.h"

#include <unistd.h>

#include <limits>


#define STORAGE_MAGIC 0x02DEBC47
#define STORAGE_BIN_HEADER_MAGIC 0x2A
#define STORAGE_BIN_FOOTER_MAGIC 0x42

#define STORAGE_BLOCK_SIZE (1024 * 4)
#define STORAGE_ALIGNMENT 8

#define STORAGE_BUFFER_CLEAR 1
#define STORAGE_BUFFER_CLEAR_CHAR '\0'

#define STORAGE_BLOCKS_GROW 8
#define STORAGE_BLOCKS_MIN_FREE 2

#define STORAGE_LAST_BLOCK_OFFSET (static_cast<off_t>(std::numeric_limits<uint32_t>::max()) * STORAGE_ALIGNMENT)

#define STORAGE_START_BLOCK_OFFSET (STORAGE_BLOCK_SIZE / STORAGE_ALIGNMENT)


constexpr int STORAGE_OPEN           = 0x00;  // Open an existing database.
constexpr int STORAGE_WRITABLE       = 0x01;  // Opens as writable.
constexpr int STORAGE_CREATE         = 0x02;  // Automatically creates the database if it doesn't exist
constexpr int STORAGE_CREATE_OR_OPEN = 0x03;  // Create database if it doesn't already exist.
constexpr int STORAGE_NO_SYNC        = 0x04;  // Don't attempt to ensure changes have hit disk.
constexpr int STORAGE_FULL_SYNC      = 0x08;  // Try to ensure changes are really written to disk.
constexpr int STORAGE_COMPRESS       = 0x10;  // Compress data in storage.


class StorageException : public Error {
public:
	template<typename... Args>
	StorageException(Args&&... args) : Error(std::forward<Args>(args)...) { }
};

#define MSG_StorageException(...) StorageException(__FILE__, __LINE__, __VA_ARGS__)


class StorageIOError : public StorageException {
public:
	template<typename... Args>
	StorageIOError(Args&&... args) : StorageException(std::forward<Args>(args)...) { }
};

#define MSG_StorageIOError(...) StorageIOError(__FILE__, __LINE__, __VA_ARGS__)


class StorageNotFound : public StorageException {
public:
	template<typename... Args>
	StorageNotFound(Args&&... args) : StorageException(std::forward<Args>(args)...) { }
};

#define MSG_StorageNotFound(...) StorageNotFound(__FILE__, __LINE__, __VA_ARGS__)


class StorageEOF : public StorageException {
public:
	template<typename... Args>
	StorageEOF(Args&&... args) : StorageException(std::forward<Args>(args)...) { }
};

#define MSG_StorageEOF(...) StorageEOF(__FILE__, __LINE__, __VA_ARGS__)


class StorageNoFile : public StorageException {
public:
	template<typename... Args>
	StorageNoFile(Args&&... args) : StorageException(std::forward<Args>(args)...) { }
};

#define MSG_StorageNoFile(...) StorageNoFile(__FILE__, __LINE__, __VA_ARGS__)


class StorageCorruptVolume : public StorageException {
public:
	template<typename... Args>
	StorageCorruptVolume(Args&&... args) : StorageException(std::forward<Args>(args)...) { }
};

#define MSG_StorageCorruptVolume(...) StorageCorruptVolume(__FILE__, __LINE__, __VA_ARGS__)


struct StorageHeader {
	struct StorageHeaderHead {
		// uint32_t magic;
		uint32_t offset;  // required
		// char uuid[36];
	} head;

	char padding[(STORAGE_BLOCK_SIZE - sizeof(StorageHeader::StorageHeaderHead)) / sizeof(char)];

	inline void init(void* /* param */) {
		head.offset = STORAGE_START_BLOCK_OFFSET;
		// head.magic = STORAGE_MAGIC;
		// strncpy(head.uuid, "00000000-0000-0000-0000-000000000000", sizeof(head.uuid));
	}

	inline void validate(void* /* param */) {
		// if (head.magic != STORAGE_MAGIC) {
		// 	throw MSG_StorageCorruptVolume("Bad header magic number");
		// }
		// if (strncasecmp(head.uuid, "00000000-0000-0000-0000-000000000000", sizeof(head.uuid))) {
		// 	throw MSG_StorageCorruptVolume("UUID mismatch");
		// }
	}
};


#pragma pack(push, 1)
struct StorageBinHeader {
	// uint8_t magic;
	// uint8_t flags;
	uint32_t size;  // required

	inline void init(void* /* param */, uint32_t size_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
	}

	inline void validate(void* /* param */) {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	throw MSG_StorageCorruptVolume("Bad bin header magic number");
		// }
	}
};


struct StorageBinFooter {
	// uint32_t checksum;
	// uint8_t magic;

	inline void init(void* /* param */, uint32_t /* checksum_ */) {
		// magic = STORAGE_BIN_FOOTER_MAGIC;
		// checksum = checksum_;
	}

	inline void validate(void* /* param */, uint32_t /* checksum_ */) {
		// if (magic != STORAGE_BIN_FOOTER_MAGIC) {
		// 	throw MSG_StorageCorruptVolume("Bad bin footer magic number");
		// }
		// if (checksum != checksum_) {
		// 	throw MSG_StorageCorruptVolume("Bad bin checksum");
		// }
	}
};
#pragma pack(pop)


template <typename StorageHeader, typename StorageBinHeader, typename StorageBinFooter>
class Storage {
	std::string path;
	int flags;
	int fd;

	int free_blocks;

	char buffer0[STORAGE_BLOCK_SIZE];
	char buffer1[STORAGE_BLOCK_SIZE];
	uint32_t buffer_offset;

	off_t bin_offset;
	StorageBinHeader bin_header;
	StorageBinFooter bin_footer;

	LZ4DecompressDescriptor dec_lz4;
	LZ4DecompressDescriptor::iterator dec_it;

	inline void growfile() {
		if (free_blocks <= STORAGE_BLOCKS_MIN_FREE) {
			off_t file_size = io::lseek(fd, 0, SEEK_END);
			if unlikely(file_size < 0) {
				throw MSG_StorageIOError("IO error: lseek");
			}
			free_blocks = static_cast<int>((file_size - header.head.offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE);
			if (free_blocks <= STORAGE_BLOCKS_MIN_FREE) {
				off_t new_size = file_size + STORAGE_BLOCKS_GROW * STORAGE_BLOCK_SIZE;
				if (new_size > STORAGE_LAST_BLOCK_OFFSET) {
					new_size = STORAGE_LAST_BLOCK_OFFSET;
				}
				if (new_size > file_size) {
					io::fallocate(fd, 0, file_size, new_size - file_size);
				}
			}
		}
	}

protected:
	StorageHeader header;

public:
	Storage()
		: flags(0),
		  fd(0),
		  free_blocks(0),
		  buffer_offset(0),
		  bin_offset(0),
		  dec_lz4(fd) { }

	virtual ~Storage() {
		close();
	}

	void open(const std::string& path_, int flags_=STORAGE_CREATE_OR_OPEN, void* param=nullptr) {
		if (path == path_ && flags == flags_) {
			seek(STORAGE_START_BLOCK_OFFSET);
			return;
		}

		close();

		path = path_;
		flags = flags_;

#if STORAGE_BUFFER_CLEAR
		if (flags & STORAGE_WRITABLE) {
			memset(buffer0, STORAGE_BUFFER_CLEAR_CHAR, STORAGE_BLOCK_SIZE);
		}
#endif

		fd = ::open(path.c_str(), (flags & STORAGE_WRITABLE) ? O_RDWR : O_RDONLY, 0644);
		if unlikely(fd < 0) {
			if (flags & STORAGE_CREATE) {
				fd = ::open(path.c_str(), (flags & STORAGE_WRITABLE) ? O_RDWR | O_CREAT : O_RDONLY | O_CREAT, 0644);
			}
			if unlikely(fd < 0) {
				throw MSG_StorageIOError("Cannot open storage file");
			}

			memset(&header, 0, sizeof(header));
			header.init(param);

			if (io::write(fd, &header, sizeof(header)) != sizeof(header)) {
				throw MSG_StorageIOError("IO error: write");
			}
		} else {
			ssize_t r = io::read(fd, &header, sizeof(header));
			if unlikely(r < 0) {
				throw MSG_StorageIOError("IO error: read");
			} else if unlikely(r != sizeof(header)) {
				throw MSG_StorageCorruptVolume("Incomplete bin data");
			}
			header.validate(param);

			if (flags & STORAGE_WRITABLE) {
				buffer_offset = header.head.offset * STORAGE_ALIGNMENT;
				size_t offset = (buffer_offset / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
				buffer_offset -= offset;
				if unlikely(io::pread(fd, buffer0, STORAGE_BLOCK_SIZE, offset) < 0) {
					throw MSG_StorageIOError("IO error: pread");
				}
			}
		}

		seek(STORAGE_START_BLOCK_OFFSET);
	}

	void close() {
		if (fd) {
			if (flags & STORAGE_WRITABLE) {
				commit();
			}
			::close(fd);
		}

		fd = 0;
		free_blocks = 0;
		bin_offset = 0;
		bin_header.size = 0;
		buffer_offset = 0;
	}

	void seek(uint32_t offset) {
		if (offset > header.head.offset) {
			throw MSG_StorageEOF("Storage EOF");
		}
		bin_offset = offset * STORAGE_ALIGNMENT;
	}

	uint32_t write(const char *data, size_t data_size, void* param=nullptr) {
		uint32_t checksum = 0;

		StorageBinHeader bin_header;
		memset(&bin_header, 0, sizeof(bin_header));
		bin_header.init(param, 0);
		const StorageBinHeader* bin_header_data = &bin_header;
		size_t bin_header_data_size = sizeof(StorageBinHeader);

		StorageBinFooter bin_footer;
		memset(&bin_footer, 0, sizeof(bin_footer));
		const StorageBinFooter* bin_footer_data = &bin_footer;
		size_t bin_footer_data_size = sizeof(StorageBinFooter);
		off_t block_offset = ((header.head.offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;

		LZ4CompressData lz4(data, data_size);
		auto it = lz4.begin();
		size_t it_offset = 0;

		char* buffer = buffer0;
		off_t tmp_block_offset = block_offset;
		uint32_t tmp_buffer_offset = buffer_offset;

		while (bin_header_data_size || it || bin_footer_data_size) {
			if (bin_header_data_size) {
				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > bin_header_data_size) {
					size = bin_header_data_size;
				}
				memcpy(buffer + buffer_offset, bin_header_data, size);
				bin_header_data_size -= size;
				bin_header_data += size;
				buffer_offset += size;
				if (buffer_offset == STORAGE_BLOCK_SIZE) {
					buffer_offset = 0;
					if (buffer == buffer1) {
						goto do_write;
					} else {
						buffer = buffer1;
						goto do_update;
					}
				}
			}

			while (it) {
				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > it->size() - it_offset) {
					size = it->size() - it_offset;
				}
				memcpy(buffer + buffer_offset, it->c_str() + it_offset, size);
				buffer_offset += size;
				if (buffer_offset == STORAGE_BLOCK_SIZE) {
					buffer_offset = 0;
					if (buffer == buffer1) {
						goto do_write;
					} else {
						buffer = buffer1;
						goto do_update;
					}
				}
				++it;
				it_offset = 0;
			}

			if (bin_footer_data_size) {
				// Update Header.
				bin_header.size = lz4.size();
				memcpy(buffer0 + tmp_buffer_offset, &bin_header.size, sizeof(bin_header.size));

				bin_footer.init(param, checksum);

				size_t size = STORAGE_BLOCK_SIZE - buffer_offset;
				if (size > bin_footer_data_size) {
					size = bin_footer_data_size;
				}
				memcpy(buffer + buffer_offset, bin_footer_data, size);
				bin_footer_data_size -= size;
				bin_footer_data += size;
				buffer_offset += size;

				// Align the buffer_offset to the next storage alignment
				buffer_offset = static_cast<uint32_t>(((block_offset + buffer_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT - block_offset);

				if (buffer_offset == STORAGE_BLOCK_SIZE) {
					buffer_offset = 0;
					if (buffer == buffer1) {
						goto do_write;
					} else {
						buffer = buffer1;
						goto do_update;
					}
				}
			}

		do_write:
			if (io::pwrite(fd, buffer, STORAGE_BLOCK_SIZE, block_offset) != STORAGE_BLOCK_SIZE) {
				throw MSG_StorageIOError("IO error: pwrite");
			}

		do_update:
			if (buffer_offset == 0) {
				block_offset += STORAGE_BLOCK_SIZE;
				if (block_offset >= STORAGE_LAST_BLOCK_OFFSET) {
					throw MSG_StorageEOF("Storage EOF");
				}
				--free_blocks;
#if STORAGE_BUFFER_CLEAR
				memset(buffer, STORAGE_BUFFER_CLEAR_CHAR, STORAGE_BLOCK_SIZE);
#endif
			}
		}

		// Write the first used buffer.
		if (buffer == buffer1) {
			if (io::pwrite(fd, buffer, STORAGE_BLOCK_SIZE, tmp_block_offset) != STORAGE_BLOCK_SIZE) {
				throw MSG_StorageIOError("IO error: pwrite");
			}
		}

		header.head.offset += (((sizeof(StorageBinHeader) + bin_header.size + sizeof(StorageBinFooter)) + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT);

		return header.head.offset;
	}

	size_t read(char* buf, size_t buf_size, uint32_t limit=-1, void* param=nullptr) {
		if (!buf_size) {
			return 0;
		}

		ssize_t r;
		uint32_t checksum = 0;

		if (!bin_header.size) {
			off_t offset = io::lseek(fd, bin_offset, SEEK_SET);
			if (offset >= header.head.offset * STORAGE_ALIGNMENT || offset >= limit * STORAGE_ALIGNMENT) {
				throw MSG_StorageEOF("Storage EOF");
			}

			r = io::read(fd, &bin_header, sizeof(StorageBinHeader));
			if unlikely(r < 0) {
				throw MSG_StorageIOError("IO error: read");
			} else if unlikely(r != sizeof(StorageBinHeader)) {
				throw MSG_StorageCorruptVolume("Incomplete bin header");
			}
			bin_offset += r;
			bin_header.validate(param);

			io::fadvise(fd, bin_offset, bin_header.size, POSIX_FADV_WILLNEED);

			dec_lz4.set_read_bytes(bin_header.size);
			dec_it = dec_lz4.begin();
			bin_offset += bin_header.size;
		}

		size_t _size = dec_it.read(buf, buf_size);
		if (_size) {
			return _size;
		}

		r = io::read(fd, &bin_footer, sizeof(StorageBinFooter));
		if unlikely(r < 0) {
			throw MSG_StorageIOError("IO error: read");
		} else if unlikely(r != sizeof(StorageBinFooter)) {
			throw MSG_StorageCorruptVolume("Incomplete bin footer");
		}
		bin_offset += r;
		bin_footer.validate(param, checksum);

		// Align the bin_offset to the next storage alignment
		bin_offset = ((bin_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT;

		bin_header.size = 0;
		return 0;
	}

	void commit() {
		if unlikely(io::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
			throw MSG_StorageIOError("IO error: pwrite");
		}
		if (!(flags & STORAGE_NO_SYNC)) {
			if (flags & STORAGE_FULL_SYNC) {
				if unlikely(io::full_fsync(fd) < 0) {
					throw MSG_StorageIOError("IO error: full_fsync");
				}
			} else {
				if unlikely(io::fsync(fd) < 0) {
					throw MSG_StorageIOError("IO error: fsync");
				}
			}
		}
		growfile();
	}

	inline uint32_t write(const std::string& data, void* param=nullptr) {
		return write(data.data(), data.size(), param);

	}

	inline std::string read(uint32_t limit=-1, void* param=nullptr) {
		std::string ret;

		size_t r;
		char buf[LZ4_BLOCK_SIZE];
		while ((r = read(buf, sizeof(buf), limit, param))) {
			ret += std::string(buf, r);
		}

		return ret;
	}

	uint32_t get_volume(const std::string& filename)
	{
		std::size_t found = filename.find_last_of(".");
		if (found == std::string::npos) {
			throw std::invalid_argument("Volume not found in " + filename);
		}
		return static_cast<uint32_t>(std::stoul(filename.substr(found + 1)));
	}
};
