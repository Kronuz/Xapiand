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
		throw std::exception(/* Error in read volumen */);
	}

	std::string ret;

	ssize_t r;
	char buf[1024];
	while ((r = io::read(fd, buf, sizeof(buf))) > 0) {
		ret += std::string(buf, r);
	}

	if unlikely(r < 0) {
		throw std::exception(/* Error in read volumen */);
	}

	return ret;
}


int compare(const std::string& filename1, const std::string& filename2) {
	int fd1 = io::open(filename1.c_str(), O_RDONLY, 0644);
	if unlikely(fd1 < 0) {
		throw std::exception(/* Error in read volumen */);
	}

	int fd2 = io::open(filename2.c_str(), O_RDONLY, 0644);
	if unlikely(fd2 < 0) {
		throw std::exception(/* Error in read volumen */);
	}

	if (lseek(fd1, 0, SEEK_END) != lseek(fd2, 0, SEEK_END)) {
		L_ERR(nullptr, "%s and %s are not equal (different sizes)\n", filename1.c_str(), filename2.c_str());
		return 1;
	}

	lseek(fd1, 0, SEEK_SET);
	lseek(fd2, 0, SEEK_SET);

	size_t r1 = 0, r2 = 0;
	char buf1[1024];
	char buf2[1024];
	while ((r1 = io::read(fd1, buf1, sizeof(buf1))) > 0 && (r2 = io::read(fd2, buf2, sizeof(buf2))) > 0) {
		if (r1 != r2 || std::string(buf1, r1) != std::string(buf2, r2)) {
			L_ERR(nullptr, "%s and %s are not equal\n", filename1.c_str(), filename2.c_str());
			return 1;
		}
	}

	return 0;
}


int test_Compress_Decompress_Data(const std::string& orig_file, const std::string& cmp_file, const std::string& dec_file) {
	try {
		// Compress Data
		{
			std::string _data = read_file(orig_file);
			L(nullptr, "Original Data Size: %zu\n", _data.size());

			int fd = io::open(cmp_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
			if unlikely(fd < 0) {
				L_ERR(nullptr, "Error in read volumen\n");
				return 1;
			}

			LZ4CompressData lz4(_data.c_str(), _data.size());
			auto it = lz4.begin();
			while (it) {
				if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
					L_ERR(nullptr, "Error in write volumen\n");
					return 1;
				}
				++it;
			}
			L(nullptr, "Size compress: %zu\n", lz4.size());
			io::close(fd);
		}

		// Decompress Data
		{
			std::string cmp_data = read_file(cmp_file.c_str());
			int fd = io::open(dec_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
			if unlikely(fd < 0) {
				L_ERR(nullptr, "Error in read volumen\n");
				return 1;
			}

			LZ4DecompressData dec_lz4(cmp_data.data(), cmp_data.size());
			auto dec_it = dec_lz4.begin();
			while (dec_it) {
				if (io::write(fd, dec_it->c_str(), dec_it.size()) != static_cast<ssize_t>(dec_it.size())) {
					L_ERR(nullptr, "Error in write volumen\n");
					return 1;
				}
				++dec_it;
			}
			L(nullptr, "Size decompress: %zu\n", dec_lz4.size());
			io::close(fd);
		}

		return compare(orig_file, dec_file);
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
				L_ERR(nullptr, "Error in read volumen\n");
				return 1;
			}

			LZ4CompressFile lz4(orig_file);
			auto it = lz4.begin();
			while (it) {
				if (io::write(fd, it->c_str(), it.size()) != static_cast<ssize_t>(it.size())) {
					L_ERR(nullptr, "Error in write volumen\n");
					return 1;
				}
				++it;
			}
			L(nullptr, "Size compress: %zu\n", lz4.size());
			io::close(fd);
		}

		// Decompress File
		{
			int fd = io::open(dec_file.c_str(), O_RDWR | O_CREAT | O_DSYNC, 0644);
			if unlikely(fd < 0) {
				L_ERR(nullptr, "Error in read volumen\n");
				return 1;
			}

			LZ4DecompressFile dec_lz4(cmp_file);
			auto dec_it = dec_lz4.begin();
			while (dec_it) {
				if (io::write(fd, dec_it->c_str(), dec_it.size()) != static_cast<ssize_t>(dec_it.size())) {
					L_ERR(nullptr, "Error in write volumen\n");
					return 1;
				}
				++dec_it;
			}
			L(nullptr, "Size decompress: %zu\n", dec_lz4.size());
			io::close(fd);
		}

		return compare(orig_file, dec_file);
	} catch (const std::exception& er) {
		L_ERR(nullptr, "%s\n", er.what());
		return 1;
	}
}


int test_small_datas() {
	std::string cmp = "compress.lz4";
	std::string dec = "decompress.dec";

	std::string orig = "examples/compress/Small_File1.txt";
	int res = test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Small_File2.txt";
	res += test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Small_File3.txt";
	res += test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	return res;
}


int test_big_datas() {
	std::string cmp = "compress.lz4";
	std::string dec = "decompress.dec";

	std::string orig = "examples/compress/Big_File1.jpg";
	int res = test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Big_File2.pdf";
	res += test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Big_File3.pdf";
	res += test_Compress_Decompress_Data(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	return res;
}


int test_small_files() {
	std::string cmp = "compress.lz4";
	std::string dec = "decompress.dec";

	std::string orig = "examples/compress/Small_File1.txt";
	int res = test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Small_File2.txt";
	res += test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Small_File3.txt";
	res += test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	return res;
}


int test_big_files() {
	std::string cmp = "compress.lz4";
	std::string dec = "decompress.dec";

	std::string orig = "examples/compress/Big_File1.jpg";
	int res = test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Big_File2.pdf";
	res += test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	orig = "examples/compress/Big_File3.pdf";
	res += test_Compress_Decompress_File(orig, cmp, dec);
	unlink(cmp.c_str());
	unlink(dec.c_str());

	return res;
}
