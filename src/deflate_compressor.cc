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

#include "deflate_compressor.h"


std::string zerr(int ret)
{
	switch (ret) {
		case Z_ERRNO:
			return "There is an error reading or writing the files";
		case Z_STREAM_ERROR:
			return "invalid compression level";
		case Z_DATA_ERROR:
			return "invalid or incomplete deflate data";
		case Z_MEM_ERROR:
			return "memory could not be allocated for processing (out of memory)";
		case Z_VERSION_ERROR:
			return "zlib version mismatch!";
		}
	return std::string();
}


int DeflateCompressData::FINISH_COMPRESS = Z_FINISH;

/*
 * Constructor compress with data
 */
DeflateCompressData::DeflateCompressData(const char* data_, size_t data_size_, bool gzip)
	: DeflateData(data_, data_size_),
	  DeflateBlockStreaming(gzip) { }


DeflateCompressData::~DeflateCompressData()
{
	if (free_strm) {
		deflateEnd(&strm);
	}
}


std::string
DeflateCompressData::init(bool start)
{
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = Z_NULL;

	stream = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | (gzip ? 16 : 0), 8, Z_DEFAULT_STRATEGY);
	if(stream != Z_OK) {
		THROW(DeflateException, zerr(stream));
	}

	free_strm = true;
	if (start && data) {
		return next();
	}
	return std::string();
}


std::string
DeflateCompressData::next(const char* input, size_t input_size, int flush)
{
	std::unique_ptr<char[]> out = std::make_unique<char[]>(input_size);
	strm.avail_in = input_size;
	strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input));

	std::string result;
	do {
		strm.avail_out = input_size;
		strm.next_out = reinterpret_cast<Bytef*>(out.get());
		stream = deflate(&strm, flush);    // no bad return value
		int compress_size = input_size - strm.avail_out;
		result.append(std::string(out.get(), compress_size));
	} while (strm.avail_out == 0);

	return result;
}


std::string
DeflateCompressData::next()
{
	int flush;
	if (data_offset > data_size) {
		state = DeflateState::END;
		return std::string();
	}

	if (data_offset < data_size) {
		strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data + data_offset));
	}

	auto remain_size = data_size - data_offset;
	if ( remain_size > DEFLATE_BLOCK_SIZE) {
		strm.avail_in = DEFLATE_BLOCK_SIZE;
		flush = Z_NO_FLUSH;
	} else {
		strm.avail_in = remain_size;
		flush = Z_FINISH;
	}

	std::string result;
	do {
		strm.avail_out = DEFLATE_BLOCK_SIZE;
		strm.next_out = reinterpret_cast<Bytef*>(cmpBuf);
		stream = deflate(&strm, flush);    // no bad return value
		int compress_size = DEFLATE_BLOCK_SIZE - strm.avail_out;
		result.append(std::string(cmpBuf, compress_size));
	} while (strm.avail_out == 0);

	data_offset += DEFLATE_BLOCK_SIZE;
	return result;
}


/*
 * Constructor decompress with data
 */
DeflateDecompressData::DeflateDecompressData(const char* data_, size_t data_size_, bool gzip)
	: DeflateData(data_, data_size_),
	  DeflateBlockStreaming(gzip) { }


DeflateDecompressData::~DeflateDecompressData()
{
	if (free_strm) {
		inflateEnd(&strm);
	}
}


std::string
DeflateDecompressData::init()
{
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	stream = inflateInit2(&strm, 15 | (gzip ? 16 : 0));
	if(stream != Z_OK) {
		THROW(DeflateException, zerr(stream));
	}

	free_strm = true;
	return next();
}


std::string
DeflateDecompressData::next()
{
	if (data_offset > data_size) {
		state = DeflateState::END;
		return std::string();
	}

	if (data_offset < data_size) {
		strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data + data_offset));
	}

	auto remain_size = data_size - data_offset;
	if ( remain_size > DEFLATE_BLOCK_SIZE) {
		strm.avail_in = DEFLATE_BLOCK_SIZE;
	} else {
		strm.avail_in = remain_size;
	}

	std::string result;
	do {
		strm.avail_out = DEFLATE_BLOCK_SIZE;
		strm.next_out = reinterpret_cast<Bytef*>(buffer);
		stream = inflate(&strm, Z_NO_FLUSH);
		if (stream != Z_OK && stream != Z_STREAM_END && stream != Z_BUF_ERROR) {
			THROW(DeflateException, zerr(stream));
		}
		auto bytes_decompressed = DEFLATE_BLOCK_SIZE - strm.avail_out;
		result.append(std::string(buffer, bytes_decompressed));
	} while (strm.avail_out == 0);

	data_offset += DEFLATE_BLOCK_SIZE;
	return result;
}


/*
 * Construct for file name
 */
DeflateCompressFile::DeflateCompressFile(const std::string& filename, bool gzip)
	: DeflateFile(filename),
	  DeflateBlockStreaming(gzip) { }


/*
 * Construct for file descriptor
 */
DeflateCompressFile::DeflateCompressFile(int fd_, off_t fd_offset_, off_t fd_nbytes_, bool gzip)
	: DeflateFile(fd_, fd_offset_, fd_nbytes_),
	  DeflateBlockStreaming(gzip) { }


DeflateCompressFile::~DeflateCompressFile()
{
	if (free_strm) {
		deflateEnd(&strm);
	}
}


std::string
DeflateCompressFile::init()
{
	if (fd_offset != -1 && io::lseek(fd, fd_offset, SEEK_SET) != static_cast<off_t>(fd_offset)) {
		THROW(DeflateIOError, "IO error: lseek");
	}

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	stream = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | (gzip ? 16 : 0), 8, Z_DEFAULT_STRATEGY);
	if(stream != Z_OK) {
		THROW(DeflateException, zerr(stream));
	}

	io::lseek(fd, 0, SEEK_END);
	size_file = io::lseek(fd, 0, SEEK_CUR);
	io::lseek(fd, 0, SEEK_SET);

	free_strm = true;
	return next();
}


std::string
DeflateCompressFile::next()
{
	int inpBytes = static_cast<int>(io::read(fd, buffer, DEFLATE_BLOCK_SIZE));
	if (inpBytes <= 0) {
		if (stream == Z_STREAM_END) {
			state = DeflateState::END;
			return std::string();
		} else {
			THROW(DeflateIOError, "IO error: read");
		}
	}
	strm.avail_in = inpBytes;
	bytes_readed+=strm.avail_in;

	int flush;
	if (bytes_readed == size_file) {
		flush = Z_FINISH;
	} else {
		flush = Z_NO_FLUSH;
	}

	std::string result;
	strm.next_in = reinterpret_cast<Bytef*>(buffer);
	do {
		strm.avail_out = DEFLATE_BLOCK_SIZE;
		strm.next_out = reinterpret_cast<Bytef*>(cmpBuf);
		stream = deflate(&strm, flush);    /* no bad return value */
		auto bytes_compressed = DEFLATE_BLOCK_SIZE - strm.avail_out;
		result.append(std::string(cmpBuf, bytes_compressed));
	} while (strm.avail_out == 0);

	return result;
}


DeflateDecompressFile::DeflateDecompressFile(const std::string& filename, bool gzip)
	: DeflateFile(filename),
	  DeflateBlockStreaming(gzip) { }


DeflateDecompressFile::DeflateDecompressFile(int fd_, off_t fd_offset_, off_t fd_nbytes_, bool gzip)
	: DeflateFile(fd_, fd_offset_, fd_nbytes_),
	  DeflateBlockStreaming(gzip) { }


DeflateDecompressFile::~DeflateDecompressFile()
{
	if (free_strm) {
		inflateEnd(&strm);
	}
}


std::string
DeflateDecompressFile::init()
{
	if (fd_offset != -1 && io::lseek(fd, fd_offset, SEEK_SET) != static_cast<off_t>(fd_offset)) {
		THROW(DeflateIOError, "IO error: lseek");
	}

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	stream = inflateInit2(&strm, 15 | (gzip ? 16 : 0));
	if(stream != Z_OK) {
		THROW(DeflateException, zerr(stream));
	}

	free_strm = true;
	return next();
}


std::string
DeflateDecompressFile::next()
{
	int inpBytes = io::read(fd, cmpBuf, DEFLATE_BLOCK_SIZE);
	if (inpBytes <= 0) {
		if (stream == Z_STREAM_END) {
			state = DeflateState::END;
			return std::string();
		} else {
			THROW(DeflateIOError, "IO error: read");
		}
	}
	strm.avail_in = inpBytes;
	strm.next_in = reinterpret_cast<Bytef*>(cmpBuf);

	std::string result;
	do {
		strm.avail_out = DEFLATE_BLOCK_SIZE;
		strm.next_out = reinterpret_cast<Bytef*>(buffer);
		stream = inflate(&strm, Z_NO_FLUSH);
		if (stream != Z_OK && stream != Z_STREAM_END && stream != Z_BUF_ERROR) {
			THROW(DeflateException, zerr(stream));
		}
		auto bytes_decompressed = DEFLATE_BLOCK_SIZE - strm.avail_out;
		result.append(std::string(buffer, bytes_decompressed));
	} while (strm.avail_out == 0);

	return result;
}
