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

#include "test_compressor.h"


#include "../src/io_utils.h"
#include "../src/log.h"
#include "../src/lz4_compressor.h"

#define RETURN(x) { Log::finish(); return x; }

static const std::string cmp_file = "examples/compressor/compress.lz4";


static const std::vector<std::string> small_files({
	"examples/compressor/Small_File1.txt",
	"examples/compressor/Small_File2.txt",
	"examples/compressor/Small_File3.txt",
	"examples/compressor/Small_File4.txt"
});


static const std::vector<std::string> big_files({
	"examples/compressor/Big_File1.jpg",
	"examples/compressor/Big_File2.pdf",
	"examples/compressor/Big_File3.pdf",
	"examples/compressor/Big_File4.pdf",
	"examples/compressor/Big_File5.pdf"
});


std::string read_file(const std::string& filename) {
	int fd = io::open(filename.c_str(), O_RDONLY, 0644);
	if unlikely(fd < 0) {
		throw MSG_Error("Cannot open file: %s", filename.c_str());
	}

	std::string ret;

	ssize_t r;
	char buf[LZ4_BLOCK_SIZE];
	while ((r = io::read(fd, buf, sizeof(buf))) > 0) {
		ret += std::string(buf, r);
	}

	if unlikely(r < 0) {
		throw MSG_Error("IO error: read");
	}

	io::close(fd);

	return ret;
}


int test_Compress_Decompress_Data(const std::string& orig_file) {
	try {
		uint32_t cmp_checksum, dec_checksum;

		// Compress Data
		std::string _data = read_file(orig_file);
		L_ERR(nullptr, "Original Data Size: %zu\n", _data.size());

		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
		if unlikely(fd < 0) {
			throw MSG_Error("Cannot open file: %s", cmp_file.c_str());
		}

		LZ4CompressData lz4(_data.c_str(), _data.size());
		auto it = lz4.begin();
		while (it) {
			if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
				throw MSG_Error("IO error: write");
			}
			++it;
		}
		cmp_checksum = lz4.get_digest();
		L_ERR(nullptr, "Size compress: %zu (checksum: %u)\n", lz4.size(), cmp_checksum);
		io::close(fd);


		// Decompress Data
		std::string cmp_data = read_file(cmp_file.c_str());

		LZ4DecompressData dec_lz4(cmp_data.data(), cmp_data.size());
		auto dec_it = dec_lz4.begin();
		while (dec_it) {
			++dec_it;
		}
		dec_checksum = dec_lz4.get_digest();
		L_ERR(nullptr, "Size decompress: %zu (checksum: %u)\n", dec_lz4.size(), dec_checksum);

		return cmp_checksum != dec_checksum;
	} catch (const Exception& err) {
		L_ERR(nullptr, "%s\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR(nullptr, "%s\n", err.what());
		return 1;
	}
}


int test_Compress_Decompress_File(const std::string& orig_file) {
	try {
		uint32_t cmp_checksum, dec_checksum;

		// Compress File
		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
		if unlikely(fd < 0) {
			throw MSG_Error("Cannot open file: %s", cmp_file.c_str());
		}

		LZ4CompressFile lz4(orig_file);
		auto it = lz4.begin();
		while (it) {
			if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
				throw MSG_Error("IO error: write");
			}
			++it;
		}
		cmp_checksum = lz4.get_digest();
		L_ERR(nullptr, "Size compress: %zu (checksum: %u)\n", lz4.size(), cmp_checksum);
		io::close(fd);


		// Decompress File
		LZ4DecompressFile dec_lz4(cmp_file);
		auto dec_it = dec_lz4.begin();
		while (dec_it) {
			++dec_it;
		}
		dec_checksum = dec_lz4.get_digest();
		L_ERR(nullptr, "Size decompress: %zu (checksum: %u)\n", dec_lz4.size(), dec_checksum);

		return cmp_checksum != dec_checksum;
	} catch (const Exception& err) {
		L_ERR(nullptr, "%s\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR(nullptr, "%s\n", err.what());
		return 1;
	}
}


int test_Compress_Decompress_BlockFile(const std::string& orig_file, size_t numBytes) {
	try {
		size_t total_size = 0;
		std::vector<size_t> read_bytes;
		std::vector<uint32_t> cmp_checksums, dec_checksums;

		// Compress File
		int orig_fd = io::open(orig_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
		if unlikely(orig_fd < 0) {
			throw MSG_Error("Cannot open file: %s", orig_file.c_str());
		}

		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
		if unlikely(fd < 0) {
			throw MSG_Error("Cannot open file: %s", cmp_file.c_str());
		}

		LZ4CompressFile lz4;
		while (true) {
			bool more_data = false;
			lz4.reset(fd, -1, numBytes);
			auto it = lz4.begin();
			while (it) {
				if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
					throw MSG_Error("IO error: write");
				}
				++it;
				more_data = true;
			}
			if (more_data) {
				total_size += lz4.size();
				read_bytes.push_back(lz4.size());
				cmp_checksums.push_back(lz4.get_digest());
				continue;
			}
			break;
		}
		L_ERR(nullptr, "Size compress: %zu\n", total_size);
		io::close(orig_fd);


		// Decompress File
		io::lseek(fd, 0, SEEK_SET);

		LZ4DecompressFile dec_lz4;
		LZ4DecompressFile::iterator dec_it;

		auto it_checksum = cmp_checksums.begin();
		for (const auto& _bytes : read_bytes) {
			dec_lz4.reset(fd, -1, _bytes);
			dec_it = dec_lz4.begin();
			while (dec_it) {
				++dec_it;
			}
			if (*it_checksum != dec_lz4.get_digest()) {
				L_ERR(nullptr, "Different checksums");
				return 1;
			}
			++it_checksum;
		}

		return 0;
	} catch (const Exception& err) {
		L_ERR(nullptr, "%s\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR(nullptr, "%s\n", err.what());
		return 1;
	}
}


int test_small_datas() {
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_Data(file);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}


int test_big_datas() {
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_Data(file);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}


int test_small_files() {
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_File(file);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}


int test_big_files() {
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_File(file);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}


int test_small_blockFile() {
	unlink(cmp_file.c_str());

	int res = 0;
	size_t numBytes = 50;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_BlockFile(file, numBytes);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}


int test_big_blockFile() {
	unlink(cmp_file.c_str());

	int res = 0;
	size_t numBytes = 2000 * 1024;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_BlockFile(file, numBytes);
		unlink(cmp_file.c_str());
	}
	RETURN (res);
}
