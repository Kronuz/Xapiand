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

#include "lz4_compressor.h"

#include "io_utils.h"


static void write_uint16(void* blockStream, uint16_t i) {
	memcpy(blockStream, &i, sizeof(uint16_t));
}


static void write_bin(void* blockStream, const void* array, int arrayBytes) {
	memcpy(blockStream, array, arrayBytes);
}


static void read_uint16(const void* blockStream, uint16_t* i) {
	memcpy(i, blockStream, sizeof(uint16_t));
}


static void read_partial_uint16(const void* blockStream, uint16_t* i, size_t numBytes, size_t offset=0) {
	memcpy(reinterpret_cast<char*>(i) + offset, blockStream, numBytes);
}


static void read_bin(const void* blockStream, void* array, int arrayBytes) {
	memcpy(array, blockStream, arrayBytes);
}


static void read_partial_bin(const void* blockStream, void* array, size_t arrayBytes, size_t offset=0) {
	memcpy(reinterpret_cast<char*>(array) + offset, blockStream, arrayBytes);
}


LZ4CompressData::LZ4CompressData(const char* data_, size_t data_size_, int seed)
	: LZ4BlockStreaming(data_size_ > LZ4_BLOCK_SIZE ? LZ4_BLOCK_SIZE : static_cast<int>(data_size_), seed),
	  lz4Stream(LZ4_createStream()),
	  data(data_),
	  data_size(data_size_) { }


LZ4CompressData::~LZ4CompressData()
{
	LZ4_freeStream(lz4Stream);
}


std::string
LZ4CompressData::init()
{
	data_offset = 0;
	return next();
}


std::string
LZ4CompressData::next()
{
	if (data_offset == data_size) {
		return std::string();
	}

	char* const inpPtr = &buffer[_offset];

	// Read line to the ring buffer.
	int inpBytes = 0;
	if ((data_offset + block_size) > data_size) {
		inpBytes = static_cast<int>(data_size - data_offset);
	} else {
		inpBytes = block_size;
	}

	memcpy(inpPtr, data + data_offset, inpBytes);
	data_offset += inpBytes;

	const int cmpBytes = LZ4_compress_fast_continue(lz4Stream, inpPtr, cmpBuf, inpBytes, cmpBuf_size, 1);
	if (cmpBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_compress_fast_continue failed!");
	}

	size_t blockBytes = sizeof(uint16_t) + cmpBytes;
	char* const blockStream = (char*)malloc(blockBytes);

	write_uint16(blockStream, (uint16_t)cmpBytes);
	write_bin(blockStream + sizeof(uint16_t), cmpBuf, cmpBytes);

	// Add and wraparound the ringbuffer offset
	_offset += inpBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, inpPtr, inpBytes);

	_size += blockBytes;
	std::string result(blockStream, blockBytes);
	free(blockStream);

	return result;
}


LZ4CompressFile::LZ4CompressFile(const std::string& filename, int seed)
	: LZ4BlockStreaming(LZ4_BLOCK_SIZE, seed),
	  lz4Stream(LZ4_createStream())
{
	fd = io::open(filename.c_str(), O_RDONLY, 0644);
	if unlikely(fd < 0) {
		LZ4_freeStream(lz4Stream);
		throw MSG_LZ4IOError("Cannot open file: %s", filename.c_str());
	}
}


LZ4CompressFile::~LZ4CompressFile()
{
	io::close(fd);
	LZ4_freeStream(lz4Stream);
}


std::string
LZ4CompressFile::init()
{
	if (io::lseek(fd, 0, SEEK_SET) != 0) {
		throw MSG_LZ4IOError("IO error: lseek");
	}

	return next();
}


std::string
LZ4CompressFile::next()
{
	char* const inpPtr = &buffer[_offset];

	// Read line to the ring buffer.
	int inpBytes = static_cast<int>(io::read(fd, inpPtr, block_size));
	if (inpBytes <= 0) {
		return std::string();
	}

	const int cmpBytes = LZ4_compress_fast_continue(lz4Stream, inpPtr, cmpBuf, inpBytes, cmpBuf_size, 1);
	if (cmpBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_compress_fast_continue failed!");
	}

	size_t blockBytes = sizeof(uint16_t) + cmpBytes;
	char* const blockStream = (char*)malloc(blockBytes);

	write_uint16(blockStream, (uint16_t)cmpBytes);
	write_bin(blockStream + sizeof(uint16_t), cmpBuf, cmpBytes);

	// Add and wraparound the ringbuffer offset
	_offset += inpBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, inpPtr, inpBytes);

	_size += blockBytes;
	std::string res(blockStream, blockBytes);
	free(blockStream);

	return res;
}


LZ4CompressDescriptor::LZ4CompressDescriptor(int& fildes, int seed)
	: LZ4BlockStreaming(LZ4_BLOCK_SIZE, seed),
	  lz4Stream(LZ4_createStream()),
	  fd(fildes),
	  read_bytes(LZ4_BLOCK_SIZE) { }


LZ4CompressDescriptor::~LZ4CompressDescriptor()
{
	LZ4_freeStream(lz4Stream);
}


std::string
LZ4CompressDescriptor::init()
{
	if (io::lseek(fd, 0, SEEK_SET) != 0) {
		throw MSG_LZ4IOError("IO error: lseek");
	}

	return next();
}


std::string
LZ4CompressDescriptor::next()
{
	if (read_bytes == 0) {
		return std::string();
	}

	char* const inpPtr = &buffer[_offset];

	// Read line to the ring buffer.
	int inpBytes = static_cast<int>(io::read(fd, inpPtr, static_cast<size_t>(block_size) > read_bytes ? read_bytes : block_size));
	if (inpBytes <= 0) {
		return std::string();
	}
	read_bytes -= inpBytes;

	const int cmpBytes = LZ4_compress_fast_continue(lz4Stream, inpPtr, cmpBuf, inpBytes, cmpBuf_size, 1);
	if (cmpBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_compress_fast_continue failed!");
	}

	size_t blockBytes = sizeof(uint16_t) + cmpBytes;
	char* const blockStream = (char*)malloc(blockBytes);

	write_uint16(blockStream, (uint16_t)cmpBytes);
	write_bin(blockStream + sizeof(uint16_t), cmpBuf, cmpBytes);

	// Add and wraparound the ringbuffer offset
	_offset += inpBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, inpPtr, inpBytes);

	_size += blockBytes;
	std::string res(blockStream, blockBytes);
	free(blockStream);

	return res;
}


LZ4DecompressData::LZ4DecompressData(const char* data_, size_t data_size_, int seed)
	: LZ4BlockStreaming(LZ4_BLOCK_SIZE, seed),
	  lz4StreamDecode(LZ4_createStreamDecode()),
	  data(data_),
	  data_size(data_size_) { }


LZ4DecompressData::~LZ4DecompressData()
{
	LZ4_freeStreamDecode(lz4StreamDecode);
}


std::string
LZ4DecompressData::init()
{
	data_offset = 0;
	return next();
}


std::string
LZ4DecompressData::next()
{
	if (data_size == data_offset) {
		return std::string();
	}

	if (sizeof(uint16_t) > (data_size - data_offset)) {
		throw MSG_LZ4CorruptVolume("Data is corrupt");
	}

	uint16_t cmpBytes = 0;
	read_uint16(data + data_offset, &cmpBytes);
	data_offset += sizeof(uint16_t);

	if (cmpBytes > (data_size - data_offset)) {
		throw MSG_LZ4CorruptVolume("Data is corrupt");
	}

	read_bin(data + data_offset, cmpBuf, cmpBytes);
	data_offset += cmpBytes;

	char* const decPtr = &buffer[_offset];
	const int decBytes = LZ4_decompress_safe_continue(lz4StreamDecode, cmpBuf, decPtr, cmpBytes, (int)block_size);

	if (decBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_decompress_safe_continue failed!");
	}

	char* const blockStream = (char*)malloc(decBytes);
	write_bin(blockStream, decPtr, decBytes);

	// Add and wraparound the ringbuffer offset
	_offset += decBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, blockStream, decBytes);

	_size += decBytes;
	std::string result(blockStream, decBytes);
	free(blockStream);

	return result;
}


LZ4DecompressFile::LZ4DecompressFile(const std::string& filename, int seed)
	: LZ4BlockStreaming(LZ4_BLOCK_SIZE, seed),
	  lz4StreamDecode(LZ4_createStreamDecode()),
	  data((char*)malloc(LZ4_FILE_READ_SIZE))
{
	fd = io::open(filename.c_str(), O_RDONLY, 0644);
	if unlikely(fd < 0) {
		free(data);
		LZ4_freeStreamDecode(lz4StreamDecode);
		throw MSG_LZ4IOError("Cannot open file: %s", filename.c_str());
	}
}


LZ4DecompressFile::~LZ4DecompressFile()
{
	io::close(fd);
	free(data);
	LZ4_freeStreamDecode(lz4StreamDecode);
}


std::string
LZ4DecompressFile::init()
{
	if (io::lseek(fd, 0, SEEK_SET) != 0) {
		throw MSG_LZ4IOError("IO error: lseek");
	}

	if unlikely((data_size = io::read(fd, data, LZ4_FILE_READ_SIZE)) < 0) {
		throw MSG_LZ4IOError("IO error: read");
	}

	data_offset = 0;
	return next();
}


std::string
LZ4DecompressFile::next()
{
	if (data_offset == static_cast<size_t>(data_size)) {
		if unlikely((data_size = io::read(fd, data, LZ4_FILE_READ_SIZE)) < 0) {
			throw MSG_LZ4IOError("IO error: read");
		}
		if (data_size == 0) {
			return std::string();
		}
		data_offset = 0;
	}

	uint16_t cmpBytes = 0;
	size_t res_size = data_size - data_offset;
	if (sizeof(uint16_t) > res_size) {
		read_partial_uint16(data + data_offset, &cmpBytes, res_size);
		data_offset = sizeof(uint16_t) - res_size;
		if ((data_size = io::read(fd, data, LZ4_FILE_READ_SIZE)) < static_cast<ssize_t>(data_offset)) {
			throw MSG_LZ4CorruptVolume("File is corrupt");
		}
		read_partial_uint16(data, &cmpBytes, data_offset, res_size);
	} else {
		read_uint16(data + data_offset, &cmpBytes);
		data_offset += sizeof(uint16_t);
	}

	res_size = data_size - data_offset;
	if (cmpBytes > res_size) {
		read_partial_bin(data + data_offset, cmpBuf, res_size);
		data_offset = cmpBytes - res_size;
		if ((data_size = io::read(fd, data, LZ4_FILE_READ_SIZE)) < static_cast<ssize_t>(data_offset)) {
			throw MSG_LZ4CorruptVolume("File is corrupt");
		}
		read_partial_bin(data, cmpBuf, data_offset, res_size);
	} else {
		read_bin(data + data_offset, cmpBuf, cmpBytes);
		data_offset += cmpBytes;
	}

	char* const decPtr = &buffer[_offset];
	const int decBytes = LZ4_decompress_safe_continue(lz4StreamDecode, cmpBuf, decPtr, cmpBytes, (int)block_size);

	if (decBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_decompress_safe_continue failed!");
	}

	char* const blockStream = (char*)malloc(decBytes);
	write_bin(blockStream, decPtr, decBytes);

	// Add and wraparound the ringbuffer offset
	_offset += decBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, blockStream, decBytes);

	_size += decBytes;
	std::string result(blockStream, decBytes);
	free(blockStream);

	return result;
}


LZ4DecompressDescriptor::LZ4DecompressDescriptor(int& fildes, int seed)
	: LZ4BlockStreaming(LZ4_BLOCK_SIZE, seed),
	  lz4StreamDecode(LZ4_createStreamDecode()),
	  fd(fildes),
	  read_bytes(LZ4_FILE_READ_SIZE),
	  data((char*)malloc(LZ4_FILE_READ_SIZE)) { }


LZ4DecompressDescriptor::~LZ4DecompressDescriptor()
{
	free(data);
	LZ4_freeStreamDecode(lz4StreamDecode);
}


std::string
LZ4DecompressDescriptor::init()
{
	if unlikely(fd <= 0) {
		MSG_LZ4Exception("Incorrect descriptor file: %d", fd);
	}

	if unlikely((data_size = io::read(fd, data, read_bytes > LZ4_FILE_READ_SIZE ? LZ4_FILE_READ_SIZE : read_bytes)) < 0) {
		throw MSG_LZ4IOError("IO error: read");
	}

	data_offset = 0;
	return next();
}


std::string
LZ4DecompressDescriptor::next()
{
	if (read_bytes == 0) {
		return std::string();
	}

	if (data_offset == static_cast<size_t>(data_size)) {
		if unlikely((data_size = io::read(fd, data, read_bytes > LZ4_FILE_READ_SIZE ? LZ4_FILE_READ_SIZE : read_bytes)) <= 0) {
			throw MSG_LZ4IOError("IO error: read");
		}
		data_offset = 0;
	}

	uint16_t cmpBytes = 0;
	size_t res_size = data_size - data_offset;
	if (sizeof(uint16_t) > res_size) {
		read_partial_uint16(data + data_offset, &cmpBytes, res_size);
		data_offset = sizeof(uint16_t) - res_size;
		read_bytes -= res_size;
		if ((data_size = io::read(fd, data, read_bytes > LZ4_FILE_READ_SIZE ? LZ4_FILE_READ_SIZE : read_bytes)) < static_cast<ssize_t>(data_offset)) {
			throw MSG_LZ4CorruptVolume("File is corrupt");
		}
		read_partial_uint16(data, &cmpBytes, data_offset, res_size);
		read_bytes -= data_offset;
	} else {
		read_uint16(data + data_offset, &cmpBytes);
		data_offset += sizeof(uint16_t);
		read_bytes -= sizeof(uint16_t);
	}

	res_size = data_size - data_offset;
	if (cmpBytes > res_size) {
		read_partial_bin(data + data_offset, cmpBuf, res_size);
		data_offset = cmpBytes - res_size;
		read_bytes -= res_size;
		if ((data_size = io::read(fd, data, read_bytes > LZ4_FILE_READ_SIZE ? LZ4_FILE_READ_SIZE : read_bytes)) < static_cast<ssize_t>(data_offset)) {
			throw MSG_LZ4CorruptVolume("File is corrupt");
		}
		read_partial_bin(data, cmpBuf, data_offset, res_size);
		read_bytes -= data_offset;
	} else {
		read_bin(data + data_offset, cmpBuf, cmpBytes);
		data_offset += cmpBytes;
		read_bytes -= cmpBytes;
	}

	char* const decPtr = &buffer[_offset];
	const int decBytes = LZ4_decompress_safe_continue(lz4StreamDecode, cmpBuf, decPtr, cmpBytes, (int)block_size);

	if (decBytes <= 0) {
		throw MSG_LZ4Exception("LZ4_decompress_safe_continue failed! [decBytes: %d]", decBytes);
	}

	char* const blockStream = (char*)malloc(decBytes);
	write_bin(blockStream, decPtr, decBytes);

	// Add and wraparound the ringbuffer offset
	_offset += decBytes;
	if (_offset >= static_cast<size_t>(LZ4_RING_BUFFER_BYTES - block_size)) {
		_offset = 0;
	}

	XXH32_update(xxh_state, blockStream, decBytes);

	_size += decBytes;
	std::string result(blockStream, decBytes);
	free(blockStream);

	return result;
}
