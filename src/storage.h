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

#include <cassert>
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

#define STORAGE_MIN_COMPRESS_SIZE 100


constexpr int STORAGE_OPEN           = 0x00;  // Open an existing database.
constexpr int STORAGE_WRITABLE       = 0x01;  // Opens as writable.
constexpr int STORAGE_CREATE         = 0x02;  // Automatically creates the database if it doesn't exist
constexpr int STORAGE_CREATE_OR_OPEN = 0x03;  // Create database if it doesn't already exist.
constexpr int STORAGE_NO_SYNC        = 0x04;  // Don't attempt to ensure changes have hit disk.
constexpr int STORAGE_FULL_SYNC      = 0x08;  // Try to ensure changes are really written to disk.
constexpr int STORAGE_COMPRESS       = 0x10;  // Compress data in storage.

constexpr int STORAGE_FLAG_COMPRESSED  = 0x01;
constexpr int STORAGE_FLAG_DELETED     = 0x02;
constexpr int STORAGE_FLAG_MASK        = STORAGE_FLAG_COMPRESSED | STORAGE_FLAG_DELETED;


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

	inline void init(void* /*param*/) {
		head.offset = STORAGE_START_BLOCK_OFFSET;
		// head.magic = STORAGE_MAGIC;
		// strncpy(head.uuid, "00000000-0000-0000-0000-000000000000", sizeof(head.uuid));
	}

	inline void validate(void* /*param*/) {
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
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void* /*param*/, uint32_t size_, uint8_t flags_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void* /*param*/) {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	throw MSG_StorageCorruptVolume("Bad bin header magic number");
		// }
		if (flags & STORAGE_FLAG_DELETED) {
			throw MSG_StorageNotFound("Bin deleted");
		}
	}
};


struct StorageBinFooter {
	// uint32_t checksum;
	// uint8_t magic;

	inline void init(void* /*param*/, uint32_t  /*checksum_*/) {
		// magic = STORAGE_BIN_FOOTER_MAGIC;
		// checksum = checksum_;
	}

	inline void validate(void* /*param*/, uint32_t /*checksum_*/) {
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
	char* buffer_curr;
	uint32_t buffer_offset;

	off_t bin_offset;
	StorageBinHeader bin_header;
	StorageBinFooter bin_footer;

	size_t bin_size;

	LZ4DecompressDescriptor dec_lz4;
	LZ4DecompressDescriptor::iterator dec_it;

	XXH32_state_t* xxhash;
	uint32_t bin_hash;

	inline void growfile() {
		if (free_blocks <= STORAGE_BLOCKS_MIN_FREE) {
			off_t file_size = io::lseek(fd, 0, SEEK_END);
			if unlikely(file_size < 0) {
				close();
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

	inline void write_buffer(char** buffer_, uint32_t& buffer_offset_, off_t& block_offset_) {
		buffer_offset_ = 0;
		if (*buffer_ == buffer_curr) {
			*buffer_ = buffer_curr == buffer0 ? buffer1 : buffer0;
			goto do_update;
		} else {
			goto do_write;
		}

	do_write:
		if (io::pwrite(fd, *buffer_, STORAGE_BLOCK_SIZE, block_offset_) != STORAGE_BLOCK_SIZE) {
			close();
			throw MSG_StorageIOError("IO error: pwrite");
		}

	do_update:
		block_offset_ += STORAGE_BLOCK_SIZE;
		if (block_offset_ >= STORAGE_LAST_BLOCK_OFFSET) {
			throw MSG_StorageEOF("Storage EOF");
		}
		--free_blocks;
#if STORAGE_BUFFER_CLEAR
		memset(*buffer_, STORAGE_BUFFER_CLEAR_CHAR, STORAGE_BLOCK_SIZE);
#endif
	}

	inline void write_bin(char** buffer_, uint32_t& buffer_offset_, const char** data_bin_, size_t& size_bin_) {
		size_t size = STORAGE_BLOCK_SIZE - buffer_offset_;
		if (size > size_bin_) {
			size = size_bin_;
		}
		memcpy(*buffer_ + buffer_offset_, *data_bin_, size);
		size_bin_ -= size;
		*data_bin_ += size;
		buffer_offset_ += size;
	}

protected:
	StorageHeader header;

public:
	Storage()
		: flags(0),
		  fd(0),
		  free_blocks(0),
		  buffer_curr(buffer0),
		  buffer_offset(0),
		  bin_offset(0),
		  bin_size(0),
		  dec_lz4(fd, STORAGE_MAGIC),
		  xxhash(XXH32_createState()),
		  bin_hash(0) {
		if ((reinterpret_cast<char*>(&bin_header.size) - reinterpret_cast<char*>(&bin_header) + sizeof(bin_header.size)) > STORAGE_ALIGNMENT) {
			XXH32_freeState(xxhash);
			throw MSG_StorageException("StorageBinHeader's size must be in the first %d bites", STORAGE_ALIGNMENT - sizeof(bin_header.size));
		}
	}

	virtual ~Storage() {
		close();
		XXH32_freeState(xxhash);
	}

	void open(const std::string& path_, int flags_=STORAGE_CREATE_OR_OPEN, void* param=nullptr) {
		L_CALL(this, "Storage::open()");

		if (path != path_ || flags != flags_) {
			close();

			path = path_;
			flags = flags_;

#if STORAGE_BUFFER_CLEAR
			if (flags & STORAGE_WRITABLE) {
				memset(buffer_curr, STORAGE_BUFFER_CLEAR_CHAR, STORAGE_BLOCK_SIZE);
			}
#endif

			fd = io::open(path.c_str(), (flags & STORAGE_WRITABLE) ? O_RDWR : O_RDONLY, 0644);
			if unlikely(fd < 0) {
				if (flags & STORAGE_CREATE) {
					fd = io::open(path.c_str(), (flags & STORAGE_WRITABLE) ? O_RDWR | O_CREAT : O_RDONLY | O_CREAT, 0644);
				}
				if unlikely(fd < 0) {
					close();
					throw MSG_StorageIOError("Cannot open storage file");
				}

				memset(&header, 0, sizeof(header));
				header.init(param);

				if (io::write(fd, &header, sizeof(header)) != sizeof(header)) {
					close();
					throw MSG_StorageIOError("IO error: write");
				}

				seek(STORAGE_START_BLOCK_OFFSET);
				return;
			}
		}

		ssize_t r = io::pread(fd, &header, sizeof(header), 0);
		if unlikely(r < 0) {
			close();
			throw MSG_StorageIOError("IO error: read");
		} else if unlikely(r != sizeof(header)) {
			throw MSG_StorageCorruptVolume("Incomplete bin data");
		}
		header.validate(param);

		if (flags & STORAGE_WRITABLE) {
			buffer_offset = header.head.offset * STORAGE_ALIGNMENT;
			size_t offset = (buffer_offset / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
			buffer_offset -= offset;
			if unlikely(io::pread(fd, buffer_curr, STORAGE_BLOCK_SIZE, offset) < 0) {
				close();
				throw MSG_StorageIOError("IO error: pread");
			}
		}

		seek(STORAGE_START_BLOCK_OFFSET);
	}

	void close() {
		L_CALL(this, "Storage::close()");

		if (fd) {
			if (flags & STORAGE_WRITABLE) {
				commit();
			}
			io::close(fd);
		}

		fd = 0;
		free_blocks = 0;
		bin_offset = 0;
		bin_size = 0;
		bin_header.size = 0;
		buffer_offset = 0;
		flags = 0;
		path.clear();
	}

	void seek(uint32_t offset) {
		L_CALL(this, "Storage::seek()");

		if (offset > header.head.offset) {
			throw MSG_StorageEOF("Storage EOF");
		}
		bin_offset = offset * STORAGE_ALIGNMENT;
	}

	uint32_t write(const char *data, size_t data_size, void* param=nullptr) {
		L_CALL(this, "Storage::write(1)");

		uint32_t curr_offset = header.head.offset;
		const char* orig_data = data;

		StorageBinHeader bin_header;
		memset(&bin_header, 0, sizeof(bin_header));
		const char* bin_header_data = reinterpret_cast<const char*>(&bin_header);
		size_t bin_header_data_size = sizeof(StorageBinHeader);

		StorageBinFooter bin_footer;
		memset(&bin_footer, 0, sizeof(bin_footer));
		const char* bin_footer_data = reinterpret_cast<const char*>(&bin_footer);
		size_t bin_footer_data_size = sizeof(StorageBinFooter);

		std::unique_ptr<LZ4CompressData> lz4;
		LZ4CompressData::iterator it;
		size_t it_size;
		bool compress = (flags & STORAGE_COMPRESS) && data_size > STORAGE_MIN_COMPRESS_SIZE;
		if (compress) {
			bin_header.init(param, 0, STORAGE_FLAG_COMPRESSED);
			lz4 = std::make_unique<LZ4CompressData>(data, data_size, STORAGE_MAGIC);
			it = lz4->begin();
			it_size = it.size();
			data = it->data();
		} else {
			bin_header.init(param, data_size, 0);
			it_size = data_size;
		}

		char* buffer = buffer_curr;
		uint32_t tmp_buffer_offset = buffer_offset;
		StorageBinHeader* buffer_header = reinterpret_cast<StorageBinHeader*>(buffer + tmp_buffer_offset);

		off_t block_offset = ((curr_offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
		off_t tmp_block_offset = block_offset;

		while (bin_header_data_size) {
			write_bin(&buffer, tmp_buffer_offset, &bin_header_data, bin_header_data_size);
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
				continue;
			}
			break;
		}

		while (it_size) {
			write_bin(&buffer, tmp_buffer_offset, &data, it_size);
			if (compress && !it_size) {
				++it;
				data = it->data();
				it_size = it.size();
			}
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
			}
		}

		while (bin_footer_data_size) {
			// Update header size in buffer.
			if (compress) {
				buffer_header->size = static_cast<uint32_t>(lz4->size());
				bin_footer.init(param, lz4->get_digest());
			} else {
				bin_footer.init(param, XXH32(orig_data, data_size, STORAGE_MAGIC));
			}

			write_bin(&buffer, tmp_buffer_offset, &bin_footer_data, bin_footer_data_size);

			// Align the tmp_buffer_offset to the next storage alignment
			tmp_buffer_offset = static_cast<uint32_t>(((block_offset + tmp_buffer_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT - block_offset);
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
				continue;
			}
			if (io::pwrite(fd, buffer, STORAGE_BLOCK_SIZE, block_offset) != STORAGE_BLOCK_SIZE) {
				close();
				throw MSG_StorageIOError("IO error: pwrite");
			}
			break;
		}

		// Write the first used buffer.
		if (buffer != buffer_curr) {
			if (io::pwrite(fd, buffer_curr, STORAGE_BLOCK_SIZE, tmp_block_offset) != STORAGE_BLOCK_SIZE) {
				close();
				throw MSG_StorageIOError("IO error: pwrite");
			}
			buffer_curr = buffer;
		}

		buffer_offset = tmp_buffer_offset;
		header.head.offset += (((sizeof(StorageBinHeader) + buffer_header->size + sizeof(StorageBinFooter)) + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT);

		return curr_offset;
	}

	uint32_t write_file(const std::string& filename, void* param=nullptr) {
		L_CALL(this, "Storage::write_file()");

		uint32_t curr_offset = header.head.offset;

		StorageBinHeader bin_header;
		memset(&bin_header, 0, sizeof(bin_header));
		const char* bin_header_data = reinterpret_cast<const char*>(&bin_header);
		size_t bin_header_data_size = sizeof(StorageBinHeader);

		StorageBinFooter bin_footer;
		memset(&bin_footer, 0, sizeof(bin_footer));
		const char* bin_footer_data = reinterpret_cast<const char*>(&bin_footer);
		size_t bin_footer_data_size = sizeof(StorageBinFooter);

		std::unique_ptr<LZ4CompressFile> lz4;
		LZ4CompressFile::iterator it;
		size_t it_size, file_size = 0;
		int fd_write = 0;
		char buf_read[STORAGE_BLOCK_SIZE];
		const char* data;

		bool compress = (flags & STORAGE_COMPRESS);
		if (compress) {
			bin_header.init(param, 0, STORAGE_FLAG_COMPRESSED);
			lz4 = std::make_unique<LZ4CompressFile>(filename, STORAGE_MAGIC);
			it = lz4->begin();
			it_size = it.size();
			data = it->data();
		} else {
			fd_write = io::open(filename.c_str(), O_RDONLY, 0644);
			if unlikely(fd_write < 0) {
				throw MSG_LZ4IOError("Cannot open file: %s", filename.c_str());
			}
			bin_header.init(param, 0, 0);
			it_size = io::read(fd_write, buf_read, sizeof(buf_read));
			data = buf_read;
			file_size += it_size;
			XXH32_reset(xxhash, STORAGE_MAGIC);
			XXH32_update(xxhash, data, it_size);
		}

		char* buffer = buffer_curr;
		uint32_t tmp_buffer_offset = buffer_offset;
		StorageBinHeader* buffer_header = reinterpret_cast<StorageBinHeader*>(buffer + tmp_buffer_offset);

		off_t block_offset = ((curr_offset * STORAGE_ALIGNMENT) / STORAGE_BLOCK_SIZE) * STORAGE_BLOCK_SIZE;
		off_t tmp_block_offset = block_offset;

		while (bin_header_data_size) {
			write_bin(&buffer, tmp_buffer_offset, &bin_header_data, bin_header_data_size);
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
				continue;
			}
			break;
		}

		while (it_size) {
			write_bin(&buffer, tmp_buffer_offset, &data, it_size);
			if (!it_size) {
				if (compress) {
					++it;
					it_size = it.size();
					data = it->data();
				} else {
					it_size = io::read(fd_write, buf_read, sizeof(buf_read));
					data = buf_read;
					file_size += it_size;
					XXH32_update(xxhash, data, it_size);
				}
			}
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
			}
		}

		while (bin_footer_data_size) {
			// Update header size in buffer.
			if (compress) {
				buffer_header->size = static_cast<uint32_t>(lz4->size());
				bin_footer.init(param, lz4->get_digest());
			} else {
				buffer_header->size = static_cast<uint32_t>(file_size);
				bin_footer.init(param, XXH32_digest(xxhash));
				io::close(fd_write);
			}

			write_bin(&buffer, tmp_buffer_offset, &bin_footer_data, bin_footer_data_size);

			// Align the tmp_buffer_offset to the next storage alignment
			tmp_buffer_offset = static_cast<uint32_t>(((block_offset + tmp_buffer_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT - block_offset);
			if (tmp_buffer_offset == STORAGE_BLOCK_SIZE) {
				write_buffer(&buffer, tmp_buffer_offset, block_offset);
				continue;
			} else {
				if (io::pwrite(fd, buffer, STORAGE_BLOCK_SIZE, block_offset) != STORAGE_BLOCK_SIZE) {
					close();
					throw MSG_StorageIOError("IO error: pwrite");
				}
				break;
			}
		}

		// Write the first used buffer.
		if (buffer != buffer_curr) {
			if (io::pwrite(fd, buffer_curr, STORAGE_BLOCK_SIZE, tmp_block_offset) != STORAGE_BLOCK_SIZE) {
				close();
				throw MSG_StorageIOError("IO error: pwrite");
			}
			buffer_curr = buffer;
		}

		buffer_offset = tmp_buffer_offset;
		header.head.offset += (((sizeof(StorageBinHeader) + buffer_header->size + sizeof(StorageBinFooter)) + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT);

		return curr_offset;
	}

	size_t read(char* buf, size_t buf_size, uint32_t limit=-1, void* param=nullptr) {
		L_CALL(this, "Storage::read(1)");

		if (!buf_size) {
			return 0;
		}

		ssize_t r;

		if (!bin_header.size) {
			off_t offset = io::lseek(fd, bin_offset, SEEK_SET);
			if (offset >= header.head.offset * STORAGE_ALIGNMENT || offset >= limit * STORAGE_ALIGNMENT) {
				throw MSG_StorageEOF("Storage EOF");
			}

			r = io::read(fd, &bin_header, sizeof(StorageBinHeader));
			if unlikely(r < 0) {
				close();
				throw MSG_StorageIOError("IO error: read");
			} else if unlikely(r != sizeof(StorageBinHeader)) {
				throw MSG_StorageCorruptVolume("Incomplete bin header");
			}
			bin_offset += r;
			bin_header.validate(param);

			io::fadvise(fd, bin_offset, bin_header.size, POSIX_FADV_WILLNEED);

			if (bin_header.flags & STORAGE_FLAG_COMPRESSED) {
				dec_lz4.reset(bin_header.size, STORAGE_MAGIC);
				dec_it = dec_lz4.begin();
				bin_offset += bin_header.size;
			} else {
				XXH32_reset(xxhash, STORAGE_MAGIC);
			}
		}

		if (bin_header.flags & STORAGE_FLAG_COMPRESSED) {
			size_t _size = dec_it.read(buf, buf_size);
			if (_size) {
				return _size;
			}
			bin_hash = dec_lz4.get_digest();
		} else {
			if (buf_size > bin_header.size - bin_size) {
				buf_size = bin_header.size - bin_size;
			}

			if (buf_size) {
				r = io::read(fd, buf, buf_size);
				if unlikely(r < 0) {
					close();
					throw MSG_StorageIOError("IO error: read");
				} else if unlikely(static_cast<size_t>(r) != buf_size) {
					throw MSG_StorageCorruptVolume("Incomplete bin data");
				}
				bin_offset += r;
				bin_size += r;
				XXH32_update(xxhash, buf, r);
				return r;
			}
			bin_hash = XXH32_digest(xxhash);
		}

		r = io::read(fd, &bin_footer, sizeof(StorageBinFooter));
		if unlikely(r < 0) {
			close();
			throw MSG_StorageIOError("IO error: read");
		} else if unlikely(r != sizeof(StorageBinFooter)) {
			throw MSG_StorageCorruptVolume("Incomplete bin footer");
		}
		bin_offset += r;
		bin_footer.validate(param, bin_hash);

		// Align the bin_offset to the next storage alignment
		bin_offset = ((bin_offset + STORAGE_ALIGNMENT - 1) / STORAGE_ALIGNMENT) * STORAGE_ALIGNMENT;

		bin_header.size = 0;
		bin_size = 0;

		return 0;
	}

	void commit() {
		L_CALL(this, "Storage::commit()");

		if unlikely(io::pwrite(fd, &header, sizeof(header), 0) != sizeof(header)) {
			close();
			throw MSG_StorageIOError("IO error: pwrite");
		}
		if (!(flags & STORAGE_NO_SYNC)) {
			if (flags & STORAGE_FULL_SYNC) {
				if unlikely(io::full_fsync(fd) < 0) {
					close();
					throw MSG_StorageIOError("IO error: full_fsync");
				}
			} else {
				if unlikely(io::fsync(fd) < 0) {
					close();
					throw MSG_StorageIOError("IO error: fsync");
				}
			}
		}
		growfile();
	}

	inline uint32_t write(const std::string& data, void* param=nullptr) {
		L_CALL(this, "Storage::write(2)");

		return write(data.data(), data.size(), param);
	}

	inline std::string read(uint32_t limit=-1, void* param=nullptr) {
		L_CALL(this, "Storage::read(2)");

		std::string ret;

		size_t r;
		char buf[LZ4_BLOCK_SIZE];
		while ((r = read(buf, sizeof(buf), limit, param))) {
			ret += std::string(buf, r);
		}

		return ret;
	}

	uint32_t get_volume(const std::string& filename) {
		L_CALL(this, "Storage::get_volume()");

		std::size_t found = filename.find_last_of(".");
		if (found == std::string::npos) {
			throw std::invalid_argument("Volume not found in " + filename);
		}
		return static_cast<uint32_t>(std::stoul(filename.substr(found + 1)));
	}
};
