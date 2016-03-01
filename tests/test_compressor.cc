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


std::string read_file(const std::string& filename) {
	int fd = io::open(filename.c_str(), O_RDONLY, 0644);
	if unlikely(fd < 0) {
		throw MSG_Error("Cannot open file: %s", filename.c_str());
	}

	std::string ret;

	ssize_t r;
	char buf[1024];
	while ((r = io::read(fd, buf, sizeof(buf))) > 0) {
		ret += std::string(buf, r);
	}

	if unlikely(r < 0) {
		throw MSG_Error("IO error: read");
	}

	io::close(fd);

	return ret;
}


int compare(const std::string& filename1, const std::string& filename2) {
	int fd1 = io::open(filename1.c_str(), O_RDONLY, 0644);
	if unlikely(fd1 < 0) {
		throw MSG_Error("Cannot open file: %s", filename1.c_str());
	}

	int fd2 = io::open(filename2.c_str(), O_RDONLY, 0644);
	if unlikely(fd2 < 0) {
		io::close(fd1);
		throw MSG_Error("Cannot open file: %s", filename2.c_str());
	}

	if (lseek(fd1, 0, SEEK_END) != lseek(fd2, 0, SEEK_END)) {
		io::close(fd1);
		io::close(fd2);
		L_ERR(nullptr, "%s and %s are not equal (different sizes)\n", filename1.c_str(), filename2.c_str());
		return 1;
	}

	if (lseek(fd1, 0, SEEK_SET) != 0 && lseek(fd2, 0, SEEK_SET) != 0) {
		io::close(fd1);
		io::close(fd2);
		throw MSG_Error("IO error: lseek");
	}

	size_t r1 = 0, r2 = 0;
	char buf1[1024];
	char buf2[1024];
	while ((r1 = io::read(fd1, buf1, sizeof(buf1))) > 0 && (r2 = io::read(fd2, buf2, sizeof(buf2))) > 0) {
		if (r1 != r2 || std::string(buf1, r1) != std::string(buf2, r2)) {
			io::close(fd1);
			io::close(fd2);
			L_ERR(nullptr, "%s and %s are not equal\n", filename1.c_str(), filename2.c_str());
			return 1;
		}
	}

	io::close(fd1);
	io::close(fd2);

	return 0;
}


int test_Compress_Decompress_Data(const std::string& orig_file, const std::string& cmp_file, const std::string& dec_file) {
	try {
		// Compress Data
		{
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
			L_ERR(nullptr, "Size compress: %zu\n", lz4.size());
			io::close(fd);
		}

		// Decompress Data
		{
			std::string cmp_data = read_file(cmp_file.c_str());
			int fd = io::open(dec_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
			if unlikely(fd < 0) {
				throw MSG_Error("Cannot open file: %s", dec_file.c_str());
			}

			LZ4DecompressData dec_lz4(cmp_data.data(), cmp_data.size());
			auto dec_it = dec_lz4.begin();
			while (dec_it) {
				if (io::write(fd, dec_it->c_str(), dec_it.size()) != static_cast<ssize_t>(dec_it.size())) {
					throw MSG_Error("IO error: write");
				}
				++dec_it;
			}
			L_ERR(nullptr, "Size decompress: %zu\n", dec_lz4.size());
			io::close(fd);
		}

		return compare(orig_file, dec_file);
	} catch (const Error& er) {
		L_ERR(nullptr, "%s\n", er.get_context());
		return 1;
	} catch (const std::exception& er) {
		L_ERR(nullptr, "%s\n", er.what());
		return 1;
	}
}


int test_Compress_Decompress_File(const std::string& orig_file, const std::string& cmp_file, const std::string& dec_file) {
	try {
		// Compress File
		{
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
			L_ERR(nullptr, "Size compress: %zu\n", lz4.size());
			io::close(fd);
		}

		// Decompress File
		{
			int fd = io::open(dec_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
			if unlikely(fd < 0) {
				throw MSG_Error("Cannot open file: %s", dec_file.c_str());
			}

			LZ4DecompressFile dec_lz4(cmp_file);
			auto dec_it = dec_lz4.begin();
			while (dec_it) {
				if (io::write(fd, dec_it->c_str(), dec_it.size()) != static_cast<ssize_t>(dec_it.size())) {
					throw MSG_Error("IO error: write");
				}
				++dec_it;
			}
			L_ERR(nullptr, "Size decompress: %zu\n", dec_lz4.size());
			io::close(fd);
		}

		return compare(orig_file, dec_file);
	} catch (const Error& er) {
		L_ERR(nullptr, "%s\n", er.get_context());
		return 1;
	} catch (const std::exception& er) {
		L_ERR(nullptr, "%s\n", er.what());
		return 1;
	}
}


static const std::string cmp = "examples/compressor/compress.lz4";
static const std::string dec = "examples/compressor/decompress.dec";


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


int test_small_datas() {
	unlink(cmp.c_str());
	unlink(dec.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_Data(file, cmp, dec);
		unlink(cmp.c_str());
		unlink(dec.c_str());
	}
	return res;
}


int test_big_datas() {
	unlink(cmp.c_str());
	unlink(dec.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_Data(file, cmp, dec);
		unlink(cmp.c_str());
		unlink(dec.c_str());
	}
	return res;
}


int test_small_files() {
	unlink(cmp.c_str());
	unlink(dec.c_str());

	int res = 0;
	for (const auto& file : small_files) {
		res += test_Compress_Decompress_File(file, cmp, dec);
		unlink(cmp.c_str());
		unlink(dec.c_str());
	}
	return res;
}


int test_big_files() {
	unlink(cmp.c_str());
	unlink(dec.c_str());

	int res = 0;
	for (const auto& file : big_files) {
		res += test_Compress_Decompress_File(file, cmp, dec);
		unlink(cmp.c_str());
		unlink(dec.c_str());
	}
	return res;
}
