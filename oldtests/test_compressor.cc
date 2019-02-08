/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_compressor.h"

#include "../src/io.hh"
#include "../src/compressor_lz4.h"
#include "utils.h"


const std::string path_test_compressor = std::string(FIXTURES_PATH) + "/examples/compressor/";


const std::string cmp_file = path_test_compressor + "compress.lz4";


const std::vector<std::string> small_files({
	path_test_compressor + "Small_File1.txt",
	path_test_compressor + "Small_File2.txt",
	path_test_compressor + "Small_File3.txt",
	path_test_compressor + "Small_File4.txt"
});


const std::vector<std::string> big_files({
	path_test_compressor + "Big_File1.jpg",
	path_test_compressor + "Big_File2.pdf",
	path_test_compressor + "Big_File3.pdf",
	path_test_compressor + "Big_File4.pdf",
	path_test_compressor + "Big_File5.pdf"
});


std::string read_file(const std::string& filename) {
	int fd = io::open(filename.c_str(), O_RDONLY);
	if unlikely(fd < 0) {
		THROW(Error, "Cannot open file: {}", filename);
	}

	std::string ret;

	ssize_t r;
	char buf[LZ4_BLOCK_SIZE];
	while ((r = io::read(fd, buf, sizeof(buf))) > 0) {
		ret += std::string(buf, r);
	}

	if unlikely(r < 0) {
		THROW(Error, "IO error: read");
	}

	io::close(fd);

	return ret;
}


int test_Compress_Decompress_Data(const std::string& orig_file) {
	try {
		uint32_t cmp_checksum, dec_checksum;

		// Compress Data
		std::string _data = read_file(orig_file);

		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT, 0644);
		if unlikely(fd < 0) {
			THROW(Error, "Cannot open file: {}", cmp_file);
		}

		LZ4CompressData lz4(_data.c_str(), _data.size());
		auto it = lz4.begin();
		while (it) {
			if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
				THROW(Error, "IO error: write");
			}
			++it;
		}
		cmp_checksum = lz4.get_digest();
		io::close(fd);


		// Decompress Data
		std::string cmp_data = read_file(cmp_file.c_str());

		LZ4DecompressData dec_lz4(cmp_data.data(), cmp_data.size());
		auto dec_it = dec_lz4.begin();
		while (dec_it) {
			++dec_it;
		}
		dec_checksum = dec_lz4.get_digest();

		return cmp_checksum != dec_checksum;
	} catch (const Exception& err) {
		L_ERR("{}\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR("{}\n", err.what());
		return 1;
	}
}


int test_Compress_Decompress_File(const std::string& orig_file) {
	try {
		uint32_t cmp_checksum, dec_checksum;

		// Compress File
		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT, 0644);
		if unlikely(fd < 0) {
			THROW(Error, "Cannot open file: {}", cmp_file.c_str());
		}

		LZ4CompressFile lz4(orig_file);
		auto it = lz4.begin();
		while (it) {
			if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
				THROW(Error, "IO error: write");
			}
			++it;
		}
		cmp_checksum = lz4.get_digest();
		L_ERR("Size compress: {} (checksum: {})\n", lz4.size(), cmp_checksum);
		io::close(fd);


		// Decompress File
		LZ4DecompressFile dec_lz4(cmp_file);
		auto dec_it = dec_lz4.begin();
		while (dec_it) {
			++dec_it;
		}
		dec_checksum = dec_lz4.get_digest();
		L_ERR("Size decompress: {} (checksum: {})\n", dec_lz4.size(), dec_checksum);

		return cmp_checksum != dec_checksum;
	} catch (const Exception& err) {
		L_ERR("{}\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR("{}\n", err.what());
		return 1;
	}
}


int test_Compress_Decompress_BlockFile(const std::string& orig_file, size_t numBytes) {
	try {
		size_t total_size = 0;
		std::vector<size_t> read_bytes;
		std::vector<uint32_t> cmp_checksums, dec_checksums;

		// Compress File
		int orig_fd = io::open(orig_file.c_str(), O_RDWR | O_CREAT, 0644);
		if unlikely(orig_fd < 0) {
			THROW(Error, "Cannot open file: {}", orig_file);
		}

		int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT, 0644);
		if unlikely(fd < 0) {
			THROW(Error, "Cannot open file: {}", cmp_file);
		}

		LZ4CompressFile lz4;
		while (true) {
			bool more_data = false;
			lz4.reset(fd, -1, numBytes);
			auto it = lz4.begin();
			while (it) {
				if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
					THROW(Error, "IO error: write");
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
		L_ERR("Size compress: {}\n", total_size);
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
				L_ERR("Different checksums");
				return 1;
			}
			++it_checksum;
		}

		return 0;
	} catch (const Exception& err) {
		L_ERR("{}\n", err.get_context());
		return 1;
	} catch (const std::exception& err) {
		L_ERR("{}\n", err.what());
		return 1;
	}
}


int test_small_datas() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_Data(file);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}


int test_big_datas() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_Data(file);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}


int test_small_files() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_File(file);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}


int test_big_files() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_File(file);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}


int test_small_blockFile() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	size_t numBytes = 50;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_BlockFile(file, numBytes);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}


int test_big_blockFile() {
	INIT_LOG
	unlink(cmp_file.c_str());

	int res = 0;
	size_t numBytes = 2000 * 1024;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_BlockFile(file, numBytes);
		unlink(cmp_file.c_str());
	}
	RETURN(res);
}
